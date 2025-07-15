

//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 1998, CALCULEX, Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//     File: fireWire_io.h
//     Version: 1.0
//     Author: adnan abbas
//
//    holds functions to send and recieve commands to Cartridge through firewire.
//
//  Revisions:   
//
//

int  M2x_InitializeFireWire(void);
int  M2x_ReadCapacity(void);
int  M2x_SendInquiry(void);
int  M2x_SendTestUnitReady(void);

int M2x_GetCartridgeTime(char *Time);

int M2x_SendBITCommand(void);
int M2x_SendHealthCommand(void);
int M2x_SendCriticalCommand(void);
int M2x_SendStatusCommand(void);
int M2x_SendEraseCommand(void);



