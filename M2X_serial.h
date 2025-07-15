//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: serial.h
//    Version: 1.0
//     Author: dm
//
//            MONSSTR 2100 Serial Interface
//
//            Linux-based wrapper for serial port functions
//
//    Revisions:   
//
#ifndef _SERIAL_H_
#define _SERIAL_H_

#include "squeue.h"

#define MAX_COMMAND_LENGTH  (256)

typedef enum { 
    BAUD_96 = 9600,
    BAUD_192 =  19200,
    BAUD_384 =  38400,
    BAUD_576 =  57600,
    BAUD_1152 = 115200
} BAUD;

typedef enum {
    NO_PARITY = 0,
    ODD_PARITY,
    EVEN_PARITY
} PARITY;

typedef enum {
    SEVEN_DATA_BITS = 7,
    EIGHT_DATA_BITS = 8
} DATA;

typedef enum {
    ONE_STOP_BIT = 1,
    TWO_STOP_BITS = 2
} STOP;

typedef enum { 
    NO_FLOW_CONTROL = 0,
    XON_XOFF_FLOW_CONTROL,
    HW_FLOW_CONTROL
} FLOW;


typedef struct {
    BAUD    Baud;
    PARITY  Parity;
    DATA    Databits;
    STOP    Stopbits;
    FLOW    Flowcontrol;
} UART_CONFIG;




// both these functions take an ID parameter that specifies the logical 
// port number as either 0 or 1.  the mapping to actual devices is
// hardwired in the implementation file.
//
// the init function also takes a pointer to a config structure.  pass
// NULL to select default of 9600/N/8/1/none.  return value of zero
// if it is successful opening and configuring the port
//
// the function to read characters must be passed a buffer that is 
// at least 256 bytes in size.  it returns the number of chars read
//
// return values are as follows:
//      -1, invalid port id
//      -2, port not open/ready
//

int SerialPortInitialize( int id, UART_CONFIG *p_config, SERIAL_QUEUE_INFO *p_tx_queue, SERIAL_QUEUE_INFO *p_rx_queue );

int SerialGetChars( int id, char *p_buffer, int buffer_size );
int SerialGetChar( int id, char *byte);

int SerialPutChars( int id, char *p_buffer );
int SerialPutChar( int id, char byte );

int GetCommand( int id, char *command );

void PutResponse( int id, char *response );




#endif  // _SERIAL_H_
