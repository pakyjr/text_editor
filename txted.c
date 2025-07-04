#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

#define CTRL_KEY(k) ((k) & 0x1f)
/**  Map character to Ctrl+<key> combo (e.g., 'A' -> Ctrl+A)
 * C doens't have binary literals so we use hexa since it's also more concise, it maps to 00011111, which means that we are disabling the
 * ctrl-key equivalent of that key using the bitwise AND stripping bit 5 and 6.
 */

/** REGION: data */
struct termios og_termios;

void die(const char *s)
{
    perror(s);
    exit(1);
}
/** REGION: terminal */
void disableRawMode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &og_termios) == -1)
        die("tcsetattr");
}

void enableRawMode(void)
{
    if (tcgetattr(STDIN_FILENO, &og_termios) == -1) // get attr inside raw
        die("tcgetattr");
    atexit(disableRawMode); // calls this function when exiting (through normal exiting).

    struct termios raw = og_termios;

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

/** REGION: input */
void editorProcessKeypress()
{
    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        exit(0);
        break;
    }
}

/** REGION: output */
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
}

/** REGION: init */
int main(void)
{
    enableRawMode();

    while (1)
    {
        editorRefreshScren();
        editorProcessKeypress();
    }

    return 0;
}
