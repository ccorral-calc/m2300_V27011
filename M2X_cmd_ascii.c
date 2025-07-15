
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//   File: cmd_ascii.c
//    Version: 1.0
//     Author: dm
//
//        MONSSTR 2100 ASCII Command Parser
//
//        IRIG-106-03, Ch 10 compliant command parser
//
//    Revisions:   
//

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "M2X_cmd_ascii.h"
#include "squeue.h"
#include "M2X_serial.h"
#include "M23_Controller.h"
#include "M2X_system_settings.h"
#include "M23_setup.h"
#include "M23_Utilities.h"
#include "M23_features.h"
#include "M23_Status.h"
#include "M23_MP_Handler.h"
#include "M23_I2c.h"
#include "M23_Stanag.h"
#include "M23_EventProcessor.h"
#include "M2X_Tmats.h"

#include "version.h"

#define    FIND_BLOCKS    0
#define    FIND_TIME      1

pthread_t Com0Thread;

//Externs
//extern int TimeSource;

static int FindMode;
static int AssignReceived;
static int Reverse;

static int DeclassSent;

static char *tmats_buffer;

//#define BLOCKS_TO_EOD   (4 *(1024*1024))
//#define BLOCKS_TO_EOD  131072 

// options:
//
static int Console_Echo;
static UART_CONFIG Serial_Config[2] = 
    {
      { BAUD_384, NO_PARITY, EIGHT_DATA_BITS, ONE_STOP_BIT, NO_FLOW_CONTROL },
      { BAUD_96, NO_PARITY, EIGHT_DATA_BITS, ONE_STOP_BIT, NO_FLOW_CONTROL }
    };

static int Change_Comm_Parameters;


// command/response queues:
//
//static UINT8 ReceiveQueue[3][MAX_BYTES_IN_QUEUE];
//static UINT8 TransmitQueue[3][MAX_BYTES_IN_QUEUE];

static SERIAL_QUEUE_INFO ReceiveQueueInfo[3];
static SERIAL_QUEUE_INFO TransmitQueueInfo[3];

static char TmpRcvBuffer[2][MAX_COMMAND_LENGTH];
static int  CurrentCommandIndex[2];


int  CommandFrom;


static int PubIndex;
static int PubStopIndex;
//static int  RTestloops;


#define WRITE32(address,data) *(volatile unsigned long *)(address) = data

