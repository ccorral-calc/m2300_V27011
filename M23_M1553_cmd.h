/****************************************************************************************
 *
 *    Copyright (C) 2004 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and 
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
 *    for any purpose except as specifically authorized in writing.
 *
 *       File: M23_Utilities.h
 *       Version: 2.0
 *     Author: pcarrion
 *
 *            MONSSTR 23000
 *
        The following defines utilities needed to complete a variety of functions.
 *
 *    Revisions:  
 ******************************************************************************************/

#ifndef __M23_M1553_CMD_H__
#define __M23_M1553_CMD_H__

#include "M23_Typedefs.h"


#define M1553_CMD_ASSIGN           0x01
#define M1553_CMD_BIT              0x02
#define M1553_CMD_DECLASSIFY       0x03
#define M1553_CMD_ERASE            0x04
#define M1553_CMD_EVENT            0x05
#define M1553_CMD_HEALTH           0x06
#define M1553_CMD_INFO             0x07
#define M1553_CMD_PAUSE            0x08
#define M1553_CMD_REPLAY           0x09
#define M1553_CMD_PUBLISH          0x0A
#define M1553_CMD_QUEUE            0x0B
#define M1553_CMD_RECORD           0x0C
#define M1553_CMD_RESET            0x0D
#define M1553_CMD_RESUME           0x0E
#define M1553_CMD_STATUS           0x0F
#define M1553_CMD_STOP             0x10
#define M1553_CMD_TIME             0x11
#define M1553_CMD_VERSION          0x12


#define M1553_STOP_MODE             0x00
#define M1553_REPLAY_MODE           0x01
#define M1553_STOP_AND_REPLAY_MODE  0x02

/*Responses to the .STATUS command*/
#define M1553_COMMAND_FAILED        0x0
#define M1553_COMMAND_SUCCESS       0x1
#define M1553_COMMAND_BUSY          0x2
#define M1553_COMMAND_ERROR         0x3

typedef struct
{
    int     Mode;
    int     resverved;
    int     PointerLSW;
    int     PointerMSW;
}CONDOR_INTERRUPT_Q;

typedef struct
{
    int RT_Enables;
    int FW_Status_Word;
    int RT_LAST_CW;
    int Filter_Table;
    int reserved;
    int ResponseTime;
    int StatusWord;
    int reserved2;
}ADDRESS_TABLE;


typedef struct
{
    int Receive[32];
    int Transmit[32];
    
}FILTER_TABLE;



typedef struct
{
    int  BufferPtrLSW;
    int  BufferPtrMSW;
    int  LegalWCLSW;
    int  LegalWCMSW;
}CONTROL_BUFFER;



typedef struct
{
    UINT32 MessagePtrLSW;
    UINT32 MessagePtrMSW;
    UINT32 Reserved1LSW;
    UINT32 Reserved1MSW;
    UINT32 RT_InterruptEnable1;
    UINT32 RT_InterruptEnable2;
    UINT32 RT_MessageStatus1;
    UINT32 RT_MessageStatus2;
    UINT32 RT_TimeTag1;
    UINT32 RT_TimeTag2;
    UINT32 RT_TimeTag3;
    UINT32 RT_CommandWord;
    UINT32 StatusWord;
    UINT32 Data[32];
    UINT32 Reserved2;
    UINT32 Reserved3;
    UINT32 Reserved4;
    UINT32 Reserved5[4];
}MESSAGE_BUFFER;




#endif /* __M23_M1553_CMD_H__ */
