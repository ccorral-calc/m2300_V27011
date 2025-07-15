//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: serial.c
//    Version: 1.0
//     Author: dm
//
//            MONSSTR 2100 Serial Interface
//
//            Linux-based wrapper for serial port functions
//
//    Revisions:   
//

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include "M23_Controller.h"
#include "M2X_serial.h"

// #define _POSIX_SOURCE 1 /* POSIX compliant source */


static char *Port[] = { "/dev/ttyS0", "/dev/ttyS1" };


static int PortDescriptor[2];
static int PortState[2];
static char CommandBuffer[2][MAX_COMMAND_LENGTH];
static int CommandBufferIndex[2];
static SERIAL_QUEUE_INFO *  pTxQueue[3];
static SERIAL_QUEUE_INFO *  pRxQueue[3];

static int CurrentPort;



int SerialPortInitialize( int id, UART_CONFIG *p_config, SERIAL_QUEUE_INFO *p_tx_queue, SERIAL_QUEUE_INFO *p_rx_queue )
{
    struct termios newtio;
    UART_CONFIG config;


    if ( id >= 2 )
        return -1;

    if ( p_config == NULL ) {
        p_config = &config;
        p_config->Baud = BAUD_384;
        p_config->Parity = NO_PARITY;
        p_config->Databits = EIGHT_DATA_BITS;
        p_config->Stopbits = ONE_STOP_BIT;
        p_config->Flowcontrol = NO_FLOW_CONTROL;
    }

    if ( p_tx_queue == NULL )        
        return -1;
    pTxQueue[id] = p_tx_queue;

    if ( p_rx_queue == NULL )
        return -1;
    pRxQueue[id] = p_rx_queue;


    if ( PortState[id] )
        close( PortDescriptor[id] );

    PortState[id] = 0;
    memset( &CommandBuffer[0], 0, sizeof( CommandBuffer[0] ) );
    memset( &CommandBuffer[1], 0, sizeof( CommandBuffer[1] ) );
    CommandBufferIndex[0] = 0;
    CommandBufferIndex[1] = 0;

    CurrentPort = 0;

    // open the port
    //
    PortDescriptor[id] = open( Port[id], O_RDWR | O_NOCTTY ); 
    if ( PortDescriptor[id] < 0 ) {
        perror( Port[id] ); 
        return -2;
    }


    // set baud rate
    //
    bzero( &newtio, sizeof(newtio) );
    newtio.c_cflag = CLOCAL | CREAD; 
    switch ( p_config->Baud ) {
      case BAUD_96:    
          newtio.c_cflag |= B9600;     
          break;
      case BAUD_192:   
          newtio.c_cflag |= B19200;    
          break;
      case BAUD_384:   
          newtio.c_cflag |= B38400;    
          break;
      case BAUD_576:   
          newtio.c_cflag |= B57600;    
          break;
      case BAUD_1152:  
          newtio.c_cflag |= B115200;   
          break;
      default:         
          newtio.c_cflag |= B9600;     
          break;
    }


    // set parity
    //
    if ( p_config->Parity == ODD_PARITY ) {
        newtio.c_cflag |= PARENB | PARODD;
        newtio.c_iflag |= INPCK;
    }
    else
    if ( p_config->Parity == EVEN_PARITY ) {
        newtio.c_cflag |= PARENB;
        newtio.c_iflag |= INPCK;
    }
    else {
        newtio.c_iflag |= IGNPAR;
    }


    // set number of data bits
    //
    if ( p_config->Databits == SEVEN_DATA_BITS )
    {
        newtio.c_cflag |= CS7;
    }
    else
    {
        newtio.c_cflag |= CS8;
    }



    // set number of stop bits
    //
    if ( p_config->Stopbits == TWO_STOP_BITS )
    {
        newtio.c_cflag |= CSTOPB;
    }
    else
    {
    }
        

    // set flow control
    //
    if ( p_config->Flowcontrol == XON_XOFF_FLOW_CONTROL )
    {
        newtio.c_iflag |= IXON | IXOFF;
    }
    else
    if ( p_config->Flowcontrol == HW_FLOW_CONTROL )
    {
        newtio.c_cflag |= CRTSCTS;
    }

    

    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;
 
    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    //newtio.c_cc[VMIN]     = 0;   /* non-blocking read (return when 0 or more chars received) */
    newtio.c_cc[VMIN]     = 1;   /* blocking read (return when 0 or more chars received) */

    tcflush( PortDescriptor[id], TCIFLUSH);
    tcsetattr( PortDescriptor[id], TCSANOW, &newtio );


    /*
    printf( "cflag %08x\n", newtio.c_cflag );
    printf( "iflag %08x\n", newtio.c_iflag );
    printf( "oflag %08x\n", newtio.c_oflag );
    printf( "lflag %08x\n", newtio.c_lflag );

    */
    PortState[id] = 1;

    //printf( "lflag %08x\n", newtio.c_lflag );


    return 0;
}