#if 0
static const char *Months[] =
{
    "EMPTY",
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};
#endif
static const char *Months[] =
{
    "EMPTY",
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

// chapter 10 related declarations
//
static CH10_ERROR LastError; 

static char *ErrorCodeText[] = {
       "Invalid Command"
      ,"Invalid Parameter"
      ,"Invalid Mode"
      ,"No Media"
      ,"Memory Full"
      ,"Command Failed"
      ,"SPARE"
      ,"Not Serial Record"
      ,"NO ERROR"
};


static char *Chap10StatusText[] = {
       "FAIL"
      ,"IDLE"
      ,"BIT"
      ,"ERASE"
      ,"DECLASSIFY"
      ,"RECORD"
      ,"PLAY"
      ,"RECORD & PLAY"
      ,"FIND"
      ,"BUSY"
      ,"ERROR"
      ,"DECLASSIFY FAIL"
      ,"DECLASSIFY PASS"
      ,"RTEST"
      ,"PTEST"
      ,"RTEST & PTEST"
};



// internal functions
//
static void CmdProcessCommands( void );
static void CmdLookup( char *command );
static void CmdDisplayError( int error );

static int GetComSettings( UART_CONFIG *config, char *parameters );
static int PutComSettings( UART_CONFIG *config ,int port,int flag);
static int SetCom( char *parameters ,int port);

// commands
//
static void CmdAssign( char *parameters ); //New for TRD
static void CmdBuiltInTest( char *parameters );
static void CmdChan( char *parameters );
static void CmdCritical( char *parameters );
static void CmdDate( char *parameters ); //New for TRD
static void CmdDeclassify( char *parameters );
static void CmdDismount( char *parameters );
//static void CmdDownload( char *parameters );
static void CmdDub( char *parameters );
static void CmdErase( char *parameters );
static void CmdEvent( char *parameters );
static void CmdFiles( char *parameters );
static void CmdFind( char *parameters );
static void CmdHealth( char *parameters );
static void CmdHelp( char *parameters );
static void CmdIrig( char *parameters ); //New for TRD
static void CmdLoop( char *parameters );
static void CmdMedia( char *parameters );
static void CmdMount( char *parameters );
static void CmdPause( char *parameters ); //New for TRD
static void CmdPlay( char *parameters );
//static void CmdPower( char *parameters );
static void CmdPTest( char *parameters );
static void CmdPublish( char *parameters ); //New for TRD
static void CmdRecord( char *parameters );
//static void CmdReplay( char *parameters );
static void CmdReset( char *parameters );
static void CmdResume( char *parameters ); //New for TRD
static void CmdRTest( char *parameters );
static void CmdSave( char *parameters );
static void CmdSet( char *parameters );
//static void CmdSetSet( char *parameters );
static void CmdSetup( char *parameters );
static void CmdShuttle( char *parameters );
static void CmdStatus( char *parameters );
static void CmdStop( char *parameters );
static void CmdTime( char *parameters );
static void CmdTemp( char *parameters );
static void CmdTmats( char *parameters );
static void CmdVersion( char *parameters );
static void CmdExit( char *parameters );

static void CmdCondor( char *parameters );
static void CmdController( char *parameters );
static void CmdCWrite( char *parameters );
static void CmdVideo( char *parameters );
static void CmdVWrite( char *parameters );
static void CmdReadMP( char *parameters );
static void CmdWriteMP( char *parameters );
//static void CmdLoadMP( char *parameters );
static void CmdHelpHelp( char *parameters );
static void CmdConfig( char *parameters );
static void CmdList( char *parameters );
static void CmdJam( char *parameters );

static void CmdSpeed( char *parameters );

static void CmdViewTime( char *parameters );
//static void CmdPCI(char *parameters);

static void CmdResetVideo(char *parameters);
static void CmdStep( char *parameters );

static void CmdEthernet( char *parameters );
static void CmdPhyRead( char *parameters );
static void CmdPhyWrite( char *parameters );

static void CmdBBList( char *parameters );
static void CmdBBRead( char *parameters );
static void CmdBBSecure( char *parameters );


static void CmdReadMem( char *parameters );
static void CmdWriteMem( char *parameters );

static int AssignedChannel;

typedef void (*COMMAND_HANDLER) ( char *command );

typedef struct {
    char        Identifier[12];
    UINT16      IdLen;
    COMMAND_HANDLER pHandlerFunction;
    char        HelpPrompt[132];
} COMMAND_MAP;


static COMMAND_MAP CommandTable[] = 
{
  { ".ASSIGN    ", 7, CmdAssign      ,"[destination-ID][source-ID]  Assign replay(output) channels to source (input) channels" },
  { ".BBLIST    ", 7, CmdBBList      ,"[type]                       Returns the unsecured/secured bad block identifiers" },
  { ".BBREAD    ", 7, CmdBBRead      ,"[block identifier]           Returns raw data from the specified bad block" },
  { ".BBSECURE  ", 9, CmdBBSecure    ,"[block identifier]           Marks an unsecured block as being secure" },
  { ".BIT       ", 4, CmdBuiltInTest ,"                             Runs all of the built-in-tests" },
  { ".CRITICAL  ", 9, CmdCritical    ,"[n [mask]]                   Specify and view masks for critical warnings" },
  { ".DATE      ", 5, CmdDate        ,"[start-date]]                Specify setting or displaying date of the removable memory real time clock" },
  { ".DECLASSIFY", 11, CmdDeclassify ,"                             Secure erases the recording media" },
  { ".DISMOUNT  ", 9, CmdDismount    ,"                             Unloads the recording media" },
  { ".DUB       ", 4, CmdDub         ,"[location]                   Same as Play but with internal clock" },
  { ".ERASE     ", 6, CmdErase       ,"                             Erases the recording media" },
  { ".EVENT     ", 6, CmdEvent       ,"[text string]                Display event table or add event to event table" },
  { ".FILES     ", 6, CmdFiles       ,"                             Displays information about each recorded file" },
  { ".FIND      ", 5, CmdFind        ,"[value [mode]]               Display current locations or find new play point" },
  { ".HEALTH    ", 7, CmdHealth      ,"[feature]                    Display detailed status of the recorder system" },
  { ".IRIG106   ", 8, CmdIrig        ,"                             Returns supported version number of IRIG 106-5 Chapter 10 Recorder Command and Control Mnemonics" },
  { ".HELP      ", 5, CmdHelp        ,"                             Displays table of \"dot\" commands" },
  { ".LOOP      ", 5, CmdLoop        ,"                             Starts record and play in read-after-write mode" },
  { ".MEDIA     ", 6, CmdMedia       ,"                             Displays media usage summary" },
  { ".MOUNT     ", 6, CmdMount       ,"                             Powers and enables the recording media" },
  { ".PAUSE     ", 6, CmdPause       ,"                             Pause current replay" },
  { ".PLAY      ", 5, CmdPlay        ,"[location][speed]            Starts Play at current Play point or specified location" },
  { ".PUBLISH   ", 8, CmdPublish     ,"[keyword][parameter]         Configure,start and stop live data over thre recorder Ethernet interface" },
  { ".RECORD    ", 7, CmdRecord      ,"[filename]                   Starts a recording at the current end of data" },
  { ".RESET     ", 6, CmdReset       ,"                             Perform software initiated system reset" },
  { ".RESUME    ", 6, CmdResume      ,"                             Resume replay from pause condition" },
  { ".SETUP     ", 6, CmdSetup       ,"[n]                          Displays or selects pre-programmed input channel setup" },
  { ".SHUTTLE   ", 8, CmdShuttle     ,"[endpoint[mode]]             Play data repeatedly from current location to endpoint location using external clock" },
  { ".STATUS    ", 7, CmdStatus      ,"                             Displays the current system status" },
  { ".STOP      ", 5, CmdStop        ,"[mode]                       Stops the current recording, playback, or both" },
  { ".TIME      ", 5, CmdTime        ,"[start-time]                 Displays or sets the internal system time" },
  { ".TMATS     ", 6, CmdTmats       ,"<mode> [n]                   Write, Read, Save, or Get TMATS file" },
  { ".CHANNELS  ", 9, CmdChan        ,"                                   Shows the Current Channel Details" },
  { ".CONFIG    ", 7, CmdConfig      ,"                                   Displays Current Loaded Configurations" },
  { "..CONDOR   ", 7, CmdCondor      ,"r,w [offset(hex)] [length,data]    Displays Condor Memory" },
  { "..CR       ", 4, CmdController  ,"[offset(hex)] [length]             Show Controller Space" },
  { "..CW       ", 4, CmdCWrite      ,"[offset(hex)] [value(hex)]         Controller Write" },
  { "..EXIT     ", 6, CmdExit        ,"                                   Exit M2300, Not recommended" },
  { "..HELP     ", 6, CmdHelpHelp    ,"                                   Help For Private Commands" },
  { "..JAM      ", 5, CmdJam         ,"                                   Jam Time From RMM" },
  { ".LIST      ", 5, CmdList        ,"[scan]                             List Memory" },
  { "..ETH      ", 5, CmdEthernet    ,"                                   Test Ethernet" },
  { "..MR       ", 4, CmdReadMP      ,"[device] [offset(hex)] [size]      Show MP Space" },
  { "..MW       ", 4, CmdWriteMP     ,"[device] [offset(hex)] [data(hex)] Write To MP Space" },
  { "..PHYR     ", 6, CmdPhyRead     ,"[reg]                              Read Phy Register" },
  { "..PHYW     ", 6, CmdPhyWrite    ,"[reg] [data]                       Write Data to Phy Register" },
  { ".PTEST     ", 6, CmdPTest       ,"[address][mode]                    Playback Test 1) 8 Bit 2)32 Bit 3)Pseudo 4) 0's 5) 5's 6)A's 7) F's  " },
  { "..READ     ", 6, CmdReadMem     ,"[offset(hex)][length]              Read Memory " },
  { ".RTEST     ", 6, CmdRTest       ,"[duration(scans)] [mode]           Record Test 1) 8 Bit 2)32 Bit 3)Pseudo 4) 0's 5) 5's 6) A's 7) F's "},
  { "..RV       ", 4, CmdResetVideo  ,"[device] [channel]                 Reset Video Channel" },
  { ".SAVE      ", 5, CmdSave        ,"                                   Saves settings to non-volatile memory" },
  { ".SET       ", 4, CmdSet         ,"[parameter [value]]                Displays or sets various system settings" },
  { "..STEP     ", 6, CmdStep        ,"                                   Single Step" },
  { "..SPEED    ", 7, CmdSpeed       ,"[location][speed]                  Changes Play Speed at current Play point or specified location" },
  { ".TEMP      ", 5, CmdTemp        ,"                                   Displays Internal Temperatures" },
  { ".VERSION   ", 8, CmdVersion     ,"                                   Show version information" },
  { "..VIEWTIME ", 10, CmdViewTime   ,"                                   View System Time" },
  { "..VR       ", 4, CmdVideo       ,"[device]                           Show Video  Space" },
  { "..VW       ", 4, CmdVWrite      ,"[device] [offset(hex)] [data(hex)] Video Write" },
  { "..Write    ", 7, CmdWriteMem    ,"[offset(hex)][data]                Write Memory "}
};


#define MAX_NUMBER_COMMANDS ( sizeof( CommandTable ) / sizeof( COMMAND_MAP ) )


void M23_ProcessScript()
{

    int i;
    int bytes;
    int cmd;
    int index = 0;

    UINT8 byte;
    UINT8 command[32][80];

    FILE *fp;

    fp = fopen("StartupScript","rb+");

    memset(command,0x0,32*80);
    if(!fp){
        printf("File Does not exist\r\n");
    }else {

        cmd = 0;
        while( !feof(fp) ) {
            bytes = fread(&byte,1,1,fp);
            if(byte == 0xA) {
                cmd++;
                index = 0;
            }else{

                if( (byte != 0xA) && (byte != 0xD) ) {
                    command[cmd][index++] = byte;
                    byte = 0;
                }

            }

        }

        fclose(fp);

        if(cmd == 0){ //no New line in file
            M23_SetCurrentPort(0);
            CmdLookup( command[0] );
        }else{
            for(i = 0; i < cmd;i++) {
                //strncat(command[i],"\r\n",2);
                M23_SetCurrentPort(0);
                CmdLookup( command[i] );
            }
        }
    }


}

void M23_AllocateTmatsBuffer()
{
    tmats_buffer = (char *)malloc(64*1024);
}

void M23_GetAssignedChan(int *chan)
{
    *chan = AssignedChannel;
}

void M23_GetAssign(int *assign)
{
    *assign = AssignReceived;
}

void M23_SetAssign(int InputChan)
{
    AssignReceived = 1;

    AssignedChannel = InputChan;
}


void M23_SetFromAuto()
{
    CommandFrom = FROM_AUTO;
}

void M23_SetDeclassSent()
{
    DeclassSent = 1;
}


// external functions
//

void CmdAsciiInitialize( void )
{
    int rv;

    Change_Comm_Parameters = 0;
    AssignReceived = 0;
    AssignedChannel = 0;
    Reverse = 0;
    DeclassSent = 0;

    CurrentCommandIndex[0] = 0;
    CurrentCommandIndex[1] = 0;

    M23_AllocateTmatsBuffer();

    // zero internal memory structures
    //
    memset( TmpRcvBuffer, 0, sizeof( TmpRcvBuffer ) );
    LastError = NO_ERROR;

#if 0
    // set up the serial queues
    //
    for ( channel = 0; channel < 3; channel++ )
    {
        SQ_Init( &ReceiveQueueInfo[channel], ReceiveQueue[channel], MAX_BYTES_IN_QUEUE );
        SQ_Init( &TransmitQueueInfo[channel], TransmitQueue[channel], MAX_BYTES_IN_QUEUE );
    }
#endif

    // initialize the serial port 
    //

    rv = SerialPortInitialize( 0, &Serial_Config[0], &TransmitQueueInfo[0], &ReceiveQueueInfo[0] );
    if ( rv < 0 )
        perror( "SerialPortInitialize: failed to open\n" );

    rv = SerialPortInitialize( 1, &Serial_Config[1], &TransmitQueueInfo[0], &ReceiveQueueInfo[0] );

    if ( rv < 0 )
        perror( "SerialPortInitialize: failed to open\n" );

    FindMode = FIND_BLOCKS;

    CommandFrom = NO_COMMAND;

}


void GetSerialCommand(int port)
{
    char byte;
    int chars_read;
    int debug;

    GSBTime time;

    static int count = 0;

    chars_read = SerialGetChar( port, &byte);

    M23_GetDebugValue(&debug);
    if(chars_read > 0)
    {
        if( byte != CarriageReturn)
        {
            if( byte == LineFeed)
            {
                // Process commands
                //
                M23_SetCurrentPort(port);
                CmdLookup( TmpRcvBuffer[port] );

                memset(TmpRcvBuffer[port],0x0,MAX_COMMAND_LENGTH);
                CurrentCommandIndex[port] = 0;
            }
            else if(byte != BackSpace)
            {
                if(CurrentCommandIndex[port] < MAX_COMMAND_LENGTH)
                {
                    TmpRcvBuffer[port][CurrentCommandIndex[port]] = byte;
                    CurrentCommandIndex[port]++;
                }
                else
                {
                    if(debug)
                        printf("Max bytes read without CRLF on port %d,%s \r\n",port,TmpRcvBuffer[port][CurrentCommandIndex[port]]);
                    CurrentCommandIndex[port] = 0;
                    memset(TmpRcvBuffer[port],0x0,MAX_COMMAND_LENGTH);
                }

            }
            else if( byte == BackSpace)
            {
                CurrentCommandIndex[port]--;
            }

        }
    }

    M23_SetCurrentPort(0);

}

void GetSerialCommand_1(int port)
{
    char byte;
    int chars_read;
    int debug;

    GSBTime time;

    static int count = 0;

    chars_read = SerialGetChar( port, &byte);

    M23_GetDebugValue(&debug);

    if(chars_read > 0)
    {
        if( byte != CarriageReturn)
        {
            if( byte == LineFeed)
            {
                // Process commands
                //
                M23_SetCurrentPort(port);
                CmdLookup( TmpRcvBuffer[port] );

                memset(TmpRcvBuffer[port],0x0,MAX_COMMAND_LENGTH);
                CurrentCommandIndex[port] = 0;
            }
            else if(byte != BackSpace)
            {
                if(CurrentCommandIndex[port] < MAX_COMMAND_LENGTH)
                {
                    TmpRcvBuffer[port][CurrentCommandIndex[port]] = byte;
                    CurrentCommandIndex[port]++;
                }
                else
                {
                    if(debug)
                        printf("Max bytes read without CRLF on port %d,%s \r\n",port,TmpRcvBuffer[port][CurrentCommandIndex[port]]);
                    CurrentCommandIndex[port] = 0;
                    memset(TmpRcvBuffer[port],0x0,MAX_COMMAND_LENGTH);
                }

            }
            else if( byte == BackSpace)
            {
                CurrentCommandIndex[port]--;
            }

        }
    }

    M23_SetCurrentPort(0);
}



void CmdAsciiMonitor( void )
{
    int loops;
    int config;
    int debug;

    // get characters
    //

    memset(&TmpRcvBuffer[0],0x0,MAX_COMMAND_LENGTH);

    M23_GetDebugValue(&debug);
    if(debug)
        printf("COM 0 Thread number %ld pid %d\n", pthread_self(),getpid());

    while(1)
    {
        M23_GetConfig(&config);

        /*Check If A command has come in from Port 0*/
        GetSerialCommand(0);
    }

}

void CmdAsciiMonitor_COM1( void )
{
    int ssric;
    int config;
    int loops = 0;


    memset(&TmpRcvBuffer[1],0x0,MAX_COMMAND_LENGTH);

    while(1)
    {
        M23_GetSsric(&ssric);
        M23_GetConfig(&config);

        if((config == 1) || (config == 2) || (config == 4) || (config == B1B_CONFIG) || (config == B52_CONFIG) )
        {
            if(ssric == 0)
            {
                /*Check If A command has come in from Port 1*/
                GetSerialCommand_1(1);
            }
        }
        else
        {
            GetSerialCommand_1(1);
        }


    }

}



// internal functions
//


void CmdProcessCommands( void )
{
    char command[MAX_COMMAND_LENGTH];

    if ( GetCommand( 0, command ) )
    {
        CmdLookup( command );
    }
}


void CmdLookup( char *command )
{
    char *p_cmd, *parameter;
    int i;


    // change first word of the command to upper case
    //

    p_cmd = command;
    while ( (*p_cmd != ' ') && (*p_cmd != '\0') ) {
    if( (*p_cmd >= 'a') && (*p_cmd <= 'z') )
        *p_cmd -= ('a' - 'A');
        p_cmd++;
    }




    // isolate the parameters portion of the command string
    //
    parameter = command;

    
    // find first whitespace
    //
    do  {
        if ( *parameter == '\0' )
            break;
    }
    while ( !isspace( *parameter++ ) );

    // skip until parameter or end of line is found
    //
    while ( *parameter != '\0' )
    {
        if( isspace( *parameter ) )
            *parameter++;
        else
            break;
    }


    // find the command -- go backwards so .SETUP is checked before .SET
    //
    //for ( i = MAX_NUMBER_COMMANDS - 1; i >= 0;  i-- ) 
    for ( i = 0 ; i <  MAX_NUMBER_COMMANDS ; i++ ) 
    {
        if ( !strncmp( CommandTable[i].Identifier, command, CommandTable[i].IdLen ) ) 
        {
            // call the function that matches the identifier
            //
            //PutResponse(0,"\r\n");
             
            (CommandTable[i].pHandlerFunction)(parameter);

            return;
        }
    }

    if(strlen(command))
        CmdDisplayError( ERROR_INVALID_COMMAND );
    else
        PutResponse(0, "*");

    return;
}




void CmdDisplayError( int error )
{
    char msg[34];
    int  state;
    int  verbose;

    M23_GetVerbose(&verbose);

    LastError = error;


    sprintf( msg, "E %02d", error );


    if ( verbose )
    {
        strcat( msg, " -- ERROR: " );
        strcat( msg, ErrorCodeText[error] );
    }



    strcat( msg, "\r\n*" );

    PutResponse( 0, msg );

    M23_RecorderState(&state);



#if 0
    M23_RecorderState(&state);


    if(state == STATUS_IDLE)
    {
        M23_SetRecorderState(STATUS_ERROR);
    }
#endif

}



int GetComSettings( UART_CONFIG *config, char *parameters )
{
    char parity;
    char flowcontrol;
    int num_conv;
    int port;

    char *p_cmd = parameters;

    if ( !parameters ) 
        return -1;


    // change to upper case
    //
    while ( *p_cmd != '\0' ) {
        if( (*p_cmd >= 'a') && (*p_cmd <= 'z') )
            *p_cmd -= ('a' - 'A');
        p_cmd++;
    }


    // Extract parameters
    //
    num_conv = sscanf( parameters, "%d,%d,%d,%c,%d,%c", 
                       &port,
                    (int *)&config->Baud,
                    (int *)&config->Databits,
                    &parity,
                    (int *)&config->Stopbits,
                    &flowcontrol 
                  );

    if ( num_conv == 1 ) 
    {
        // try it with spaces instead of commas
        //
        num_conv = sscanf( parameters, "%d %d %d %c %d %c", 
                &port,
                (int *)&config->Baud,
                (int *)&config->Databits,
                &parity,
                (int *)&config->Stopbits,
                &flowcontrol 
              );
    }


    if ( ( 5 == num_conv ) || ( 4 == num_conv ) )
    {
        if ( 4 == num_conv )
        {
            flowcontrol = 'N';
        }
    }
    else
    {
        return -1;
    }

    // Range check all values
    //

    // take the nearest supported baud rate that is less than or equal to
    // the value entered by the user.
    //
    if      ( config->Baud >= BAUD_1152 ) config->Baud = BAUD_1152;
    else if ( config->Baud >= BAUD_576 )  config->Baud = BAUD_576;
    else if ( config->Baud >= BAUD_384 )  config->Baud = BAUD_384;
    else if ( config->Baud >= BAUD_192 )  config->Baud = BAUD_192;
    else                                  config->Baud = BAUD_96;


    if ( ( config->Databits != EIGHT_DATA_BITS ) && 
         ( config->Databits != SEVEN_DATA_BITS ) ) {
printf("fail2 \r\n");
        return -1;
    }


    if      ( parity == 'N' )   config->Parity = NO_PARITY;
    else if ( parity == 'E' )   config->Parity = EVEN_PARITY;
    else if ( parity == 'O' )   config->Parity = ODD_PARITY;
    else {
printf("fail3 \r\n");
        return -1;
    }


    if ( ( config->Stopbits != ONE_STOP_BIT ) &&
         ( config->Stopbits != TWO_STOP_BITS ) ) {
printf("fail4 \r\n");
        return -1;
    }


    if      ( flowcontrol == 'H' )   config->Flowcontrol = HW_FLOW_CONTROL;
    else if ( flowcontrol == 'X' )   config->Flowcontrol = XON_XOFF_FLOW_CONTROL;
    else                             config->Flowcontrol = NO_FLOW_CONTROL;


    return 0;
}


int PutComSettings( UART_CONFIG *config,int port,int flag)
{
    char tmp_buffer[30];
    char ch_parity = 'N';

    if ( !config ) 
        return -1;


    if      ( config->Parity == EVEN_PARITY )   ch_parity = 'E';
    else if ( config->Parity == ODD_PARITY )    ch_parity = 'O';

    if(flag == 1)
    {
        sprintf( tmp_buffer, "COM%d         %d,%1d,%c,%1d",\
                            port+1,
                            config->Baud, 
                            config->Databits, 
                            ch_parity,
                            config->Stopbits
                            );
    }
    else
    {
        sprintf( tmp_buffer, "COM%d %d,%1d,%c,%1d",\
                            port+1,
                            config->Baud, 
                            config->Databits, 
                            ch_parity,
                            config->Stopbits
                            );
    }

    if      ( config->Flowcontrol == XON_XOFF_FLOW_CONTROL )    strcat( tmp_buffer, ",X" );
    else if ( config->Flowcontrol == HW_FLOW_CONTROL )          strcat( tmp_buffer, ",H" );

    strcat( tmp_buffer, "\r\n" );

    PutResponse( 0, tmp_buffer );

    return 0;
}




int SetCom( char *parameters ,int port)
{
    UART_CONFIG new_cfg;

    if ( !parameters ) 
        return -1;


    if ( !*parameters ) {        

        // list current settings
        //
        PutComSettings( &Serial_Config[port] ,port,0);
    }
    else {                      

        // get new settings
        //
        if ( GetComSettings( &new_cfg, parameters ) ) 
        {
            return ERROR_INVALID_PARAMETER;
        }

        PutResponse(0,"SET ");
        PutComSettings( &new_cfg, port,0);          // display values for confirmation

        Serial_Config[port] = new_cfg;

        PutResponse( 0, "*" );

        CmdAsciiInitialize();
        //Change_Comm_Parameters = 1; // will wait until transmit queue is empty before switching
    }


    return 0;
}







// internal functions, chapter 10 command wrappers
//


void CmdBuiltInTest( char *parameters )
{
    int  return_status = 0;
    int  config;

    M23_GetConfig(&config);


    return_status = BuiltInTest();
   
    if( return_status == NO_ERROR )
    {
        if(config == LOCKHEED_CONFIG)
        {
            M23_SetFaultLED(0);

            if(EventTable[7].CalculexEvent)
            {
                EventSet(7);
            }
        }

        PutResponse( 0, "*" );
        
    }
    else
    {
        CmdDisplayError( return_status );
    }

}

void CmdChan( char *parameters )
{
    int          i;
    int          j;
    int          gain;
    int          remote;
    int          VideoBitRate = 4000000;
    int          loops;
    int          setup;
    char         tmp[160];
    M23_CHANNEL  const *config;



    SetupGet(&config);
    M23_GetConfig(&setup);

    memset(tmp,0x0,160);

    if(setup == LOCKHEED_CONFIG)
    {
        if(IEEE1394_Lockheed_Bus_Speed == BUS_SPEED_S400)
        {
            PutResponse(0,"BUS SPEED S400    ");
        }
        else
        {
            PutResponse(0,"BUS SPEED S800    ");
        }
    }
    else
    {
        if(IEEE1394_Bus_Speed == BUS_SPEED_S400)
        {
            PutResponse(0,"BUS SPEED S400    ");
        }
        else
        {
            PutResponse(0,"BUS SPEED S800    ");
        }
    }

    M23_GetRemoteValue(&remote);
    if(remote)
    {
        PutResponse(0,"RMM External    ");
    }
    else
    {
        PutResponse(0,"RMM Internal    ");
    }

    if(Video_Output_Format == RGB)
    {
        PutResponse(0,"VIDEO OUT  RGB    ");
    }
    else
    {
        PutResponse(0,"VIDEO OUT  NTSC    ");
    }

    if(config->IndexIsEnabled)
    {
        PutResponse(0,"INDEX Enabled    ");
    }
    else
    {
        PutResponse(0,"INDEX Disabled    ");
    }

    if(config->EventsAreEnabled)
    {
        PutResponse(0,"EVENTS Enabled  ");
    }
    else
    {
    
        PutResponse(0,"EVENTS Disabled  ");
    }
 

    if(config->M1553_RT_Control != M1553_RT_NONE)
    {
        if(config->M1553_RT_Control == M1553_RT_HOST_CONTROL)
        {
            PutResponse(0,"RT Host Control\r\n");
        }
        else
        {
            memset(tmp,0x0,160);
            sprintf(tmp,"RT Control - 0x%d\r\n",config->M1553_RT_Control);
            PutResponse(0,tmp);
        }
    }
    else
    {
        PutResponse(0,"NO RT Control\r\n");
    }
    

    PutResponse(0,"\r\n                           Time \r\n");
    PutResponse(0,"------------------------------------------------------------------------------------------------ \r\n");
    memset(tmp,0x0,160);
    if(config->timecode_chan.Source == TIMESOURCE_IS_INTERNAL)
    {
        sprintf(tmp,"0x%02x  IRIG-%c  Internal   ",config->timecode_chan.chanID,config->timecode_chan.Format);
    }
    else if(config->timecode_chan.Source == TIMESOURCE_IS_RMM)
    {
        sprintf(tmp,"0x%02x  IRIG-%c  Remote   ",config->timecode_chan.chanID,config->timecode_chan.Format);
    }
    else if(config->timecode_chan.Source == TIMESOURCE_IS_IRIG)
    {
        if((config->timecode_chan.Format == 'U') || (config->timecode_chan.Format == 'N') )
        {
            sprintf(tmp,"0x%02x  GPS            ",config->timecode_chan.chanID);
        }
        else
        {
            sprintf(tmp,"0x%02x  IRIG-%c  IRIG   ",config->timecode_chan.chanID,config->timecode_chan.Format);
        }
    }
    else if(config->timecode_chan.Source == TIMESOURCE_IS_M1553)
    {
        if(config->timecode_chan.m1553_timing.Format == F15_EGI)
        {
            sprintf(tmp,"0x%02x  IRIG-%c  M1553MT  ID %d  Format F15_EGI   ",config->timecode_chan.chanID,config->timecode_chan.Format,config->timecode_chan.m1553_timing.ChannelId);
        }
        else
        {
            sprintf(tmp,"0x%02x  IRIG-%c  M1553MT  ID %d  Format Unknown   ",config->timecode_chan.chanID,config->timecode_chan.Format,config->timecode_chan.m1553_timing.ChannelId);
        }

    }

    PutResponse(0,tmp);

    if(TimeSet_Command == 0)
    {
        PutResponse(0,"No Secondary Time Source\r\n");
    }
    else
    {
        PutResponse(0,"Secondary Time Source is RMM\r\n");
    }

    /* Do the Voice Channels Here */
    if(config->NumConfiguredVoice > 0)
    {
        PutResponse(0,"\r\n                           Voice \r\n");
        PutResponse(0,"------------------------------------------------------------------------------------------------ \r\n");
        for(i = 0; i < config->NumConfiguredVoice;i++)
        {
            switch(config->voice_chan[i].Gain)
            {
                case 0x0:
                    gain = 0;
                    break;
                case 0x1:
                    gain = 3;
                    break;
                case 0x2:
                    gain = 6;
                    break;
                case 0x3:
                    gain = 9;
                    break;
                case 0x4:
                    gain = 12;
                    break;
                case 0x5:
                    gain = 15;
                    break;
                case 0x6:
                    gain = 18;
                    break;
                case 0x7:
                    gain = 21;
                    break;
                case 0x8:
                    gain = 24;
                    break;
            }

            memset(tmp,0x0,160);

            if(i == 0)
            {
                sprintf(tmp,"0x%02x    Source  Right   Gain %ddB\r\n",config->voice_chan[i].chanID,gain);
            }
            else
            {
                sprintf(tmp,"0x%02x    Source  Left    Gain %ddB\r\n",config->voice_chan[i].chanID,gain);
            }

            PutResponse(0,tmp);
        }
    }

    if(config->NumConfiguredPCM > 0)
    {
        PutResponse(0,"\r\n                               PCM \r\n");
        PutResponse(0,"----------------------------------------------------------------------------------------------------- \r\n");
        for(i = 0; i < config->NumConfiguredPCM;i++)
        {

            if(config->pcm_chan[i].pcmCode != PCM_UART )
            {
                if(config->pcm_chan[i].isEnabled)
                {
                    memset(tmp,0x0,160);
                    sprintf(tmp,"0x%02x  %08d",config->pcm_chan[i].chanID,config->pcm_chan[i].pcmBitRate);

                    PutResponse(0,tmp);

                    if(config->pcm_chan[i].pcmInputSource == PCM_422)
                    {
                        PutResponse(0,"   RS422");
                    }
                    else
                    {
                        PutResponse(0,"   TTL  ");
                    }

                    if(config->pcm_chan[i].pcmDataPackingOption == PCM_UNPACKED)
                    {
                        PutResponse(0,"   UNPACKED   ");
                    }
                    else if(config->pcm_chan[i].pcmDataPackingOption == PCM_PACKED)
                    {
            
                        PutResponse(0,"   PACKED     ");
                    }
                    else if(config->pcm_chan[i].pcmDataPackingOption == PCM_THROUGHPUT)
                    {
                        PutResponse(0,"   THROUGHPUT ");
                    }

                    if(config->pcm_chan[i].pcmCode == PCM_NRZL)
                    {
                        PutResponse(0,"NRZL   ");
                    }
                    else if(config->pcm_chan[i].pcmCode == PCM_RNRZL)
                    {
                        PutResponse(0,"RNRZL  ");
                    }
                    else if(config->pcm_chan[i].pcmCode == PCM_BIOL)
                    {
                        PutResponse(0,"BiPhase");
                    }

                    memset(tmp,0x0,160);
                    sprintf(tmp,"WL -  %2d  MPM - %3d WIM - %4d  ",config->pcm_chan[i].pcmCommonWordLength,config->pcm_chan[i].pcmMinorFramesInMajorFrame,config->pcm_chan[i].pcmWordsInMinorFrame);
                    PutResponse(0,tmp); 

                    memset(tmp,0x0,160);
                    sprintf(tmp,"SYNC - 0x%08x  MASK - 0x%08x LEN - %d",config->pcm_chan[i].pcmSyncPattern,config->pcm_chan[i].pcmSyncMask,config->pcm_chan[i].pcmSyncLength);
                    PutResponse(0,tmp); 

                    PutResponse(0,"\r\n");
                }
            }
            else
            {
                if(config->pcm_chan[i].isEnabled)
                {
                    if(config->pcm_chan[i].OnController == 0)
                    {
                        memset(tmp,0x0,160);
                        sprintf(tmp,"0x%02x  %d",config->pcm_chan[i].chanID,config->pcm_chan[i].UARTBaudRate);
                        PutResponse(0,tmp);

                        if(config->pcm_chan[i].UARTDataBits == 7)
                        {
                            PutResponse(0,"   7-BIT");
                        }
                        else
                        {
                            PutResponse(0,"   8-BIT");
                        }

                        if(config->pcm_chan[i].UARTParity == PARITY_EVEN)
                        {
                            PutResponse(0,"   EVEN PARITY");
                        }
                        else if(config->pcm_chan[i].UARTParity == PARITY_ODD)
                        {
                            PutResponse(0,"   ODD PARITY ");
                        }
                        else
                        {
                            PutResponse(0,"   NO PARITY  ");
                        }

                        PutResponse(0,"\r\n");
                    }
                    else
                    {
                        memset(tmp,0x0,160);
                        sprintf(tmp,"0x%02x  Controller UART\r\n",config->pcm_chan[i].chanID);
                        PutResponse(0,tmp);
                    }
                }
            }
        }
    }
            
    if(config->NumConfigured1553 > 0)
    {
        PutResponse(0,"\r\n                           M1553\r\n");
        PutResponse(0,"----------------------------------------------------------------------------- \r\n");
        PutResponse(0,"Chan ID   Watch Word Pattern   Watch Word Mask    Watch Word Interval\r\n");
        for(i = 0 ; i < config->NumConfigured1553;i++)
        {
            if(config->m1553_chan[i].isEnabled)
            {
                    
                memset(tmp,0x0,160);
                sprintf(tmp,"0x%02x        0x%08x             0x%08x            %d\r\n",config->m1553_chan[i].chanID,config->m1553_chan[i].m_WatchWordPattern,config->m1553_chan[i].m_WatchWordMask,config->m1553_chan[i].m_WatchWordLockIntervalInMilliseconds);
                PutResponse(0,tmp);

            }

        }
    }

    if(config->NumConfiguredVideo > 0)
    {
        PutResponse(0,"\r\n");
        PutResponse(0,"                           VIDEO\r\n");
        PutResponse(0,"----------------------------------------------------------------------------------------------------------- \r\n");
        for(i = 0;i<config->NumConfiguredVideo;i++)
        {
            if(config->video_chan[i].isEnabled)
            {
                switch(config->video_chan[i].internalClkRate)
                {
                    case VIDEO_2_0M_CLOCKRATE :
                        VideoBitRate = 2000000;
                        break;
                    case VIDEO_3_0M_CLOCKRATE :
                        VideoBitRate = 3000000;
                        break;
                    case VIDEO_4_0M_CLOCKRATE :
                        VideoBitRate = 4000000;
                        break;
                    case VIDEO_6_0M_CLOCKRATE :
                        VideoBitRate = 6000000;
                        break;
                    case VIDEO_8_0M_CLOCKRATE :
                        VideoBitRate = 8000000;
                        break;
                    case VIDEO_10_0M_CLOCKRATE :
                        VideoBitRate = 10000000;
                        break;
                    case VIDEO_12_0M_CLOCKRATE :
                        VideoBitRate = 12000000;
                    case VIDEO_15_0M_CLOCKRATE :
                        VideoBitRate = 15000000;
                        break;
                }
                memset(tmp,0x0,40);
                sprintf(tmp,"0x%02x  %08d",config->video_chan[i].chanID,VideoBitRate);
                PutResponse(0,tmp);

                if(config->video_chan[i].videoFormat == VIDEO_NTSC_VIDFORMAT)
                {
                    PutResponse(0,"   NTSC ");
                }
                else
                {
                    PutResponse(0,"   PAL  ");
                }

                switch(config->video_chan[i].videoCompression)
                {
                    case VIDEO_I_VIDCOMP:
                        PutResponse(0," I ONLY");
                        break;
                    case VIDEO_IP_5_VIDCOMP:
                        PutResponse(0," IP-5        ");
                        break;
                    case VIDEO_IPB_5_VIDCOMP:
                        PutResponse(0," IPB-6       ");
                        break;
                    case VIDEO_IP_15_VIDCOMP:
                        PutResponse(0," IP-15       ");
                        break;
                    case VIDEO_IP_45_VIDCOMP:
                        PutResponse(0," IP-45       ");
                        break;
                    case VIDEO_IPB_15_VIDCOMP:
                        PutResponse(0," IPB-16      ");
                        break;
                    case VIDEO_IPB_45_VIDCOMP:
                        PutResponse(0," IPB-45      ");
                        break;
                    default:
                        PutResponse(0," IP-15       ");
                        break;
         
                }

                if(config->video_chan[i].videoInput == VIDEO_COMP_VIDINPUT)
                {
                    PutResponse(0,"Composite");
                }
                else
                {
                    PutResponse(0,"S-VIDEO  ");
                }
 
                PutResponse(0,"  Audio : ");


                if(config->video_chan[i].audioSourceL == AUDIO_LOCAL)
                {
                    PutResponse(0," Left - Local       ");
                }
                else if(config->video_chan[i].audioSourceL == AUDIO_NONE)
                {
                    PutResponse(0," Left - Mute ");
                }
                else if(config->video_chan[i].audioSourceL == AUDIO_EVENT_TONE)
                {
                    PutResponse(0," Left - Event Tone");
                }
                else if(config->video_chan[i].audioSourceL > 0)
                {
                    memset(tmp,0x0,40);
                    sprintf(tmp," Left - Source %d",config->video_chan[i].audioSourceL);
                    PutResponse(0,tmp);
                }

                if(config->video_chan[i].audioSourceR == AUDIO_LOCAL)
                {
                    PutResponse(0," Right - Local ");
                }
                else if(config->video_chan[i].audioSourceR == AUDIO_NONE)
                {
                    PutResponse(0," Right - Mute ");
                }
                else if(config->video_chan[i].audioSourceR == AUDIO_EVENT_TONE)
                {
                    PutResponse(0," Right - Event Tone");
                }
                else if(config->video_chan[i].audioSourceR > 0)
                {
                    memset(tmp,0x0,40);
                    sprintf(tmp," Right - Source %d",config->video_chan[i].audioSourceR);
                    PutResponse(0,tmp);
                }
            }

            PutResponse(0,"\r\n");
        }
    }

    /* Do the Discrete Channel Here */
    if(config->NumConfiguredDiscrete > 0)
    {
        PutResponse(0,"\r\n                           DISCRETE\r\n");
        PutResponse(0,"------------------------------------------------------------------------------------- \r\n");
        PutResponse(0,"Chan ID      Mask\r\n");
        memset(tmp,0x0,160);
        sprintf(tmp,"0x%02x     0x%08x\r\n",config->dm_chan.chanID,config->dm_chan.DiscreteMask);
        PutResponse(0,tmp);

    }


    /* Do the Ethernet Channel Here */
    if(config->NumConfiguredEthernet > 0)
    {
        PutResponse(0,"\r\n                           Ethernet \r\n");
        PutResponse(0,"------------------------------------------------------------------------------------------------ \r\n");
        if(config->eth_chan.isEnabled)
        {
            PutResponse(0,"Enabled\r\n");
        }
        else
        {
            PutResponse(0,"Not Enabled\r\n");
        }

    }

    /* Do the UART Channel Here */
    if(config->NumConfiguredUART > 0)
    {
        PutResponse(0,"\r\n                          Controller UART\r\n");
        PutResponse(0,"------------------------------------------------------------------------------------------------ \r\n");
        if(config->uart_chan.isEnabled)
        {
            PutResponse(0,"Enabled\r\n");
        }
        else
        {
            PutResponse(0,"Not Enabled\r\n");
        }

    }



    PutResponse(0,"\r\n*");
}

void CmdCritical( char *parameters )
{
    UINT32       *masks=NULL;
    UINT32       mask = 0;
    int          number_of_features=0,i,j;
    char         feature_str[5]={0,0,0,0,0};
    char         mask_str[9]={0,0,0,0,0,0,0,0,0};
    char         **decoded_masks;
    char         tmp[32];
    int          feature_int=0;
    int          current_index;
    int          mask_int=0;
    int          return_status = NO_ERROR;
    int          config;
    BOOL         feature_flag = TRUE;
    BOOL         token1=FALSE,token2=FALSE;
    UINT32       maskbit=0x00000001;
   
    M23_GetConfig(&config);

    i = 0;
    j = 0;

    if ( *parameters != '\0' )
    {
        while( *parameters != '\0' )
        {
           if(isspace(*parameters))
           {
                *parameters++;
                feature_flag=FALSE;
                continue;
           }

           if(feature_flag)
           {
               token1=TRUE;
               feature_str[i]=*parameters;
               i++;
           }
           else
           {
               token2=TRUE;
               mask_str[j]=*parameters;
               j++;
           }

            *parameters++;
        }

       sscanf( feature_str, "%d", &feature_int );
       sscanf( mask_str, "%x", &mask_int );

       current_index = feature_int;
    }


    if(token1 == FALSE && token2 == FALSE)
    {
        return_status = FeaturesCriticalView(&number_of_features, &masks);

        if(return_status != NO_ERROR)
        {
            CmdDisplayError( return_status );
        }
        else
        {
            //print the critical list
            for (i = 0; i < MAX_FEATURES; i++ )
            {
                if(HealthArrayTypes[i] != NO_FEATURE)
                {
                    sprintf( tmp, "%2d %08lx\r\n",i,masks[i]);

                    PutResponse( 0, tmp );
                }

            }

            PutResponse( 0, "*" );

        }

    }
    else if (token1 == TRUE && token2 == FALSE)
    {
        return_status = CriticalView(feature_int,&mask,&decoded_masks);

        if(return_status != NO_ERROR )
        {
            CmdDisplayError( return_status );
        }
        else
        {
            // print the all possible warnings of specified feature
            // skip the 31st one, it is the enable bit
            //
            maskbit=0x00000001;
            for( i = 0; i < 31; i++ )
            {
                if(mask & maskbit)
                {
                    sprintf( tmp, "%08lx %s\r\n",(mask & maskbit),decoded_masks[i]);
                    PutResponse( 0, tmp );
                }

                maskbit = maskbit << 1;
            }
            PutResponse( 0, "*" );
        }
    }
    else if (token1 == TRUE && token2 == TRUE)
    {
       
        if(config != B1B_CONFIG)
        {
            if(feature_int == RECORDER_FEATURE)
            {
                mask_int |= M23_MEMORY_NOT_PRESENT;
                mask_int |= M23_MEMORY_FULL;
                mask_int |= M23_RECORDER_CONFIGURATION_FAIL;
                mask_int |= M23_DISK_NOT_CONNECTED;
            }
        }

        return_status=CriticalSet( feature_int, mask_int );;

        if(return_status != NO_ERROR )
        {
            CmdDisplayError( return_status );
        }
        else
        {
            Critical_Save();
            sync();
            sync();
            //print the feature and the mask
            sprintf( tmp, "%2d %08x\r\n*", feature_int,mask_int);
            PutResponse( 0, tmp );

        }

  
    }

}


void CmdDeclassify( char *parameters )
{
    int  return_status;
    int  config;
   
    M23_GetConfig(&config);

    return_status = DiskIsConnected();

    if(return_status)
    {
        return_status = Declassify();
   
        if(return_status == NO_ERROR)
        {
            DeclassSent = 1;
            if(config == LOCKHEED_CONFIG)
            {
                M23_SetDataLED();
                M23_SetEraseLED();
            }
            else
            {
	        M23_SetDeclassLED();
            }

            PutResponse( 0, "*" );
        }
        else
        {
            CmdDisplayError( return_status );

        }
    }
    else
    {
        CmdDisplayError( ERROR_NO_MEDIA );
    }



}


void CmdDismount( char *parameters )
{
    int return_status = NO_ERROR;

    return_status = Dismount();
   
    if(return_status == NO_ERROR )
    {
        PutResponse( 0, "*" );
    }
    else
    {
        CmdDisplayError( return_status );

    }

}

void CmdDownload( char *parameters )
{
    int return_status = NO_ERROR;

   
    return_status = DiskIsConnected();

    if(return_status) 
    {
        return_status = Download(0);

        if(return_status == NO_ERROR )
        {
            PutResponse( 0, "*" );
            M23_SetRecorderState(STATUS_PLAY);

        }
        else
        {
            CmdDisplayError( ERROR_COMMAND_FAILED );
        }
    }
    else
    {
        CmdDisplayError( ERROR_NO_MEDIA );
    }


}


void CmdDub( char *parameters )
{
    PutResponse(0,"Command Not Implemented\r\n*");
   //CmdDisplayError( ERROR_COMMAND_FAILED );
}

void CmdErase( char *parameters )
{
    int return_status = 0;
    int config;
    int state;

    M23_RecorderState(&state);

    return_status = DiskIsConnected();

    if(return_status)
    {
        if( (state == STATUS_ERROR) || (state == STATUS_IDLE) )
        {
            M23_GetConfig(&config);
            if( (config == 1) || (config == 4) || (config == LOCKHEED_CONFIG) || (config == B52_CONFIG) )
            {
                M23_SetEraseLED();
                SSRIC_SetEraseStatus();

                return_status = Erase();

                if(return_status == NO_ERROR)
                {
                    M23_SetEraseLED();
                    SSRIC_SetEraseStatus();

                    sleep(1);
                    SSRIC_ClearEraseStatus( );
                    M23_ClearEraseLED();

                    sleep(1);

                    M23_SetEraseLED();
                    SSRIC_SetEraseStatus();
                    if(config != LOCKHEED_CONFIG)
                    {
                        if(LoadedTmatsFromCartridge == 1)
                        {
                            //RecordTmats = 1;
                            //RecordTmatsFile();
                            RecordTmats = 0;
                            SSRIC_ClearEraseStatus( );
                            M23_ClearEraseLED();
                        }
                    }
                    else
                    {
                        M23_ClearDataLED();
                    }

                    M23_SetRecorderState(STATUS_IDLE);

                }
            }
            else
            {
                return_status = Erase();

                if(return_status == NO_ERROR)
                {
                    M23_SetEraseLED();
                    SSRIC_SetEraseStatus();

                    if(LoadedTmatsFromCartridge == 1)
                    {
                        //RecordTmats = 1;
                        //RecordTmatsFile();
                        RecordTmats = 0;
                        SSRIC_ClearEraseStatus( );
                        M23_ClearEraseLED();
                    }

                    M23_SetRecorderState(STATUS_IDLE);
                }

            }

   
            if(return_status == NO_ERROR)
            {
                PutResponse( 0, "*" );
            }
            else
            {
                CmdDisplayError( return_status );
    
            }
        }
        else
        {
            CmdDisplayError( ERROR_INVALID_MODE );
        }
    }
    else
    {
        CmdDisplayError( ERROR_NO_MEDIA );
    }



}


void CmdEvent( char *parameters )
{
    int     return_status = 0;
    int     event;
    int     i = 0;
    char    num[3];

    memset(num,0x0,3);

    if ( *parameters != '\0' )
    {
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            {
                *parameters++;
            }
            else
            {
                if( !isdigit(*parameters) )
                {
                    return_status = 1;
                    break;
                }
                else
                {
                    num[i++] = *parameters;
                    *parameters++;
                }
            }
        }

        if(return_status == 0)
        {
            event = atoi(num);
            return_status = EventSet(event);
            M23_ReverseVideo();
            Reverse = 1;

            if(return_status == NO_ERROR)
            {
                //event_StartOverlayTimer();
                PutResponse( 0, "*" );
            }
            else
            {
                CmdDisplayError( return_status );
            }
        }
        else
        {
            CmdDisplayError( return_status );
        }

    }
    else
    {
        m23_DisplayEvent( );
        PutResponse( 0, "*" );
    }
}


