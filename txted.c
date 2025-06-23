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

    // disables ECHO
    raw.c_lflag &= ~(ECHO | ICANON); // ECHO prints every typed char into terminal. ~ flips the bits, bitwise & operator is useful to set to 0 all the bits in the ECHO
    // lflag is for "local flags", can be used as a dumping ground for other states.

    // TCA Flush argument specifies when to apply the set.
    // it waits for all pending output to be written to the terminal.
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // set
}

int main(void)
{
    enableRawMode();

    // Switched the terminal from canonical to raw mode
    char c;
    // read 1 byte from the std input into char c. Keep doing it until there are no more bytes to read. Returns the n of bytes it read.
    while ((read(STDIN_FILENO, &c, 1) == 1) && c != 'q')
        ;
    return 0; // return 0 at the end of the file
}
