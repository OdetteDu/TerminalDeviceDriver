#include <hardware.h>
#include <terminals.h>
#include <threads.h>
#include <stdio.h>
#include <stdlib.h>

#define INPUT_BUFFER_CAPACITY 4096
#define ECHO_BUFFER_CAPACITY 10

struct Buffer
{	  
	int initialized;

	char inputBuffer[INPUT_BUFFER_CAPACITY];
	int inputBufferLength;
	int inputBufferCurrentLineLength;
	int inputBufferPopIndex;
	int inputBufferPushIndex;

	char echoBuffer[ECHO_BUFFER_CAPACITY];
	int echoBufferLength;
	int echoBufferPopIndex;
	int echoBufferPushIndex;

	char *outputBuffer;
	int outputBufferLength;
};

struct Buffer buffers[MAX_NUM_TERMINALS];
struct termstat statistics;

static cond_id_t hasCharacter[MAX_NUM_TERMINALS];
static cond_id_t writeCharacter[MAX_NUM_TERMINALS];
static cond_id_t writeLine[MAX_NUM_TERMINALS];
static cond_id_t readLine[MAX_NUM_TERMINALS];

int carriageOutputted[MAX_NUM_TERMINALS];

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
		printf("Pop char from Echo Buffer: The echo buffer [term: %d] is empty.\n", term);
		return 0;
	}
}

int PushCharIntoInputBuffer(int term, char c)
{
	if( buffers[term].inputBufferLength < INPUT_BUFFER_CAPACITY - 1 )
	{
		buffers[term].inputBuffer[buffers[term].inputBufferPushIndex] = c;
		buffers[term].inputBufferLength ++;
		buffers[term].inputBufferCurrentLineLength ++;
		buffers[term].inputBufferPushIndex ++;

		if( buffers[term].inputBufferPushIndex >= ECHO_BUFFER_CAPACITY )
		{
			buffers[term].inputBufferPushIndex = buffers[term].inputBufferPushIndex % INPUT_BUFFER_CAPACITY;
		}

		if( c == '\n' )
		{
			buffers[term].inputBufferCurrentLineLength = 0;
			CondSignal(readLine[term]);
		}
		
		printf("Push Input Buffer: [term: %d, Length: %d, CurrentLineLength: %d, PushIndex: %d, PopIndex: %d, Buffer: %s].\n", term, buffers[term].inputBufferLength, buffers[term].inputBufferCurrentLineLength, buffers[term].inputBufferPushIndex, buffers[term].inputBufferPopIndex, buffers[term].inputBuffer);

		return 0;
	}
	else
	{
		printf("Push char to Input Buffer: The input buffer [term: %d, char: %c] is full.\n", term, c);
		return -1;
	}
}

char PopCharFromInputBuffer(int term)
{
	if( buffers[term].inputBufferLength > 0 )
	{
		if ( buffers[term].inputBufferLength == buffers[term].inputBufferCurrentLineLength )
		{
			printf("Trying to pop input character without hitting enter");
		}

		char c = buffers[term].inputBuffer[buffers[term].inputBufferPopIndex];

		printf("Pop Char: [term: %d, char: %c].\n", term, c);

		buffers[term].inputBufferLength --;
		buffers[term].inputBufferPopIndex ++;

		if( buffers[term].inputBufferPopIndex >= INPUT_BUFFER_CAPACITY )
		{
			buffers[term].inputBufferPopIndex = buffers[term].inputBufferPopIndex % INPUT_BUFFER_CAPACITY;
		}
		
		printf("Pop Input Buffer: [term: %d, Length: %d, CurrentLineLength: %d, PushIndex: %d, PopIndex: %d, Buffer: %s].\n", term, buffers[term].echoBufferLength, buffers[term].inputBufferCurrentLineLength, buffers[term].echoBufferPushIndex, buffers[term].echoBufferPopIndex, buffers[term].echoBuffer);

		return c;
	}
	else
	{
		printf("Pop char from Input Buffer: The echo buffer [term: %d] is empty.\n", term);
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
		PushCharIntoEchoBuffer(term, '\r');
		PushCharIntoEchoBuffer(term, '\n');

		//for input buffer, convert to be '\n'
		PushCharIntoInputBuffer(term, '\n');
	}
	else if ( c == '\b' || c == '\177' )
	{
		if ( buffers[term].inputBufferCurrentLineLength > 0 )
		{
			//for echo buffer, has char, convert to be '\b' ' ' '\b'
			PushCharIntoEchoBuffer(term, '\b');
			PushCharIntoEchoBuffer(term, ' ');
			PushCharIntoEchoBuffer(term, '\b');

			//for input buffer, has char, remove the character
			buffers[term].inputBufferLength --;
			buffers[term].inputBufferCurrentLineLength --;
			buffers[term].inputBufferPushIndex --;
		}
		else
		{
			//for echo buffer, empty, convert to be '\7'
			PushCharIntoEchoBuffer(term, '\7');

			//for input buffer, empty, ignore
		}
	}
	else
	{
		//put the char in the echo buffer
		PushCharIntoEchoBuffer(term, c);

		//put the char in the input buffer
		PushCharIntoInputBuffer(term, c);
	}

	CondSignal(hasCharacter[term]);
	
}