void CmdFiles( char *parameters )
{

    int     return_status = 0;

    return_status = DiskIsConnected();

    if(return_status)
    {
        sPrintFiles();
        PutResponse( 0, "*" );
    }
    else
    {
        CmdDisplayError( ERROR_NO_MEDIA );
    }


}


void CmdFind( char *parameters )
{

    char    tmp[40];
    int     return_status = 0;
    int     milli;
    UINT32  play_point = 0;
    UINT32  record_point = 0;

    INT8    *token_ptr1 = NULL;
    INT8    *token_ptr2 = NULL;

    GSBTime   time;

    static int mode = 0; //default is blocks, time = 1;

    return_status = DiskIsConnected();

    memset(tmp,0x0,40);

    if(return_status)
    {
        if ( *parameters != '\0' )
        {
            token_ptr1 = strtok(parameters," ");
            if(token_ptr1)
            {
                token_ptr2 = strtok(NULL," ");
                if(token_ptr2 != NULL)
                {
                    if(strncasecmp(token_ptr1,"TIME",4) == 0)
                    {
                        mode = 1;
                    }
                    else if(strncasecmp(token_ptr1,"BLOCKS",6) == 0)
                    {
                        mode = 0;
                    }
                }
                
            }
            if(mode == 1)
            {
                time.Minutes = 0;
                time.Seconds = 0;
                milli = 0; 
                sscanf(token_ptr2,"%d-%d:%d:%d.%d",&time.Days,&time.Hours,&time.Minutes,&time.Seconds,&milli);
                play_point = M23_FindTimeBlock(time);

                DiskPlaybackSeek(play_point);
                printf("Play point is %d\r\n",play_point);
            }
            else if(mode == 0)
            {
                if( (strncasecmp(token_ptr2,"BOD",3) == 0) ||
                    (strncasecmp(token_ptr2,"BOM",3) == 0) )
                {
                    play_point = START_OF_DATA;
                }
                else if(strncasecmp(token_ptr2,"BOF",3) == 0)
                {
                    play_point = M23_FindFirstInLastFile();
                }
                else if(isdigit(*token_ptr2) )
                {
                    play_point = atoi(token_ptr1);
                }
                else
                {
                    CmdDisplayError( ERROR_COMMAND_FAILED );
                    return;
                }

                sCurrentBlock(&record_point);
                if(play_point >= record_point)
                {
                    CmdDisplayError( ERROR_COMMAND_FAILED );
                    return;
                }

                DiskPlaybackSeek(play_point);
                sprintf( tmp, "F  %ld\r\n*", play_point);
                PutResponse(0,tmp);
            }
            else
            {
                CmdDisplayError( ERROR_COMMAND_FAILED );
                return;
            }


        }
        else //No parameter is passed Display the current Record position
        {
            sGetPlayLocation(&play_point);
            sCurrentBlock(&record_point);

            sprintf( tmp, "F %ld %ld\r\n*", 
                     record_point,
                     play_point
                   );

            PutResponse( 0, tmp );
        }
    }
    else
    {
        CmdDisplayError( ERROR_NO_MEDIA );
    }
}


void CmdHealth( char *parameters )
{
    int     return_status = NO_ERROR;
    int     feature_int = 0;
    int     i = 0;
    int     current_index = 0;
    int     number_of_features = 0;
    UINT32  *health_status = NULL;
    char    feature_str[4] = {0,0,0,0};
    char    tmp[64];
    UINT32  mask=0;
    char    **decoded_masks=NULL;

    if ( *parameters == '\0' )
    {
		//This will return the Standard Features
        return_status = HealthViewAll( &number_of_features, &health_status);

		/*Do the Standard Features First*/
        if(return_status == NO_ERROR)
        {
            // print health list for all channels
            //
            for (i = 0; i < MAX_FEATURES; i++ )
            {
                if(HealthArrayTypes[i] != NO_FEATURE)
                {
                    if((health_status[i] & 0x80000000))//see if status is ON
                    {
                        sprintf( tmp, "%2d  %08lx %s\r\n", i,
                                        (health_status[i] & ~0x80000000),
                                        FeatureDescriptions[i]);
                    }
                    else
                    {
                        sprintf( tmp, "%2d  -------- %s\r\n", i,FeatureDescriptions[i]);
                    }

                    PutResponse( 0, tmp );
                }
            }

            PutResponse( 0, "*" );

        }
        else
        {
            CmdDisplayError( return_status );
        }

        // TRD  M23_ClearVideoHealth();
    }
    else
    {
        strcpy(feature_str,parameters);

        sscanf( feature_str, "%d", &feature_int );

        current_index = feature_int;

        return_status = HealthView(feature_int, &mask, &decoded_masks ,current_index);

        if ( return_status != NO_ERROR )
        {
            CmdDisplayError( return_status );
        }
        else
        {
            // list with enabled errors for the feature
            // skip the 31st one, it is the enable bit
            //
            for( i = 0; i < 31; i++ )
            {
                if( mask & (1 << i) )
                {
                    sprintf( tmp, "%s\r\n", decoded_masks[i] );
                    PutResponse( 0, tmp );
                }
            }

            PutResponse( 0, "*" );
        }
    }
        
}


void CmdHelp( char *parameters )
{
    int  i;
    char tmp[256];
    

    for (i = 0; i < (MAX_NUMBER_COMMANDS - 27); i++ )
    {
        memset(tmp,0x0,256);

        sprintf( tmp, "  %13s %s\r\n", 
                 CommandTable[i].Identifier,
                 CommandTable[i].HelpPrompt
               );

        PutResponse( 0, tmp );
    }

    PutResponse( 0, "*" );
}

void CmdHelpHelp( char *parameters )
{
    int i;
    char tmp[300];
    
    if ( *parameters == '\0' )
    {
        for (i = (MAX_NUMBER_COMMANDS - 27); i < MAX_NUMBER_COMMANDS ; i++ )
        {
            memset(tmp,0x0,300);
            sprintf( tmp, "  %13s %s\r\n", 
                     CommandTable[i].Identifier,
                     CommandTable[i].HelpPrompt
                       );
             PutResponse( 0, tmp );
        }
    }
    else
    {
        if(strncmp(parameters,"CONFIG",6) == 0)
        {
            PutResponse( 0,"CONFIG <release> -> 0-Standard, 1->Lockheed\r\n");
        }
    }

    PutResponse( 0, "*" );
}

void CmdConfig( char *parameters )
{
    int  i;
    int  count;
    char tmp[80];
    char line[80];
    FILE *fp;

    PutResponse(0,"       Configurations Currently Loaded\r\n");
    PutResponse(0,"-------------------------------------------\r\n");

    for(i = 0; i < 16;i++)
    {
        memset(tmp,0x0,80);
        sprintf(tmp,"Setup%d.tmt",i);
        fp = fopen(tmp,"r");

        if(fp != NULL)
        {
            count = 0;
            memset(line,0x0,80);
            while(1)
            {
                fread(&line[count],1,1,fp);
                if(line[count] == '\n')
                {
                    if( (line[0] == 'G') && (line[2] == 'P') && (line[3] == 'N') )
                    {
                        memset(tmp,0x0,80);
                        sprintf(tmp,"Setup%d.tmt is %s\r\n",i,&line[5]);
                        PutResponse(0,tmp);
                        break;
                    }
                    else
                    {
                        count = 0;
                        memset(line,0x0,80);
                    }

                }
                else if(feof(fp))
                {
                    break;
                }
                else
                {
                    count++;
                }
            }

            if(fp)
                fclose(fp);
        }

    }

    PutResponse(0,"*");

}

