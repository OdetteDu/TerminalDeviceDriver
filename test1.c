#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <terminals.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    InitTerminalDriver();
    InitTerminal(1);
	char *buf = malloc(10*sizeof(char));
	ReadTerminal(1,buf,10);
	printf("Recived %s \n", buf);

    sleep(3);

    exit(0);
}
