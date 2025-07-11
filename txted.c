#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>

#define TXTED_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
/**
 * Map character to Ctrl+<key> combo (e.g., 'A' -> Ctrl+A)
 * C doesn't have binary literals so we use hex since it's also more concise, it maps to 00011111, which means that we are disabling the
 * ctrl-key equivalent of that key using the bitwise AND stripping bit 5 and 6.
 */

enum editorKey {
    ARROW_LEFT = 1000, 
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN
};

/** REGION: data */
struct editorConfig
{
    int cursor_x;
    int cursor_y;
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

/**
 * Print error message and exit, restoring terminal state.
 */
void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    // reference for these two write calls at line 110

    perror(s);
    exit(1);
}
/** REGION: terminal */
/**
 * Disable raw mode and restore original terminal settings.
 */
void disableRawMode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

/**
 * Enable raw mode for terminal input.
 */
void enableRawMode(void)
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) // get attr inside raw
        die("tcgetattr");
    atexit(disableRawMode); // calls this function when exiting (through normal exiting).

    struct termios raw = E.orig_termios;

    raw.c_cflag |= (CS8);
    // using the or we are not disabling, rather we are adding a bit mask
    // CS stands for character size, set to 8
    // cflag stands for "control flag"

    raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
    /**
     * i flag is referred as "input flag"
     * ICRNL Disables ctrl M
     * IXON disables ctrl
     */

    raw.c_oflag &= ~(OPOST);
    /**
     * oflag is referred as output flags
     * OPOST Flag disables Post Processing output for /n /r
     */

    // disables ECHO
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /**
     * ECHO prints every typed char into terminal. ~ flips the bits, bitwise & operator is useful to set to 0 all the bits in the ECHO
     * SUSPEND CANON MODE
     * SUSPEND ISIG (CTRL Z & CTRL C FLAGS) (i can only quit if I press q (line 41))
     * SUSPEND IEXTEN (Suspend CTRL O & CTRL V)
     * lflag is for "local flags", can be used as a dumping ground for other states.
     */

    // TCA Flush argument specifies when to apply the set.
    // it waits for all pending output to be written to the terminal.

    // MIN num of bytes allowed before returning
    raw.c_cc[VMIN] = 0;
    // Sets a timeout
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr"); // set
}

/**
 * Read a single keypress from standard input.
 */
int editorReadKey(void)
{
    // read 1 byte from the std input into char c. Keep doing it until there are no more bytes to read. Returns the n of bytes it read.
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    
    if(c == '\x1b') {
        char seq[3];

        if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if(seq[0] == '[') {
            if(seq[1] >= '0' && seq[1] <= 9) {
                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if(seq[2] == '~') {
                    switch (seq[1]) {
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                    }
                }
            } else 
            switch (seq[1]) {
                case 'A' : return ARROW_UP;
                case 'B' : return ARROW_DOWN;
                case 'C' : return ARROW_RIGHT;
                case 'D' : return ARROW_LEFT;
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}


/**
 * Fallback method to get the window size, the hard way, using the cursor position to determine the size.
 */
int getCursorPosition(int *rows, int *cols)
{
    // TODO FIND BUFF EXPLANINATION.
    char buffer[32];
    unsigned int i = 0;

    /**
     * The n command queries the terminal for status information, the 6 argument asks for the cursos position.
     * on the write we pass the STDOUTPUT, that can be read in the read() using the STDINPUT.
     */
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    /**
     * We use the buffer to read the response from the escape sequence, when we reach the R command we add a terminator character
     */
    while (i < sizeof(buffer) - 1)
    {
        if (read(STDIN_FILENO, &buffer[i], 1) != 1)
            break;
        if (buffer[i] == 'R')
            break;
        i++;
    }

    buffer[i] = '\0';
    /* We start printing at index 1, otherwise the terminal would interpret the escape sequence, not printing it.*/
    if (buffer[0] != '\x1b' || buffer[1] != '[') // buffer should contain <esc>[24;80
        return -1;
    /**
     * we are passing a string of the form 24;80 to sscanf().
     * We are also passing it the string %d;%d which tells it to parse two integers separated by a ;
     * and put the values into the rows and cols variables.
     */
    if (sscanf(&buffer[2], "%d;%d", rows, cols) != 2)
        return -1;

    editorReadKey();

    return -1;
}

/**
 * Get the size of the terminal window.
 */
int getWindowSize(int *rows, int *cols)
{
    struct winsize ws; // from ioctl.h

    /**
     * TIOCGWINSZ - Terminal IOCtl (which itself stands for Input/Output Control) Get WINdow SiZe.)
     * ioctl() will place the number of columns wide and the number of rows high the terminal is into the given winsize struct
     */
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) // fallback method of getting the window size. (ioctl doesnt work on some systems)
            // position the cursor at the bottom-right of the screen, then use escape sequences that let us query the position of the cursor.
            // C command pushes the cursor to the right, B pushes the cursor to the bottom, we pass 999 as an argument so we're sure its on the bottom right
            // x1b is the byte sequence that is equivalent to <ESC>
            return -1;
        return getCursorPosition(rows, cols); // replies with an escape sequence.
    }

    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}
/** REGION: append buffer */

/**
 * We need a dynamic string type that supports appending, since C doesn't have it by default, let's make our own
 */
struct append_buffer
{
    char *buffer;
    int len;
};

#define ABUFF_INIT {NULL, 0} //{pointer, length}

/** REGION: input */

void editorMoveCursor(int key)
{
    switch (key)
    {
    case ARROW_LEFT:
        if(E.cursor_x != 0){ 
            E.cursor_x--;
        }
        break;
    case ARROW_RIGHT:
        if(E.cursor_x != -1){ 
            E.cursor_x++;
        }
        break;
    case ARROW_UP:
        if(E.cursor_y != 0){ 
            E.cursor_y--;
        }
        break;
    case ARROW_DOWN:
        if(E.cursor_y != -1){ 
            E.cursor_y++;
        }
        break;
    }
}

/**
 * Process a keypress from the user.
 */
void editorProcessKeypress(void)
{
    int c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        // reference for these two write calls at line 110

        exit(0);
        break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    }
}