void CmdJam( char *parameters )
{
    char    time[120];
    int     status;

    memset(time,0x0,120);

    status = M2x_GetCartridgeTime(time);
    if(status == 0)
    {
        M23_SetSystemTime(time);
        PutResponse(0,"*");
    }
    else
    {
        CmdDisplayError( ERROR_COMMAND_FAILED );
    }


}
void CmdList( char *parameters )
{
    int     totalFileCount;
    int     i;
    int     j;
    int     k;
    int     chars_read;
    int     word_offset = 0;
    int     return_status;
    char    byte;
    BOOL    done = FALSE;
    BOOL    search = FALSE;
    BOOL    channel_search = FALSE;
    UINT32  end_scan = 0;
    UINT32  start_scan;
    UINT32  before_search_scan;
    UINT64  total_bytes = 0;
    UINT64  before_search_bytes = 0;
    UINT32  bytes_to_show;
    UINT16  search_pattern;
    UINT16  channel = 0;
    UINT16  *pcurr_scan;
    UINT8   Buffer[512];
    UINT8   tmp[20];
    UINT8   pattern[8];
    const   STANAG_DIR_BLOCK_HOLDER *pFileTable; //local copy of STANAG to load finetable to


    return_status = DiskIsConnected();


    if(return_status == 0)
    {
        CmdDisplayError( ERROR_NO_MEDIA );
        return;
    }

    pFileTable = sGetFileTable(&totalFileCount);


    if( *parameters == '\0'  )
    {
        CmdDisplayError( ERROR_INVALID_PARAMETER );
    }
    else
    {
        start_scan = atoi(parameters);
        DiskReadSeek(start_scan);
        total_bytes = 512;

#if 0
        for(i = 0; i < MAX_STANAG_BLOCKS;i++)
        {
            for(j = 0 ; j < pFileTable[i].STANAG_NumberOfFilesEntries ; j++)
            {
                //end_scan = (UINT32)pFileTable[i].STANAG_FileEntries[j].FILE_StartAddress + (UINT32)(pFileTable[i].STANAG_FileEntries[j].FILE_Size/512);
                scan = pFileTable[i].STANAG_FileEntries[j].FILE_StartAddress + (pFileTable[i].STANAG_FileEntries[j].FILE_Size/512);

                if( pFileTable[i].STANAG_FileEntries[j].FILE_Size % 512)
                {
                    scan++;
                }

                end_scan = (UINT32)scan;

            //    if( (start_scan >=  (UINT32)pFileTable[i].STANAG_FileEntries[j].FILE_StartAddress) && (start_scan < end_scan) )
                if(1)
                {
                    j = MAX_STANAG_BLOCKS;

                    DiskReadSeek(start_scan);
                    total_bytes = (UINT64)(end_scan - start_scan) * 512;
                }
            }

            if(j == MAX_STANAG_BLOCKS)
            {
                i = MAX_STANAG_BLOCKS;
            }
        }
#endif

        if(total_bytes == 0)
        {
            CmdDisplayError( ERROR_INVALID_PARAMETER );
        }
        else
        {
            while(done != TRUE)
            {
                DiskRead((char*)Buffer, 512);
#if 0
                if(total_bytes < 512)
                {
                    bytes_to_show = (UINT32)total_bytes;
                    total_bytes = 0;
                }
                else
                {
                    bytes_to_show = 512;
                    total_bytes -= 512;
                }
#endif

                bytes_to_show = 512;
                
                PutResponse(0,"\r\n");
                memset(tmp,0x0,20);
                if( (search == TRUE) && ((word_offset*2) < 16 ) )
                {
                    sprintf(tmp,"%d_%03d--> - ",start_scan,i+1);
                    search = FALSE;
                }
                else
                {
                    sprintf(tmp,"%d_000 - ",start_scan);
                }
                
                PutResponse(0,tmp);
                j = 0;
                k = 0;
                for(i = 0; i < bytes_to_show;i++)
                {
                    memset(tmp,0x0,20);
                    sprintf(tmp,"%02x",Buffer[i]);
                    PutResponse(0,tmp);

                    if( k == 1)
                    {
                        PutResponse(0," ");
                        k = 0;
                    }
                    else
                    {
                        k++;
                    }
                    if(j == 15)
                    {
                        PutResponse(0,"\r\n");
                        if( (i+1) < 512)
                        {
                            memset(tmp,0x0,20);
                            if( (search == TRUE) && ( ((word_offset*2) >= i+1) && (word_offset*2) < i+17) )
                            {
//printf("\r\nWO %d,i+1=%d,i+17 = %d\r\n",word_offset*2,i+1,i+17);
                                sprintf(tmp,"%d_03d--> - ",start_scan,i+1);
                                search = FALSE;
                            }
                            else
                            {
                                sprintf(tmp,"%d_%03d - ",start_scan,i+1);
                            }
                            PutResponse(0,tmp);
                        }
                        j = 0;
                    }
                    else
                    {
                        j++;
                    }
                }
                start_scan++;
                search_pattern = 0x25EB;
                if( total_bytes != 0)
                {
                    PutResponse(0,"\r\np->search packet, c->search channel,q->quit,<other keys>->more");
                    while(1)
                    {
                        chars_read = SerialGetChar( 0, &byte);
                        if(chars_read > 0)
                        {
                            if( (byte == 'Q') ||(byte == 'q') ) //Quit List
                            {
                                done = TRUE;
                                search = FALSE;
                                channel_search = FALSE;
                                break;
                            }
                            else if( (byte == 'C') ||(byte == 'c') ) //Search file
                            {
                                PutResponse(0,"\r\nEnter Channel -> ");
                                memset(pattern,0x0,8);
                                j = 0;
                                while(1)
                                {
                                    chars_read = SerialGetChar( 0, &byte);
                                    if(chars_read > 0)
                                    {
                                        if( byte != CarriageReturn)
                                        {
                                            if(byte == LineFeed)
                                            {
                                                break;
                                            }
                                            else if(byte != BackSpace)
                                            {
                                                pattern[j] = byte;
                                                j++;
                                            }
                                        }
                                 
                                    }
                                }
                                sscanf(pattern,"%x",&chars_read);
                                channel = SWAP_TWO(chars_read);
                                search = TRUE;
                                channel_search = TRUE;
                            }
                            else if( (byte == 'P') ||(byte == 'p') ) //Search file
                            {
printf("packet search\r\n");
                                channel_search = FALSE;
                                search = TRUE;
                            }
                            else if( (byte == 'N') ||(byte == 'n') ) //Search file
                            {
                                if(search == TRUE)
                                {
                                    channel_search = TRUE;
                                    search = TRUE;
                                }
                                else
                                {
                                    search = FALSE;
                                    channel_search = FALSE;
                                    break;
                                }
                            }
 
                            /*This is where all the searching will be done*/
                            if(search == TRUE)
                            {
                                before_search_scan = start_scan - 1;
                                before_search_bytes = total_bytes;
                                //start_scan++;
                                while(1)
                                {
                                    if(start_scan < end_scan)
                                    {
                                        DiskReadSeek(start_scan);
                                        DiskRead((char*)Buffer, 512);
                                        total_bytes -= 512;
                                        pcurr_scan = (UINT16*)Buffer;
                                        for( i = 0; i < 256;i++)
                                        {
                                            if( *pcurr_scan == search_pattern)
                                            {
                                                if(channel_search == TRUE)
                                                {
                                                    pcurr_scan++;
                                                    if( *pcurr_scan == channel)
                                                    {
                                                        word_offset = i;
                                                        i = 512;
                                                        DiskReadSeek(start_scan);
                                                    }
                                                }
                                                else
                                                {
                                                    word_offset = i;
                                                    i = 512;
                                                    DiskReadSeek(start_scan);
                                                }
                                            }
                                            else
                                            {
                                                pcurr_scan++;
                                            }
                                        }
                                        if(i < 512)
                                        {
                                            start_scan++;
                                        }
                                        else
                                        {
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        PutResponse(0,"\r\nPattern Not Found!!");
                                        DiskReadSeek(before_search_scan);
                                        start_scan = before_search_scan;
                                        total_bytes = before_search_bytes;
                                        search = FALSE;
                                        channel_search = FALSE;
                                        //done = TRUE; 
                                        break;
                                    }
                                }
                            }
                            break;
  
                        }
                        else
                        {
                            usleep(1000);
                        }
                    }
                }
                else
                {
                    //printf("DONE LISTING\r\n");
                    done = TRUE;
                }
            }
        }

    }

    PutResponse(0,"\r\n*");


}

void CmdLoop( char *parameters )
{
    PutResponse(0,"Command Not Implemented\r\n*");
   //CmdDisplayError( ERROR_COMMAND_FAILED );
}

void CmdMedia( char *parameters )
{
    int return_status = 0;
    UINT32 bytes_per_block = 0;
    UINT32 blocks_used = 0;
    UINT32 blocks_remaining = 0;
    char tmp[64];

    return_status = DiskIsConnected();

    if(return_status)
    {
        return_status = Media( &bytes_per_block, &blocks_used, &blocks_remaining );
        //if(blocks_remaining < 0)
        //printf( "MEDIA %d %u\r\n",blocks_used,blocks_remaining);
   
        if(return_status == NO_ERROR)
        {
            sprintf( tmp, "MEDIA %d %d %u\r\n*", bytes_per_block,blocks_used,blocks_remaining);
            PutResponse( 0, tmp );

        }
        else
        {
            CmdDisplayError( return_status );
        }
    }
    else
    {
        CmdDisplayError( ERROR_NO_MEDIA );
    }

}


void CmdMount( char *parameters )
{
    int return_status = 0;
    int status;
    int numfiles;

    return_status = M23_GetCartridgeStatus(); 
    if(return_status == 1 )
    {
        status = DismountCommandIssued();
        if(status == 0)
        {
            return_status = Mount();

            if(return_status == NO_ERROR)
            {
                PutResponse( 0, "*" );
            }
            else
            {
                CmdDisplayError( return_status );
            }
        }
        else
        {
            ClearDismountCommand();
            M23_ClearHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);

            if(TmatsLoaded)
                M23_ClearFaultLED(0);

            STANAG_GetNumberOfFiles(&numfiles);
            if(numfiles == 0)
            {
                M23_SetEraseLED();
                M23_ClearDataLED();
            }
            else
            {
                M23_ClearEraseLED();
                M23_SetDataLED();
            }

            PutResponse( 0, "*" );
        }

    }
    else if(return_status == 0 )
    {
        /*Check if this was from a dismount command, if so clear that variable*/
        status = DismountCommandIssued();
        if(status == 1)
        {
            ClearDismountCommand();
            M23_ClearHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);
            if(TmatsLoaded)
                M23_ClearFaultLED(0);

            STANAG_GetNumberOfFiles(&numfiles);
            if(numfiles == 0)
            {
                M23_SetEraseLED();
                M23_ClearDataLED();
            }
            else
            {
                M23_ClearEraseLED();
                M23_SetDataLED();
            }

        }

        PutResponse( 0, "*" );
    }
    else
    {
        CmdDisplayError( ERROR_NO_MEDIA );
    }

}

void CmdPCI( char *parameters )
{
}

void CmdPlay( char *parameters )
{
    char FileName[20];
    int  offset = 0;
    int  CurrentPlay;
    int  CurrentRecord;
    int  speed = 0;
    int  status;
    int  state;
    int  i;
    int  end;
    int  config;
    char mult_div = '@';

    M23_RecorderState(&state);

    if( (state == STATUS_PLAY) || (state == STATUS_REC_N_PLAY) )
    {
	CmdDisplayError( ERROR_INVALID_MODE );
        return;
    }

    if(AssignReceived == 0)
    {
	CmdDisplayError( ERROR_INVALID_MODE );
        return;
    }

    M23_GetConfig(&config);
    if( (config == 0) || (config == P3_CONFIG) )
    {

        status = DiskIsConnected();
        if(status)
        {

            while ( *parameters != '\0' )
            {
                if ( isspace( *parameters ) )
                    *parameters++;
                else
                    break;
            }

            if(*parameters == '\0' )
            {
                sGetPlayLocation(&CurrentPlay);    
                sCurrentBlock(&CurrentRecord);
                if(CurrentPlay >= CurrentRecord)
                {
                    CmdDisplayError( ERROR_COMMAND_FAILED );
                    return;
                }
            }
            else if(isdigit(*parameters) )
            {
                /*Location is Block Number*/
                sscanf(parameters,"%d %c%d",&CurrentPlay,&mult_div,&speed);
            }
            else if( (*parameters == '*') || (*parameters == '/') )
            {
                sGetPlayLocation(&CurrentPlay);    
                sscanf(parameters,"%c%d",&mult_div,&speed);
            }
            else
            {
                /*We need to check if it is Filename[offset] */

                memset(FileName,0x0,20);
                i = 0;
                while(1)
                {
                    if( (isspace(*(parameters + i)) ) || (*(parameters + i) == '\0') )
                    {
                        break;
                    }
                    else
                    {
                        i++;
                    }
                }

                if( (*(parameters +(i+1)) == '*') || (*(parameters + (i+1)) == '/') )
                {
                    sscanf(parameters,"%s %c%d",FileName,&mult_div,&speed);
                }
                else if( isdigit( *(parameters +(i+1) ) ) )
                {
                    sscanf(parameters,"%s %d %c%d",FileName,&offset,&mult_div,&speed);
                    if( (mult_div == '*') || (mult_div == '/') )
                    {
                    }
                    else
                    {
                        CmdDisplayError( ERROR_INVALID_PARAMETER );
                        return;
                    }
                }
                else
                {
                    speed  = 0;
                    offset = 0;
                    sscanf(parameters,"%s %c%d",FileName,&mult_div,&speed);

                }


                status = M23_GetBlockFromFile(FileName,offset,&CurrentPlay,&end);
                if(status != 0)
                {
                    CmdDisplayError( ERROR_COMMAND_FAILED );
                    return;
                }
            }
     
        }
        else
        {
            CmdDisplayError( ERROR_NO_MEDIA );
            return;
        }


        /*Now Start the Playback*/
        //printf("Start Playback @ %d\r\n",CurrentPlay);
        M23_StartPlayback(CurrentPlay);

        if(mult_div == '*')
        {
//printf("Speed is > 2x\r\n");
            M23_CS6651_03X();
            M23_SetPlaySpeedStatus(0x2);
        }
        else if(mult_div == '/')
        {
            if(speed > 2)
            {
//printf("Speed is  1/30 \r\n");
                speed = 30;
                M23_SetPlaySpeedStatus(0xF);
            }
            else
            {
//printf("Speed is  1/2 \r\n");
                speed = 2;
                M23_SetPlaySpeedStatus(0xE);
            }
            M23_CS6651_SlowX(speed);
        }
        else
        {
            M23_SetPlaySpeedStatus(0x1);
#if 0
            if(first > 0)
            {
                M23_CS6651_Normal();
            }
            first = 1;
#endif
//printf("Speed is Normal \r\n");
        }

        PutResponse(0,"*");
    }
    else
    {
        CmdDisplayError( ERROR_INVALID_COMMAND );
    }
   
} 

void CmdSpeed( char *parameters )
{
    char FileName[20];
    int  offset = 0;
    int  CurrentPlay;
    int  CurrentRecord;
    int  speed = 0;
    int  status;
    int  i;
    int  end;
    int  config;
    char mult_div = '@';

    M23_GetConfig(&config);
    if( (config == 0) || (config == P3_CONFIG) )
    {

        status = DiskIsConnected();
        if(status)
        {

            while ( *parameters != '\0' )
            {
                if ( isspace( *parameters ) )
                    *parameters++;
                else
                    break;
            }

            if(*parameters == '\0' )
            {
                sGetPlayLocation(&CurrentPlay);    
                sCurrentBlock(&CurrentRecord);
                if(CurrentPlay >= CurrentRecord)
                {
                    CmdDisplayError( ERROR_COMMAND_FAILED );
                    return;
                }
            }
            else if(isdigit(*parameters) )
            {
                /*Location is Block Number*/
                sscanf(parameters,"%d %c%d",&CurrentPlay,&mult_div,&speed);
            }
            else if( (*parameters == 'x') || (*parameters == 'X') || (*parameters == '/') )
            {
                sGetPlayLocation(&CurrentPlay);    
                sscanf(parameters,"%c%d",&mult_div,&speed);
            }
            else
            {
                /*We need to check if it is Filename[offset] */

                memset(FileName,0x0,20);
                i = 0;
                while(1)
                {
                    if( (isspace(*(parameters + i)) ) || (*(parameters + i) == '\0') )
                    {
                        break;
                    }
                    else
                    {
                        i++;
                    }
                }

                if( (*(parameters +(i+1)) == 'x') || (*(parameters +(i+1)) == 'X') || (*(parameters + (i+1)) == '/') )
                {
                    sscanf(parameters,"%s %c%d",FileName,&mult_div,&speed);
                }
                else if( isdigit( *(parameters +(i+1) ) ) )
                {
                    sscanf(parameters,"%s %d %c%d",FileName,&offset,&mult_div,&speed);
                    if( (mult_div == 'x') || (mult_div == 'X') || (mult_div == '/') )
                    {
                    }
                    else
                    {
                        CmdDisplayError( ERROR_INVALID_PARAMETER );
                        return;
                    }
                }
                else
                {
                    speed  = 0;
                    offset = 0;
                    sscanf(parameters,"%s %c%d",FileName,&mult_div,&speed);

                }


                status = M23_GetBlockFromFile(FileName,offset,&CurrentPlay,&end);
                if(status != 0)
                {
                    CmdDisplayError( ERROR_COMMAND_FAILED );
                    return;
                }
            }
     
        }
        else
        {
            CmdDisplayError( ERROR_NO_MEDIA );
            return;
        }


        /*Now Start the Playback*/
        //printf("Start Playback @ %d\r\n",CurrentPlay);
        M23_StartPlayback(CurrentPlay);

        if( (mult_div == 'x') || (mult_div == 'X') )
        {
//printf("Speed is > 2x\r\n");
            if(speed == 2)
            {
                M23_CS6651_02X();
            }
            else
            {
                M23_CS6651_03X();
            }
        }
        else if(mult_div == '/')
        {
            if(speed > 2)
            {
//printf("Speed is  1/30 \r\n");
                speed = 30;
            }
            else
            {
//printf("Speed is  1/2 \r\n");
                speed = 2;
            }
            M23_CS6651_SlowX(speed);
        }
        else
        {
            M23_CS6651_Normal();
//printf("Speed is Normal \r\n");
        }

        PutResponse(0,"*");
    }
    else
    {
        CmdDisplayError( ERROR_INVALID_COMMAND );
    }
   
} 




void CmdPTest( char *parameters )
{
    int     status;
    int     state;
    UINT32  start_scan = 129;
    UINT32  test = 1;



    if(*parameters == '\0' )
    {
        CmdDisplayError( ERROR_INVALID_PARAMETER );
    }
    else
    {
        sscanf(parameters,"%d %d",&start_scan,&test);
        status = M23_PerformPtest(start_scan,test);

        if(status == 0 )
        {
            printf("Starting PTEST @ %d with test %d\r\n",start_scan,test);
            M23_RecorderState(&state);

            if(state == STATUS_RTEST)
            {
                M23_SetRecorderState(STATUS_P_R_TEST);
            }
            else
            {
                M23_SetRecorderState(STATUS_PTEST);
            }

            PutResponse( 0, "*" );
        }
        else
        {
            CmdDisplayError( ERROR_COMMAND_FAILED );
        }
    }

}

void CmdRecord( char *parameters )
{
    int   return_status = 0;
    int   config;
    char  Unit[33];



    M23_GetConfig(&config);

    if( (config == 2) || (AutoRecord == 1) )
    {
        PutResponse( 0, "*" );
	return;
    }

    return_status = DiskIsConnected();

    if(return_status)
    {
        if(*parameters == '\0' )
        {

            return_status = Record();
   
            if(return_status == NO_ERROR)
            {
                memset(Unit,0x0,33);
                M23_GetUnitName(Unit);

                if(EventTable[6].CalculexEvent)
                {
                    EventRecordSet(6);
                }
                else
                {
                    m23_WriteSavedEvents();
                }
 
                PutResponse( 0, "*" );
                CommandFrom = FROM_COM;
            }
            else
            {
                CmdDisplayError( return_status );

            }
        }
        else
        {
            return_status = RecordFile( parameters );
   
            if(return_status == NO_ERROR)
            {
                if(EventTable[6].CalculexEvent)
                {
                    EventRecordSet(6);
                }

                PutResponse( 0, "*" );
                CommandFrom = FROM_COM;
            }
            else
            {
                CmdDisplayError( return_status );

            }
    
        }
    }
    else
    {
        CmdDisplayError( ERROR_NO_MEDIA );
    }


}

void CmdReplay( char *parameters )
{
    PutResponse(0,"Command Not Implemented\r\n*");
   //CmdDisplayError( ERROR_COMMAND_FAILED );
}

void CmdReset( char *parameters )
{
    int  config;
   
    M23_ResetSystem();
}

void CmdRTest( char *parameters )
{
    int  scans = 0;
    int  test = 5;
    int  return_status;
    char tmp[40];


    return_status = DiskIsConnected();

    if(return_status)
    {
        memset(tmp,0x0,40);


        if(*parameters == '\0' )
        {
            //scans = 0;
            scans = -1;
        }
        else
        {
            sscanf(parameters,"%d",&test);
        }

        M23_PerformRtest(scans,test,0);
        M23_SetRecorderState(STATUS_RTEST);
        M23_SetRecordLED();
        M23_ClearEraseLED();    
        M23_SetDataLED();


        PutResponse( 0, "*" );
    }
    else
    {
        CmdDisplayError( ERROR_NO_MEDIA );
    }

}


void CmdSave( char *parameters )
{
    int return_status = 0;

    return_status = SaveSettings();

    if ( return_status != NO_ERROR )
        CmdDisplayError( return_status );
    else
    {
        sync();
        sync();
        PutResponse( 0, "*" );
    }
}