int SerialGetChar( int id, char *byte)
{
    int nchars;

    if ( id > 2 )
    {
        return -1;
    }

    if ( PortState[id] == 0 )
        return -2;

    nchars = read( PortDescriptor[id], byte, 1);
   if(nchars < 0 )
    perror("read");

    return nchars;
}


void SerialGetLine(int id, char *p_buffer,int *size)
{
    int  nchars;
    char byte;
    int  total = 0;

    if ( id > 2 )
        return;

    if ( PortState[id] == 0 )
        return;

    while(1)
    {
        nchars = read( PortDescriptor[id], &byte, 1 );
        if(nchars < 0 )
        {
            break;
        }
        else if(nchars == 1)
        {
            if( byte == '\n')
            {
                break;
            }
            else if(byte != BackSpace)
            {
                p_buffer[total] = byte;
                total++;
                if(total == 7)
                {
                    break;
                }
            }

        }
    }

    *size = total;

}

int SerialGetChars( int id, char *p_buffer, int buffer_size )
{
    int nchars;

    if ( id > 2 )
        return -1;

    if ( PortState[id] == 0 )
        return -2;

    nchars = read( PortDescriptor[id], p_buffer, buffer_size );

    return nchars;
}


int SerialPutChars( int id, char *p_buffer )
{
    int ssric;
    int length;

    if ( id > 2 )
        return -1;

    if ( PortState[id] == 0 )
        return -2;

    if ( p_buffer == NULL )
        return -3;

    length = strlen( p_buffer );

    M23_GetSsric(&ssric);

    if ( length )
    {
       if( (id == 1) && (ssric == 1) )
       {
           length = 22;
       }
        write( PortDescriptor[id], p_buffer, length );
        /*write( PortDescriptor[1], p_buffer, length );*/
    }

    return 0;
}


int SerialPutChar( int id, char byte )
{

    if ( id > 2 )
        return -1;

    if ( PortState[id] == 0 )
        return -2;

    write( PortDescriptor[id], &byte, 1 );

    return 0;
}

int GetCommand( int id, char *command )
{
    // TODO: protect SQ functions with a semaphore to make them re-entrant
    char ch;
    int cmd_ready = 0;

    if ( id > 2 ) 
        return 0;

    if ( SQ_GetBytes( pRxQueue[id], &ch, 1 ) )
    {
        // check for backspace character
        //
        if ( ch == '\b' )
        {
            if ( CommandBufferIndex[id] > 0 ) 
                CommandBufferIndex[id]--;
            return 0;
        }
      
        //---  Toss all Line-Feed characters
        if ( ch == '\n' )
            return 0;

        // replace all carriage returns with a null
        if ( ch == '\r' )
            ch = '\0';


        CommandBuffer[id][ CommandBufferIndex[id]++ ] = ch;
        CommandBuffer[id][ CommandBufferIndex[id] ] = '\0';


        // check to see if it is a terminating character
        //
        if ( ( CommandBufferIndex[id] >= MAX_COMMAND_LENGTH - 1) ||
             ( ch == '\0' ) 
           )
        {
            CommandBuffer[id][ CommandBufferIndex[id] ] = '\0';
            CommandBufferIndex[id] = 0;
            cmd_ready = 1;
        }
    }


    if ( cmd_ready )
    {
        strcpy( command, CommandBuffer[id] );
        return 1;
    }

    return 0;
}

void M23_SetCurrentPort(int port)
{
    CurrentPort = port;
}

void PutResponse( int id, char *response )
{
    int length;
    int config;
    int ssric;

    if ( id > 2 ) 
        return;

#if 0
//PC
printf("%s",response);
return;
#endif

    if ( response == NULL )
        return;

    length = strlen( response );


    //SerialPutChars(id,response);

    M23_GetConfig(&config);
    M23_GetSsric(&ssric);

    if(config == LOCKHEED_CONFIG)
    {
        SerialPutChars(id,response);
        SerialPutChars(id+1,response);
    }
    else
    {
        if(CurrentPort == 0)
        {
            SerialPutChars(0,response);
        }
        else if(CurrentPort == 1)
        {
            if(ssric == 0)
            {
                SerialPutChars(1,response);
            }
        }
    }


    /*SQ_AddBytes( pTxQueue[id], response, length );*/
}
