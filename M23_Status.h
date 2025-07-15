
/****************************************************************************************
 *
 *    Copyright (C) 2004 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and 
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
 *    for any purpose except as specifically authorized in writing.
 *
 *    File: M23_status.h
 *
 *    Version: 1.0
 *    Author: pcarrion
 *
 *    MONSSTR 2300
 *
 *    Defines the parameters that will make up the NVRAM of the 2300 system
 *       
 *
 *    Revisions:  
 ******************************************************************************************/
#ifndef _M23_STATUS_H_
#define _M23_STATUS_H_


extern UINT32 CHAP10_NonCriticalWarnings;
extern UINT32 CHAP10_CriticalWarnings;

extern UINT32  NumberOfPCMChannels;
extern UINT32  NumberOf1553Channels;
extern UINT32  NumberOfVideoChannels;
extern UINT32  NumberOfVoiceChannels;
extern UINT32  NumberOfTimeChannels;

extern UINT32  PTest_Percentage;
extern UINT32  Play_Percentage;


/* Prototypes used to define Initialization and Setting of Health and Critical bits */

void M23_HandleStatusChange();
void M23_RetrieveStatus();
void M23_InitializeHealth();
void M23_SetHealthBit(int channel, int status);
void M23_ClearHealthBit(int channel, int status);
void M23_HealthProcessor();
void M23_InitializeChannelHealth();




#endif