void CmdSet( char *parameters )
{
    char         tmp[80];
    char         *p_param;
    int          value1;
    int          value;
    int          return_status = 0;
    int          state;
    int          numVideo;
    int          device;
    int          channel;
    UINT32       CSR;
    M23_CHANNEL  const *config;

    M23_RecorderState(&state);

    /*The Following are for setting Overlay*/

    // change parameters to upper case
    //
    p_param = parameters;
    while ( *p_param != '\0' ) {
        if( (*p_param >= 'a') && (*p_param <= 'z') )
            *p_param -= ('a' - 'A');
        p_param++;
    }



    if ( !strncmp( parameters, "COM1", 4 ) )
    {
        // is there a value?
        //
        parameters += 3;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
                *parameters++;
            else
                break;
        }

        if(*(parameters+1) == '\0')
        {
            PutComSettings( &Serial_Config[0] ,0,0);
            PutResponse(0,"*");
        }
        else
        {
            return_status = SetCom( parameters ,0);

            if ( return_status )
                CmdDisplayError( return_status );
        }
    }
    else if ( !strncmp( parameters, "COM2", 4) )
    {
        // is there a value?
        //
        parameters += 3;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
                *parameters++;
            else
                break;
        }

        if(*(parameters + 1) == '\0')
        {
	    PutComSettings( &Serial_Config[1] ,1,0);
            PutResponse(0,"*");
        }
        else
        {
            // skip until parameter or end of line is found
            // 
            while ( *parameters != '\0' )
            {
                if ( isspace( *parameters ) )
                    *parameters++;
                else
                    break;
            }

            return_status = SetCom( parameters ,1);

            if ( return_status )
                CmdDisplayError( return_status );
        }
    }
    else
    if ( !strncmp( parameters, "ECHO", 4 ) )
    {
        // is there a value?
        //
        parameters += 4;


        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
                *parameters++;
            else
                break;
        }


        if ( 1 == sscanf( parameters, "%d", &value1 ) )
        {
            if ( value1 > 1 )
                return_status = ERROR_INVALID_PARAMETER;

            if ( return_status )
                CmdDisplayError( return_status );
            else
            {
                //Console_Echo = value1;
                Console_Echo = 0;
                sprintf( tmp, "SET ECHO %d\r\n*", Console_Echo );
                PutResponse( 0, tmp );
            }
        }
        else
        {
            sprintf( tmp, "SET ECHO %d\r\n*", Console_Echo );
            PutResponse( 0, tmp );
        }
    }
    else
    if ( !strncmp( parameters, "VERBOSE", 7 ) )
    {
        // is there a value?
        //
        parameters += 7;


        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0')
        {
            /*Display Current value Here*/
            M23_GetVerbose(&value);
            if(value)
            {
                PutResponse( 0, "VERBOSE ON\r\n*");
            }
            else
            {
                PutResponse( 0, "VERBOSE OFF\r\n*");
            }
        }
        else if(strncmp(parameters,"ON",2) == 0 )
        {
            value1 = 1;
            sprintf( tmp, "SET VERBOSE ON\r\n*");
            PutResponse( 0, tmp );
        }
        else if(strncmp(parameters,"OFF",3) == 0 )
        {
            value1 = 0;
            sprintf( tmp, "SET VERBOSE OFF\r\n*");
            PutResponse( 0, tmp );
        }
        else
        {
            return_status = ERROR_INVALID_PARAMETER;
        }

        if ( return_status )
        {
            CmdDisplayError( return_status );
        }
        else 
        {
            M23_SetVerbose(value1);
        }
 

    }
    else if ( !strncmp( parameters, "ONTIMECAL", 9 ) )
    {
        parameters += 9;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0')
        {
            /*Display Current value Here*/
            M23_GetTimeDebug(&value);
            if(value)
            {
                PutResponse( 0, "SET ONTIMECAL ON\r\n*");
            }
            else
            {
                PutResponse( 0, "SET ONTIMECAL OFF\r\n*");
            }
        }
        else if(strncmp(parameters,"ON",2) == 0 )
        {
            value1 = 1;
            sprintf( tmp, "SET ONTIMECAL ON\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"OFF",3) == 0 )
        {
            value1 = 0;
            sprintf( tmp, "SET ONTIMECAL OFF\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else
        {
            return_status = ERROR_INVALID_PARAMETER;
            value1 = 2;
        }

        if ( return_status )
        {
            CmdDisplayError( return_status );
        }
        else 
        {
            M23_SetTimeDebug(value1);
            CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ADJUST_CSR);
            if(value1 == 1)
            {
                M23_WriteControllerCSR(CONTROLLER_TIMECODE_ADJUST_CSR,CSR | CONTROLLER_TIMECODE_DEBUG);
            }
            else if(value1 == 0 )
            {
                M23_WriteControllerCSR(CONTROLLER_TIMECODE_ADJUST_CSR,CSR & ~CONTROLLER_TIMECODE_DEBUG);
            }
        }

    }
    else if ( !strncmp( parameters, "ONTIMEVAL", 9 ) )
    {
        parameters += 9;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        SetupGet(&config);
        if(*parameters == '\0')
        {
            /*Display Current value Here*/
            if(config->timecode_chan.Format == 'A')
            {
                M23_GetOnTime(&value,1);
            }
            else if(config->timecode_chan.Format == 'B')
            {
                M23_GetOnTime(&value,0);
            }
            else //This must be G
            {
                M23_GetOnTime(&value,2);
            }
            sprintf( tmp, "ONTIME 0x%x\r\n*", value );
            PutResponse( 0, tmp );
        }
        else if ( 1 == sscanf( parameters, "%x", &value1 ) )
        {
            if(config->timecode_chan.Format == 'A')
            {
                M23_SetTimeAdjust(1,value1,1);
            }
            else if(config->timecode_chan.Format == 'B')
            {
                M23_SetTimeAdjust(1,value1,0);
            }
            else //This must be G
            {
                M23_SetTimeAdjust(1,value1,2);
            }

            M23_WriteControllerCSR(CONTROLLER_TIMECODE_ADJUST_CSR,value1);
            sprintf( tmp, "SET ONTIMEVAL 0x%x\r\n*", value1 );
            PutResponse( 0, tmp );
        }

    }
    else if ( !strncmp( parameters, "VOLUME", 6 ) )
    {
        parameters += 6;

        // skip until parameter or end of line is found
        // 
        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_PrintVolumeName();
        }
        else if( (strlen(parameters) > 32) )
        {
            CmdDisplayError( ERROR_INVALID_PARAMETER );
        }
        else
        {
            while ( *parameters != '\0' )
            {
                if ( isspace( *parameters ) )
                *parameters++;
                else
                break;
            }
    
            return_status = DiskIsConnected();

            if(return_status)
            {
                M23_SetVolumeName(parameters);
                sprintf( tmp, "SET VOLUME  %s\r\n*",parameters);
                PutResponse( 0, tmp );
            }
            else
            {
                CmdDisplayError( ERROR_NO_MEDIA );
            }
        }

    }
    else if ( !strncmp( parameters, "UNITNAME", 8 ) )
    {
        parameters += 8;

        // skip until parameter or end of line is found
        // 

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_PrintUnitName();
        }
        else if( (strlen(parameters) > 32) )
        {
            CmdDisplayError( ERROR_INVALID_PARAMETER );
        }
        else
        {
            while ( *parameters != '\0' )
            {
                if ( isspace( *parameters ) )
                *parameters++;
                else
                break;
            }
    
            M23_SetUnitName(parameters);

            
            memset(tmp,0x0,80);
            sprintf( tmp, "SET UNITNAME  %s\r\n*",parameters);
            PutResponse( 0, tmp );

        }

    }
    else if ( !strncmp( parameters, "RTADDR", 6 ) )
    {
        parameters += 6;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetRemoteTerminalValue(&value);
            if(value == -1)
            {
                PutResponse( 0, "RTADDR OFF\r\n*");
            }
            else
            {
                sprintf( tmp, "RTADDR  %d\r\n",value);
                PutResponse( 0, tmp );
            }
        }
        else if(strncmp(parameters,"OFF",3) == 0 )
        {
            value1 = -1;
            sprintf( tmp, "SET RTADDR OFF\r\n");
            PutResponse( 0, tmp );
        }
        else if ( 1 == sscanf( parameters, "%d", &value1 ) )
        {
            if( (value1 > 30) || (value1 < 0) )
            {
                return_status = ERROR_INVALID_PARAMETER;
            } 
            else
            {
                sprintf( tmp, "SET RTADDR  %d\r\n",value1);
                PutResponse( 0, tmp );
            }
          
        }
        else
        {
            return_status = ERROR_INVALID_PARAMETER;
        }

        if ( return_status )
        {
            CmdDisplayError( return_status );
        }
        else 
        {
            M23_SetRemoteTerminalValue(value1);
            if(value1 == -1)
            {
                M23_ClearRT();
            }
            else
            {
                M23_SetupRT(value1);
            }
            PutResponse( 0, "*" );
        }
    }
    else if ( !strncmp( parameters, "SSRIC", 5 ) )
    {
        parameters += 5;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if( *parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetSsric(&value);
            if(value)
            {
                PutResponse( 0, "SSRIC ON\r\n*");
            }
            else
            {
                PutResponse( 0, "SSRIC OFF\r\n*");
            }
        }
        else if(strncmp(parameters,"ON",2) == 0 )
        {
            value1 = 1;
            sprintf( tmp, "SET SSRIC ON\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"OFF",3) == 0 )
        {
            value1 = 0;
            sprintf( tmp, "SET SSRIC OFF\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else
        {
            return_status = ERROR_INVALID_PARAMETER;
            value1 = 2;
        }
        if ( return_status )
        {
            CmdDisplayError( return_status );
        }
        else 
        {
            M23_SetSsric(value1);
        }
    }
    else if ( !strncmp( parameters, "TMOUT", 5 ) )
    {
        parameters += 5;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
                *parameters++;
            else
                break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetLiveVideo(&value);   
            sprintf( tmp, "TMOUT %d\r\n*",value);
            PutResponse( 0, tmp );
        }
        else if ( 1 == sscanf( parameters, "%d", &value1 ) )
        {
            numVideo = M23_MP_GetNumVideo();
            SetupGet(&config);
            if( value1 > ( numVideo * 4))
            {
                CmdDisplayError( ERROR_INVALID_PARAMETER );
            }
            else if( (!config->video_chan[value1 -1].isEnabled) && (value1 != 0) )
          {
                CmdDisplayError( ERROR_INVALID_PARAMETER );
            }
            else
            {
                if( (state != STATUS_PLAY) && (state != STATUS_REC_N_PLAY) )
                {
                    M23_SetLiveVideo(value1);
                    sprintf( tmp, "SET TMOUT %d\r\n*",value1);
                    PutResponse( 0, tmp );
                }
                else
                {
                    CmdDisplayError( ERROR_INVALID_MODE );
                }
            }
        }
        else
        {
            M23_GetLiveVideo(&value1);
            sprintf( tmp, "SET TMOUT %d\r\n*",value1);
            PutResponse( 0, tmp );
        }
      
    }
    else if ( !strncmp( parameters, "LIVEVID", 7 ) )
    {
        parameters += 7;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
                *parameters++;
            else
                break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetLiveVideo(&value);   
            sprintf( tmp, "LIVEVID %d\r\n*",value);
            PutResponse( 0, tmp );
        }
        else if ( 1 == sscanf( parameters, "%d", &value1 ) )
        {
            numVideo = M23_MP_GetNumVideo();
            SetupGet(&config);
            if( value1 > ( numVideo * 4))
            {
                CmdDisplayError( ERROR_INVALID_PARAMETER );
            }
            else if( (!config->video_chan[value1 -1].isEnabled) && (value1 != 0) )
            {
                CmdDisplayError( ERROR_INVALID_PARAMETER );
            }
            else
            {
                if( (state != STATUS_PLAY) && (state != STATUS_REC_N_PLAY) )
                {
                    M23_SetLiveVideo(value1);
                    sprintf( tmp, "SET LIVEVID %d\r\n*",value1);
                    PutResponse( 0, tmp );
                }
                else
                {
                    CmdDisplayError( ERROR_INVALID_MODE );
                }
            }
        }
        else
        {
            M23_GetLiveVideo(&value1);
            sprintf( tmp, "SET LIVEVID %d\r\n*",value1);
            PutResponse( 0, tmp );
        }
      
    }
    else if ( !strncmp( parameters, "SERIAL1", 7 ) )
    {
        parameters += 7;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetSerial1(&value);
            if(value == 0)
            {
                PutResponse( 0, "SERIAL1 NONE\r\n*");
            }
            else if(value == 1)
            {
                PutResponse( 0, "SERIAL1 PCM\r\n*");
            }
            else if(value == 2)
            {
                PutResponse( 0, "SERIAL1 VID\r\n*");
            }
            else if(value == 3)
            {
                PutResponse( 0, "SERIAL1 MIL1553\r\n*");
            }
        }
        else if(strncmp(parameters,"NONE",4) == 0 )
        {
            value1 = 0;
            sprintf( tmp, "SET SERIAL1 NONE\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"PCM",3) == 0 )
        {
            value1 = 1;
            sprintf( tmp, "SET SERIAL1 PCM\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"VID",3) == 0 )
        {
            value1 = 2;
            sprintf( tmp, "SET SERIAL1 VID\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"MIL1553",7) == 0 )
        {
            value1 = 3;
            sprintf( tmp, "SET SERIAL1 MIL1553\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else
        {
            return_status = ERROR_INVALID_PARAMETER;
        }
        if ( return_status )
        {
            CmdDisplayError( return_status );
        }
        else 
        {
            M23_SetSerial(1,value1);
        }
    }
    else if ( !strncmp( parameters, "SERIAL2", 7 ) )
    {
        parameters += 7;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetSerial2(&value);
            if(value == 0)
            {
                PutResponse( 0, "SERIAL2 NONE\r\n*");
            }
            else if(value == 1)
            {
                PutResponse( 0, "SERIAL2 PCM\r\n*");
            }
            else if(value == 2)
            {
                PutResponse( 0, "SERIAL2 VID\r\n*");
            }
            else if(value == 3)
            {
                PutResponse( 0, "SERIAL2 MIL1553\r\n*");
            }
        }
        else if(strncmp(parameters,"NONE",4) == 0 )
        {
            value1 = 0;
            sprintf( tmp, "SET SERIAL2 NONE\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"PCM",3) == 0 )
        {
            value1 = 1;
            sprintf( tmp, "SET SERIAL2 PCM\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"VID",3) == 0 )
        {
            value1 = 2;
            sprintf( tmp, "SET SERIAL2 VID\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"MIL1553",7) == 0 )
        {
            value1 = 3;
            sprintf( tmp, "SET SERIAL2 MIL1553\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else
        {
            return_status = ERROR_INVALID_PARAMETER;
        }
        if ( return_status )
        {
            CmdDisplayError( return_status );
        }
        else 
        {
            M23_SetSerial(2,value1);
        }
    }
    else if ( !strncmp( parameters, "BINGO", 5 ) )
    {
        parameters += 5;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetBingo(&value);
            sprintf( tmp, "BINGO %d\r\n*", value );
            PutResponse( 0, tmp );
        }
        else if ( 1 == sscanf( parameters, "%d", &value1 ) )
        {
            if( (value1 < 0) || (value1 > 100) )
            {
                CmdDisplayError( ERROR_INVALID_PARAMETER );
            }
            else
            {
                M23_SetBingo(value1);
                sprintf( tmp, "SET BINGO %d\r\n*", value1 );
                PutResponse( 0, tmp );
            }
        }

    }
    else if ( !strncmp( parameters, "YEAR", 4 ) )
    {
        parameters += 4;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetYear(&value);
            sprintf( tmp, "YEAR %d\r\n*", value );
            PutResponse( 0, tmp );
        }
        else if ( 1 == sscanf( parameters, "%d", &value1 ) )
        {
            if(value1 < 0)
            {
                CmdDisplayError( ERROR_INVALID_PARAMETER );
            }
            else
            {
                M23_SetYear(value1);
                sprintf( tmp, "SET YEAR %d\r\n*", value1 );
                PutResponse( 0, tmp );
            }
        }

    }
    else if ( !strncmp( parameters, "IGNORE", 6 ) )
    {
        parameters += 6;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetIgnorePresent(&value);
            if(value)
            {
                PutResponse( 0, "IGNORE ON\r\n*");
            }
            else
            {
                PutResponse( 0, "IGNORE OFF\r\n*");
            }
        }
        else if(strncmp(parameters,"ON",2) == 0 )
        {
            value1 = 1;
            M23_SetIgnorePresent(value1);
            sprintf( tmp, "SET IGNORE ON\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"OFF",3) == 0 )
        {
            value1 = 0;
            M23_SetIgnorePresent(value1);
            sprintf( tmp, "SET IGNORE OFF\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else
        {
            CmdDisplayError( ERROR_INVALID_PARAMETER );
        }

    }
    else if ( !strncmp( parameters, "SCRIPT", 6 ) )
    {
        parameters += 6;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetScript(&value);
            if(value)
            {
                PutResponse( 0, "SCRIPT ON\r\n*");
            }
            else
            {
                PutResponse( 0, "SCRIPT OFF\r\n*");
            }
        }
        else if(strncmp(parameters,"ON",2) == 0 )
        {
            M23_SetScript(1);
            sprintf( tmp, "SET SCRIPT ON\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"OFF",3) == 0 )
        {
            M23_SetScript(0);
            sprintf( tmp, "SET SCRIPT OFF\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else
        {
            CmdDisplayError( ERROR_INVALID_PARAMETER );
        }

    }
    else if ( !strncmp( parameters, "MASKPCM", 7 ) )
    {
        parameters += 8;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetPCMStatusMask(&value);
            sprintf( tmp, "MASKPCM 0x%x\r\n*", value );
            PutResponse( 0, tmp );
        }
        else if ( 1 == sscanf( parameters, "%x", &value1 ) )
        {
            if(value1 < 0)
            {
                CmdDisplayError( ERROR_INVALID_PARAMETER );
            }
            else
            {
                M23_SetPCMStatusMask(value1);
                sprintf( tmp, "SET MASKPCM 0%x\r\n*", value1 );
                PutResponse( 0, tmp );
            }
        }

    }
    else if ( !strncmp( parameters, "MASK1553", 8 ) )
    {
        parameters += 8;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetM1553StatusMask(&value);
            sprintf( tmp, "MASK1553 0x%x\r\n*", value );
            PutResponse( 0, tmp );
        }
        else if ( 1 == sscanf( parameters, "%x", &value1 ) )
        {
            if(value1 < 0)
            {
                CmdDisplayError( ERROR_INVALID_PARAMETER );
            }
            else
            {
                M23_SetM1553StatusMask(value1);
                sprintf( tmp, "SET MASK1553 0%x\r\n*", value1 );
                PutResponse( 0, tmp );
            }
        }

    }
    else if ( !strncmp( parameters, "VIDEO", 5 ) )
    {
        parameters += 5;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }


        if(*parameters == '\0')
        {
            /*Display all Video Information*/
            PutOverlayInformation();
            PutResponse( 0, "*" );
        }
        else
        {
            /*Set the information*/
            if ( 1 == sscanf( parameters, "%d", &value1 ) )
            {
                if(value1 < 0)
                {
                    CmdDisplayError( ERROR_INVALID_PARAMETER );
                }
                else
                {
                    return_status = M23_SetOverlayParameters(value1);
                    PutResponse( 0, "*" );
                }
            }
        }

    }
    else if ( !strncmp( parameters, "TEMP", 4 ) )
    {
        return_status = SetTempValues();
    
        if(return_status == 0)
        {
            PutResponse( 0, "*" );
        }
        else
        {
            CmdDisplayError( ERROR_COMMAND_FAILED );
        }
    }
    else if ( !strncmp( parameters, "POWER", 5 ) )
    {
        return_status = SetPowerValues();
   
        if(return_status == 0)
        {
            PutResponse( 0, "*" );
        }
        else
        {
            CmdDisplayError( ERROR_COMMAND_FAILED );
        }
    }
    else if ( !strncmp( parameters, "DRIFTCAL", 8 ) )
    {

        /*Make sure time is external and is locked*/
        return_status = M23_CheckTimecodeSync();
        if(return_status == 0)
        {
            M23_SetTimeCodeOffset();
            M23_GetTimeCodeOffset(&value1);
            sprintf( tmp, "DRIFTCAL %d\r\n*",value1);
            PutResponse( 0, tmp );
        }
        else
        {
            CmdDisplayError( ERROR_COMMAND_FAILED );
        }
    }
    else if ( !strncmp( parameters, "REMOTE", 6 ) )
    {
        parameters += 6;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetRemoteValue(&value);
            if(value)
            {
                PutResponse( 0, "REMOTE ON\r\n*");
            }
            else
            {
                PutResponse( 0, "REMOTE OFF\r\n*");
            }
        }
        else if(strncmp(parameters,"ON",2) == 0 )
        {
            value1 = 1;
            M23_SetRemoteValue(value1);
            sprintf( tmp, "SET REMOTE ON\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"OFF",3) == 0 )
        {
            value1 = 0;
            M23_SetRemoteValue(value1);
            sprintf( tmp, "SET REMOTE OFF\r\n");
            PutResponse( 0, tmp );
            PutResponse( 0, "*" );
        }
        else
        {
            CmdDisplayError( ERROR_INVALID_PARAMETER );
        }
    }
    else if ( !strncmp( parameters, "IEEE1394B", 9 ) )
    {
        parameters += 9;

        // skip until parameter or end of line is found
        // 
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(*parameters == '\0') 
        {
            /*Display Current value Here*/
            M23_GetLongCableValue(&value);
            if(value)
            {
                PutResponse( 0, "IEEE1394B S400\r\n*");
            }
            else
            {
                PutResponse( 0, "IEEE1394B S800\r\n*");
            }
        }
        else if(strncmp(parameters,"S400",4) == 0 )
        {
            value1 = 1;
            M23_SetLongCableValue(value1);
            sprintf( tmp, "SET IEEE1394B S400\r\n");
            PutResponse( 0, tmp );
            PutResponse(0,"Must perform a .save and cycle Power to take effect\r\n");
            PutResponse( 0, "*" );
        }
        else if(strncmp(parameters,"S800",3) == 0 )
        {
            value1 = 0;
            M23_SetLongCableValue(value1);
            sprintf( tmp, "SET IEEE1394B S800\r\n");
            PutResponse( 0, tmp );
            PutResponse(0,"Must perform a .save and cycle Power to take effect\r\n");
            PutResponse( 0, "*" );
        }
        else
        {
            CmdDisplayError( ERROR_INVALID_PARAMETER );
        }
    }
    else if ( !strncmp( parameters, "TAIL", 4 ) )
    {
        parameters += 4;

        // skip until parameter or end of line is found
        //
        if((*parameters == '\0'))
        {
            M23_PrintTailNumbers();
            PutResponse( 0, "*" );
        }
        else
        {
            while ( *parameters != '\0' )
            {
                if ( isspace( *parameters ) )
                *parameters++;
                else
                break;
            }
            memset(tmp,0x0,80);
            if ( 1 == sscanf( parameters, "%d", &value1) )
            {
                if( (value1 < 0) || (value1 > 31) )
                {
                    CmdDisplayError( ERROR_INVALID_PARAMETER );
                }
                else
                {
                    if(value1 < 10)
                    {
                        parameters += 2;
                    }
                    else
                    {
                        parameters += 3;
                    }

                    if( *parameters != '\0' )
                    {
                        M23_SetTailNumber(value1,parameters);
                        PutResponse( 0, "*" );
                    }
                    else
                    {
                        CmdDisplayError( ERROR_INVALID_PARAMETER );
                    }
                }
            }
        }
    }
    else if ( !strncmp( parameters, "DAC", 3 ) )
    {
        parameters += 3;

        M23_TrainTimeCode();
        // skip until parameter or end of line is found
        //
    }
    else if ( !strncmp( parameters, "CONFIG", 6 ) )
    {
        parameters += 6;

        // skip until parameter or end of line is found
        //
        if(*parameters == '\0' )
        {
            M23_GetConfig(&value1);
            sprintf( tmp, "Current CONFIG %d\r\n", value1 );
            PutResponse( 0, tmp );
            PutResponse(0,"\r\nAvailable Configurations\r\n");
            PutResponse(0,"0 - F15 DVRS\r\n");
            PutResponse(0,"1 - Standard Eglin AFB\r\n");
            PutResponse(0,"2 - Eglin AFB with Auto Record\r\n");
            PutResponse(0,"3 - Airbus Ethernet\r\n");
            PutResponse(0,"4 - Edwards AFB\r\n");
            PutResponse(0,"5 - Lockheed F16\r\n");
            PutResponse(0,"6 - P3\r\n");
            PutResponse(0,"7 - B1B\r\n");
            PutResponse(0,"8 - B52\r\n");
        }
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }
        if ( 1 == sscanf( parameters, "%d", &value1 ) )
        {
            if ( (value1 < 0 ) || (value1 > 255))
                return_status = ERROR_INVALID_PARAMETER;

            if ( return_status )
            {
                CmdDisplayError( return_status );
            }
            else
            {
                M23_SetConfig(value1);
                sprintf( tmp, "SET CONFIG %d\r\n*", value1 );
                PutResponse( 0, tmp );
            }
        }

    }
    else if ( !strncmp( parameters, "PCM_DAC", 8 ) )
    {
        parameters += 8;

        // skip until parameter or end of line is found
        //
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }
        if ( 1 == sscanf( parameters, "%d %d %x %x",&device,&channel, &value,&value1 ) )
        {
            if ( (value1 < 0 ) || (value1 > 255))
                return_status = ERROR_INVALID_PARAMETER;

            if ( return_status )
                CmdDisplayError( return_status );
            else
            {
                M23_SetPCMDAC(channel,value,value1);
                M23_MP_SetDAC(device,channel,value,value1);
                sprintf( tmp, "SET PCM_DAC %d 0x%x 0x%x\r\n*",channel,value, value1 );
                PutResponse( 0, tmp );
            }
        }

    }
    else if ( !strncmp( parameters, "DEBUG", 5 ) )
    {
        parameters += 5;

        // skip until parameter or end of line is found
        //
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(strncmp(parameters,"ON",2) == 0 )
        {
            M23_SetDebugValue(1);
            PutResponse( 0, "SET DEBUG ON\r\n*");
        }
        else if(strncmp(parameters,"OFF",3) == 0 )
        {
            M23_SetDebugValue(0);
            PutResponse( 0, "SET DEBUG OFF\r\n*");
        }
        else
        {
            CmdDisplayError( ERROR_INVALID_PARAMETER );
        }
    }
    else if ( !strncmp( parameters, "GPS_DEBUG", 9 ) )
    {
        parameters += 9;

        // skip until parameter or end of line is found
        //
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }

        if(strncmp(parameters,"ON",2) == 0 )
        {
            M23_SetGPSDebugValue(1);
            PutResponse( 0, "SET GPS_DEBUG ON\r\n*");
        }
        else if(strncmp(parameters,"OFF",3) == 0 )
        {
            M23_SetGPSDebugValue(0);
            PutResponse( 0, "SET GPS_DEBUG OFF\r\n*");
        }
        else
        {
            CmdDisplayError( ERROR_INVALID_PARAMETER );
        }
    }
    else if ( !strncmp( parameters, "FACTOR", 6 ) )
    {
        parameters += 6;

        SetupGet(&config);
        // skip until parameter or end of line is found
        //
        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }
        if ( 1 == sscanf( parameters, "%x", &value1 ) )
        {
            if(config->timecode_chan.Format == 'A')
            {
                M23_SetFactor(value1,1);
            }
            else if(config->timecode_chan.Format == 'B')
            {
                M23_SetFactor(value1,0);
            }
            else //This must be G
            {
                M23_SetFactor(value1,2);
            }

            sprintf( tmp, "SET FACTOR  0x%x\r\n*", value1 );
            PutResponse( 0, tmp );
        }

    }
    else if ( !strncmp( parameters, "TIMEMASK", 8 ) )
    {
        // skip until parameter or end of line is found
        //
        parameters += 8;

        while ( *parameters != '\0' )
        {
            if ( isspace( *parameters ) )
            *parameters++;
            else
            break;
        }
        if ( 1 == sscanf( parameters, "%x", &value1 ) )
        {

            value1 = (value1 & 0xFFF) | 0x7FFF000;
            M23_PutTimeMask(value1);
            sprintf( tmp, "SET TIMEMASK  0x%x\r\n*", value1 );
            M23_WriteControllerCSR(CONTROLLER_TIME_MASK_CSR,value1);
            PutResponse( 0, tmp );
        }
    }

    else
    {
        // no parameter -- show all settings
        //


        SetupGet(&config);
        if(config->timecode_chan.Format == 'A')
        {
            M23_PutSettings(1);
        }
        else if(config->timecode_chan.Format == 'B')
        {
            M23_PutSettings(0);
        }
        else //This must be G
        {
            M23_PutSettings(2);
        }

        PutResponse( 0, "*" );

    }
}

void SetPutCom()
{
    PutComSettings( &Serial_Config[0] ,0,1);
    PutComSettings( &Serial_Config[1] ,1,1);
}


void CmdSetup( char *parameters )
{
    int   return_status = 0;
    int   newsetup = 0;
    int   setup = 0;
    int   state;
    char  tmp[64];


    if ( *parameters != '\0' )
    {
        M23_RecorderState(&state);

        if(state == STATUS_RECORD)
        {
            CmdDisplayError(  ERROR_INVALID_MODE );
        }
        else
        {
            newsetup = atoi(parameters);

            return_status = SetupViewCurrent(&setup);
            //if(setup != newsetup)
           // {
                //return_status = SetupSet(newsetup,0);
                return_status = M23_Setup(newsetup,0);

                SaveSetup(newsetup);

                M23_SetCurrentLiveVideo();
                if( return_status == NO_ERROR )
                {
                 
                    sprintf( tmp, "SETUP %d\r\n*",newsetup);

                    //PC 4_21_05 return_status = SaveSettings();
                    Setup_Save();

                    system("sync");
                    //M23_ResetSystem();
                    PutResponse( 0, tmp );
                }
                else
                {
                    CmdDisplayError( return_status );
                }
#if 0
            }
            else
            {
                sprintf( tmp, "SETUP %d\r\n*",setup);
                PutResponse( 0, tmp );
            }
#endif
        }

    }
    else
    {
        return_status = SetupViewCurrent(&setup);
   
        if(return_status == NO_ERROR)
        {
            sprintf( tmp, "SETUP %d\r\n*", 
                     setup
                   );

            PutResponse( 0, tmp );

        }
        else
        {
            CmdDisplayError( return_status );

        }
    }

}

void CmdShuttle( char *parameters )
{
    PutResponse(0,"Command Not Implemented\r\n*");
   //CmdDisplayError( ERROR_COMMAND_FAILED );
}


void CmdStatus( char *parameters )
{
    char msg[32];
    int state = STATUS_IDLE, non_critical_warnings = 0, critical_warnings = 0, percent_complete = 0;
    int verbose;

    M23_GetVerbose(&verbose);

    Status( 
        &state, 
        &non_critical_warnings, 
        &critical_warnings, 
        &percent_complete 
      );


    switch( state )
    {
    case STATUS_BIT:
    case STATUS_ERASE:
    case STATUS_DECLASSIFY:
    case STATUS_PLAY:
    case STATUS_RECORD:
    case STATUS_RTEST:
    case STATUS_REC_N_PLAY:
    case STATUS_FIND:

        sprintf(msg, "S %02d %02d %02d %d%c",
                state,
                non_critical_warnings,
                critical_warnings,
                percent_complete,
                '%'
              );
        break;
    case STATUS_PTEST:
        sprintf(msg, "S %02d %02d %02d %d%c",
                state,
                non_critical_warnings,
                critical_warnings,
                PTest_Percentage,
                '%'
              );
        break;

    default:
        sprintf(msg, "S %02d %02d %02d",
                state,
                non_critical_warnings,
                critical_warnings
              );
        break;
    }


    if ( verbose )
    {
        strcat( msg, " -- " );
        strcat( msg, Chap10StatusText[state] );
    }

    strcat( msg, "\r\n*" );

    PutResponse( 0, msg );

    //system("free");
}


void CmdStop( char *parameters )
{
    int    return_status = NO_ERROR;
    int    state;
    int    stopplay   = 0;
    int    stoprecord = 0;
    int    config;
    char   *p_param;

    UINT32 CSR;

    M23_GetConfig(&config);
    if( (config == 2) || (AutoRecord == 1) )
    {
        PutResponse( 0, "*" );
        return;
    }

    if ( *parameters != '\0' )
    {
        // change parameters to upper case
        //
        p_param = parameters;
        while ( *p_param != '\0' ) {
            if( (*p_param >= 'a') && (*p_param <= 'z') )
                *p_param -= ('a' - 'A');
            p_param++;
        }

        if( strncmp(parameters,"RECORD",6) == 0)
        {
            stoprecord = 1;
        }
        else if( strncmp(parameters,"PLAY",4) == 0)
        {
            stopplay = 1;
        }
    }
    else
    {
        if ( (RecorderStatus == STATUS_PLAY) || (RecorderStatus == STATUS_REC_N_PLAY) )
        {
            stopplay = 1;
        }


        if ( (RecorderStatus == STATUS_RECORD) || (RecorderStatus == STATUS_REC_N_PLAY) )
        {
            stoprecord = 1;
        }
    }

    M23_RecorderState(&state);

    if (  (state == STATUS_RECORD ) || (state == STATUS_REC_N_PLAY))
    {

        if( (CommandFrom == FROM_COM) || (config == LOCKHEED_CONFIG) || (CommandFrom == FROM_AUTO) )
        {

            if(RecorderStatus == STATUS_REC_N_PLAY)
            {
                if(stopplay == 1)
                {

                    M23_StopPlayback(1);
                }
            }

            if(stoprecord == 1)
            {
                //if(config == LOCKHEED_CONFIG)
                //{
                    if(EventTable[5].CalculexEvent)
                    {
                        EventSet(5);
                    }
                //}
                return_status = Stop( 0 );
            }
   
            if(RecorderStatus == STATUS_IDLE)
            {
                CommandFrom = NO_COMMAND;
            }

            if(return_status == NO_ERROR)
            {
                PutResponse( 0, "*" );
            }
            else
            {
                CmdDisplayError( return_status );

            }
        }
        else
        {
            if ( (RecorderStatus == STATUS_PLAY) || (RecorderStatus == STATUS_REC_N_PLAY) )
            {
	        if(stopplay == 1)
	        {
	    	    M23_StopPlayback(1);
	        }
                else
                {
                    CmdDisplayError( ERROR_RECORD_SWITCH );
                }
  	    }
	    else
	    {
                CmdDisplayError( ERROR_RECORD_SWITCH );
	    }
        }
    }
    else if ( (RecorderStatus == STATUS_P_R_TEST) || (RecorderStatus == STATUS_PTEST) )
    {
        //M23_StopPlayback(1);
        M23_PerformRtest(0,0,2);
        M23_SetRecorderState(STATUS_IDLE);
        M23_ClearRecordLED();
        PutResponse( 0, "*" );
    }
    else if ( RecorderStatus == STATUS_RTEST )
    {
        M23_PerformRtest(0,0,2);
        M23_ClearRecordLED();
        M23_SetRecorderState(STATUS_IDLE);
        PutResponse( 0, "*" );
    }
    else if ( RecorderStatus == STATUS_PLAY )
    {
   
        if(stopplay == 1)
        {
            M23_StopPlayback(1);
        }
        PutResponse( 0, "*" );
        M23_CS6651_Normal();
        if(RecorderStatus == STATUS_IDLE)
        {
            CommandFrom = NO_COMMAND;
        }
      
    }
    else
    {
        CmdDisplayError( ERROR_INVALID_MODE );
    }

    if(stopplay == 1)
    {
        M23_SetLiveVideo(0);
    }
}

void CmdTemp( char *parameters )
{
    DisplayTemp(); 
    PutResponse(0,"\r\n*");
}

void CmdPower( char *parameters )
{
    DisplayPower(); 
    PutResponse(0,"\r\n*");
}


void CmdTime( char *parameters )
{
    GSBTime time;
    char    para_string[48];
    int     token_counter = 0;
    int     year;
    int     state;
    int     invalid = 0;
    int     timeset;
    UINT32  Upper;
    UINT32  Lower;
    UINT32  CSR;

    int   day = 0;
    int   hour = 0;
    int   minuts = 0;
    int   sec = 0;
    int   milliSec = 0;

    char seps[]   = "-:.";
    char *token = NULL;

    int  return_status = 0;
    char tmp[32];


    strcpy(para_string,parameters);


    if ( *parameters != '\0' )
    {
        M23_RecorderState(&state);

        if(state == STATUS_RECORD)
        {
            CmdDisplayError(  ERROR_INVALID_MODE );
        }
        else
        {

            if( (para_string[1] == '-') || (para_string[2] == '-') || (para_string[3]=='-') )
            {
                token = strtok( para_string, seps );
                while( token != NULL )
                {
                    if(token_counter == 0 )
                    {
                        day = atoi(token);/*day of the year*/

                        if((day >= 366) || (day <= 0) )
                        {
                            day = 1;
                        }

                    }

                    if(token_counter == 1)
                        hour = atoi(token);

                    if(token_counter == 2)
                        minuts = atoi(token);

                    if(token_counter == 3)
                        sec = atoi(token);

                    if(token_counter == 4)
                        milliSec = atoi(token);


                   /* Get next token: */
                   token = strtok( NULL, seps );

                   token_counter++;
                }
            }
            else if(para_string[2]==':')
            {
                token = strtok( para_string, seps );
                while( token != NULL )
                {
                    if(token_counter == 0 )
                        hour = atoi(token);

                    if(token_counter == 1)
                        minuts = atoi(token);

                    if(token_counter == 2)
                        sec = atoi(token);

                    if(token_counter == 3)
                        milliSec = atoi(token);

                   /* Get next token: */
                   token = strtok( NULL, seps );

                   token_counter++;
                }
            }
            else if(para_string[2]=='.')
            {
                token = strtok( para_string, seps );
                while( token != NULL )
                {
                    if(token_counter == 0 )
                        sec = atoi(token);

                    if(token_counter == 1)
                        milliSec = atoi(token);

                   /* Get next token: */
                   token = strtok( NULL, seps );

                   token_counter++;
                }

            }
            else if(para_string[0]=='.')
            {
                token = strtok( para_string, seps );
                while( token != NULL )
                {
                    if(token_counter == 0 )
                        milliSec = atoi(token);

                   /* Get next token: */
                   token = strtok( NULL, seps );

                   token_counter++;
                }

            }
            else
            {
                CmdDisplayError( ERROR_COMMAND_FAILED );
            }

            if(TimeSource == TIMESOURCE_EXTERNAL)
            {
                invalid = 1;
                CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);

                if( CSR & CONTROLLER_TIMECODE_LOSS_CARRIER)
                {
                    invalid = 0;
                }
                else if( !(CSR & CONTROLLER_TIMECODE_CARRIER))
                {
                    invalid = 0;
                }
                if( !(CSR & CONTROLLER_TIMECODE_IN_SYNC))
                {
                    invalid = 0;
                }
            }

            if(invalid == 0)
            {

                Upper  = (day/100);

                Lower  = ((day % 100)/10) << 28;
                Lower |= (day % 10) << 24;
                Lower |= (hour/10) << 20;
                Lower |= (hour % 10) << 16;
                Lower |= (minuts/10) << 12;
                Lower |= (minuts % 10) << 8;
                Lower |= (sec/10) << 4;
                Lower |= (sec % 10) ;


                time.Days = day;
                time.Hours = hour;
                time.Minutes = minuts;
                time.Seconds = sec;
                time.Microseconds = milliSec * 1000;

                M23_WriteControllerCSR(CONTROLLER_TIMECODE_LOAD_BCD_LOWER_CSR,Lower);
                M23_WriteControllerCSR( CONTROLLER_TIMECODE_LOAD_BCD_UPPER_CSR,Upper);

                sprintf( tmp, "TIME %03ld-%02ld:%02ld:%02ld.%03ld\r\n*",
                        time.Days,
                        time.Hours,
                        time.Minutes,
                        time.Seconds,
                        time.Microseconds/1000
                  );

                M23_GetYear(&year);
                PutResponse( 0, tmp );

            }
            else
            {
                CmdDisplayError(  ERROR_INVALID_MODE );
            }
        }
    }
    else
    {
        return_status = TimeView( &time );

        if(return_status == NO_ERROR)
        {
            sprintf( tmp, "TIME %03ld-%02ld:%02ld:%02ld.%02ld0\r\n*",
                        time.Days,
                        time.Hours,
                        time.Minutes,
                        time.Seconds,
                        time.Microseconds/100
                      );

            PutResponse( 0, tmp );
        }
        else
        {
            CmdDisplayError( return_status );
        }
    }

}

void CmdTmats( char *parameters )
{
    int  i = 0, j = 0;
    char token1[34];
    char token2[34];
    char tmats_char[4];
    char tmats_line_holder[255];

    int  return_status = 0;
    int  setup = 0;
    int  state;
    BOOL token1_flag = TRUE;
    BOOL token1_exist = FALSE;
    BOOL token2_exist = FALSE;

    memset(token1,0,sizeof(token1));
    memset(token2,0,sizeof(token2));

    M23_RecorderState(&state);

    if(state == STATUS_RECORD)
    {
        CmdDisplayError(  ERROR_INVALID_MODE );
    }
    else
    {
        if ( *parameters != '\0' )
        {
            //PARSE PARAMETER
            while( *parameters != '\0' )
            {
               if(isspace(*parameters))
               {
                   *parameters++;
                   token1_flag = FALSE;
                   continue;
               }

               if(token1_flag)
               {
                   token1_exist = TRUE;
                   token1[i] = *parameters;
                   i++;
               }
               else
               {
                   token2_exist = TRUE;
                   token2[j] = *parameters;
                   j++;
               }

                *parameters++;
            }


            if(strncasecmp( token1 ,"WRITE",5) == 0)
            {
                //Initialize the buffer to 0's
                memset(tmats_char,0,sizeof(tmats_char));
                memset(tmats_line_holder,0,sizeof(tmats_line_holder));
                memset(tmats_buffer,0,64*1024);

                //Change System State to reading Tmars from user
            
                //Poll for END command
                while(1)
                {
                    memset(tmats_line_holder,0,sizeof(tmats_line_holder));
                    memset(tmats_char,0,sizeof(tmats_char));

                    //Poll on the compoart for TMATS chars
                    while(memcmp(tmats_char,"\n",1) != 0)
                    {
                        memset(tmats_char,0,sizeof(tmats_char));
                        if( SerialGetChars( 0, &tmats_char[0], 1 ) )
                        {
                            //store chars the TMATS line
                            strcat(tmats_line_holder,tmats_char);
                        }
                    }

                    if( (strncasecmp( tmats_line_holder, "\nEND", 4 ) == 0) || 
                        (strncasecmp( tmats_line_holder , "END" , 3 ) == 0) )
                        break;

                    //store everything to the TMATS buffer
                    strcat(tmats_buffer,tmats_line_holder);

                }
    
                //call TMATSWrite with the TMATS buffer
                TmatsWrite(tmats_buffer);

                //and return the prompt
                PutResponse( 0, "*" );
      
            }
            else if(strncasecmp(token1,"READ",4) == 0)
            {
                //CALL TMATS READ

                TmatsRead( );
                PutResponse( 0, "*" );


            }
            else if(strncasecmp(token1,"DELETE",6) == 0)
            {
                if(state == STATUS_RECORD)
                {
                    CmdDisplayError(  ERROR_INVALID_MODE );
                }
                else
                {
                    if(strncasecmp(token2,"ALL",3) == 0)
                    {
                        TmatsDeleteAll();
                    }
                    else
                    {
                        sscanf( token2, "%d", &setup );
                        if( (setup >= 0) && (setup < 16) )
                        {
                            TmatsDelete(setup);
                        }
                        else
                        {
                            CmdDisplayError( ERROR_INVALID_PARAMETER );
                        }
                    }
                    PutResponse( 0, "*" );
                }

            }
            else if( (strncasecmp( token1 ,"SAVE",4) == 0) && (token2_exist == TRUE))
            {
                sscanf( token2, "%d", &setup );

                return_status = TmatsSave(setup );
   
                if(return_status == NO_ERROR )
                {
                    PutResponse( 0, "*" );
                }
                else
                {
                    CmdDisplayError( return_status );
                }
        
            }
            else if((strncasecmp( token1 ,"GET",3) == 0 )&& (token2_exist == TRUE))
            {
                sscanf( token2, "%d", &setup );

                return_status = TmatsGet(setup );
   
                if(return_status == NO_ERROR)
                {
                    PutResponse( 0, "*" );
                }
                else
                {
                    CmdDisplayError( return_status );
                }
            }
            else
            {
                CmdDisplayError( ERROR_INVALID_PARAMETER );
            }

        }
        else
        {
            //SHOULD ALWAYS HAVE A PARAMETER
            return_status = ERROR_INVALID_PARAMETER;
            CmdDisplayError( return_status );

        }
    }

}

void CmdExit( char *parameters )
{
    UINT32 CSR;

    M23_StopRecorder();
    /*Clear Host Go*/
    CSR = M23_ReadControllerCSR(CONTROLLER_HOST_CONTROL_CSR);
    M23_WriteControllerCSR(CONTROLLER_HOST_CONTROL_CSR,CSR & ~CONTROLLER_HOST_CHANNEL_GO);

    PutResponse(0,"Exiting...\r\n");
    exit( -1 );
}



void CmdVersion( char *parameters )
{
    int    i;
    int    numMP;
    int    month;
    int    day;
    int    year;
    int    hour;
    int    return_status;

    UINT32 CSR;
    char   tmp[256];
    char   version[8];

    M23_NumberMPBoards(&numMP);

    memset(version,0x0,8);
    return_status = DiskIsConnected();

    if(return_status)
    {
        strncpy(version,RMM_Version,RMM_VersionSize);
    }

    sprintf(tmp,"\r\n System Versions   %s      \r\n",SystemVersion);

    PutResponse( 0, tmp );
    sprintf(tmp,"-----------------------------------\r\n");
    PutResponse( 0, tmp );


    /*Print out the M2300 OCP version*/
    sprintf( tmp, 
            "Firmware Version          - %s %s\r\n", VERSION_DATE,VERSION_TIME);

    PutResponse( 0, tmp );


    /*Print out the Controller version*/
    memset(tmp,0x0,256);

    CSR = M23_ReadControllerCSR(CONTROLLER_COMPILE_TIME);

    year  = (CSR >> 8) & 0x000000FF;
    month = (CSR >> 24) & 0x000000FF;

    if(month > 9)
    {
        month -= 6;
    }

    if(year > 9)
    {
        year -= 6;
    }

    day   = (CSR >> 16) & 0x000000FF;

    hour = CSR & 0x000000FF;
 
    sprintf(tmp,"Controller Board          - %s %02x %d %02x:00:00\r\n",Months[month],day,year+2000,hour);
    PutResponse( 0, tmp );
     
    for(i = 0; i < numMP;i++)
    {
        memset(tmp,0x0,256);
        CSR = M23_mp_read_csr(i,BAR1,MP_COMPILE_TIME_OFFSET);
        hour  = CSR & 0x000000FF;


        if(M23_MP_IS_PCM(i))
        {
            year  = (CSR >> 8) & 0x000000FF;
            month = (CSR >> 24) & 0x000000FF;

            if(month > 9)
            {
                month -= 6;
            }

            if(year > 9)
            {
                year -= 6;
            }


            day   = (CSR >> 16) & 0x000000FF;
            if(M23_MP_PCM_4_Channel(i))
            {
                sprintf(tmp,"PCM  %d (4 Channel)        - %s %02x %d %02x:00:00\r\n",i+1,Months[month],day,year+2000,hour);
                PutResponse( 0, tmp );
            }
            else
            {
                sprintf(tmp,"PCM %d (8 Channel)         - %s %02x %d %02x:00:00\r\n",i+1,Months[month],day,year+2000,hour);
                PutResponse( 0, tmp );
            }
        }
        else if(M23_MP_IS_M1553(i))
        {
            year  = (CSR >> 8) & 0x000000FF;
            month = (CSR >> 24) & 0x000000FF;

            if(month > 9)
            {
                month -= 6;
            }
            if(year > 9)
            {
                year -= 6;
            }
            day   = (CSR >> 16) & 0x000000FF;
            /*Month Day year(yymmddhh) are different than PCM(mmddyyhh) */
            if(M23_MP_M1553_4_Channel(i))
            {
                sprintf(tmp,"M1553 %d (4 Channel)       - %s %02x %d %02x:00:00\r\n",i+1,Months[month],day,year+2000,hour);
                PutResponse( 0, tmp );
            }
            else
            {
                sprintf(tmp,"M1553 %d (8 Channel)       - %s %02x %d %02x:00:00\r\n",i+1,Months[month],day,year+2000,hour);
                PutResponse( 0, tmp );
            }

        }
        else if(M23_MP_IS_DM(i))
        {
            year  = (CSR >> 24) & 0x000000FF;
            month = (CSR >> 16) & 0x000000FF;
            if(month > 9)
            {
                month -= 6;
            }
            if(year > 9)
            {
                year -= 6;
            }
            day   = (CSR >> 8) & 0x000000FF;
            /*Month Day year(yymmddhh) are different than PCM(mmddyyhh) */
            sprintf(tmp,"DMP %d (4-1553 1-Dis)      - %s %02x %d %02x:00:00\r\n",i+1,Months[month],day,year+2000,hour);
            PutResponse( 0, tmp );
        }
        else
        {
            year  = (CSR >> 8) & 0x000000FF;
            month = (CSR >> 24) & 0x000000FF;

            if(month > 9)
            {
                month -= 6;
            }
            if(year > 9)
            {
                year -= 6;
            }

            day   = (CSR >> 16) & 0x000000FF;
            sprintf(tmp,"Video  %d (4 Channel)      - %s %02x %d %02x:00:00\r\n",i+1,Months[month],day,year+2000,hour);
            PutResponse( 0, tmp );
        }

    }

    sprintf( tmp, 
            "\r\nTMATS Version             - %d\r\n", TmatsVersion);
    PutResponse( 0, tmp );

    if( (return_status) && (RMM_VersionSize > 0) )
    {
        sprintf( tmp,"RMM Version %8s      - %s",version,&RMM_Version[RMM_VersionSize+1]); 
        PutResponse( 0, tmp );
    }


    PutResponse( 0, "*" );

    
}

void CmdReadMem( char *parameters )
{
    int offset;
    int value;

    if ( *parameters != '\0' )
    {
        sscanf( parameters, "%x %d" ,&offset,&value);
    }

    M23_ReadMemorySpace(offset,value);
    PutResponse(0,"*");

}

void CmdWriteMem( char *parameters )
{
    int offset = 0;
    int value;

    if ( *parameters != '\0' )
    {
        sscanf( parameters, "%x %x",&offset,&value);
        WRITE32(offset,value);
    }

    PutResponse(0,"*");

}


void CmdController( char *parameters )
{
    int offset = 0;
    int value = 0;

    if ( *parameters != '\0' )
    {
        sscanf( parameters, "%x %d" ,&offset,&value);
    }

    M23_ReadControllerMemorySpace(offset,value);
    PutResponse(0,"*");

}

void CmdCondor( char *parameters )
{
    int  offset;
    int  value;
    char dir;

    if ( *parameters != '\0' )
    {
        sscanf( parameters, "%c %x %x" ,&dir,&offset,&value);
    }

    if( (dir == 'r') || (dir == 'R') )
    {
        M23_M1553_ReadCondorMem(offset,value);
    }
    else if( (dir == 'w') || (dir == 'W') )
    {
        M23_M1553_WriteCondor((offset << 2),value);
    }

    PutResponse(0,"*");

}

void CmdCWrite( char *parameters )
{
    int offset = 0;
    int value;

    if ( *parameters != '\0' )
    {
        sscanf( parameters, "%x %x",&offset,&value);
        //device = atoi(parameters);
    }

    M23_WriteControllerCSR(offset,value);

    PutResponse(0,"*");
}

void CmdViewTime( char *parameters )
{
    UINT32 CSR;

    UINT32 CSR2;
    UINT32 Time1;
    UINT32 Time2;
    UINT32 Time3;

    int    numMP;
    int    device;
    int    i;
    int    j;

    char tmp[40];


    M23_NumberMPBoards(&numMP);
    printf("Number Of Boards %d\r\n",numMP);
    /*Clear The Sync Enable*/
    CSR = M23_ReadControllerCSR(CONTROLLER_TIME_SYNCH_CONTROL_CSR);
    CSR &= ~CONTROLLER_TIME_GEN_ENABLE;
    CSR = 0;
    M23_WriteControllerCSR( CONTROLLER_TIME_SYNCH_CONTROL_CSR,CSR);

    /*Read the Master Times*/
    Time1 = M23_ReadControllerCSR(CONTROLLER_MASTER_TIME_UPPER);
    Time2 = M23_ReadControllerCSR(CONTROLLER_MASTER_TIME_LOWER);

    memset(tmp,0x0,40);
    sprintf(tmp,"Controller Master Time  -> 0x%x%x\r\n",Time1,Time2);
    PutResponse(0,tmp);

    for(i = 0;i<numMP;i++)
    {
        device = MP_DEVICE_0 + i;

        if(M23_MP_IS_M1553(i) )
        {
            for(j = 1;j<9;j++)
            {
                CSR2 = (0x021 << 16);
                CSR2 |= WRITE_COMMAND;
                M23_mp_write_csr(device,BAR1,CSR2,(MP_CHANNEL_OFFSET * j) + MP_CONDOR_OFFSET);
                usleep(1);

                CSR2 = (0x021 << 16);
                CSR2 |= READ_COMMAND;
                M23_mp_write_csr(device,BAR1,CSR2,(MP_CHANNEL_OFFSET * j) + MP_CONDOR_OFFSET);
                Time3 = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * j) + MP_CONDOR_READ_OFFSET);
                Time3 &= 0xFFFF;

                CSR2 = (0x021 << 16);
                CSR2 |= READ_COMMAND;
                M23_mp_write_csr(device,BAR1,CSR2,(MP_CHANNEL_OFFSET * j) + MP_CONDOR_OFFSET);
                Time2 = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * j) + MP_CONDOR_READ_OFFSET);
                Time2 &= 0xFFFF;

                CSR2 = (0x021 << 16);
                CSR2 |= READ_COMMAND;
                M23_mp_write_csr(device,BAR1,CSR2,(MP_CHANNEL_OFFSET * j) + MP_CONDOR_OFFSET);
                Time1 = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * j) + MP_CONDOR_READ_OFFSET);
                Time1 &= 0xFFFF;

                memset(tmp,0x0,40);
                sprintf(tmp,"M1553 Channel %d Time    -> 0x%x%x%x\r\n",j,Time1,Time2,Time3);
                PutResponse(0,tmp);
            }
        }
        else if(M23_MP_IS_PCM(i))
        {
            for(j = 1;j<5;j++)
            {
                Time1 = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * j) + MP_TIME_MSB_READ);
                Time2 = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * j) + MP_TIME_LSB_READ);
                memset(tmp,0x0,40);
                sprintf(tmp,"PCM Channel %d Time      -> 0x%x%x\r\n",j,Time1,Time2);
                PutResponse(0,tmp);

            }
        }
        else //This is a Video Board
        {
            CSR2 = 0x11;
            M23_mp_write_csr(device,BAR1,CSR2,MP_VIDEO_RELATIVE_TIME_GO_OFFSET);

            Time1 = M23_mp_read_csr(device,BAR1, MP_VIDEO_RELATIVE_TIME_MSB);
            Time2 = M23_mp_read_csr(device,BAR1, MP_VIDEO_RELATIVE_TIME_LSB);
            memset(tmp,0x0,40);
            sprintf(tmp,"Video Time              -> 0x%x%x\r\n",Time1,Time2);
            PutResponse(0,tmp);
        }

    }

    PutResponse(0,"*");
}


void CmdReadMP( char *parameters )
{
    int device = 0;
    int offset = 0;
    int size = 0;
    int NumMPBoards;

    M23_NumberMPBoards(&NumMPBoards);

    if ( *parameters != '\0' )
    {
        sscanf( parameters, "%d %x %d", &device,&offset,&size);
    }

    if(device <= NumMPBoards)
    {
        if(NumMPBoards != 0 )
        {
            //M23_mp_read(device,256,1,0);
            M23_mp_read(device,size,1,offset);
        }
    }

    PutResponse(0,"*");
}

void CmdWriteMP( char *parameters )
{
    int device = 0;
    int offset = 0;
    int value = 0;
    int NumMPBoards;

    M23_NumberMPBoards(&NumMPBoards);

    if ( *parameters != '\0' )
    {
      
    sscanf( parameters, "%d %x %x", &device,&offset,&value);
      //device = atoi(parameters);
    }

    if(device <= NumMPBoards)
    {
        if(NumMPBoards != 0 )
        {
            //M23_mp_read(device,256,1,0);
            M23_mp_write_csr(device,1,value,offset);
        }
    }

    PutResponse(0,"*");
}

void CmdVideo( char *parameters )
{
    int device = 0;
    int offset = 0;
    int size = 0;
    int NumMPBoards;

    M23_NumberMPBoards(&NumMPBoards);

    if ( *parameters != '\0' )
    {
      
    sscanf( parameters, "%d %x %d", &device,&offset,&size);
      //device = atoi(parameters);
    }

    if(device <= NumMPBoards)
    {
        if(NumMPBoards != 0 )
        {
            //M23_mp_read(device,256,1,0);
            M23_mp_read(device,size,1,offset);
        }
    }

    PutResponse(0,"*");
}

void CmdVWrite( char *parameters )
{
    int device = 0;
    int offset = 0;
    int value = 0;
    int NumMPBoards;

    M23_NumberMPBoards(&NumMPBoards);

    if ( *parameters != '\0' )
    {
      
        sscanf( parameters, "%d %x %x", &device,&offset,&value);
    }

    if(device <= NumMPBoards)
    {
        if(NumMPBoards != 0 )
        {
            //M23_mp_read(device,256,1,0);
            M23_mp_write_csr(device,1,value,offset);
        }
    }

    PutResponse(0,"*");
}

void CmdResetVideo( char *parameters )
{
    int state;
    int device;
    int channel;
    int slot;

    if ( *parameters != '\0' )
    {
      
        sscanf( parameters, "%d", &channel);
        device = M23_GetVideoDevice(channel);

        slot = (1 << (device + 1) );

        state = M23_I2C_GetCompressorState(channel,slot);

        printf("State = 0x%x\r\n",state);

    }

    PutResponse(0,"*");
}


void CmdLoadMP( char *parameters )
{
    int value1;

    // skip until parameter or end of line is found
    // 
    while ( *parameters != '\0' )
    {
        if ( isspace( *parameters ) )
            *parameters++;
        else
            break;
    }


    if ( 1 == sscanf( parameters, "%d", &value1 ) )
    {
        printf("Load Logic For MP Board %d\r\n",value1);
    }

    PutResponse(0,"*");

}



int ConsoleEchoSet( int enable )
{
    if ( enable > 1 )
        return ERROR_INVALID_PARAMETER;

    //Console_Echo = enable;
    Console_Echo = 0;

    return NO_ERROR;
}


int ConsoleEchoView( int *p_enable )
{
    *p_enable = Console_Echo;

    return NO_ERROR;
}




int SerialComSet( UART_CONFIG *p_config ,int port)
{
    Serial_Config[port] = *p_config;
    CmdAsciiInitialize();

    return NO_ERROR;
}


int SerialComView( UART_CONFIG **h_config ,int port)
{
    *h_config = &Serial_Config[port];

    return NO_ERROR;
}


void SetTime(GSBTime CartridgeTime)
{
    UINT32 Upper;
    UINT32 Lower;

    Upper  = CartridgeTime.Days/100;

    Lower  = ((CartridgeTime.Days % 100)/10) << 28;
    Lower |= (CartridgeTime.Days % 10) << 24;
    Lower |= (CartridgeTime.Hours/10) << 20;
    Lower |= (CartridgeTime.Hours % 10) << 16;
    Lower |= (CartridgeTime.Minutes/10) << 12;
    Lower |= (CartridgeTime.Minutes % 10) << 8;
    Lower |= (CartridgeTime.Seconds/10) << 4;
    Lower |= (CartridgeTime.Seconds % 10) ;

    M23_WriteControllerCSR( CONTROLLER_TIMECODE_LOAD_BCD_UPPER_CSR,Upper);
    M23_WriteControllerCSR(CONTROLLER_TIMECODE_LOAD_BCD_LOWER_CSR,Lower);

}

/*New command to support TRD*/
void CmdAssign(char *parameters)
{
    int  OutputChannel = 0xFF;
    int  InputChannel = 0xFF;
    int  OutputOffset = 0xFF;
    int  InputOffset = 0xFF;
    char tmp[40];
    char tmp1[40];


    if ( *parameters != '\0' )
    {
      
        //sscanf( parameters, "%d %d", &OutputChannel,&InputChannel);
        sscanf( parameters, "%s %s",tmp,tmp1);

        if( (tmp[7] == 'N') || (tmp[7] == 'n') )
        {
	    OutputOffset = 0;
        }
        else
        {
	    OutputOffset = atoi(&tmp[7]);
        }

        if( (tmp[7] == 'N') || (tmp[7] == 'n') )
        {
	    InputOffset = 0;
        }
        else
        {
	    InputOffset = atoi(&tmp1[6]);
        }

        //OutputChannel = VideoIds[OutputOffset];
        OutputChannel = 1;
        InputChannel = VideoIds[InputOffset-1];

        if( InputChannel == 0xFF)
        {
            CmdDisplayError( ERROR_INVALID_COMMAND );
        }
        else
        {
            AssignReceived = 1;
            M23_PerformAssign(OutputChannel,InputChannel);
            memset(tmp,0x0,40);
            //sprintf(tmp,"ASSIGN %d %d\r\n*",OutputChannel,InputChannel);
            //PutResponse(0,tmp);
            PutResponse(0,"*");
            AssignedChannel = InputOffset;
        }
    }
    else
    {
        sprintf(tmp,"VIDOUT-1  VIDIN-%d\r\n*",AssignedChannel);
        PutResponse(0,tmp);
    }

}

void CmdDate(char *parameters)
{
    int  return_status;
    int  year = 0;
    int  month = 0;
    int  day   = 0;
    char tmp[20];

    return_status = DiskIsConnected();

    if(return_status)
    {
        memset(tmp,0x0,20);

        if ( *parameters != '\0' )
        {
            sscanf(parameters,"%d-%d-%d",&year,&month,&day);
            if( year == 0)
            {
                CmdDisplayError(ERROR_INVALID_PARAMETER);
            }
            else if( (month < 1) || (month > 12) )
            {
                CmdDisplayError(ERROR_INVALID_PARAMETER);
            }
            else if( (day < 1) || (day > 31) )
            {
                CmdDisplayError(ERROR_INVALID_PARAMETER);
            }
            else
            {
                return_status = M2x_SetCartridgeDate(parameters);
                if(return_status == 0)
                {
                    sprintf(tmp,"Date %d-%d-%d\r\n*",year,month,day);
                    PutResponse(0,tmp);
                }
                else
                {
                    CmdDisplayError( ERROR_COMMAND_FAILED );
                }
            }
        }
        else
        {
            return_status = M2x_RetrieveCartridgeDate(tmp);
            if(return_status == 0)
            {
                PutResponse(0,tmp);
            }
            else
            {
                CmdDisplayError( ERROR_COMMAND_FAILED );
            }
        }

    }
    else
    {
        CmdDisplayError( ERROR_NO_MEDIA );
    }
}

void CmdIrig(char *parameters)
{
    char               tmp[20];
    M23_CHANNEL  const *config;

    SetupGet(&config);

    memset(tmp,0x0,20);

    //sprintf(tmp,"IRIG 106 %d\r\n*",config->VersionNumber);
    sprintf(tmp,"%d\r\n*",config->VersionNumber);
    PutResponse(0,tmp);

}

void CmdPause(char *parameters)
{
    int config;

    M23_GetConfig(&config);

    if( (config == 0) || (config == P3_CONFIG) )
    {
        /*Pause the C26651*/
        M23_CS6651_Pause(0);
        M23_SetPlaySpeedStatus(0x0);
        PutResponse(0,"*");
    }
    else
    {
        CmdDisplayError( ERROR_INVALID_COMMAND );
    }
}

void CmdStep(char *parameters)
{
    int config;

    M23_GetConfig(&config);

    if( (config == 0) || (config == P3_CONFIG) )
    {
        /*Single Step*/
        M23_CS6651_Single();
    }
    else
    {
        CmdDisplayError( ERROR_INVALID_COMMAND );
    }
}

void CmdEthernet(char *parameters)
{
    M23_StartEthernet(0xFFFF,0xFFFF,0xFFFF);
    PutResponse(0,"*");
}

void CmdPhyRead(char *parameters)
{
    int reg = 0;
    int value;

    if ( *parameters != '\0' )
    {
        sscanf(parameters,"%d",&reg);
        value = M23_smc_phy_read(reg);

        printf("Read 0x%x from Phy Reg %d\r\n",value,reg);
    }
    
    PutResponse(0,"*");
}

void CmdPhyWrite(char *parameters)
{
    int reg = 0;
    int value = 0;

    if ( *parameters != '\0' )
    {
        sscanf(parameters,"%d %x",&reg,&value);
        printf("Writing 0x%x from Phy Reg %d\r\n",value,reg);
        value = M23_smc_phy_write(reg,value);
    }
    
    PutResponse(0,"*");
}


int AllPublishStopped()
{
    int i;
    int pub = 0;
    int stop = 0;

    for(i = 0; i < 50;i++)
    {
        if(publish.id[i] != 0xFF)
        {
            pub++;
        }

        if(stop_publish.id[i] != 0xFF)
        {
            stop++;
        }
    }

    if(pub < stop)
    {
        return(0);
    }

    return(1);
}

int parsePublish(char *line, ctoken *token, int *numTokens)
{
    int i;
    char *ptr;

    /* empty the token structure */
    for(i=0; i<50; i++)
        memset(token[i].txt, 0x0,MAX_STRING);

    *numTokens = 0;
    i = 0;

    ptr = strtok(line, " ");
    if(ptr == NULL)
    {
        return(0);
    }

    while(ptr != NULL)
    {
        strcpy(token[i].txt, ptr);
        i++;
        ptr = strtok(NULL, " ");
    }

    *numTokens = i;

    return(0);
}

void PublishPrint()
{
    int i;
    char tmp[10];


    memset(tmp,0x0,10);
    PutResponse(0,publish.IP);
    sprintf(tmp," %d",publish.port);
    PutResponse(0,tmp);

    memset(tmp,0x0,10);
    for(i = 0; i < 50;i++)
    {
        memset(tmp,0x0,10);
        if(publish.id[i] != 0xFF)
        {
            sprintf(tmp," %d",publish.id[i]);
            PutResponse(0,tmp);
        }
    }


    PutResponse(0,"\r\n");
    PutResponse(0,tmp);

}
int IsRange(char *line,int *start,int *end)
{

    char *ptr;
    char tmp[10];

    memset(tmp,0x0,10);

    strcpy(tmp,line);

    ptr = strtok(tmp,"-");
    if(ptr != NULL)
    {
	*start = atoi(ptr);
        ptr = strtok(NULL,"-");
		
        if(ptr != NULL)
        {
  	    *end = atoi(ptr);
            //printf("Start %d, end %d\r\n",*start,*end);
            return(1);
        }

    }

    return(0);

}

void  FindPubType(int id,int *type)
{
    int i;
    M23_CHANNEL  const *config;

    SetupGet(&config);

    /*Is this a timecode Channel ID*/
    if(id == config->timecode_chan.chanID)
    {
        *type = PUB_TIME;
        return;
    }
   
    /*Is this a voice Channel ID*/
    for(i = 0; i < config->NumConfiguredVoice;i++)
    {
        if(id == config->voice_chan[i].chanID)
        {
            *type = PUB_VOICE;
            return;
        }
    }

    /*Is this a 1553 Channel ID*/
    for(i = 0; i < config->NumConfigured1553;i++)
    {
        if(id == config->m1553_chan[i].chanID)
        {
            *type = PUB_1553;
            return;
        }
    }

    /*Is this a PCM Channel ID*/
    for(i = 0; i < config->NumConfiguredPCM;i++)
    {
        if(id == config->pcm_chan[i].chanID)
        {
            *type = PUB_PCM;
            return;
        }
    }
    /*Is this a Video Channel ID*/
    for(i = 0; i < config->NumConfiguredVideo;i++)
    {
        if(id == config->video_chan[i].chanID)
        {
            *type = PUB_VIDEO;
            return;
        }
    }

    /*Is this an Ethernet Channel ID*/
    for(i = 0; i < config->NumConfiguredEthernet;i++)
    {
        if(id == config->eth_chan.chanID)
        {
            *type = PUB_ETH;
            return;
        }
    }

    /*Is this an UART Channel ID*/
    for(i = 0; i < config->NumConfiguredUART;i++)
    {
        if(id == config->uart_chan.chanID)
        {
            *type = PUB_UART;
            return;
        }
    }

    /*Is this a Discrete Channel ID*/
    if(id == config->dm_chan.chanID)
    {
        *type = PUB_DISCRETE;
        return;
    }

    *type = 0xFF;

    return;
}

int FillInPublish(char *line)
{
    int  i;
    int  start;
    int  end;
    int  status;
    int  type;
    int  ret = 1;



    /*Find if this is a range*/
    status = IsRange(line,&start,&end);

    if(start == 0)
    {
        ret = 0;
    }
    else
    {
        if(status == 1) /*This is a range of Channel ID's*/
        {
            for(i = start; i < (end + 1); i++)
            {
                publish.id[PubIndex] = i;
                FindPubType(i,&type);
                publish.Type[PubIndex] = type;
                PubIndex++;
            }
        }
        else /*This is a single ID*/
        {
            publish.id[PubIndex] = start;
            FindPubType(start,&type);
            publish.Type[PubIndex] = type;
            PubIndex++;
        }
    }

    return(ret);
  
}

int FillInStopPublish(char *line)
{
    int  i;
    int  start;
    int  end;
    int  status;
    int  type;
    int  ret = 1;

    /*Find if this is a range*/
    status = IsRange(line,&start,&end);

    if(start == 0)
    {
        ret = 0;
    }
    else
    {
        if(status == 1) /*This is a range of Channel ID's*/
        {
            for(i = start; i < (end + 1); i++)
            {
                stop_publish.id[PubStopIndex] = i;
                FindPubType(i,&type);
                stop_publish.Type[PubStopIndex] = type;
                PubStopIndex++;
            }
        }
        else /*This is a single ID*/
        {
            stop_publish.id[PubStopIndex] = start;
            FindPubType(start,&type);
            stop_publish.Type[PubStopIndex] = type;
            PubStopIndex++;
        }
    }

    return(ret);
  
}

void FillAllIDs()
{
    int i;
    int type;

    for(i = 0; i < 50;i++)
    {
        FindPubType(i,&type);
        if(type != 0xFF)
        {
           publish.Type[i] = type;
           publish.id[i]   = i;
        }
    }
}

void FillPublishDualPort()
{
    int    i;
    int    debug;

    int    IPHigh;
    int    IPMid1;
    int    IPMid2;
    int    IPLow;

    int    IPHigh_dest;
    int    IPMid1_dest;
    int    IPMid2_dest;
    int    IPLow_dest;

    UINT16  Dest_Low;

    UINT16 MAC_HIGH;
    UINT16 MAC_MID;
    UINT16 MAC_LOW;

    UINT32 CSR;

    M23_GetDebugValue(&debug);

    GetMACAddress(&MAC_HIGH,&MAC_MID,&MAC_LOW);

    if(debug)
    {
        printf("MAC -> %04x-%04x-%04x\r\n",MAC_HIGH,MAC_MID,MAC_LOW);
    }

    //sscanf(publish.IP,"%d.%d.%d.%d",&IPHigh,&IPMid1,&IPMid2,&IPLow); 
    sscanf(publish.IP,"%d.%d.%d.%d",&IPMid1_dest,&IPHigh_dest,&IPLow_dest,&IPMid2_dest); 

    /*Address 0 is set to 0*/
    CSR = 0;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 1 is set to 0*/
    CSR = 0x10000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*PC june 03 2008 - Change to IP Multicast -> Ethernet Multicast 01-00-5E-00-00-00*/
    /*Address 2 is set to the Dest MAC UPPER*/
    CSR = 0x2FFFF;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);
#if 0
    Removed by Marty-Tad Request
    CSR = 0x20001; 
#endif

    /*Address 3 is set to the Dest MAC MIDDLE*/
    CSR = 0x3FFFF;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);
