#include <hardware.h>
#include <terminals.h>
#include <threads.h>
#include <stdio.h>
#include <stdlib.h>


struct Buffer
{	  
	char inputBuffer[4096];
	int inputBufferLength;

	char echoBuffer[1024];
	int echoBufferLength;
	int echoBufferStartPosition;

	char *outputBuffer;
	int outputBufferLength;
};

struct Buffer buffers[4];//need to remove 4
struct termstat statistics;

static cond_id_t readTerminal[4];//need to remove 4
static cond_id_t writeTerminal[4];//need to remove 4

void ReceiveInterrupt(int term)
{
	Declare_Monitor_Entry_Procedure();
	char c = ReadDataRegister(term);
	//check if c is a special character, such as space, delete, or tab
	
	//put c into the input buffer
	buffers[term].inputBuffer[buffers[term].inputBufferLength] = c;
	buffers[term].inputBufferLength++;
	printf("receive interrupt for terminal %d, for char %c:%c, with total inputBufferLength %d\n", term, c, buffers[term].inputBuffer[buffers[term].inputBufferLength-1], buffers[term].inputBufferLength);
	CondSignal(readTerminal[term]);
	
	//put c into the echo buffer
	buffers[term].echoBuffer[buffers[term].echoBufferStartPosition+buffers[term].echoBufferLength] = c;//need to check if the echoBuffer goes around
	buffers[term].echoBufferLength++;
}

void TransmitInterrupt(int term)
{
	Declare_Monitor_Entry_Procedure();
	printf("transmit interrupt");

	//check if echo buffer is empty
	if(buffers[term].echoBufferLength != 0)
	{
		char c = buffers[term].echoBuffer[buffers[term].echoBufferStartPosition];
		buffers[term].echoBufferStartPosition++;//need to check if the echoBuffer goes around
		buffers[term].echoBufferLength--;
		WriteDataRegister(term, c);
	}
	//check if output buffer is empty
	else if(buffers[term].outputBufferLength != 0)
	{
		char c = *(buffers[term].outputBuffer);
		buffers[term].outputBuffer++;
		buffers[term].outputBufferLength--;
		WriteDataRegister(term, c);
	}
}

int WriteTerminal(int term, char *buf, int buflen)
{
	Declare_Monitor_Entry_Procedure();
	buffers[term].outputBuffer = buf;
	buffers[term].outputBufferLength = buflen;
	return buflen;
}

int ReadTerminal(int term, char *buf, int buflen)
{
	Declare_Monitor_Entry_Procedure();
	if(buffers[term].inputBufferLength == 0)
	{
		CondWait(readTerminal[term]);	  
	}
	
	buf = buffers[term].inputBuffer;//need to consider how the string is copied
	return buffers[term].inputBufferLength;
}

int InitTerminal(int term)
{
	InitHardware(term);
	buffers[term].inputBufferLength = 0;
	buffers[term].echoBufferLength = 0;
	buffers[term].echoBufferStartPosition = 0;
	buffers[term].outputBufferLength = 0;
	return 0;
}

int TerminalDriverStatistics(struct termstat *stats)
{
	return 0;
}

int InitTerminalDriver()
{
	int i;

	for (i = 0; i < 4; i++)//need to remove 4
	{
		  readTerminal[i] = CondCreate();
		  writeTerminal[i] = CondCreate();
	}
	statistics.tty_in = 0;
	statistics.tty_out = 0;
	statistics.user_in = 0;
	statistics.user_out = 0;
	return 0;
}


