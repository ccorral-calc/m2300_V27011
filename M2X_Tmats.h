/***********************************************************************
 *
 *     Copyright (c) CALCULEX, Inc., 2004
 *
 ***********************************************************************
 *
 *   Filename :   M23_Tmats.h
 *   Author   :   bdu
 *   Revision :   1.00
 *   Date     :   May 27, 2003
 *   Product  :   Header file for TMATS parsing
 *                                                      
 *   Revision :   
 *
 *   Description:	Header file for SPIDR TMATS file parsing.
 *                                                                        
 ***********************************************************************/

#include "M2X_channel.h"


typedef struct 
{
    char txt[MAX_STRING];
}ctoken;

int ParseTMATSFile (char *fileContents,M23_CHANNEL *m23_chan);
//int ParseTMATSFile (M23_CHANNEL *m23_chan);
int Decode_VideoChannel(char *videoTmats, VIDEO_CHANNEL *m23_video, int *status,int vchan);
int Decode_M1553Channel(char *m1553Tmats, M1553_CHANNEL *m23_m1553, DISCRETE_CHANNEL *m23_dis,int *status);
int Decode_PCMChannel(char *pcmTmats, PCM_CHANNEL *m23_pcm, int *status);
int Decode_TimeChannel(char *TimeTmats, M23_CHANNEL *m23_chan,TIMECODE_CHANNEL *m23_time, int *status);
int Decode_VoiceChannel(char *VoiceTmats, VOICE_CHANNEL *m23_voice, int *status);

/* error code for TMATS parsing */
#define TMATS_PARSE_SUCCESS					0
#define TMATS_M1553_ERROR_1					10
#define TMATS_M1553_ERROR_2					11
#define TMATS_M1553_ERROR_3					12
#define TMATS_M1553_ERROR_4					13
#define TMATS_M1553_ERROR_5					14
#define TMATS_M1553_ERROR_6					15
#define TMATS_VIDEO_ERROR_1					20
#define TMATS_VIDEO_ERROR_2					21
#define TMATS_VIDEO_ERROR_3					22
#define TMATS_VIDEO_ERROR_4					23
#define TMATS_VIDEO_ERROR_5					24
#define TMATS_VIDEO_ERROR_6					25

