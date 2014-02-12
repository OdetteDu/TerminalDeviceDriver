#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <terminals.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    InitTerminalDriver();
    InitTerminal(1);

    sleep(300);
    exit(0);
}