/**
 * Append a new string to our current one.
 */
void append_string(struct append_buffer *ab, const char *s, int len)
{
    char *new = realloc(ab->buffer, ab->len + len); // Make enough space to append the new string
    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len); // We copy the new string to the end of our current one ab->len is the adress of the end of the current string
    ab->buffer = new;              // the new buffer is the old string plus the appended value
    ab->len += len;                // the new length is the old one plus the new string length
}

/**
 * Destructor for append_buffer.
 */
void ab_free(struct append_buffer *ab)
{
    free(ab->buffer);
}

/** REGION: output */
/**
 * Draw the rows of the editor, including the welcome message and tildes.
 */
void editorDrawRows(struct append_buffer *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        if (y == E.screenrows / 3)
        {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                                      "Kilo editor -- version %s", TXTED_VERSION);
            if (welcomelen > E.screencols)
                welcomelen = E.screencols;

            int padding = (E.screencols - welcomelen) / 2;
            if (padding)
            {
                append_string(ab, "~", 1);
                padding--;
            }
            while (padding--)
                append_string(ab, " ", 1);

            append_string(ab, welcome, welcomelen);
        }
        else
        {
            append_string(ab, "~", 1);
        }
        append_string(ab, "\x1b[K", 3);
        if (y < E.screenrows - 1)
        {
            append_string(ab, "\r\n", 2);
        }
    }
}

/**
 * Refresh the editor screen, drawing all rows and repositioning the cursor.
 */
void editorRefreshScreen(void)
{
    struct append_buffer ab = ABUFF_INIT;
    append_string(&ab, "\x1b[?25l", 6); // l command is for "reset mode"
    // append_string(&ab, "\x1b[2J", 4); //commented to increase perfomance, since clearing everytime it's less optimal.
    // Write 4 bytes to the terminal: an escape sequence (\x1b[2J) that clears the screen.
    // Escape sequences start with \x1b (ESC) and control terminal formatting.
    // The escape sequence \x1b[2J uses the 'J' command (Erase In Display) with argument 2 to clear the entire screen.
    // Other options: [0J clears from cursor to end, [1J clears from start to cursor, [J defaults to [0J.

    append_string(&ab, "\x1b[H", 3);
    // Repositions the cursor at the top left side of the screen
    // The escape sequence \x1b[H uses the 'H' command (Cursor Position) to move the cursor.
    // It takes two arguments: row and column (e.g., \x1b[12;40H centers the cursor on an 80×24 screen).

    editorDrawRows(&ab);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", E.cursor_y + 1, E.cursor_x + 1);
    append_string(&ab, buffer, strlen(buffer));

    // append_string(&ab, "\x1b[H", 3);    // reposition again after printing tilde rows
    append_string(&ab, "\x1b[?25h", 6); // h command is for "set mode"

    write(STDOUT_FILENO, ab.buffer, ab.len);
    ab_free(&ab);
}

/** REGION: init */

/**
 * Initialize the editor state and get the window size.
 */
void initEditor(void)
{
    E.cursor_x = 0;
    E.cursor_y = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main(void)
{
    enableRawMode();
    initEditor();

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
