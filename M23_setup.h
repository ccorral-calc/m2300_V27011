//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: setup.c
//    Version: 1.0
//     Author: dm
//
//            MONSSTR 2100 Setup Module
//
//            Coordinates parsing of TMATS files and the configuration
//            of each of the channel types.
//
//    Revisions:   
//
#ifndef _SETUP_H_
#define _SETUP_H_

#include "M23_Typedefs.h"
#include "M2X_channel.h"



// Call this function to prepare for a recording (in response to 
// the .SETUP N command).
//
// returns
//  0 if the TMATS file is found and procesessed sucessfully
// -1, if an error occurs
//
int SetupChannels();

int SetupGet( M23_CHANNEL const **hConfig );


#endif // _SETUP_H
