#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

struct termios original_termios;

void disable_raw_mode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

void enable_raw_mode()
{
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(disable_raw_mode);
    struct termios raw = original_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_lflag &= ~OPOST;
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void panic(const char *message)
{
    perror(message);
    exit(1);
}

int main()
{
    enable_raw_mode();
    while (true)
    {
        char c = '\0';
        read(STDIN_FILENO, &c, 1);
        if (iscntrl(c))
        {
            printf("%d\r\n", c);
        }
        else
        {
            printf("%d (%c)\r\n", c, c);
        }
        if (c == 'q')
        {
            break;
        }
    }
    return 0;
}
