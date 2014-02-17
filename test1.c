#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <terminals.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    InitTerminalDriver();
    InitTerminal(1);

	sleep(5);
	printf("finish sleeping\n");

	char *string = "abcdefg\nhijklmn\nopqrst\nuvwxyz";
	int length = 29;

	WriteTerminal(1, string, length);
	char *buf = malloc(10 * sizeof(char));
	ReadTerminal(1, buf, 10);

	struct termstat *stats = malloc(sizeof(struct termstat));
	TerminalDriverStatistics(stats);
	printf("[tty_in: %d, tty_out: %d, user_in: %d, user_out: %d].\n", stats->tty_in, stats->tty_out, stats->user_in, stats->user_out);

    sleep(3);
    exit(0);
}