#if 0
    Removed by Marty-Tad Request
    CSR = ((IPHigh_dest << 8) & 0x7F00); 
    CSR |= 0x3005E;
#endif

    /*Address 4 is set to the Dest MAC Lower*/
    CSR = 0x4FFFF;
#if 0
    Removed by Marty-Tad Request
    Dest_Low = (IPMid2_dest << 8); 
    Dest_Low |= IPLow_dest; 
    CSR = (Dest_Low | 0x40000);
#endif
    
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 5 is set to the Source MAC Upper*/
    CSR = (MAC_HIGH | 0x50000);
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 6 is set to the Source MAC Middle*/
    CSR = (MAC_MID | 0x60000);
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 7 is set to the Source MAC Lower*/
    CSR = (MAC_LOW | 0x70000);
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 8 is set to 0x0800*/
    CSR = 0x80008;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 9 is set to 0x04 0x05 0x00*/
    CSR = 0x90045;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 12 is set to  0x00*/
    CSR = 0xC0000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 13 is set to  0xFF11*/
    CSR = 0xD11FF;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 15 is set to  Source IP Address High*/
#if 0
    IPLow = 250;
    IPMid2 = 2;
    IPMid1 = 168;
    IPHigh = 192;
#endif

    IPLow = 0;
    IPMid2 = 250;
    IPMid1 = 192;
    IPHigh = 168;

    CSR = (IPMid1 | (IPHigh << 8) | (0xF0000) );
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 16 is set to  Source IP Address Low*/
    CSR = (IPLow | (IPMid2 << 8) | 0x100000);
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);



    CSR = IPLow_dest;
    CSR |= (IPMid2_dest <<8);
    CSR |= (IPMid1_dest <<16);
    CSR |= (IPHigh_dest <<24);

    /*Address 17 is set to  Dest IP Address High*/
    CSR = (IPMid1_dest | (IPHigh_dest << 8) | 0x110000 );
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 18 is set to  Dest IP Address Low*/
    CSR = (IPLow_dest | (IPMid2_dest << 8) | 0x120000);
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 19 is set to  Source Port*/
    CSR = (UINT16)publish.port;
    SWAP_TWO(CSR);
    CSR |= 0x130000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 20 is set to  Dest Port*/
    CSR = (UINT16)publish.port;
    SWAP_TWO(CSR);
    CSR |= 0x140000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 21 is set to  0*/
    CSR = 0x150000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 22 is set to  0*/
    CSR = 0x160000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 23 is set to  0*/
    CSR = 0x170000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 24 is set to  0*/
    CSR = 0x180000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 25 is set to  EB25*/
    //CSR = 0xEB25;
    //CSR |= 0x190000;
    CSR = 0x190000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 26 is set to 0*/
    CSR = 0x1A0000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 27 is set to 0*/
    CSR = 0x1B0000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 28 is set to 0*/
    CSR = 0x1C0000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 29 is set to 0*/
    CSR = 0x1D0000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 30 is set to 0*/
    CSR = 0x1E0000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);

    /*Address 31 is set to 0*/
    CSR = 0x1F0000;
    M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR | ETHERNET_DUAL_XMIT);


    if(debug)
    {
        for(i = 0;i< 32;i++)
        {
            
            CSR = (i << 16);
            M23_WriteControllerCSR(ETHERNET_DUAL_PORT,CSR);
            CSR = M23_ReadControllerCSR(ETHERNET_DUAL_PORT);
            printf("%d - 0x%x\r\n",i,CSR & 0xFFFF);
        }
    }

}