void TransmitInterrupt(int term)
{
	Declare_Monitor_Entry_Procedure();

	//check if echo buffer is empty
	while ( buffers[term].echoBufferLength == 0 && buffers[term].outputBufferLength == 0 )
	{
		CondWait(hasCharacter[term]);
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
		char c = *(buffers[term].outputBuffer);

		if( c == '\n' && carriageOutputted[term] == 0 )
		{
			WriteDataRegister(term, '\r');
			carriageOutputted[term] = 1;
		}
		else
		{
			if( c == '\n' )
			{
				  carriageOutputted[term] = 0;
			}

			buffers[term].outputBuffer ++;
			buffers[term].outputBufferLength --;
			printf("Output Buffer: [term: %d, length: %d, buffer: %s].\n", term, buffers[term].outputBufferLength, buffers[term].outputBuffer);

			WriteDataRegister(term, c);
			CondSignal(writeCharacter[term]);
			printf("Write Data Register from output: [term: %d, char: %c].\n", term, c);
		}
	}
	else
	{
		printf("Impossible! Both echo and output buffer are empty.");
	}
}

int WriteTerminal(int term, char *buf, int buflen)
{
	Declare_Monitor_Entry_Procedure();
	while ( buffers[term].outputBufferLength != 0 )
	{
		CondWait(writeLine[term]);
	}

	buffers[term].outputBuffer = buf;
	buffers[term].outputBufferLength = buflen;
	int numberCharacterOutputted = 0;
	
	while ( numberCharacterOutputted < buflen )
	{
		CondWait(writeCharacter[term]);
		numberCharacterOutputted ++; 
	}

	//buffers[term].outputBuffer = NULL;
	//buffers[term].outputBufferLength = 0;
	CondSignal(writeLine[term]);

	return buflen;
}

int ReadTerminal(int term, char *buf, int buflen)
{
	Declare_Monitor_Entry_Procedure();
	while ( buffers[term].inputBufferLength == buffers[term].inputBufferCurrentLineLength )
	{
		CondWait(readLine[term]);
	}

	int numberCharacterRead = 0;
	while ( numberCharacterRead < buflen )
	{
		char c = PopCharFromInputBuffer(term);
		*(buf + numberCharacterRead) = c;
		printf("Read Terminal: [term: %d, count: %d, buffer: %s].\n", term, numberCharacterRead, buf);
		
		if( c == '\n' )
		{
			break;
		}

		numberCharacterRead ++;
	}
	
	return numberCharacterRead;
}

int InitTerminal(int term)
{
	if( term >= MAX_NUM_TERMINALS || term < 0 )
	{
		printf("Invalid terminal number: %d, should be less than %d and greater than or equal to 0", term, MAX_NUM_TERMINALS - 1);
		return -1;
	}

	if( buffers[term].initialized > 0 )
	{
		printf("The terminal: %d you are trying to initialize has already been initialized", term);
		return -1;
	}

	InitHardware(term);
	WriteDataRegister(term, '\r');
	buffers[term].initialized = 1;
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
		hasCharacter[i] = CondCreate();
		writeCharacter[i] = CondCreate();
		writeLine[i] = CondCreate();
		readLine[i] = CondCreate();

		buffers[i].initialized = 0;
	}

	statistics.tty_in = 0;
	statistics.tty_out = 0;
	statistics.user_in = 0;
	statistics.user_out = 0;

	return 0;
}


