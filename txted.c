#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define CTRL_KEY(k) ((k) & 0x1f)
/**  Map character to Ctrl+<key> combo (e.g., 'A' -> Ctrl+A)
 * C doens't have binary literals so we use hexa since it's also more concise, it maps to 00011111, which means that we are disabling the
 * ctrl-key equivalent of that key using the bitwise AND stripping bit 5 and 6.
 */

/** REGION: data */
struct editorConfig
{
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    // reference for these two write calls at line 110

    perror(s);
    exit(1);
}
/** REGION: terminal */
void disableRawMode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

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
    /*
    i flag is referred as "input flag"
    ICRNL Disables ctrl M
    IXON disables ctrl
    */

    raw.c_oflag &= ~(OPOST);
    // oflag is referred as output flags
    // OPOST Flag disables Post Processing output for /n /r

    // disables ECHO
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* ECHO prints every typed char into terminal. ~ flips the bits, bitwise & operator is useful to set to 0 all the bits in the ECHO
    SUSPEND CANON MODE
    SUSPEND ISIG (CTRL Z & CTRL C FLAGS) (i can only quit if I press q (line 41))
    SUSPEND IEXTEN (Suspend CTRL O & CTRL V)
    lflag is for "local flags", can be used as a dumping ground for other states. */

    // TCA Flush argument specifies when to apply the set.
    // it waits for all pending output to be written to the terminal.

    // MIN num of bytes allowed before returning
    raw.c_cc[VMIN] = 0;
    // Sets a timeout
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr"); // set
}

char editorReadKey()
{
    // read 1 byte from the std input into char c. Keep doing it until there are no more bytes to read. Returns the n of bytes it read.
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }

    return c;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws; // from ioctl.h

    // TIOCGWINSZ - Terminal IOCtl (which itself stands for Input/Output Control) Get WINdow SiZe.)
    // ioctl() will place the number of columns wide and the number of rows high the terminal is into the given winsize struct
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return -1;

    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

/** REGION: input */
void editorProcessKeypress()
{
    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        // reference for these two write calls at line 110

        exit(0);
        break;
    }
}

/** REGION: output */
void editorDrawRows()
{
    int y;
    for (y = 0; y < E.screenrows; y++) // 24rows temp.
    {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void editorRefreshScren()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // Write 4 bytes to the terminal: an escape sequence (\x1b[2J) that clears the screen.
    // Escape sequences start with \x1b (ESC) and control terminal formatting.
    // The escape sequence \x1b[2J uses the 'J' command (Erase In Display) with argument 2 to clear the entire screen.
    // Other options: [0J clears from cursor to end, [1J clears from start to cursor, [J defaults to [0J.

    write(STDOUT_FILENO, "\x1b[H", 3);
    // Repositions the cursor at the top left side of the screen
    // The escape sequence \x1b[H uses the 'H' command (Cursor Position) to move the cursor.
    // It takes two arguments: row and column (e.g., \x1b[12;40H centers the cursor on an 80×24 screen).

    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3); // reposition again after printing tilde rows
}

/** REGION: init */

void initEditor()
{
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

int main(void)
{
    enableRawMode();
    initEditor();

    while (1)
    {
        editorRefreshScren();
        editorProcessKeypress();
    }

    return 0;
}