void CmdPublish(char *parameters)
{
    int    i;
    int    numTokens;
    int    status;


    static int pubbed = 0;

    ctoken tokens[50];
   

    char *p_cmd = parameters;

    // change to upper case
    //
    while ( *p_cmd != '\0' ) {
        if( (*p_cmd >= 'a') && (*p_cmd <= 'z') )
            *p_cmd -= ('a' - 'A');
        p_cmd++;
    }

    if( *parameters != '\0' )
    {
        parsePublish(parameters, tokens, &numTokens);
    
        if(strncmp(tokens[0].txt,"START",5) == 0 )
        {

            PubIndex = 0;

            if(numTokens < 4)
            {
                CmdDisplayError(ERROR_INVALID_PARAMETER);
                return;
            }


            if(pubbed == 1)
            {
                if(strcmp(publish.IP,tokens[1].txt) != 0)
                {
                    CmdDisplayError(ERROR_INVALID_PARAMETER);
                    return;
                }
            }
            publish.command = PUB_START;

            strcpy(publish.IP,tokens[1].txt);

            /*First Stop Any Publish Commands that have Been Set*/
            M23_StopAllPublish();

            /*Now We get the parameters for Start*/
            for(i = 0; i < 50;i++)
            {
                publish.Type[i] = 0xFF;
                publish.id[i]   = 0xFF;
            }         
            publish.port = atoi(tokens[2].txt);

            /*Now Fill In all the Dual Port RAM Values*/
            FillPublishDualPort();

            if( strncmp(tokens[3].txt,"ALL",3) == 0)
            {
                FillAllIDs();
            }
            else
            {
                for(i = 0 ; i < (numTokens - 3); i++)
                {
                    status = FillInPublish(tokens[i+3].txt);
                    if(status == 0)
                    {
                        CmdDisplayError(ERROR_INVALID_PARAMETER);
                        return;
                    }
                }
            }

            M23_StartAllPublish();

            pubbed = 1;
        }
        else if(strncmp(tokens[0].txt,"STOP",4) == 0 )
        {
            /*Now We get the parameters for Start*/
            for(i = 0; i < 50;i++)
            {
                stop_publish.Type[i] = 0xFF;
                stop_publish.id[i]   = 0xFF;
            }         

            PubStopIndex = 0;

            if(numTokens < 2)
            {
                CmdDisplayError(ERROR_INVALID_PARAMETER);
                return;
            }

            publish.command = PUB_STOP;
            if( strncmp(tokens[1].txt,"ALL",3) == 0)
            {
                M23_StopAllPublish();
               // M23_ClearEthernetBroadcast();
            }
            else
            {
                for(i = 0 ; i < (numTokens - 1); i++)
                {
                    status = FillInStopPublish(tokens[i+1].txt);
                    if(status == 0)
                    {
                        CmdDisplayError(ERROR_INVALID_PARAMETER);
                        return;
                    }
                }

                M23_StopPublish();

#if 0
                status = AllPublishStopped();
                if(status == 1)
                {
                    M23_ClearEthernetBroadcast();
                }
#endif
            }

            pubbed = 0;
        }
        else
        {
            CmdDisplayError(ERROR_INVALID_PARAMETER);
            return;
        }

    }
    else
    {
        PublishPrint();
    }

    PutResponse(0,"*");
}

