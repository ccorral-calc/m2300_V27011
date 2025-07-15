//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: cmd_ascii.h
//    Version: 1.0
//     Author: dm
//
//            MONSSTR 2100 ASCII Command Parser
//
//            IRIG-106-03, Ch 10 compliant command parser
//
//    Revisions:   
//

#include "M23_Typedefs.h"
#include "M2X_serial.h"




void CmdAsciiInitialize( void );

void CmdAsciiMonitor( void );



// called for setting/getting system paramters
//
int VerboseSet( int enable );
int VerboseView( int *p_enable );
int ConsoleEchoSet( int enable );
int ConsoleEchoView( int *p_enable );
int SerialComSet( UART_CONFIG *p_config ,int port);
int SerialComView( UART_CONFIG **h_config ,int port);
