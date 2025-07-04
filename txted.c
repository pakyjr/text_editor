#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

struct termios og_termios;

void disableRawMode(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &og_termios);
}

void enableRawMode(void)
{
    tcgetattr(STDIN_FILENO, &og_termios); // get attr inside raw
    atexit(disableRawMode);               // calls this function when exiting (through normal exiting).

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

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // set
}

int main(void)
{
    enableRawMode();

    // Switched the terminal from canonical to raw mode
    // read 1 byte from the std input into char c. Keep doing it until there are no more bytes to read. Returns the n of bytes it read.
    while (1)
    {
        char c = '\0';
        read(STDIN_FILENO, &c, 1);

        if (iscntrl(c)) // test wether a char is control char. (non printable characters)
        {
            printf("%d\r\n", c);
        }
        else
        {
            printf("%d ('%c')\r\n", c, c);
        }

        if (c == 'q')
            break;
    }

    return 0; // return 0 at the end of the file
}