void CmdResume(char *parameters)
{
    int config;

    M23_GetConfig(&config);

    if( (config == 0) || (config == P3_CONFIG) )
    {
        /*Set the C26651 to Normal Operation*/
        M23_CS6651_Pause(1);
        PutResponse(0,"*");
    }
    else
    {
        CmdDisplayError( ERROR_INVALID_COMMAND );
    }
}

void CmdBBList(char *parameters)
{
    int  status;

    char *p_cmd = parameters;

    // change to upper case
    //
    while ( *p_cmd != '\0' ) {
        if( (*p_cmd >= 'a') && (*p_cmd <= 'z') )
            *p_cmd -= ('a' - 'A');
        p_cmd++;
    }

    if( *parameters != '\0' )
    {
        if(DeclassSent)
        {
            if(strncmp(parameters,"UNSECURED",9) == 0)
            {
                status = M23_SendBBListCommand(1);
                PutResponse(0,"*");
            }
            else if(strncmp(parameters,"SECURED",7) == 0)
            {
                status =  M23_SendBBListCommand(0);
                PutResponse(0,"*");
            }
            else
            {
                CmdDisplayError( ERROR_INVALID_COMMAND );
            }
        }
        else
        {
            CmdDisplayError( ERROR_INVALID_COMMAND );
        }
    }
    else
    {
        CmdDisplayError( ERROR_INVALID_COMMAND );
    }
}

void CmdBBRead(char *parameters)
{
    int block;

    if( *parameters != '\0' )
    {
        if(DeclassSent)
        {
            sscanf(parameters,"%d",&block);
            M23_SendBBReadCommand(block);
            PutResponse(0,"*");
        }
        else
        {
            CmdDisplayError( ERROR_INVALID_COMMAND );
        }
    }
    else
    {
        CmdDisplayError( ERROR_INVALID_COMMAND );
    }

}

void CmdBBSecure(char *parameters)
{
    int block;

    if( *parameters != '\0' )
    {
        if(DeclassSent)
        {
            sscanf(parameters,"%d",&block);
            M23_SendBBSecureCommand(block);
            PutResponse(0,"*");
        }
        else
        {
            CmdDisplayError( ERROR_INVALID_COMMAND );
        }
    }
    else
    {
        CmdDisplayError( ERROR_INVALID_COMMAND );
    }
}

void M23_StartMonitorThread()
{
    int status;
    int debug;

    M23_GetDebugValue(&debug);

    status = pthread_create(&Com0Thread, NULL, (void *)CmdAsciiMonitor, NULL);
    if(debug)
    {
        if(status == 0)
        {
            printf("COM 0  Thread Created Successfully\r\n");
        }
        else
        {
            perror("COM0 Thread Create\r\n");
        }
    }
}


