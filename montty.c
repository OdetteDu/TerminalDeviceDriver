#include <hardware.h>
#include <terminals.h>
#include <stdio.h>
#include <stdlib.h>


struct Buffer
{	  
	char *inputBuffer;
	int inputBufferLength;

	char *echoBuffer;
	int echoBufferLength;

	char *outputBuffer;
	int outputBufferLength;
};

struct Buffer buffers[4];
struct termstat statistics;

void ReceiveInterrupt(int term)
{
	char c = ReadDataRegister(term);
	//check if c is a special character, such as space, delete, or tab
	
	//put c into the input buffer
	buffers[term].inputBufferLength++;
	char *inputBufferPointer = buffers[term].inputBuffer;
	inputBufferPointer += buffers[term].inputBufferLength;
	*inputBufferPointer = c;
	printf("receive interrupt for terminal %d, for char %c, with total inputBufferLength %d.", term, *inputBufferPointer, buffers[term].inputBufferLength);
	
	//put c into the echo buffer
}

void TransmitInterrupt(int term)
{
	printf("transmit interrupt");

	//check if echo buffer is empty
	
	//check if output buffer is empty
	if(buffers[term].outputBufferLength != 0)
	{
		char c = *(buffers[term].outputBuffer);
		buffers[term].outputBuffer++;
		buffers[term].outputBufferLength--;
		WriteDataRegister(term, c);
	}
}

int WriteTerminal(int term, char *buf, int buflen)
{
	buffers[term].outputBuffer = buf;
	buffers[term].outputBufferLength = buflen;
	return buflen;
}

int ReadTerminal(int term, char *buf, int buflen)
{
	  return 0;
}

int InitTerminal(int term)
{
	buffers[term].inputBuffer = malloc (256 * sizeof(char));
	buffers[term].inputBufferLength = 0;
	buffers[term].echoBuffer = malloc (256 * sizeof(char));
	buffers[term].echoBufferLength = 0;
	buffers[term].outputBufferLength = 0;
	return 0;
}

int TerminalDriverStatistics(struct termstat *stats)
{
	  return 0;
}

int InitTerminalDriver()
{
	statistics.tty_in = 0;
	statistics.tty_out = 0;
	statistics.user_in = 0;
	statistics.user_out = 0;
	  return 0;
}


