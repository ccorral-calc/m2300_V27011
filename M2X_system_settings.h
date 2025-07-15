//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: file_system.h
//    Version: 1.0
//     Author: dm
//
//            MONSSTR 2100 File System
//
//            Handles all file management functions
//
//    Revisions:   
//
#ifndef _SYSTEM_SETTINGS_H_
#define _SYSTEM_SETTINGS_H_

#include "M23_Typedefs.h"



// called to restore settings to values saved the last time Update() was called.
//
// returns:
//  0 on success
// -1 if the file cannot be opened or restored (if necessary)
// -2 if the file is restored because it is missing or corrupted
//
int settings_Initialize( void );


// called to save settings so the can be restored later.
//
int settings_Update( void );

void PutSettings(void);



#endif  // _SYSTEM_SETTINGS_H_
