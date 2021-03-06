#include <hardware.h>
#include <terminals.h>
#include <threads.h>
#include <stdio.h>
#include <stdlib.h>

#define INPUT_BUFFER_CAPACITY 4096
#define ECHO_BUFFER_CAPACITY 1024

struct Buffer
{	  
	int initialized;
	int isTerminalBusy;

	char inputBuffer[INPUT_BUFFER_CAPACITY];
	int inputBufferLength;
	int inputBufferCurrentLineLength;
	int inputBufferPopIndex;
	int inputBufferPushIndex;

	int echoBufferCarriageStatus;
	int echoBufferBackspaceStatus;
	char echoBuffer[ECHO_BUFFER_CAPACITY];
	int echoBufferLength;
	int echoBufferPopIndex;
	int echoBufferPushIndex;

	char *outputBuffer;
	int outputBufferLength;
};

int terminalDriverInitialized;

struct Buffer buffers[MAX_NUM_TERMINALS];
struct termstat statistics;

static cond_id_t hasCharacter[MAX_NUM_TERMINALS];

static cond_id_t writeCharacter[MAX_NUM_TERMINALS];
static cond_id_t writeLine[MAX_NUM_TERMINALS];
static cond_id_t readLine[MAX_NUM_TERMINALS];

int carriageOutputted[MAX_NUM_TERMINALS];

