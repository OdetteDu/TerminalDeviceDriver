#include <hardware.h>
#include <terminals.h>
#include <threads.h>
#include <stdio.h>
#include <stdlib.h>

#define INPUT_BUFFER_CAPACITY 4096
#define ECHO_BUFFER_CAPACITY 10

struct Buffer
{	  
	char inputBuffer[INPUT_BUFFER_CAPACITY];
	int inputBufferLength;

	char echoBuffer[ECHO_BUFFER_CAPACITY];
	int echoBufferLength;
	int echoBufferPopIndex;
	int echoBufferPushIndex;

	char *outputBuffer;
	int outputBufferLength;
};

struct Buffer buffers[MAX_NUM_TERMINALS];
struct termstat statistics;

static cond_id_t readLine[MAX_NUM_TERMINALS];
static cond_id_t writeCharacter[MAX_NUM_TERMINALS];
static cond_id_t writeLine[MAX_NUM_TERMINALS];

int PushCharIntoEchoBuffer(int term, char c)
{
	if( buffers[term].echoBufferLength < ECHO_BUFFER_CAPACITY - 1 )
	{
		buffers[term].echoBuffer[buffers[term].echoBufferPushIndex] = c;
		buffers[term].echoBufferLength ++;
		buffers[term].echoBufferPushIndex ++;

		if( buffers[term].echoBufferPushIndex >= ECHO_BUFFER_CAPACITY )
		{
			buffers[term].echoBufferPushIndex = buffers[term].echoBufferPushIndex % ECHO_BUFFER_CAPACITY;
		}
		
		//printf("Push Echo Buffer: [term: %d, Length: %d, PushIndex: %d, PopIndex: %d, Buffer: %s].\n", term, buffers[term].echoBufferLength, buffers[term].echoBufferPushIndex, buffers[term].echoBufferPopIndex, buffers[term].echoBuffer);

		return 0;
	}
	else
	{
		printf("Push char to Echo Buffer: The echo buffer [term: %d, char: %c] is full.\n", term, c);
		return -1;
	}
}

char PopCharFromEchoBuffer(int term)
{
	if( buffers[term].echoBufferLength > 0 )
	{
		char c = buffers[term].echoBuffer[buffers[term].echoBufferPopIndex];

		//printf("Pop Char: [term: %d, char: %c].\n", term, c);

		buffers[term].echoBufferLength --;
		buffers[term].echoBufferPopIndex ++;

		if( buffers[term].echoBufferPopIndex >= ECHO_BUFFER_CAPACITY )
		{
			buffers[term].echoBufferPopIndex = buffers[term].echoBufferPopIndex % ECHO_BUFFER_CAPACITY;
		}
		
		//printf("Pop Echo Buffer: [term: %d, Length: %d, PushIndex: %d, PopIndex: %d, Buffer: %s].\n", term, buffers[term].echoBufferLength, buffers[term].echoBufferPushIndex, buffers[term].echoBufferPopIndex, buffers[term].echoBuffer);

		return c;
	}
	else
	{
		printf("Pop char to Echo Buffer: The echo buffer [term: %d] is empty.\n", term);
		return 0;
	}
}

void ReceiveInterrupt(int term)
{
	Declare_Monitor_Entry_Procedure();
	char c = ReadDataRegister(term);
	printf("Receive Interrupt: [term: %d, char: %c].\n", term, c);
	//check if c is a special character, such as space, delete, or tab
	if( c == '\r' )
	{
		//for echo buffer, convert to be '\r' '\n'

		//for input buffer, convert to be '\n'
	}
	else if ( c == '\b' || c == '\177' )
	{
		//for echo buffer, has char, convert to be '\b' ' ' '\b'
		//empty, convert to be '\7'

		//for input buffer, has char, remove the character
		//empty, ignore
	}
	else
	{
		//put the char in the echo buffer
		PushCharIntoEchoBuffer(term, c);

		//put the char in the input buffer

	}

	CondSignal(writeCharacter[term]);
	
}

void TransmitInterrupt(int term)
{
	Declare_Monitor_Entry_Procedure();

	//check if echo buffer is empty
	while ( buffers[term].echoBufferLength == 0 && buffers[term].outputBufferLength == 0 )
	{
		CondWait(writeCharacter[term]);
	}

	if ( buffers[term].echoBufferLength != 0)
	{	  
		char c = PopCharFromEchoBuffer(term);
		WriteDataRegister(term, c);
		printf("Write Data Register from echo: [term: %d, char: %c].\n", term, c);
	}
	else if ( buffers[term].outputBufferLength != 0)
	{
		//Output from the output buffer
	}
	else
	{
		printf("Impossible! Both echo and output buffer are empty.");
	}
}

int WriteTerminal(int term, char *buf, int buflen)
{
	Declare_Monitor_Entry_Procedure();
	return buflen;
}

int ReadTerminal(int term, char *buf, int buflen)
{
	Declare_Monitor_Entry_Procedure();
	return buffers[term].inputBufferLength;
}

int InitTerminal(int term)
{
	InitHardware(term);
	WriteDataRegister(term, 'a');
	buffers[term].inputBufferLength = 0;
	buffers[term].echoBufferLength = 0;
	buffers[term].echoBufferPushIndex = 0;
	buffers[term].echoBufferPopIndex = 0;
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
		writeCharacter[i] = CondCreate();
		readLine[i] = CondCreate();
		writeLine[i] = CondCreate();
	}

	statistics.tty_in = 0;
	statistics.tty_out = 0;
	statistics.user_in = 0;
	statistics.user_out = 0;

	return 0;
}


