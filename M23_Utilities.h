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

#ifndef __M23_UTILITIES_H__
#define __M23_UTILITIES_H__

#include "M23_Typedefs.h"

#define  START_OF_DATA              129

/*****************************************************
          Updgrade Info
 ****************************************************/
#define MAX_UPGRADABLE_DEVICES    16

#define M2300_UPGRADE             0x0
#define CONTROLLER_SLUSHWARE      0x1
#define VIDEO_SLUSHWARE           0x2
#define M1553_SLUSHWARE           0x3
#define PCM_SLUSHWARE             0x4
#define ETHERNET_KERNEL		  0x5

#define AUDIO_LOCAL            100 
#define AUDIO_EVENT_TONE       101 
#define AUDIO_GLOBAL_LEFT      102
#define AUDIO_GLOBAL_RIGHT     103
#define AUDIO_NONE             0xFE


#define TIMESOURCE_IS_INTERNAL    0
#define TIMESOURCE_IS_IRIG        1
#define TIMESOURCE_IS_RMM         2
#define TIMESOURCE_IS_M1553       3

#pragma pack(1)
typedef struct
{
    UINT8    Type;
    UINT32   Length;
}UPGRADE_ENTRIES;

typedef struct
{
    UINT16           NumEntries;
    UINT8            Major;
    UINT8            Minor;
    UPGRADE_ENTRIES  Files[MAX_UPGRADABLE_DEVICES];
}RECORDER_UPGRADE;
#pragma pack(0)

/*****************************************************
          OVERLAY Information
 *****************************************************/
#define WHITE_TRANS   0
#define BLACK_TRANS   1
#define WHITE_BLACK   2
#define BLACK_WHITE   3

#define OVERLAY_OFF 0
#define OVERLAY_ON  1

#define GENERATE_OFF     0
#define GENERATE_BW      1
#define GENERATE_COLOR   2

#define LARGE_FONT 0
#define SMALL_FONT 1

#define TIME_ONLY     0   //No milliseconds
#define TIME_DAY      1   //with millisecondes
#define TIME_MS       2   //Time only with milliseconds
#define TIME_DAY_MS   3   //Time with Day and milliseconds
#define NO_TIME_DAY   4


#define EVENT_TOGGLE_OFF 0
#define EVENT_TOGGLE_ON 1

#define CHAR_TOGGLE_OFF 0
#define CHAR_TOGGLE_ON 1

/***************************************************************************************************************************************************
 *  Name :    M23_ReadCascadeData()
 *
 *  Purpose : This function will check if data is available on any of the video channels. If available , a DMA will be used to transfer data to the 
 *            host buffer. The audio data will be included in the video data.
 *
 *  Inputs :
 *
 ***************************************************************************************************************************************************/
int M23_ReadCascadeData(int close);


#ifdef __cplusplus
}
#endif

#endif /* __M2X_UTILITIES_H__ */
