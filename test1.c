#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <terminals.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    InitTerminalDriver();
    InitTerminal(1);

	char *string = "abcdefg\nhijklmn\nopqrst\nuvwxyz";
	int length = 29;

	WriteTerminal(1, string, length);
	char *buf = malloc(10 * sizeof(char));
	ReadTerminal(1, buf, 10);

    sleep(300);
    exit(0);
}
