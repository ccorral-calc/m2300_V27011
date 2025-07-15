//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: M32_Main.c
//    Version: 1.0
//     Author: Paul Carrion
//
//            MONSSTR 2300V1 Main
//
//            Initial entry point and Task Initialization
//
//    Revisions:   
//

#include <unistd.h>
#include "version.h"

int main( void )
{
    int status;

    // Initialize recorder API before anything else.  This function restores 
    // defaults and gets the majority of the system up and running
    //

    status = M23_ControllerInitialize();


    //CmdVersion( NULL );
    PutResponse(0 ,"\r\n*");
    SerialPutChars(1,"\r\n*");

    M23_StartMonitorThread();

    CmdAsciiMonitor_COM1();


    return 1;
}