static int PushCharIntoEchoBuffer(int term, char c)
{
	if( buffers[term].echoBufferLength < ECHO_BUFFER_CAPACITY )
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

static char PopCharFromEchoBuffer(int term)
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

static int PushCharIntoInputBuffer(int term, char c)
{
	if( buffers[term].inputBufferLength < INPUT_BUFFER_CAPACITY )
	{
		buffers[term].inputBuffer[buffers[term].inputBufferPushIndex] = c;
		buffers[term].inputBufferLength ++;
		buffers[term].inputBufferCurrentLineLength ++;
		buffers[term].inputBufferPushIndex ++;

		if( buffers[term].inputBufferPushIndex >= INPUT_BUFFER_CAPACITY )
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

static char PopCharFromInputBuffer(int term)
{
	if( buffers[term].inputBufferLength > 0 )
	{
		if ( buffers[term].inputBufferLength == buffers[term].inputBufferCurrentLineLength )
		{
			printf("Trying to pop input character without hitting enter.\n");
		}

		char c = buffers[term].inputBuffer[buffers[term].inputBufferPopIndex];

		//printf("Pop Char: [term: %d, char: %c].\n", term, c);

		buffers[term].inputBufferLength --;
		buffers[term].inputBufferPopIndex ++;

		if( buffers[term].inputBufferPopIndex >= INPUT_BUFFER_CAPACITY )
		{
			buffers[term].inputBufferPopIndex = buffers[term].inputBufferPopIndex % INPUT_BUFFER_CAPACITY;
		}
		
		printf("Pop Input Buffer: [term: %d, Length: %d, CurrentLineLength: %d, PushIndex: %d, PopIndex: %d, Buffer: %s].\n", term, buffers[term].inputBufferLength, buffers[term].inputBufferCurrentLineLength, buffers[term].inputBufferPushIndex, buffers[term].inputBufferPopIndex, buffers[term].inputBuffer);

		return c;
	}
	else
	{
		printf("Pop char from Input Buffer: The input buffer [term: %d] is empty.\n", term);
		return 0;
	}
}

static int EchoCharacter(int term)
{
	if( term >= MAX_NUM_TERMINALS || term < 0 )
	{
		printf("Invalid terminal number: %d, should be less than %d and greater than or equal to 0", term, MAX_NUM_TERMINALS - 1);
		return -1;
	}

	if( buffers[term].initialized == 0 )
	{
		printf("The terminal: %d you are trying to access has not been initialized.\n", term);
		return -1;
	}

	if ( buffers[term].echoBufferCarriageStatus != 0 )
	{
		  int status = buffers[term].echoBufferCarriageStatus % 2;
		  switch (status)
		  {
			    case 0:
					WriteDataRegister(term, '\r');
					buffers[term].isTerminalBusy = 1;
					statistics.tty_out ++;
					buffers[term].echoBufferCarriageStatus --; 
					//printf("Write Data Register from echo: [term: %d, char: %c].\n", term, '\r');
					break;
				case 1:
					WriteDataRegister(term, '\n');
					buffers[term].isTerminalBusy = 1;
					statistics.tty_out ++;
					buffers[term].echoBufferCarriageStatus --; 
					//printf("Write Data Register from echo: [term: %d, char: %c].\n", term, '\n');
					break;
		  }
	}
	else if ( buffers[term].echoBufferBackspaceStatus != 0 )
	{
		  int status = buffers[term].echoBufferBackspaceStatus % 3;
		  //printf("Echo status: [backspace: %d, status: %d].\n", buffers[term].echoBufferBackspaceStatus, status);
		  switch (status)
		  {
				case 1:
					//printf("Case 1");
					WriteDataRegister(term, '\b');
					buffers[term].isTerminalBusy = 1;
					statistics.tty_out ++;
					buffers[term].echoBufferBackspaceStatus --; 
					//printf("Write Data Register from echo: [term: %d, char: %c].\n", term, '\b');
					break;
				case 2:
					//printf("Case 2");
					WriteDataRegister(term, ' ');
					buffers[term].isTerminalBusy = 1;
					statistics.tty_out ++;
					buffers[term].echoBufferBackspaceStatus --; 
					//printf("Write Data Register from echo: [term: %d, char: %c].\n", term, ' ');
					break;
				case 0:
					//printf("Case 0");
					WriteDataRegister(term, '\b');
					buffers[term].isTerminalBusy = 1;
					statistics.tty_out ++;
					buffers[term].echoBufferBackspaceStatus --; 
					//printf("Write Data Register from echo: [term: %d, char: %c].\n", term, '\b');
					break;
		  }
	}
	else if ( buffers[term].echoBufferLength != 0)
	{	
		char c = PopCharFromEchoBuffer(term);
		WriteDataRegister(term, c);
		buffers[term].isTerminalBusy = 1;
		statistics.tty_out ++;
		//printf("Write Data Register from echo: [term: %d, char: %c].\n", term, c);
	}

	return 0;
}

static int WriteCharacter(int term)
{
	if( term >= MAX_NUM_TERMINALS || term < 0 )
	{
		printf("Invalid terminal number: %d, should be less than %d and greater than or equal to 0.\n", term, MAX_NUM_TERMINALS - 1);
		return -1;
	}

	if( buffers[term].initialized == 0 )
	{
		printf("The terminal: %d you are trying to access has not been initialized.\n", term);
		return -1;
	}

	if ( buffers[term].outputBufferLength != 0)
	{
		//Output from the output buffer
		char c = *(buffers[term].outputBuffer);

		if( c == '\n' && carriageOutputted[term] == 0 )
		{
			WriteDataRegister(term, '\r');
			buffers[term].isTerminalBusy = 1;
			statistics.tty_out ++;
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
			//printf("Output Buffer: [term: %d, length: %d, buffer: %s].\n", term, buffers[term].outputBufferLength, buffers[term].outputBuffer);

			WriteDataRegister(term, c);
			buffers[term].isTerminalBusy = 1;
			statistics.tty_out ++;
			CondSignal(writeCharacter[term]);
			//printf("Write Data Register from output: [term: %d, char: %c].\n", term, c);
		}
	}
	else
	{
		printf("Impossible! Both echo and output buffer are empty.");
	}

	return 0;
}

static int OutputCharacter(int term)
{
	while(buffers[term].echoBufferCarriageStatus == 0 && buffers[term].echoBufferBackspaceStatus == 0 && buffers[term].echoBufferLength == 0 && buffers[term].outputBufferLength == 0)
	{
		//printf("Wait for Characters.\n");
		CondWait(hasCharacter[term]);
	}

	if (buffers[term].echoBufferCarriageStatus != 0 || buffers[term].echoBufferBackspaceStatus != 0 || buffers[term].echoBufferLength != 0)
	{ 
		//printf("Echo Character.\n");
		EchoCharacter(term);
	}
	else if(buffers[term].outputBufferLength != 0)
	{
		//printf("Write Character.\n");
		WriteCharacter(term);
	}

	return 0;
}

extern void ReceiveInterrupt(int term)
{
	Declare_Monitor_Entry_Procedure();
	char c = ReadDataRegister(term);
	statistics.tty_in ++;
	//printf("Receive Interrupt: [term: %d, char: %c].\n", term, c);

	//check if c is a special character, such as space, delete, or tab
	if( c == '\r' )
	{
		//for echo buffer, convert to be '\r' '\n'
		//PushCharIntoEchoBuffer(term, '\r');
		//PushCharIntoEchoBuffer(term, '\n');

		//for input buffer, convert to be '\n'
		if( buffers[term].inputBufferLength == INPUT_BUFFER_CAPACITY )
		{
			printf("Receive Interrupt: The input buffer [term: %d] is full, but receive a carriage return.\n", term);
			buffers[term].inputBufferCurrentLineLength = 0;
			buffers[term].echoBufferCarriageStatus += 2;
			CondSignal(readLine[term]);
		}
		else if( buffers[term].inputBufferLength >= INPUT_BUFFER_CAPACITY )
		{
			printf("Receive Interrupt: The input buffer [term: %d, char: %c] is full.\n", term, c);
			PushCharIntoEchoBuffer(term, '\7');
		}
		else
		{
			PushCharIntoInputBuffer(term, '\n');
			buffers[term].echoBufferCarriageStatus += 2;
		}
	}
	else if ( c == '\b' || c == '\177' )
	{
		if ( buffers[term].inputBufferCurrentLineLength > 0 )
		{
			//for echo buffer, has char, convert to be '\b' ' ' '\b'
			//PushCharIntoEchoBuffer(term, '\b');
			//PushCharIntoEchoBuffer(term, ' ');
			//PushCharIntoEchoBuffer(term, '\b');

			//for input buffer, has char, remove the character
			buffers[term].inputBufferLength --;
			buffers[term].inputBufferCurrentLineLength --;
			if( buffers[term].inputBufferPushIndex > 0 )
			{
				buffers[term].inputBufferPushIndex --;
			}
			else
			{
				buffers[term].inputBufferPushIndex = INPUT_BUFFER_CAPACITY - 1;
			}
			printf("Backspace Input Buffer: [term: %d, Length: %d, CurrentLineLength: %d, PushIndex: %d, PopIndex: %d, Buffer: %s].\n", term, buffers[term].inputBufferLength, buffers[term].inputBufferCurrentLineLength, buffers[term].inputBufferPushIndex, buffers[term].inputBufferPopIndex, buffers[term].inputBuffer);
			buffers[term].echoBufferBackspaceStatus += 3;
		}
		else
		{
			//for echo buffer, empty, convert to be '\7'
			PushCharIntoEchoBuffer(term, '\7');

			//for input buffer, empty, ignore
			printf("Recive Interrupt: The input buffer for [term: %d] is empty.\n", term);
		}
	}
	else
	{
		if ( buffers[term].inputBufferLength < INPUT_BUFFER_CAPACITY )
		{
			PushCharIntoInputBuffer(term, c);
			PushCharIntoEchoBuffer(term, c);
		}
		else
		{
			PushCharIntoEchoBuffer(term, '\7');
			printf("Receive Interrupt: The input buffer [term: %d, char: %c] is full.\n", term, c);
		}
	}


	CondSignal(hasCharacter[term]);

	if(buffers[term].isTerminalBusy == 0)
	{
			OutputCharacter(term);
	}
}

extern void TransmitInterrupt(int term)
{
	Declare_Monitor_Entry_Procedure();
	OutputCharacter(term);
}

extern int WriteTerminal(int term, char *buf, int buflen)
{
	Declare_Monitor_Entry_Procedure();

	//printf("Write Terminal.\n");

	if( term >= MAX_NUM_TERMINALS || term < 0 )
	{
		printf("Invalid terminal number: %d, should be less than %d and greater than or equal to 0.\n", term, MAX_NUM_TERMINALS - 1);
		return -1;
	}

	if( buffers[term].initialized == 0 )
	{
		printf("The terminal: %d you are trying to access has not been initialized.\n", term);
		return -1;
	}

	while ( buffers[term].outputBufferLength != 0 )
	{
		printf("Wait for character to be outputted.\n");
		CondWait(writeLine[term]);
	}

	buffers[term].outputBuffer = buf;
	buffers[term].outputBufferLength = buflen;
	int numberCharacterOutputted = 0;

	CondSignal(hasCharacter[term]);

	if(buffers[term].isTerminalBusy == 0)
	{
		OutputCharacter(term);
		numberCharacterOutputted ++;
	}

	while ( numberCharacterOutputted < buflen )
	{
		CondWait(writeCharacter[term]);
		numberCharacterOutputted ++; 
	}

	statistics.user_in += buflen;
	//buffers[term].outputBuffer = NULL;
	//buffers[term].outputBufferLength = 0;
	CondSignal(writeLine[term]);
	
	return buflen;
}

extern int ReadTerminal(int term, char *buf, int buflen)
{
	Declare_Monitor_Entry_Procedure();

	if( term >= MAX_NUM_TERMINALS || term < 0 )
	{
		printf("Invalid terminal number: %d, should be less than %d and greater than or equal to 0.\n", term, MAX_NUM_TERMINALS - 1);
		return -1;
	}

	if( buffers[term].initialized == 0 )
	{
		printf("The terminal: %d you are trying to access has not been initialized.\n", term);
		return -1;
	}

	while ( buffers[term].inputBufferLength == buffers[term].inputBufferCurrentLineLength )
	{
		CondWait(readLine[term]);
	}

	int numberCharacterRead = 0;
	while ( numberCharacterRead < buflen )
	{
		char c = PopCharFromInputBuffer(term);
		*(buf + numberCharacterRead) = c;
		//printf("Read Terminal: [term: %d, count: %d, buffer: %s].\n", term, numberCharacterRead, buf);
		
		if( c == '\n' || buffers[term].inputBufferLength == 0)
		{
			break;
		}

		numberCharacterRead ++;
	}
	
	if ( buffers[term].inputBufferLength != buffers[term].inputBufferCurrentLineLength )
	{
		  CondSignal(readLine[term]);
	}
	
	statistics.user_out += numberCharacterRead;
	return numberCharacterRead;
}

extern int InitTerminal(int term)
{
	Declare_Monitor_Entry_Procedure();
	
	if( term >= MAX_NUM_TERMINALS || term < 0 )
	{
		printf("Invalid terminal number: %d, should be less than %d and greater than or equal to 0.\n", term, MAX_NUM_TERMINALS - 1);
		return -1;
	}

	if( buffers[term].initialized > 0 )
	{
		printf("The terminal: %d you are trying to initialize has already been initialized.\n", term);
		return -1;
	}

	InitHardware(term);
	buffers[term].initialized = 1;
	buffers[term].inputBufferLength = 0;
	buffers[term].echoBufferLength = 0;
	buffers[term].echoBufferPushIndex = 0;
	buffers[term].echoBufferPopIndex = 0;
	buffers[term].outputBufferLength = 0;

	//WriteDataRegister(term, '\r');
	return 0;
}

extern int TerminalDriverStatistics(struct termstat *stats)
{
	Declare_Monitor_Entry_Procedure();

	if (terminalDriverInitialized == 0 )
	{
		printf("The Terminal Driver has not been initialized. No statistics is available\n");
	}

	stats -> tty_in = statistics.tty_in;
	stats -> tty_out = statistics.tty_out;
	stats -> user_in = statistics.user_in;
	stats -> user_out = statistics.user_out;

	printf("[tty_in: %d, tty_out: %d, user_in: %d, user_out: %d].\n", statistics.tty_in, statistics.tty_out, statistics.user_in, statistics.user_out);
	
	return 0;
}

int InitTerminalDriver()
{
	if (terminalDriverInitialized != 0 )
	{
		printf("The Terminal Driver has already been initialized\n");
		return -1;
	}

	terminalDriverInitialized = 1;

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


