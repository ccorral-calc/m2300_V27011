//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//     Name: M23_MP_Handler.c  
//    Version: 1.0
//     Author:paulc 
//
//    M2300 MP Board Handler. Provides read and write routines.           
//
//    Revisions:   
//

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include "M23_MP_Handler.h"
#include "M23_Typedefs.h"
#include "M2X_channel.h"
#include "M23_Utilities.h"
#include "M2X_Const.h"
#include "M23_Controller.h"
#include "M23_EventProcessor.h"
#include "cxmp.h"

#define	TIMEOUT	1000000


static int BusEvents[16][32];

static unsigned char tmpbuf[256];
static int MP_Device_Handles[8];

static int MP_PCM_4_Board[4];
static int MP_PCM_8_Board[4];
static int MP_M1553_4_Board[4];
static int MP_M1553_8_Board[4];
static int MP_Video_Board[4];
static int MP_DM_1_4_Board[4];

static int MP_Video_Index;
static int MP_M1553_4_Index;
static int MP_M1553_8_Index;
static int MP_PCM_4_Index;
static int MP_PCM_8_Index;
static int M1553Index = 0;
static int MP_DMIndex    = 0;
static int PCMIndex = 0;



void M23_MPClearSync(int device,int channel);
void M23_MPSetReSync(int device,int channel);
void M23_MPClearReSync(int device,int channel);
void M23_MP_Reset_Video_Channel(int device,int channel);

int TwoAudio[4];

#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))


int myLog2(unsigned int x)
{
 int r = 0;
  while ((x >> r) != 0) {
    r++;
  }

  return r;

}

int myLog2Ceil(unsigned int x)
{
 int r = 0;
  while ((x >> r) != 0) {
    r++;
  }

  if( (x % 2) == 0)
  {
      return r-1; // returns -1 for x==0, floor(log2(x)) otherwise
  }
  else
  {
      return r;
  }

}


void M23_AllocateBusEvents()
{

    //printf("Event def size %d,%d\r\n",16*sizeof(M1553_BUS),sizeof(M1553_BUS));
    EventDef       = (M1553_BUS*)malloc(16*sizeof(M1553_BUS));

    //printf("PCMEvent def size %d,%d\r\n",16*sizeof(PCM_DEF),sizeof(PCM_DEF));
    //PCMEventDef    = (PCM_DEF*)malloc(16*sizeof(PCM_DEF));

    //printf("DataConversion size %d %d\r\n",16*sizeof(DATA_CONVERSION),sizeof(DATA_CONVERSION));
    dataConversion = (DATA_CONVERSION*)malloc(16*sizeof(DATA_CONVERSION));

}

int M23_GetM1553Index()
{
    return(M1553Index);
}

int M23_GetPCMIndex()
{
    return(PCMIndex);
}

void M23_ResetIndex()
{
    int i;

    M1553Index = 0;
    PCMIndex = 0;

    for(i =0;i < 16;i++)
    {
        TableEntryNumber[i] = 0;
        PCMTableEntryNumber[i] = 0;
    }

}


UINT8 M23_IsTimeEvent(UINT8 TableEntry,UINT8 BusNumber,UINT16 *WordNum,UINT32 *Event)
{
    int i;

    for(i = 0; i < M1553Index;i++)
    {
        if( (M1553Events[i].TableEntry == (UINT32)TableEntry) && (M1553Events[i].BusNumber == (UINT32)BusNumber) )
        {
            if( (M1553Events[i].EventNum >= TFOM_F15C_EVENT) && (M1553Events[i].EventNum <= R0_EVENT) )
            {
                 *WordNum = M1553Events[i].DataWordNum;
                 *Event   = M1553Events[i].EventNum;
                 return(1);
            }
        }
    }

    return(0);
}

UINT8 M23_IsMultiEvent(UINT8 TableEntry,UINT8 BusNumber,UINT16 *CW,int *index)
{
    int i;

    for(i = 0; i < M1553Index;i++)
    {
        if( (M1553Events[i].TableEntry == (UINT32)TableEntry) && (M1553Events[i].BusNumber == (UINT32)BusNumber) )
        {
            if( M1553Events[i].MultiEvent == 1)
            {
                 *CW = M1553Events[i].CommandWord;
                 *index = M1553Index;
                 return(1);
            }
        }
    }

    return(0);
}


void M23_SwapM1553Event(int index)
{
    int cw;
    int id;
    int event;
    int dw;
    int mask;
    int enable;
    int mode;
    int ic;
    int contin;
    int cw_only;

    cw = M1553Events[index].CommandWord;
    id = M1553Events[index].ChannelID;
    event = M1553Events[index].EventNum;
    dw = M1553Events[index].DataWordNum;
    mask = M1553Events[index].Mask;
    enable = M1553Events[index].EventEnable;
    mode = M1553Events[index].EventMode;
    ic = M1553Events[index].EventIC;
    contin = M1553Events[index].EventContinue;
    cw_only = M1553Events[index].CW_Only;

    M1553Events[index].CommandWord = M1553Events[index - 1].CommandWord;
    M1553Events[index].ChannelID = M1553Events[index -1].ChannelID;
    M1553Events[index].EventNum = M1553Events[index - 1].EventNum;
    M1553Events[index].DataWordNum = M1553Events[index - 1].DataWordNum;
    M1553Events[index].Mask = M1553Events[index - 1].Mask;
    M1553Events[index].EventEnable = M1553Events[index - 1].EventEnable;
    M1553Events[index].EventMode = M1553Events[index - 1].EventMode;
    M1553Events[index].EventIC =  M1553Events[index - 1].EventIC;
    M1553Events[index].EventContinue = 0;
    M1553Events[index].CW_Only =  M1553Events[index - 1].CW_Only;

    M1553Events[index -1].CommandWord = cw;
    M1553Events[index -1].ChannelID = id;
    M1553Events[index -1].EventNum = event;
    M1553Events[index -1].DataWordNum = dw;
    M1553Events[index -1].Mask = mask;
    M1553Events[index -1].EventEnable = enable;
    M1553Events[index -1].EventMode = mode;
    M1553Events[index -1].EventIC = ic;

    if(M1553Events[index -1].CW_Only == 1)
    {
        M1553Events[index -1].EventContinue = 0;
    }
    else
    {
        M1553Events[index -1].EventContinue = 1;
    }

    M1553Events[index -1].CW_Only =  cw_only;
}

void M23_CreateM1553Mask()
{
    int i;
    int k;

    int first;
    int index = 0;

    M23_CHANNEL  const *config;

    SetupGet(&config);

    for(i = 0; i < config->NumConfigured1553;i++)
    {
        first = 0;
        for(k = 1; k < M1553Index;k++)
        {
            if(M1553Events[k].ChannelID == config->m1553_chan[i].chanID)
            {
                if(M1553Events[k].CommandWord == M1553Events[k - 1].CommandWord)
                {
                    if( M1553Events[k].DataWordNum ==  M1553Events[k - 1].DataWordNum)
                    {
                        if(first == 0)
                        {
                            index = k-1; //This is the first Entry
                            M1553Events[index].MultiEvent = 1;
                            M1553Events[index].TotalMask = M1553Events[k].Mask | M1553Events[k-1].Mask;
                            first = 1;
                        }
                        else
                        {
                            M1553Events[index].TotalMask |= M1553Events[k].Mask;
                        }
                    }
                }
                else
                {
                    first = 0;
                }
            }
        }
    }
}


void M23_SortM1553Event()
{
    int i;
    int j;
    int k;

    M23_CHANNEL  const *config;

    SetupGet(&config);

    for(i = 0; i < config->NumConfigured1553;i++)
    {
        for(j = 0; j < 32;j++)
        {
            for(k = 1; k < M1553Index;k++)
            {
                if(M1553Events[k].ChannelID == config->m1553_chan[i].chanID)
                {
                    if( M1553Events[k].CW_Only == 0)
                    {
                        if(M1553Events[k].CommandWord == M1553Events[k - 1].CommandWord)
                        {
                            if( M1553Events[k].DataWordNum <  M1553Events[k - 1].DataWordNum)
                            {
                                M23_SwapM1553Event(k);
                            }
                            else if( M1553Events[k].DataWordNum ==  M1553Events[k - 1].DataWordNum)
                            {
                                if( M1553Events[k].Mask  <  M1553Events[k - 1].Mask )
                                {
                                    M23_SwapM1553Event(k);
                                }
                            }
                        }
                    }
                }
         
            }
        }
    }

    /*Set the Continue bit*/
    for(i = 0; i < M1553Index;i++)
    {
        if(M1553Events[i].ChannelID == M1553Events[i+1].ChannelID)
        {
            if(M1553Events[i].CommandWord == M1553Events[i+1].CommandWord)
            {
                if(M1553Events[i].CW_Only == 0)
                {
                    M1553Events[i].EventContinue = 1;
                }
                else
                {
                    M1553Events[i].EventContinue = 0;
                }
                M1553Events[i+1].EventContinue = 0;
            }
   
        }
        
    }
}

void M23_PrintEvent()
{
    int i;


    for(i = 0; i < M1553Index;i++)
    {
        printf("M1553 Event %d Bus Number = 0x%x\r\n",i, M1553Events[i].BusNumber);
        printf("M1553 Event %d CW = 0x%x\r\n",i, M1553Events[i].CommandWord);
        printf("M1553 Event %d ID = %d\r\n",i, M1553Events[i].ChannelID);
        printf("M1553 Event %d Event = %d\r\n",i, M1553Events[i].EventNum);
        printf("M1553 Event %d TableEntry = 0x%x\r\n",i, M1553Events[i].TableEntry);
        printf("M1553 Event %d Word Num = %d\r\n",i, M1553Events[i].DataWordNum);
        printf("M1553 Event %d Mask = 0x%x\r\n",i, M1553Events[i].Mask);
        printf("M1553 Event %d Mode = 0x%x\r\n",i, M1553Events[i].EventMode);
        printf("M1553 Event %d Capture1 = 0x%x\r\n",i, M1553Events[i].Capture1);
        printf("M1553 Event %d Cont = 0x%x\r\n",i, M1553Events[i].EventContinue);
        printf("M1553 Event %d Capture Yet = 0x%x\r\n",i, M1553Events[i].NotCapturedYet);
        printf("M1553 Event %d Index = %d\r\n",i, M1553Events[i].ConversionIndex);
        printf("M1553 Event %d Conversion = %d\r\n",i, M1553Events[i].IsDataConversion);
        printf("M1553 Event %d Is Trigger = %d\r\n",i, M1553Events[i].IsTrigger);
        printf("M1553 Event %d Is Multi = %d\r\n",i, M1553Events[i].MultiEvent);
    }

}

void M23_AddPauseResumeEvents()
{
    /*Add the Pause Event*/
    M1553Events[M1553Index].CommandWord    = PauseResume.PauseCW;
    M1553Events[M1553Index].ChannelID      = PauseResume.ChanID;
    M1553Events[M1553Index].EventNum       = PAUSE_EVENT;
    M1553Events[M1553Index].DataWordNum    = 0;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 1;
    M1553Events[M1553Index].EventMode      = 0;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

    /*Add the Resume Event*/
    M1553Events[M1553Index].CommandWord    = PauseResume.ResumeCW;
    M1553Events[M1553Index].ChannelID      = PauseResume.ChanID;
    M1553Events[M1553Index].EventNum       = RESUME_EVENT;
    M1553Events[M1553Index].DataWordNum    = 0;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 1;
    M1553Events[M1553Index].EventMode      = 0;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

}

void M23_AddF15M1553TimeEvents(int chanID)
{

    /*We will be using 4096,4095,4094*/

    /*Add the R0 Event*/
    //M1553Events[M1553Index].CommandWord    = 0x2011;
    //M1553Events[M1553Index].ChannelID      = chanID;
    //M1553Events[M1553Index].EventNum       = R0_EVENT;
    //M1553Events[M1553Index].DataWordNum    = 1;
    //M1553Events[M1553Index].Mask           = 0xFFFF;
    //M1553Events[M1553Index].EventEnable    = 1;
    //M1553Events[M1553Index].CW_Only        = 0;
    //M1553Events[M1553Index].EventMode      = 1;
    //M1553Events[M1553Index].EventIC        = 0;
    //M1553Events[M1553Index].EventContinue  = 0;

    M1553Events[M1553Index].CommandWord    = 0x2011;
    M1553Events[M1553Index].ChannelID      = chanID;
    M1553Events[M1553Index].EventNum       = R0_EVENT;
    M1553Events[M1553Index].DataWordNum    = 1;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 1;
    M1553Events[M1553Index].EventMode      = 1;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

    /*Add the T9 Event*/
    M1553Events[M1553Index].CommandWord    = 0x2538;
    M1553Events[M1553Index].ChannelID      = chanID;
    M1553Events[M1553Index].EventNum       = T9_EVENT;
    //M1553Events[M1553Index].DataWordNum    = 3;
    M1553Events[M1553Index].DataWordNum    = 22;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 0;
    M1553Events[M1553Index].EventMode      = 1;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

    /*Add the T20 Event*/
    M1553Events[M1553Index].CommandWord    = 0x2684;
    M1553Events[M1553Index].ChannelID      = chanID;
    M1553Events[M1553Index].EventNum       = T20_EVENT;
    M1553Events[M1553Index].DataWordNum    = 2;
    M1553Events[M1553Index].Mask           = 0x0100;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 0;
    M1553Events[M1553Index].EventMode      = 1;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

}

void M23_AddB1BM1553TimeEvents(int chanID)
{

    /*We will be using 4096,4095,4094*/

    /*Add the B1 T17 Event*/
    M1553Events[M1553Index].CommandWord    = 0x0811;
    M1553Events[M1553Index].ChannelID      = chanID;
    M1553Events[M1553Index].EventNum       = B1_T17_EVENT;
    M1553Events[M1553Index].DataWordNum    = 1;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 1;
    M1553Events[M1553Index].EventMode      = 1;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

    /*Add the B1B T9 Event*/
    M1553Events[M1553Index].CommandWord    = 0x0D3E;
    M1553Events[M1553Index].ChannelID      = chanID;
    M1553Events[M1553Index].EventNum       = B1_T9_EVENT;
    M1553Events[M1553Index].DataWordNum    = 22;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 0;
    M1553Events[M1553Index].EventMode      = 1;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

    /*Add the T16 Event*/
    M1553Events[M1553Index].CommandWord    = 0x0E00;
    M1553Events[M1553Index].ChannelID      = chanID;
    M1553Events[M1553Index].EventNum       = B1_T16_EVENT;
    M1553Events[M1553Index].DataWordNum    = 1;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 1;
    M1553Events[M1553Index].EventMode      = 1;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

}

void M23_AddB52M1553TimeEvents(int chanID)
{

    /*We will be using 4085,4086,4087*/

    /*Add the B52 CC05 Event*/
    M1553Events[M1553Index].CommandWord    = 0xCC05;
    M1553Events[M1553Index].ChannelID      = chanID;
    M1553Events[M1553Index].EventNum       = B52_CC05_EVENT;
    M1553Events[M1553Index].DataWordNum    = 0;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 1;
    M1553Events[M1553Index].EventMode      = 0;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

    /*Add the B52 CC29 Event*/
    M1553Events[M1553Index].CommandWord    = 0xCC29;
    M1553Events[M1553Index].ChannelID      = chanID;
    M1553Events[M1553Index].EventNum       = B52_CC29_EVENT;
    M1553Events[M1553Index].DataWordNum    = 0;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 1;
    M1553Events[M1553Index].EventMode      = 0;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

    /*Add the B52 CC29 Event*/
    M1553Events[M1553Index].CommandWord    = 0xCC2B;
    M1553Events[M1553Index].ChannelID      = chanID;
    M1553Events[M1553Index].EventNum       = B52_CC2B_EVENT;
    M1553Events[M1553Index].DataWordNum    = 0;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 1;
    M1553Events[M1553Index].EventMode      = 0;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;

    /*Add the B52 CD00 Event*/
    M1553Events[M1553Index].CommandWord    = 0xCD00;
    M1553Events[M1553Index].ChannelID      = chanID;
    M1553Events[M1553Index].EventNum       = B52_CD00_EVENT;
    M1553Events[M1553Index].DataWordNum    = 0;
    M1553Events[M1553Index].Mask           = 0xFFFF;
    M1553Events[M1553Index].EventEnable    = 1;
    M1553Events[M1553Index].CW_Only        = 1;
    M1553Events[M1553Index].EventMode      = 0;
    M1553Events[M1553Index].EventIC        = 0;
    M1553Events[M1553Index].EventContinue  = 0;

    M1553Index++;


}

void M23_FillInM1553DataConversionEvent(int index,int EventNumber,int msg,int Fragment)
{
    //static int         ch = 0;
    int                CW;
    int                EventIndex = (dataConversion+index)->EventIndex;
    int                debug;
    M23_CHANNEL  const *config;

    SetupGet(&config);

    M23_GetDebugValue(&debug);

    M1553Events[M1553Index].ConversionIndex   = (UINT16)index;
    M1553Events[M1553Index].CW_Only           = 0;
    M1553Events[M1553Index].EventNum          = EventNumber+1;
    M1553Events[M1553Index].EventEnable       = 1;
    M1553Events[M1553Index].EventContinue     = 0;
    M1553Events[M1553Index].IsDataConversion  = 1;
    M1553Events[M1553Index].EventMode         = (dataConversion+index)->EventMode;
    M1553Events[M1553Index].EventIC           = (dataConversion+index)->EventIC;
    M1553Events[M1553Index].EventType         = (dataConversion+index)->EventType;
    M1553Events[M1553Index].ConvType          = (dataConversion+index)->ConvType;
    M1553Events[M1553Index].NotCapturedYet    = 1;
    M1553Events[M1553Index].TotalMask         = 0x0;
    M1553Events[M1553Index].MultiEvent        = 0;
    M1553Events[M1553Index].DiscreteValue     = 0;

    if(msg == 1)
    {

        M1553Events[M1553Index].DataWordNum    = (dataConversion+index)->Msg1.Event[EventIndex].WordNumber[Fragment];
        M1553Events[M1553Index].FragPosition   = (dataConversion+index)->Msg1.Event[EventIndex].Position[Fragment];
        M1553Events[M1553Index].Mask           = (dataConversion+index)->Msg1.Event[EventIndex].Mask[Fragment];

        CW = ( (dataConversion+index)->Msg1.RT << 11) ;
        CW |= ((dataConversion+index)->Msg1.TransRcv << 10);
        CW |= ((dataConversion+index)->Msg1.SubAddr << 5);
        CW |= ((dataConversion+index)->Msg1.WordCount);

        if(debug)
            printf("1553 Data Conversion %d, CW 0x%x\r\n",M1553Index,CW);

        M1553Events[M1553Index].CommandWord       = (UINT16)CW;
        M1553Events[M1553Index].ChannelID         = (dataConversion+index)->ChanID1;
        M1553Events[M1553Index].IsTrigger         = (dataConversion+index)->Msg1.Event[EventIndex].IsTrigger;

        if((dataConversion+index)->Msg1.Event[EventIndex].CW_Only == 1)
        {
            M1553Events[M1553Index].CW_Only        = 1;
            M1553Events[M1553Index].DataWordNum    = 0;
            M1553Events[M1553Index].Mask           = 0xFFFF;
            M1553Events[M1553Index].NotCapturedYet = 0;
        }

    }
    else
    {
        M1553Events[M1553Index].DataWordNum    = (dataConversion+index)->Msg2.Event[EventIndex].WordNumber[Fragment];
        M1553Events[M1553Index].FragPosition   = (dataConversion+index)->Msg2.Event[EventIndex].Position[Fragment];
        M1553Events[M1553Index].Mask           = (dataConversion+index)->Msg2.Event[EventIndex].Mask[Fragment];

        CW = ( (dataConversion+index)->Msg2.RT << 11) ;
        CW |= ((dataConversion+index)->Msg2.TransRcv << 10);
        CW |= ((dataConversion+index)->Msg2.SubAddr << 5);
        CW |= ((dataConversion+index)->Msg2.WordCount);

        M1553Events[M1553Index].CommandWord       = (UINT16)CW;
        M1553Events[M1553Index].ChannelID         = (dataConversion+index)->ChanID2;
        M1553Events[M1553Index].IsTrigger         = (dataConversion+index)->Msg2.Event[EventIndex].IsTrigger;

        if((dataConversion+index)->Msg2.Event[EventIndex].CW_Only == 1)
        {
            M1553Events[M1553Index].CW_Only        = 1;
            M1553Events[M1553Index].DataWordNum    = 0;
            M1553Events[M1553Index].Mask           = 0xFFFF;
            M1553Events[M1553Index].NotCapturedYet = 0;
        }
    }

    M1553Index++;
}

void M23_FillInM1553Event(int chanIndex,int BusIndex,int MeasIndex,int EventIndex,int MeasEvent,int Fragment)
{
    //static int         ch = 0;
    int                CW;
    M23_CHANNEL  const *config;

    SetupGet(&config);

    if(EventTable[EventIndex + 1].DualEvent == 1)
    {
        if((EventDef+BusIndex)->Msg[MeasIndex].RT <= 16)
        {
            (EventDef+BusIndex)->Msg[MeasIndex].RT = (EventDef+BusIndex)->Msg[MeasIndex].RT + 16;
        }
        else
        {
            (EventDef+BusIndex)->Msg[MeasIndex].RT = (EventDef+BusIndex)->Msg[MeasIndex].RT - 16;
        }

        if((EventDef+BusIndex)->Msg[MeasIndex].SubAddr <= 16)
        {
            (EventDef+BusIndex)->Msg[MeasIndex].SubAddr = (EventDef+BusIndex)->Msg[MeasIndex].SubAddr + 16;
        }
        else
        {
            (EventDef+BusIndex)->Msg[MeasIndex].SubAddr = (EventDef+BusIndex)->Msg[MeasIndex].SubAddr - 16;
        }

        M1553Events[M1553Index].CW_Only        = 1;
        M1553Events[M1553Index].DataWordNum    = 0;
        M1553Events[M1553Index].FragPosition   = 0;
        M1553Events[M1553Index].Mask           = 0xFFFF;
        M1553Events[M1553Index].EventMode      = 0;
        M1553Events[M1553Index].EventIC        = 0;


    }
    else if(EventTable[EventIndex + 1].NoResponseEvent == 1)
    {

        M1553Events[M1553Index].CW_Only        = 1;
        M1553Events[M1553Index].DataWordNum    = 0;
        M1553Events[M1553Index].FragPosition   = 0;
        M1553Events[M1553Index].Mask           = 0xFFFF;
        M1553Events[M1553Index].EventMode      = 0;
        M1553Events[M1553Index].EventType      = EVENT_TYPE_MEAS_DISCRETE;

    }
    else
    {
        M1553Events[M1553Index].CW_Only        = (EventDef+BusIndex)->Msg[MeasIndex].Event[MeasEvent].CW_Only;
        M1553Events[M1553Index].DataWordNum    = (EventDef+BusIndex)->Msg[MeasIndex].Event[MeasEvent].WordNumber[Fragment];
        M1553Events[M1553Index].FragPosition   = (EventDef+BusIndex)->Msg[MeasIndex].Event[MeasEvent].Position[Fragment];
        M1553Events[M1553Index].Mask           = (EventDef+BusIndex)->Msg[MeasIndex].Event[MeasEvent].Mask[Fragment];
        M1553Events[M1553Index].EventMode      = EventTable[EventIndex + 1].EventMode;
        M1553Events[M1553Index].EventIC        = EventTable[EventIndex + 1].EventIC;
        M1553Events[M1553Index].EventType      = EventTable[EventIndex + 1].EventType;

        if(M1553Events[M1553Index].CW_Only == 1)
        {
            M1553Events[M1553Index].Mask  = 0xFFFF;
        }
    }

    CW = ( (EventDef+BusIndex)->Msg[MeasIndex].RT << 11) ;
    CW |= ((EventDef+BusIndex)->Msg[MeasIndex].TransRcv << 10);
    CW |= ((EventDef+BusIndex)->Msg[MeasIndex].SubAddr << 5);
    CW |= ((EventDef+BusIndex)->Msg[MeasIndex].WordCount);

    M1553Events[M1553Index].CommandWord       = (UINT16)CW;
    M1553Events[M1553Index].ChannelID         = config->m1553_chan[chanIndex].chanID;
    M1553Events[M1553Index].EventNum          = EventIndex + 1;
    M1553Events[M1553Index].EventEnable       = 1;
    M1553Events[M1553Index].EventContinue     = 0;
    M1553Events[M1553Index].IsDataConversion  = 0;
    M1553Events[M1553Index].IsTrigger         = 0;
    M1553Events[M1553Index].ConversionIndex   = 0;
    M1553Events[M1553Index].ConvType          = 0;
    M1553Events[M1553Index].NotCapturedYet    = 0;
    M1553Events[M1553Index].TotalMask         = 0x0;
    M1553Events[M1553Index].MultiEvent        = 0;
    M1553Events[M1553Index].DiscreteValue     = 0;

    M1553Index++;

}

void M23_Initialize1553Events()
{
    int i;

    for(i = 0; i < MAX_NUM_EVENTS; i++)
    {
        M1553Events[i].ChannelID = 0xDEADBEEF;
    }
    
}

void M23_DefineM1553Event()
{
    int i;
    int j;
    int k;
    int l;
    int x;
    int e;

    char meas[80];
    char link[80];


    int debug;

    M23_CHANNEL  const *config;
    SetupGet(&config);

    M23_Initialize1553Events();

    M23_GetDebugValue(&debug);

    for(i = 0; i < config->NumConfigured1553;i++)
    {
        for(j = 1; j < config->NumBuses + 1;j++)
        {
            if( (strncmp(config->m1553_chan[i].BusLink,(EventDef+j)->BusName,strlen((EventDef+j)->BusName)) == 0 ) )
            {
                for(k = 1; k < (EventDef+j)->NumMsgs + 1;k++)
                {
                    for(l =0; l < config->NumEvents;l++)
                    {
                        if( (strncmp(config->m1553_chan[i].BusLink,EventTable[l+1].MeasurementSource,strlen(config->m1553_chan[i].BusLink))  == 0)  )
                        {

                            for(x = 1; x < (EventDef+j)->Msg[k].NumMeas + 1;x++)
                            {
                                memset(meas,0x0,80);
                                memset(link,0x0,80);
                                strncpy(meas,(EventDef+j)->Msg[k].Event[x].MeasName,strlen((EventDef+j)->Msg[k].Event[x].MeasName));
                                strncpy(link,EventTable[l+1].MeasurementLink,strlen(EventTable[l+1].MeasurementLink));

                                //if(debug)
                                //    printf("meas %s, link %s\r\n",meas,link);

                                if( (strcmp(meas,link) == 0)  )
                                {
                                    for(e = 1; e < (EventDef+j)->Msg[k].Event[x].NumWords+1;e++)
                                    {
                                        M23_FillInM1553Event(i,j,k,l,x,e);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }


    /*Add the M1553 Time Events*/
    if(config->timecode_chan.m1553_timing.IsEnabled)
    {
        if(config->timecode_chan.m1553_timing.Format == B1B_EGI)
        {
            M23_AddB1BM1553TimeEvents(config->timecode_chan.m1553_timing.ChannelId);
        }

    }

    if(PauseResume.Enabled)
    {
        M23_AddPauseResumeEvents();
    }



}


void M23_DefineM1553DataConversion()
{
    int i;
    int j;
    int k;
    int x;
    int y;
    int ii;

    char der[33];
    char meas[33];
    char meas_1[33];
    char meas_2[33];
    char trigger[33];

    int num_events = 0;

    int debug;

    M23_CHANNEL  const *config;
    SetupGet(&config);

    M23_GetDebugValue(&debug);

    for(i = 0; i < config->NumEvents;i++)
    {
        for(j = 1; j < config->NumberConversions + 1;j++)
        {

            memset(der,0x0,33);
            memset(meas_1,0x0,33);
            memset(meas_2,0x0,33);
            memset(trigger,0x0,33);

            strncpy(der,(dataConversion+j)->DerivedName,strlen((dataConversion+j)->DerivedName) );

            strncpy(meas_1,(dataConversion+j)->MeasName_1,strlen((dataConversion+j)->MeasName_1));
            strncpy(meas_2,(dataConversion+j)->MeasName_2,strlen((dataConversion+j)->MeasName_2));
            strncpy(trigger,(dataConversion+j)->TriggerName,strlen((dataConversion+j)->TriggerName));


            for(x = 0; x < 8;x++)
            {
                (dataConversion+j)->CurrentValue_1[x] = 0xDEADBEEF;
                (dataConversion+j)->CurrentValue_2[x] = 0xDEADBEEF;
            }
            (dataConversion+j)->InitialTrigger = 0;

            //if( strncmp(EventTable[i+1].MeasurementLink,der,strlen(der) - 1) == 0)
            if( strcmp(EventTable[i+1].MeasurementLink,der) == 0)
            {
                for(k = 1; k < config->NumBuses + 1;k++)
                {
                    for(x = 1; x < (EventDef+k)->NumMsgs + 1;x++)
                    {
                        for(y = 1; y < (EventDef+k)->Msg[x].NumMeas + 1;y++)
                        {
                            memset(meas,0x0,33);
                            strncpy(meas,(EventDef+k)->Msg[x].Event[y].MeasName,strlen((EventDef+k)->Msg[x].Event[y].MeasName));
                            if( (strncmp(meas,meas_1,strlen(meas)) == 0)  )
                            {
                                num_events++;
                                (dataConversion+j)->IsPCM           = 0;
                                (dataConversion+j)->ChanID1         = config->m1553_chan[k-1].chanID;
                                (dataConversion+j)->Msg1.TransRcv   = (EventDef+k)->Msg[x].TransRcv;
                                (dataConversion+j)->Msg1.SubAddr    = (EventDef+k)->Msg[x].SubAddr;
                                (dataConversion+j)->Msg1.WordCount  = (EventDef+k)->Msg[x].WordCount;
                                (dataConversion+j)->Msg1.RT         = (EventDef+k)->Msg[x].RT;
                                (dataConversion+j)->Msg1.NumMeas    = (EventDef+k)->Msg[x].NumMeas;
                                (dataConversion+j)->EventIndex = y;

                                (dataConversion+j)->Msg1.Event[y].NumWords = (EventDef+k)->Msg[x].Event[y].NumWords;

                                (dataConversion+j)->Msg1.Event[x].CW_Only   = (EventDef+k)->Msg[x].Event[y].CW_Only;
                                for(ii = 1; ii < (EventDef+k)->Msg[x].Event[y].NumWords+1;ii++)
                                {
                                    (dataConversion+j)->Msg1.Event[y].TransOrder[ii]   = (EventDef+k)->Msg[x].Event[y].TransOrder[ii];
                                    (dataConversion+j)->Msg1.Event[y].Mask[ii]         = (EventDef+k)->Msg[x].Event[y].Mask[ii];
                                    (dataConversion+j)->Msg1.Event[y].Position[ii]     = (EventDef+k)->Msg[x].Event[y].Position[ii];
                                    (dataConversion+j)->Msg1.Event[y].WordNumber[ii]   = (EventDef+k)->Msg[x].Event[y].WordNumber[ii];
                                }
                                if( (strncmp(trigger,meas_1,strlen(trigger)) == 0)  )
                                {
                                    (dataConversion+j)->Msg1.Event[y].IsTrigger   = 1;
                                }
                                else
                                {
                                    (dataConversion+j)->Msg1.Event[y].IsTrigger   = 0;
                                }
                            }
                            else if( (strncmp(meas,meas_2,strlen(meas)) == 0)  )
                            {
                                num_events++;
                                (dataConversion+j)->IsPCM           = 0;
                                (dataConversion+j)->ChanID2         = config->m1553_chan[k-1].chanID;
                                (dataConversion+j)->Msg2.TransRcv   = (EventDef+k)->Msg[x].TransRcv;
                                (dataConversion+j)->Msg2.SubAddr    = (EventDef+k)->Msg[x].SubAddr;
                                (dataConversion+j)->Msg2.WordCount  = (EventDef+k)->Msg[x].WordCount;
                                (dataConversion+j)->Msg2.RT         = (EventDef+k)->Msg[x].RT;
                                (dataConversion+j)->Msg2.NumMeas    = (EventDef+k)->Msg[x].NumMeas;
                                (dataConversion+j)->EventIndex = y;

                                (dataConversion+j)->Msg2.Event[x].CW_Only   = (EventDef+k)->Msg[x].Event[x].CW_Only;

                                (dataConversion+j)->Msg2.Event[y].NumWords = (EventDef+k)->Msg[x].Event[y].NumWords;
                                for(ii = 1; ii < (EventDef+k)->Msg[x].Event[y].NumWords+1;ii++)
                                {
                                    (dataConversion+j)->Msg2.Event[y].TransOrder[ii]   = (EventDef+k)->Msg[x].Event[y].TransOrder[ii];
                                    (dataConversion+j)->Msg2.Event[y].Mask[ii]         = (EventDef+k)->Msg[x].Event[y].Mask[ii];
                                    (dataConversion+j)->Msg2.Event[y].Position[ii]     = (EventDef+k)->Msg[x].Event[y].Position[ii];
                                    (dataConversion+j)->Msg2.Event[y].WordNumber[ii]   = (EventDef+k)->Msg[x].Event[y].WordNumber[ii];
                                }
                                if( (strncmp(trigger,meas_2,strlen(trigger)) == 0)  )
                                {
                                    (dataConversion+j)->Msg2.Event[y].IsTrigger   = 1;
                                }
                                else
                                {
                                    (dataConversion+j)->Msg2.Event[y].IsTrigger   = 0;
                                }
                            }
                        }
                    }
                }
           }
        }
    }

    if(num_events > 0)
    {
        for(i = 0; i < config->NumEvents;i++)
        {
            for(j = 1; j < config->NumberConversions + 1;j++)
            {
                memset(der,0x0,33);
                strncpy(der,(dataConversion+j)->DerivedName,strlen((dataConversion+j)->DerivedName));

                //if( strncmp(EventTable[i+1].MeasurementLink,der,strlen(der) - 1) == 0)
                if( strcmp(EventTable[i+1].MeasurementLink,der) == 0)
                {
                    (dataConversion+j)->EventType = EventTable[i+1].EventType;
                    (dataConversion+j)->EventIC   = EventTable[i+1].EventIC;
                    (dataConversion+j)->EventMode = EventTable[i+1].EventMode;
                    for(x = 1; x < (dataConversion+j)->Msg1.NumMeas + 1;x++)
                    {
                        for(y = 1; y < (dataConversion+j)->Msg1.Event[x].NumWords+1;y++)
                        {
                            M23_FillInM1553DataConversionEvent(j,i,1,y);
                        }
                    }

                    for(x = 1; x < (dataConversion+j)->Msg2.NumMeas+1;x++)
                    {
                        for(y = 1; y < (dataConversion+j)->Msg2.Event[x].NumWords+1;y++)
                        {
                            M23_FillInM1553DataConversionEvent(j,i,2,y);
                        }
                    }
                }
            }
        }
    }
}



void M23_MP_StopChannels(int device,int channel)
{
    UINT32 CSR;

    /*Clear the CM run bit*/
    CSR = (0x0 << 16) & 0xFFFF0000;
    CSR |= 0x0000;
    CSR |= WRITE_COMMAND;
    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CONDOR_OFFSET);

    usleep(10);
    CSR = 0;
    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CONDOR_OFFSET); 
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CONDOR_OFFSET);
    //printf("Reading CONDOR 0x%x\r\n",CSR);

    /*Clear the Enable Bit*/
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
    M23_mp_write_csr(device,BAR1,CSR & ~MP_CHANNEL_ENABLE,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);


}

void M23_MP_StopPCMChannels(int device,int channel)
{
    UINT32 CSR;

    /*Clear the Enable Bit*/
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
    M23_mp_write_csr(device,BAR1,CSR & ~MP_CHANNEL_ENABLE,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
}
void M23_NumberMPBoards(int *numboards)
{
    int i;
    int boards = 0;

    for(i = 0;i< 5;i++)
    {

        if(MP_Device_Exists[i] == 1)
            boards++;
    }

    *numboards = boards;
}

void M23_ExcludeAll(int device,int channel,int stream)
{
    int    i;
    int    j;
    int    k;

    UINT32 CSR;
    UINT32 FILTER = 0;

    for(i = 0; i < 32;i++)
    {
        for(j = 0; j < 2;j++)
        {
            for(k = 0; k < 32;k++)
            {
                FILTER = (M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET) & 0xFFFF0000);
                FILTER |= (( (i <<11) | (j << 10) | (k << 5) ) & 0xFFE0);
                M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                FILTER = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                FILTER &= 0xFFFF000F;

                FILTER &= ~(1 << ( stream)) ;
                FILTER |= MP_CHANNEL_FILTER_WRITE_ENABLE;
                FILTER |= (1 << (16 + stream)) ;
                M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET); //Exclude All Messages
                CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                if(CSR != FILTER)
                {
                    M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET); //Exclude All Messages
                }
            }
        }
    }
}

void M23_IncludeAll(int device,int channel, int stream)
{
    int    i;
    int    j;
    int    k;

    UINT32 CSR;
    UINT32 FILTER = 0;

    for(i = 0; i < 32;i++)
    {
        for(j = 0; j < 2;j++)
        {
            for(k = 0; k < 32;k++)
            {
                FILTER = (M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET) & 0xFFFF0000);
                FILTER |= (( (i <<11) | (j << 10) | (k << 5) ) & 0xFFE0);
                M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                FILTER = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                FILTER &= 0xFFFF000F;

                FILTER &= ~(1 <<  stream) ;
                FILTER |= MP_CHANNEL_FILTER_WRITE_ENABLE;
                FILTER &= ~(1 << (16 + stream)) ;
                M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET); //Exclude All Messages
                CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                if(CSR != FILTER)
                {
                    M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET); //Exclude All Messages
                }
            }
        }
    }
}


void M23_MP_Enable_M1553_Channel(int device,int channel)
{
    UINT32 CSR    = 0;

    /*Set the Enable Bit*/
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
    M23_mp_write_csr(device,BAR1,CSR | MP_CHANNEL_ENABLE,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);

}

void M23_MP_Initialize_M1553_Channel(int device,int channel,M1553_CHANNEL chan,int Enabled)
{

    int    i;
    int    j;
    int    k;
    int    l;
    int    index;
    int    debug;
    int    rt;
    int    sa;
    int    tr;
    int    IsEvent;
    UINT32 PKT    = 0;
    UINT32 CSR    = 0;
    UINT32 FILTER = 0;
    UINT32 Enables;

    UINT32 UpperTable = 0x0;
    UINT32 LowerTable = 0x0;


    M23_CHANNEL  const *config;


    SetupGet(&config);

    M23_GetDebugValue(&debug);


    M23_ExcludeAll(device,channel,1);
    if(MP_Device_Exists[device])
    {
        if(Enabled)
        {

            if(chan.pktMethod == M1553_PACKET_METHOD_AUTO)
            {
                PKT = MP_M1553_CHANNEL_PKT_MODE;

            }
            else
            {
                PKT = MP_M1553_SINGLE_MSG_PKT;
                PKT |= 0x01;
            }

            PKT |= (M1553_MAX_PACKET_SIZE << 8);

            /*Set the PKT Method*/
            M23_mp_write_csr(device,BAR1,PKT,(MP_CHANNEL_OFFSET * channel) + MP_CONDOR_PACKET_OFFSET);

            /*Write The channel ID*/
            M23_mp_write_csr(device,BAR1,(chan.chanID) | BROADCAST_MASK,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ID_OFFSET);

            /*Setup the Watch Words*/
            /*Write the Lock Interval in 25 nanosecond ticks, 40,000 ticks per millisecond*/
            CSR = 40000 * chan.m_WatchWordLockIntervalInMilliseconds;
            M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_WATCH_WORD_TIMEOUT_OFFSET);

            /*Write The Mask and the Pattern*/
            CSR = (chan.m_WatchWordMask << 16);
            CSR |= (chan.m_WatchWordPattern & chan.m_WatchWordMask);
            M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_WATCH_WORD_OFFSET);

            /*Write To clear the sticky bit*/
            M23_mp_write_csr(device,BAR1,0x0,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_WATCH_WORD_EVENT_OFFSET);


            /*Setup the RT*/
            if( chan.chanID == config->M1553_RT_Control)
            {
                M23_mp_write_csr(device,BAR1,channel,MP_M1553_RT_SELECTION);
            }

            if( (chan.FilterType == FILTERING_NONE ) || (chan.NumFiltered == 0) )
            {
                /*I need to clear out all the Table entries for stream 1 and 2*/
                if(debug)
                    printf("No Filter, so Include ALL\r\n");
                M23_IncludeAll(device,channel,0);
            }

           /*Now Setup any Filtering that needs to be done for this channel*/
            if(chan.NumFiltered > 0 )
            {
                if( chan.FilterType == EXCLUDE) //Setup to Include all, will exclude on the FLY
                {
                    M23_IncludeAll(device,channel,0);
                }
                else
                {
                    M23_ExcludeAll(device,channel,0);
                }


                for(l = 0; l < chan.NumFiltered;l++)
                {
                    FILTER = (M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET) & 0xFFFF0000);

                    rt = chan.FilteredRT[l];
                    sa = chan.FilteredSA[l];
                    tr = chan.FilteredTR[l];

                    if(rt == 0xFF)
                    {
                        if(tr == 0xFF)
                        {
                            if(sa == 0xFF)
                            {
                                for(i = 0; i < 32;i++)
                                {
                                    for(j = 0; j < 2;j++)
                                    {
                                        for(k = 0; k < 32;k++)
                                        {
                                            FILTER |= ( (i <<11) | (j << 10) | (k << 5) ) & 0xFFE0;
                                            if(chan.FilterType == INCLUDE)
                                            {
                                                FILTER |= MP_CHANNEL_FILTER_INCLUDE;
                                            }
                                            else
                                            {
                                                FILTER &= ~MP_CHANNEL_FILTER_INCLUDE;
                                            }
                                            FILTER |= 0x1;
                                            FILTER |= MP_CHANNEL_FILTER_ERRORS;
                                            FILTER |= MP_CHANNEL_FILTER_WRITE_ENABLE;
                                            M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                                        }
                                    }
                                }
                            }
                            else /*RT = X, TR = X SA = 0-31*/
                            {
                                for(i = 0; i < 32;i++)
                                {
                                    for(j = 0; j < 2;j++)
                                    {
                                        FILTER |= ( (i <<11) | (j << 10) | (sa << 5) ) & 0xFFE0;
                                        if(chan.FilterType == INCLUDE)
                                        {
                                            FILTER |= MP_CHANNEL_FILTER_INCLUDE;
                                        }
                                        else
                                        {
                                            FILTER &= ~MP_CHANNEL_FILTER_INCLUDE;
                                        }
                                        FILTER |= 0x1;
                                        FILTER |= MP_CHANNEL_FILTER_ERRORS;
                                        FILTER |= MP_CHANNEL_FILTER_WRITE_ENABLE;
                                        M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                                    }
                                }
                            }
                        }
                        else /* TR = 0,1 */
                        {
                            if(sa == 0xFF) /*RT = X, TR 0,1, SA = X*/
                            {
                                for(i = 0; i < 32;i++)
                                {
                                    for(k = 0; k < 32;k++)
                                    {
                                        FILTER |= ( (i <<11) | (tr << 10) | (k << 5) ) & 0xFFE0;
                                        if(chan.FilterType == INCLUDE)
                                        {
                                            FILTER |= MP_CHANNEL_FILTER_INCLUDE;
                                        }
                                        else
                                        {
                                            FILTER &= ~MP_CHANNEL_FILTER_INCLUDE;
                                        }
                                        FILTER |= 0x1;
                                        FILTER |= MP_CHANNEL_FILTER_ERRORS;
                                        FILTER |= MP_CHANNEL_FILTER_WRITE_ENABLE;
                                        M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                                    }
                                }
                            }
                            else /*RT = X, TR 0,1 SA 0-31*/
                            {
                                for(i = 0; i < 32;i++)
                                {
                                    FILTER |= ( (i <<11) | (tr << 10) | (sa<< 5) ) & 0xFFE0;
                                    if(chan.FilterType == INCLUDE)
                                    {
                                        FILTER |= MP_CHANNEL_FILTER_INCLUDE;
                                    }
                                    else
                                    {
                                        FILTER &= ~MP_CHANNEL_FILTER_INCLUDE;
                                    }
                                    FILTER |=  0x1;
                                    FILTER |= MP_CHANNEL_FILTER_ERRORS;
                                    FILTER |= MP_CHANNEL_FILTER_WRITE_ENABLE;
                                    M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                                }
                            }
                        }
                    }
                    else /*RT != X*/
                    {
                        if(tr == 0xFF)
                        {
                            if(sa == 0xFF)
                            {
                                for(i = 0; i < 2; i++)
                                {
                                    for(j = 0; j <  32; j++)
                                    {
                                        FILTER |= ( (rt <<11) | (i << 10) | (j<< 5) ) & 0xFFE0;
                                        if(chan.FilterType == INCLUDE)
                                        {
                                            FILTER |= MP_CHANNEL_FILTER_INCLUDE;
                                        }
                                        else
                                        {
                                            FILTER &= ~MP_CHANNEL_FILTER_INCLUDE;
                                        }
                                        FILTER |= 0x1;
                                        FILTER |= MP_CHANNEL_FILTER_ERRORS;
                                        FILTER |= MP_CHANNEL_FILTER_WRITE_ENABLE;
                                        M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                                    }
                                }
                            }
                            else /*RT - 0-31 TR = X SA - 0-31*/
                            {
                                for(i = 0; i < 2; i++)
                                {
                                    FILTER |= ( (rt <<11) | (i << 10) | (sa<< 5) ) & 0xFFE0;
                                    if(chan.FilterType == INCLUDE)
                                    {
                                        FILTER |= MP_CHANNEL_FILTER_INCLUDE;
                                    }
                                    else
                                    {
                                        FILTER &= ~MP_CHANNEL_FILTER_INCLUDE;
                                    }
                                    FILTER |= 0x1;
                                    FILTER |= MP_CHANNEL_FILTER_ERRORS;
                                    FILTER |= MP_CHANNEL_FILTER_WRITE_ENABLE;
                                    M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                                }
                            }
                        }
                        else
                        {
                            if(sa == 0xFF)
                            {
                                for(j = 0; j <  32; j++)
                                {
                                    FILTER |= ( (rt <<11) | (tr << 10) | (j<< 5) ) & 0xFFE0;
                                    if(chan.FilterType == INCLUDE)
                                    {
                                        FILTER |= MP_CHANNEL_FILTER_INCLUDE;
                                    }
                                    else
                                    {
                                        FILTER &= ~MP_CHANNEL_FILTER_INCLUDE;
                                    }
                                    FILTER |= 0x1;
                                    FILTER |= MP_CHANNEL_FILTER_ERRORS;
                                    FILTER |= MP_CHANNEL_FILTER_WRITE_ENABLE;
                                    M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                                }
                            }
                            else
                            {
                                FILTER |= ( (rt <<11) | (tr << 10) | (sa<< 5) ) & 0xFFE0;
                                if(chan.FilterType == INCLUDE)
                                {
                                    FILTER |= MP_CHANNEL_FILTER_INCLUDE;
                                }
                                else
                                {
                                    FILTER &= ~MP_CHANNEL_FILTER_INCLUDE;
                                }
                                FILTER |=  0x1;
                                FILTER |= MP_CHANNEL_FILTER_ERRORS;
                                FILTER |= MP_CHANNEL_FILTER_WRITE_ENABLE;
                                M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);

                                CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET);
                                if(CSR != FILTER)
                                {
                                    M23_mp_write_csr(device,BAR1,FILTER,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_FILTER_OFFSET); //Exclude All Messages
                                }
                            }
                        }
                    }
                }
            }
        }
        /*Now Setup the CONDOR core*/

        /*Set the BBM Current Pointer*/
        CSR = (0x108 << 16);
        CSR |= 0x800;
        CSR |= WRITE_COMMAND;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CONDOR_OFFSET);

        usleep(1);

        CSR = (0x109 << 16);
        CSR |= 0x000;
        CSR |= WRITE_COMMAND;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CONDOR_OFFSET);

        usleep(1);

        /*Set the BBM BUFF SIZE*/
        CSR = (0x10A << 16);
        CSR |= 0x1000;
        CSR |= WRITE_COMMAND;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CONDOR_OFFSET);

        usleep(1);

#if 0
        /*Set the time value and the late response value*/
        CSR = (0x016 << 16);
        CSR |= (0x70 << 8);
        CSR |= 0x60;
        CSR |= WRITE_COMMAND;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CONDOR_OFFSET);

        usleep(1);
#endif

        /*Set the BBM Index file register to 0*/
        CSR = (0x10B << 16);
        CSR |= 0x000;
        CSR |= WRITE_COMMAND;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CONDOR_OFFSET);

        /*Set the CM run bit*/
        CSR = (0x0 << 16);
        CSR |= 0x0C08;
        CSR |= WRITE_COMMAND;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CONDOR_OFFSET);

        /*Setup the Events for this Channel*/
        index = 0;
        for(i = 0; i < MAX_NUM_EVENTS;i++)
        {
            if(M1553Events[i].ChannelID == chan.chanID)
            {
                BusEvents[channel -1][index] = i;

                if( M1553Events[i].EventType == EVENT_TYPE_MEAS_LIMIT)
                {
                    if( (dataConversion+M1553Events[i].ConversionIndex)->NumOperands == 1)
                    {
                        M1553Events[i].IsTrigger = 1;
                    }

                    (dataConversion+M1553Events[i].ConversionIndex)->HighThreshold = 0;
                    (dataConversion+M1553Events[i].ConversionIndex)->LowThreshold = 0;

                    if(0)
                    {
                        if(M1553Events[i].EventMode == 5) //HIGH Limit
                        {
                            LowerTable = (((UINT16)(dataConversion+M1553Events[i].ConversionIndex)->HighLimit << 16) & 0xFFFF0000);
                        }
                        else //Low Limit
                        {
                            LowerTable = (((UINT16)(dataConversion+M1553Events[i].ConversionIndex)->LowLimit << 16) & 0xFFFF0000);
                        }
                    }
                    else
                    {
                        if(M1553Events[i].MultiEvent == 1)
                        {
                            LowerTable = ((M1553Events[i].TotalMask << 16) & 0xFFFF0000);
                        }
                        else
                        {
                            LowerTable = ((M1553Events[i].Mask << 16) & 0xFFFF0000);
                        }
                    }
                }
                else
                {
                    if(M1553Events[i].MultiEvent == 1)
                    {
                        LowerTable = ((M1553Events[i].TotalMask << 16) & 0xFFFF0000);
                    }
                    else
                    {
                        LowerTable = ((M1553Events[i].Mask << 16) & 0xFFFF0000);
                    }
                }
                LowerTable |= (UINT16)M1553Events[i].EventNum;
                if(M1553Events[i].EventNum == PAUSE_EVENT)
                {
                    LowerTable |= MP_M1553_PAUSE_CW;
                }
                else if(M1553Events[i].EventNum == RESUME_EVENT)
                {
                    LowerTable |= MP_M1553_RESUME_CW;
                }

#if 0
                if(debug)
                {
                    printf("Table - writing 0x%x to 0x%x\r\n",LowerTable,(MP_CHANNEL_OFFSET *channel) + MP_M1553_EVENT_DESC_LOWER);
                }
#endif


                M23_mp_write_csr(device,BAR1,LowerTable,(MP_CHANNEL_OFFSET * channel) + MP_M1553_EVENT_DESC_LOWER);

                UpperTable = (M1553Events[i].CommandWord & 0xFFFF);
                UpperTable |= ((M1553Events[i].DataWordNum << 16) & 0x3F0000);
                if(M1553Events[i].CW_Only == 1)
                {
                    UpperTable |= M1553_COMMAND_WORD_ONLY;
                }

                if( (M1553Events[i].EventType == EVENT_TYPE_MEAS_DISCRETE) && (M1553Events[i].ConvType == 0) )
                {
                    if(M1553Events[i].MultiEvent == 0)
                    {
                        if(M1553Events[i].EventMode == 1) //Match UP and Down
                        {
                            UpperTable |= (0x3 << 24);
                        }
                        else if(M1553Events[i].EventMode == 2) //Match UP
                        {
                            UpperTable |= (0x1 << 24);
                        }
                        else if(M1553Events[i].EventMode == 3) //Match Down
                        {
                            UpperTable |= (0x2 << 24);
                        }
                    }
                    else
                    {
                        UpperTable |= M1553_EVENT_CAPTURED;
                        UpperTable |= (0x7 << 24);
                    }
                }
                else
                {
                    if( M1553Events[i].EventType == EVENT_TYPE_MEAS_LIMIT)
                    {

                        if(M1553Events[i].NotCapturedYet == 1)
                        {
                            UpperTable |= M1553_EVENT_CAPTURED;
                        }

                        if(M1553Events[i].EventMode == 5) //HIGH Limit
                        {
                            UpperTable |= (0x7 << 24);
                        }
                        else //Low Limit
                        {
                            UpperTable |= (0x6 << 24);
                        }
                    }
                    else //This is a Discrete limit conversion
                    {
                        if(M1553Events[i].NotCapturedYet == 1)
                        {
                            UpperTable |= M1553_EVENT_CAPTURED;
                            UpperTable |= (0x7 << 24);
                        }
                    }

                }

                if(M1553Events[i].EventContinue == 1)
                {
                    UpperTable |= MP_M1553_EVENT_CONT;
                }

                if(M1553Events[i].EventEnable == 1)
                {
                    UpperTable |= M1553_EVENT_ENABLED;
                }
                if(M1553Events[i].Capture1 == 1)
                {
                    UpperTable |= M1553_EVENT_CAPTURE1;
                }

#if 0
                if(debug)
                {
                    printf("Table - writing 0x%x to 0x%x\r\n",UpperTable,(MP_CHANNEL_OFFSET *channel) + MP_M1553_EVENT_DESC_UPPER);
                }
#endif

                M23_mp_write_csr(device,BAR1,UpperTable,(MP_CHANNEL_OFFSET * channel) + MP_M1553_EVENT_DESC_UPPER);

#if 0
                if(debug)
                {
                    printf("Table Address 0x%x to 0x%x\r\n",(TableEntryNumber[channel] | M1553_WRITE_TABLE),(MP_CHANNEL_OFFSET * channel) + MP_M1553_EVENT_TABLE_ADDRESS);
                }
#endif
                M23_mp_write_csr(device,BAR1,(TableEntryNumber[channel] | M1553_WRITE_TABLE),(MP_CHANNEL_OFFSET * channel) + MP_M1553_EVENT_TABLE_ADDRESS);
                M1553Events[i].TableEntry = TableEntryNumber[channel];
                TableEntryNumber[channel]++;

                M1553Events[i].LowerEntry = LowerTable;
                M1553Events[i].UpperEntry = UpperTable;

                M1553Events[i].BusNumber = channel;

                index++;
            }
        }
         /*Now Setup for Time Sync*/
         if(config->timecode_chan.m1553_timing.IsEnabled)
         {
             if(config->timecode_chan.m1553_timing.ChannelId == chan.chanID )
             {
                 if(config->timecode_chan.m1553_timing.Format == F15_EGI)
                 {
                     M23_M1553_SetTiming(device,channel);
                 }
                 else if(config->timecode_chan.m1553_timing.Format == B1B_EGI)
                 {
                     M23_M1553_SetTiming(device,channel);
                 }
                 else if(config->timecode_chan.m1553_timing.Format == B52_EGI)
                 {
                     M23_M1553_SetTiming(device,channel);
                 }


             }
         }
    }

}


void M23_MP_Initialize_DM_Channel(int device,int channel,DISCRETE_CHANNEL chan)
{
    int    i;
    int    debug;
    UINT32 CSR    = 0;


    M23_GetDebugValue(&debug);

    if(MP_Device_Exists[device])
    {
        if(chan.isEnabled)
        {
            /*Set the Enable Bit*/
            CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
            M23_mp_write_csr(device,BAR1,CSR | MP_CHANNEL_ENABLE,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
 
            M23_mp_write_csr(device,BAR1,(chan.chanID | BROADCAST_MASK),(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ID_OFFSET);

            if(chan.DiscreteMask == 0)
            {
                chan.DiscreteMask = 0xFFFF;
            }

            /*Write the Mask*/
            if(debug)
                printf("Writing 0x%x to 0x%x\r\n",chan.DiscreteMask,(MP_CHANNEL_OFFSET * channel) + DMP_DISCRETE_OFFSET);

            M23_mp_write_csr(device,BAR1,chan.DiscreteMask,(MP_CHANNEL_OFFSET * channel) + DMP_DISCRETE_OFFSET);
        }
    }

}


int M23_MP_GetEventNum(int chan,int index)
{
    int debug;

    M23_GetDebugValue(&debug);
    return(BusEvents[chan][index]);
}

void M23_MP_TurnOnRelativeTime(int device)
{
    UINT32 CSR;
    int    i;
    int    loops;

    if(MP_Device_Exists[device])
    {
        /*Set the Channel 0  Enable Bit*/
        CSR = M23_mp_read_csr(device,BAR1,MP_CHANNEL_ENABLE_OFFSET);
        M23_mp_write_csr(device,BAR1,CSR | MP_CHANNEL0_ENABLE, MP_CHANNEL_ENABLE_OFFSET);

        if( (M23_MP_M1553_4_Channel( device)) || (M23_MP_IS_DM(device)) )
        {
            loops = 5;
        }
        else
        {
            loops = 9;
        }

        CSR = (0x018 << 16);
        CSR |= 0x0001;
        CSR |= WRITE_COMMAND;
        for(i = 1; i < loops;i++)
        {
            M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * i) + MP_CONDOR_OFFSET);
            usleep(1);
        }

        CSR = 0;
        CSR |= (0x018 << 16);
        CSR |= WRITE_COMMAND;
        for(i = 1; i < loops;i++)
        {
            M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * i) + MP_CONDOR_OFFSET);
            usleep(1);
        }

        CSR = (0x005 << 16);
        CSR |= 0x008A;
        CSR |= WRITE_COMMAND;
        for(i = 1; i < loops;i++)
        {
            M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * i) + MP_CONDOR_OFFSET);
            usleep(1);
        }
        
    }
}

int M23_MP_M1553_4_Channel(int device)
{
    int i;

    for(i = 0;i < MP_M1553_4_Index;i++)
    {
        if(device == MP_M1553_4_Board[i])
        {
            return(1);
        }
    }

    return(0);
}

int M23_MP_PCM_4_Channel(int device)
{
    int i;

    for(i = 0;i < MP_PCM_4_Index;i++)
    {
        if(device == MP_PCM_4_Board[i])
        {
            return(1);
        }
    }

    return(0);
}

int M23_MP_IS_Video(int device)
{
    int i;

    for(i = 0; i < MP_Video_Index;i++)
    {
        if(MP_Video_Board[i] == device )
        {
            return(1);
        }
    }

    return(0);
}

int M23_MP_IS_PCM(int device)
{
    int i;

    if(M23_MP_PCM_4_Channel(device) )
    {
        return(1);
    }

    for(i = 0; i < MP_PCM_8_Index;i++)
    {
        if(MP_PCM_8_Board[i] == device )
        {
            return(1);
        }
    }

    return(0);
}

int M23_MP_IS_M1553(int device)
{
    int i;

    if(M23_MP_M1553_4_Channel(device))
    {
        return(1);
    }

    for(i = 0; i < MP_M1553_8_Index;i++)
    {
        if(MP_M1553_8_Board[i] == device )
        {
            return(1);
        }
    }

    return(0);
}

int M23_MP_IS_DM(int device)
{
    int i;


    for(i = 0; i < MP_DMIndex;i++)
    {
        if(MP_DM_1_4_Board[i] == device )
        {
            return(1);
        }
    }

    return(0);
}

int M23_MP_GetNumVideo()
{
    return(MP_Video_Index);
}

int M23_MP_GetNumM1553_8()
{
    return(MP_M1553_8_Index);
}

int M23_MP_GetNumM1553_4()
{
    return(MP_M1553_4_Index);
}

int M23_MP_GetNumPCM_4()
{
    return(MP_PCM_4_Index);
}

int M23_MP_GetNumPCM_8()
{
    return(MP_PCM_8_Index);
}

int M23_GetVideoDevice(int channel)
{
    int index = 0;

    if(channel < 4)
    {
        index = 0;
    }
    else if(channel < 8)
    {
        index = 1;
    }
    else if(channel < 12)
    {
        index = 2;
    }
   

    return(MP_Video_Board[index]);
}


int M23_GetM1553_4_Device(int channel)
{
    int index = 0;

    if(channel < 4)
    {
        index = 0;
    }
    else if(channel < 8)
    {
        index = 1;
    }
    else if(channel < 12)
    {
        index = 2;
    }
    else
    {
        index = 3;
    }

    return(MP_M1553_4_Board[index]);
}

int M23_GetM1553_8_Device(int channel)
{
    int index;

    if(channel < 8)
    {
        index = 0;
    }
    else if(channel < 16)
    {
        index = 1;
    }
    else if(channel < 24)
    {
        index = 2;
    }
    else
    {
        index = 3;
    }

    return(MP_M1553_8_Board[index]);
}

int M23_GetPCM_4_Device(int channel)
{
    int index;

    if(channel < 4)
    {
        index = 0;
    }
    else if(channel < 8)
    {
        index = 1;
    }
    else if(channel < 12)
    {
        index = 2;
    }
    else
    {
        index = 3;
    }

    return(MP_PCM_4_Board[index]);
}

int M23_GetPCM_8_Device(int channel)
{
    int index;

    if(channel < 8)
    {
        index = 0;
    }
    else if(channel < 16)
    {
        index = 1;
    }
    else if(channel < 24)
    {
        index = 2;
    }
    else
    {
        index = 3;
    }

    return(MP_PCM_8_Board[index]);
}

void M23_MP_Initialize()
{
    int i;
   
    for(i = 0;i< 5;i++)
    {
        MP_Device_Exists[i] = 0;
    }

    for(i = 0; i < 4;i++)
    {
        MP_PCM_4_Board[i]   = 0xFF;
        MP_PCM_8_Board[i]   = 0xFF;
        MP_M1553_4_Board[i] = 0xFF;
        MP_M1553_8_Board[i] = 0xFF;
        MP_DM_1_4_Board[i]  = 0xFF;
        MP_Video_Board[i]   = 0xFF;
    }

    MP_Video_Index = 0;
    MP_M1553_4_Index = 0;
    MP_M1553_8_Index = 0;
    MP_PCM_4_Index = 0;
    MP_PCM_8_Index = 0;

}

int M23_mp_open(int device)
{
    UINT32 CSR;
    char   device_name[40];

    memset(device_name,0x0,40);

    sprintf(device_name,"/dev/cxmp%d",device);

    MP_Device_Handles[device] = open(device_name,O_RDWR);

    if( MP_Device_Handles[device] < 0)
    {
        //printf("MP device %d does not exist\r\n",device);
        return(-1);
    }

    MP_Device_Exists[device] = 1;
    CSR = M23_mp_read_csr(device,BAR1,MP_CSR_OFFSET);

    if( (CSR & 0xFF000000) == 0)
    {
        CSR = M23_mp_read_csr(device,BAR1,MP_BOARD_TYPE_OFFSET);

        if( (CSR & 0x1F0) == 0x140 )
        {
            MP_DM_1_4_Board[MP_DMIndex] = device;
            MP_DMIndex++;
        }
        else if( (CSR & 0x0F) == 0x04 )
        {
            //printf("4 Channel PCM Board 0x%x\r\n",CSR);
            MP_PCM_4_Board[MP_PCM_4_Index] = device;
            MP_PCM_4_Index++;
        }
        else if( (CSR & 0x0F) == 0x08 )
        {
            //printf("8 Channel PCM Board 0x%x\r\n",CSR);
            MP_PCM_8_Board[MP_PCM_8_Index] = device;
            MP_PCM_8_Index++;
        }
        else if( (CSR & 0xF0) == 0x40 )
        {
            //printf("4 Channel M1553 Board 0x%x\r\n",CSR);
            MP_M1553_4_Board[MP_M1553_4_Index] = device;
            MP_M1553_4_Index++;
        }
        else if( (CSR & 0xF0) == 0x80 )
        {
            //printf("8 Channel M1553 Board 0x%x\r\n",CSR);
            MP_M1553_8_Board[MP_M1553_8_Index] = device;
            MP_M1553_8_Index++;
        }
        else //This is a video board
        {
            MP_Video_Board[MP_Video_Index] = device;
            MP_Video_Index++;
        }
    }
    else
    {
        printf("Board %d is Erased\r\n",device);
    }

    return(0);
}


/********************************************************************************************
     PCM Channel Section
 *******************************************************************************************/
void M23_MP_PCM_StartArbiter(int device)
{
    UINT32 CSR;

    /*Set the Channel 0  Enable Bit*/
    CSR = M23_mp_read_csr(device,BAR1,MP_CHANNEL_ENABLE_OFFSET);
    M23_mp_write_csr(device,BAR1,CSR | MP_CHANNEL0_ENABLE, MP_CHANNEL_ENABLE_OFFSET);
    CSR = 0x1;
    M23_mp_write_csr(device,BAR1,CSR,MP_ARBITER_GO_OFFSET);
}

void M23_MP_TurnOnRelativeTimePCM(int device)
{
    UINT32 CSR = 0x1;
    UINT32 read;
    int i;

    int end;

    if(M23_MP_PCM_4_Channel(device))
    {
        end = 5;
    }
    else
    {
        end = 9;
    }

    for(i=1;i<end;i++)
    {
    //    printf("Setting Relative Time to Offset 0x%x,device = %d\r\n",(MP_CHANNEL_OFFSET * i) + MP_PCM_TIME_OFFSET,device);
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * i) + MP_PCM_TIME_OFFSET);
        read = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * i) + MP_PCM_TIME_OFFSET);
        //printf("Reading Relative Time CSR = 0x%x\r\n",read);
    }

}


void M23_DM_TurnOnRelativeTime(int device)
{
    M23_mp_write_csr(device,BAR1,0x1,(MP_CHANNEL_OFFSET * 5) + MP_PCM_TIME_OFFSET);
}

void M23_MP_SetDAC(int device,int chan,int Data,int clock)
{
//printf("Writing DAC for device %d,channel %d,Clock 0x%x,data 0x%x ->0x%x\r\n",device,chan,clock,Data,(MP_CHANNEL_OFFSET*chan) + MP_PCM_DATA_DAC_OFFSET);
    M23_mp_write_csr(device,BAR1,clock,(MP_CHANNEL_OFFSET * chan) + MP_PCM_CLOCK_DAC_OFFSET);
    M23_mp_write_csr(device,BAR1,Data,(MP_CHANNEL_OFFSET * chan) + MP_PCM_DATA_DAC_OFFSET);

}

void M23_MP_PCM_Clear_Term(int device)
{
    UINT32 CSR   = 0;

    CSR = M23_mp_read_csr(device,BAR1,MP_BOARD_TYPE_OFFSET);
    CSR = CSR & 0x000000FF;
    M23_mp_write_csr(device,BAR1,CSR, MP_BOARD_TYPE_OFFSET);
}

void M23_MP_Initialize_PCM_Channel(int device,int channel,PCM_CHANNEL chan)
{
    int    i,j;

    int    debug;
    int    sync1;
    int    sync2;
    int    syncShift;
    int    ValShift;

    UINT16 LengthTableEntry;
    INT32  Clock;
    INT32  Data;

    UINT32 CSR   = 0;
    UINT32 Value = 0;
    UINT32 syncAdd = 0;
    float  RealValue = 0;
    INT64  Offset = 0L;
    UINT32 majors_PerPacket=0;
    UINT32 wordsPerPacket=0;
    UINT32 NumberOfMinors;
    UINT32 NumberOfMinorsTP;

    UINT32 Nominal;
    UINT32 Upper;
    UINT32 Lower;

    UINT32 TFW;
    UINT32 TMaW;
    UINT32 BMFS;
    UINT32 MFW;

    UINT16 MIPP;
    UINT16 MJPP = 0;
    UINT16 DFWP;
    UINT32 MAX_WPP;
    UINT32 TBIM;

    INT32  MFN;

    BOOL   MFE;

    double power;
    double powervalue;

    M23_GetDebugValue(&debug);
    if(chan.pcmCode != PCM_UART)
    {

        /*Write The channel ID*/
        M23_mp_write_csr(device,BAR1,(chan.chanID | BROADCAST_MASK),(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ID_OFFSET);

        /* Setup the Relative Time offset*/
        /*Lower 16 bits, lower 8 must be 0 to gurantee a multiple of 256*/

        //Offset = chan.pcmSyncLength * (10000000/(chan.pcmBitRate));
        RealValue = (71.0/(float)(chan.pcmBitRate) ) * 10000000.0 ;
        if(chan.pcmBitRate >= 1000000)
        {
            Offset = (INT64)(RealValue  + 45.0);
        }
        else
        {
            if(chan.pcmBitRate >= 500000)
            {
               Offset = (INT64)(RealValue  + 35.0);
            }
            else
            {
                Offset = (INT64)RealValue;
            }
        }
        Offset = Offset * -1;

        M23_mp_write_csr(device,BAR1,(INT32)(Offset & 0x0000FFFFF),(MP_CHANNEL_OFFSET * channel) + MP_TIME_LSB_OFFSET);
        M23_mp_write_csr(device,BAR1,(INT32)(Offset>>16) ,(MP_CHANNEL_OFFSET * channel) + MP_TIME_MSB_OFFSET);

        M23_mp_write_csr(device,BAR1,MP_CHANNEL_ENABLE_LOAD_REL_TIME,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE);

        /*Set the Enable Bit*/
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
        M23_mp_write_csr(device,BAR1,CSR | MP_CHANNEL_ENABLE | MP_LOAD_RELATIVE_TIME,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);


        /*Setup Offset 0x100*/
        if(chan.pcmInputSource == PCM_422)
        {
            CSR = MP_PCM_422;
        }
        else
        {
            CSR = MP_PCM_TTL;
        }

        if(chan.pcmDataPolarity == PCM_INVERTED)
        {
            CSR |= MP_PCM_INVERT_DATA;
        }

        if(chan.pcmClockPolarity == PCM_INVERTED)
        {
            CSR |= MP_PCM_INVERT_CLOCK;
        }


        if(chan.PCM_CRCEnable == MINOR_CRC)
        {
            CSR |= MP_PCM_CRC_CHECK;
        }

        if(chan.pcmCode == PCM_NRZL)
        {
            CSR |= (MP_PCM_NRZL << 24);
        }
        else if(chan.pcmCode == PCM_RNRZL)
        {
            if(chan.pcmDirection == PCM_NORMAL)
            {
                CSR |= (MP_PCM_RNRZL << 24);
            }
            else
            {
                CSR |= (MP_PCM_REVERSE_RNRZL << 24);
            }
        }
        else if(chan.pcmCode == PCM_BIOL)
        {
            CSR |= (MP_PCM_BIPHASE << 24);
        }

        if(chan.pcmDataPackingOption == PCM_UNPACKED)
        {
            CSR |= (MP_PCM_UNPACKED << 28);
            CSR |= MP_PCM_INTRA_PKT_HDR;

            CalcUnPackedMode_New(chan.pcmBitRate, chan.pcmWordsInMinorFrame,chan.pcmMinorFramesInMajorFrame,chan.pcmCommonWordLength,&MFE, &TFW,&TMaW,&MFW,chan.pcmSyncLength);

        }
        else if(chan.pcmDataPackingOption == PCM_PACKED)
        {
            CSR |= (MP_PCM_PACKED << 28);
            CSR |= MP_PCM_INTRA_PKT_HDR;


            CalcPackedMode(chan.pcmBitRate, chan.pcmWordsInMinorFrame,chan.pcmMinorFramesInMajorFrame,chan.pcmCommonWordLength,&MFE, &TFW,&TMaW,&MFW,chan.pcmSyncLength);

        }
        else if(chan.pcmDataPackingOption == PCM_THROUGHPUT)
        {
            CSR |= (MP_PCM_THROUGHPUT << 28);
            CalcThroughPutMode(chan.pcmBitRate, &wordsPerPacket,&NumberOfMinorsTP);


            /*Added 2-8-06, Needs a change to the TMATS Builder*/
            //chan.pcmSyncMask = 0xFFFFFFFF;
            chan.pcmSyncMask = 0x0;

        }

        if(chan.PCM_RecordFilteredSelect == FALSE)
        {
            CSR |= MP_PCM_RECORD_RAW;
        }

        if(chan.PCM_TransmitFilteredSelect == FALSE)
        {
            CSR |= MP_PCM_TRANSMIT_RAW;
        }
        if(chan.pcmBitSync == TRUE)
        {
            CSR |= MP_PCM_BIT_SYNC;
        }

        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_FRAME_TYPE_OFFSET);

        if(chan.pcmDataPackingOption == PCM_PACKED)
        {
            MAX_WPP = chan.pcmBitRate/176;
            if(MAX_WPP > 8000)
            {
               MAX_WPP = 8000;
            }

            if( MFE == TRUE)
            {
                DFWP = TMaW * (UINT16)(MAX_WPP/TMaW);
                if(debug)
                    printf("MFE = TRUE TFW = %d, MAX = %d, X = %d\r\n",TFW,MAX_WPP,(UINT16)MAX_WPP/TFW);
            }
            else
            {
                if(debug)
                    printf("TFW = %d, MAX = %d, X = %d\r\n",TFW,MAX_WPP,(UINT16)MAX_WPP/TFW);
                DFWP = TFW * (UINT16)(MAX_WPP/TFW);
            }



            if( MFE == TRUE)
            {
                MJPP = (UINT16)(MAX_WPP/TMaW);
                if(debug)
                    printf("MFE = TRUE MJPP = %d \r\n",MJPP);
                MIPP = MJPP * chan.pcmMinorFramesInMajorFrame;
            }
            else
            {
                MIPP = (UINT16)(MAX_WPP/TFW);
                MJPP = 0;
            }
        }
        else if(chan.pcmDataPackingOption == PCM_UNPACKED)
        {
            if( MFE == TRUE)
            {
                DFWP = TMaW * (UINT16)(8000/TMaW);
            }
            else
            {
                DFWP = TFW * (UINT16)(8000/TFW);
            }



            if( MFE == TRUE)
            {
                MJPP = (UINT16)(8000/TMaW);
                MIPP = MJPP * chan.pcmMinorFramesInMajorFrame;
            }
            else
            {
                MIPP = (UINT16)(8000/TFW);
                MJPP = 0;
            }
        }
        else //We are in Throughput Mode
        {
            MIPP =  NumberOfMinorsTP;
            MJPP = 0;
        }

        if(debug)
            printf("MIPP = %d\r\n",MIPP);


        /*Setup offset 0x108*/
        if(chan.pcmSFIDBitOffset == 0xFFFFFFFF)
        {
            BMFS = chan.pcmSyncLength + ((chan.pcmSFIDCounterLocation-1) * chan.pcmCommonWordLength);
        }
        else
        {
            BMFS = chan.pcmSFIDBitOffset;
        }

        CSR = (BMFS << 16);
#if 0 //Remove Per Marty on 3-14-2011
        if(chan.pcmDataPackingOption == PCM_UNPACKED)
        {
            if( (chan.pcmSyncLength != 16) || (chan.pcmSyncLength != 32) )
            {
                CSR |= (UINT16)( chan.pcmWordsInMinorFrame);
            }
            else
            {
                CSR |= (UINT16)( chan.pcmWordsInMinorFrame - 1);
            }
        }
        else
        {
            CSR |= (UINT16)( chan.pcmWordsInMinorFrame - 1);
        }
#endif
        if(debug)
            printf("Writing 0x%x to 0x%x\r\n",CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_MAIN_FRAME_WORDS_OFFSET);

        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_MAIN_FRAME_WORDS_OFFSET);

        /*Setup offset 0x10C*/
        //CSR = (chan.pcmCommonWordLength - 1) << 16;
        CSR = (chan.pcmCommonWordLength ) << 16; //Change to 1 -origin
        if( (MFE == FALSE) || (chan.pcmDataPackingOption == PCM_THROUGHPUT) )
        {
            CSR |= 0;
        }
        else
        {
            CSR |= MP_PCM_MAJOR_ALIGN;
        }

        if(chan.pcmDataPackingOption == PCM_UNPACKED)
        {
            if( (chan.pcmSyncLength != 16) || (chan.pcmSyncLength != 32) )
            {
                sync1 = chan.pcmSyncLength/2;
                sync2 = sync1;
                if(chan.pcmSyncLength % 2)  /*The sync Length is odd*/
                {
                    sync1++;
                }
                CSR |= (sync1 & 0x3F);
                CSR |= ((sync2 & 0x3F) << 8);

            }
            else
            {
                CSR |= (chan.pcmSyncLength & 0x3F);
            }
        }
        else
        {
                CSR |= (chan.pcmSyncLength & 0x3F);
        }

        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_SYNC_DEFINITION_OFFSET);

        /*Setup offset 0x110 and 0x114*/
        syncShift = 32 - chan.pcmSyncLength;
        chan.pcmSyncPattern = chan.pcmSyncPattern & chan.pcmSyncMask;
        if(debug)
            printf("Sync Pattern 0x%x to 0x%x\r\n", chan.pcmSyncPattern << syncShift,(MP_CHANNEL_OFFSET * channel) + MP_PCM_MAIN_FRAME_SYNC_PATTERN_OFFSET_MSB);

        M23_mp_write_csr(device,BAR1,chan.pcmSyncPattern << syncShift,(MP_CHANNEL_OFFSET * channel) + MP_PCM_MAIN_FRAME_SYNC_PATTERN_OFFSET_MSB);
        M23_mp_write_csr(device,BAR1,0x0,(MP_CHANNEL_OFFSET * channel) + MP_PCM_MAIN_FRAME_SYNC_PATTERN_OFFSET);

        /*Setup offset 0x118 and 0x11C*/
        if(debug)
            printf("Sync Mask 0x%x to 0x%x\r\n", chan.pcmSyncMask << syncShift,(MP_CHANNEL_OFFSET * channel) + MP_PCM_MAIN_FRAME_SYNC_MASK_OFFSET_MSB);
        M23_mp_write_csr(device,BAR1,chan.pcmSyncMask << syncShift,(MP_CHANNEL_OFFSET * channel) + MP_PCM_MAIN_FRAME_SYNC_MASK_OFFSET_MSB);
        M23_mp_write_csr(device,BAR1,0x0,(MP_CHANNEL_OFFSET * channel) + MP_PCM_MAIN_FRAME_SYNC_MASK_OFFSET);

        /*Setup offset 0x120 and 0x124*/
        if( chan.pcmMinorFramesInMajorFrame > 1)
        {
            if( chan.pcmDataPackingOption == PCM_THROUGHPUT)
            {
                M23_mp_write_csr(device,BAR1,0,(MP_CHANNEL_OFFSET * channel) + MP_PCM_SUBFRAME_SYNC_PATTERN_OFFSET);
                M23_mp_write_csr(device,BAR1,0,(MP_CHANNEL_OFFSET * channel) + MP_PCM_SUBFRAME_SYNC_MASK_OFFSET);
            }
            else
            {
                ValShift = 16 - chan.pcmSFIDLength;
                M23_mp_write_csr(device,BAR1,chan.pcmIdCounterInitialValue << ValShift,(MP_CHANNEL_OFFSET * channel) + MP_PCM_SUBFRAME_SYNC_PATTERN_OFFSET);
                M23_mp_write_csr(device,BAR1,chan.pcmIdCounterMask << ValShift,(MP_CHANNEL_OFFSET * channel) + MP_PCM_SUBFRAME_SYNC_MASK_OFFSET);
            }

        }
        else
        {
            M23_mp_write_csr(device,BAR1,0,(MP_CHANNEL_OFFSET * channel) + MP_PCM_SUBFRAME_SYNC_PATTERN_OFFSET);
            M23_mp_write_csr(device,BAR1,0,(MP_CHANNEL_OFFSET * channel) + MP_PCM_SUBFRAME_SYNC_MASK_OFFSET);
        }


        /*Setup offset 0x128*/
        CSR =  (chan.pcmMinorFramesInMajorFrame << 16);
        CSR |= (UINT16)MIPP;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_SUBFRAME_1_BIT_OFFSET);

        if(chan.pcmDataPackingOption == PCM_THROUGHPUT)
        {
            /*Setup offset 0x140*/
             CSR = wordsPerPacket;
             M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_MAJOR_FRAMES_PACKET_OFFSET);

             /*Setup offset 0x144*/
             M23_mp_write_csr(device,BAR1, CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_BIT_LENGTH_LAST_WORD);
        }
        else
        {
            /*Setup offset 0x140*/
             CSR = chan.pcmBitsInMinorFrame * chan.pcmMinorFramesInMajorFrame;
             M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_MAJOR_FRAMES_PACKET_OFFSET);

             /*Setup offset 0x144*/
             M23_mp_write_csr(device,BAR1, chan.pcmBitsInMinorFrame,(MP_CHANNEL_OFFSET * channel) + MP_PCM_BIT_LENGTH_LAST_WORD);
        }

        /*Write the Major/Monor Frame Sync Thresholds , offset 0x164*/
        //CSR  = (chan.pcmSyncPatternCriteria & 0x1f) << 24;
        //CSR |= (chan.pcmSyncPatternCriteria & 0x1f) << 16;

        CSR  = (0xF & 0x1f) << 24;
        CSR |= (0x3F & 0x3f) << 16;
        CSR |= (chan.pcmNumOfDisagrees & 0x0f) << 12;
        CSR |= (chan.pcmNumOfDisagrees & 0x0f) << 4;
        CSR |= (chan.pcmInSyncCriteria & 0x0f) << 8;
        CSR |= (chan.pcmInSyncCriteria & 0x0f);

        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_FRAME_SYNC_THRESHOLD_OFFSET);

        /*Setup The Watch word parameters*/

        /*Now Write the Mask*/
        M23_mp_write_csr(device,BAR1,chan.pcm_WatchWordMask << 16,(MP_CHANNEL_OFFSET * channel) + MP_PCM_WATCH_WORD_SFID_MASK);

        /*Now Write the Pattern*/
        M23_mp_write_csr(device,BAR1,chan.pcm_WatchWordPattern << 16,(MP_CHANNEL_OFFSET * channel) + MP_PCM_WATCH_WORD_OFFSET);

        /*Write The Watch Word Bit Offset*/
        M23_mp_write_csr(device,BAR1,chan.pcm_WatchWordOffset,(MP_CHANNEL_OFFSET * channel) + MP_PCM_WATCH_WORD_BIT_OFFSET);



#if 0 //Remove on 3-14-2011 Per Marty's request
        /*Write offset 0x12C*/
        if(chan.pcmDataPackingOption == PCM_UNPACKED)
        {
            if( (chan.pcmSyncLength != 16) || (chan.pcmSyncLength != 32) )
            {
                sync1 = chan.pcmSyncLength/2;
                sync2 = sync1;
                if(chan.pcmSyncLength % 2)  /*The sync Length is odd*/
                {
                    sync1++;
                }

                /*Write the First Table Entry which is the Sync word*/
                //CSR = (UINT16)sync1;
                LengthTableEntry = ((sync1-1) << 11);
                CSR = (UINT16)LengthTableEntry;
                CSR |= MP_PCM_WRITE_STROBE;
                M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_LENGTH_TABLE_OFFSET);

                /*Write the Second Table Entry*/
                CSR = (1<<16);
                LengthTableEntry = ((sync2-1) << 11);
                LengthTableEntry |= 0x1;
                CSR |= (UINT16)(LengthTableEntry);
                CSR |= MP_PCM_WRITE_STROBE;
                M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_LENGTH_TABLE_OFFSET);

                /*Write the Third Table Entry*/
                CSR = (2<<16);
                CSR |= (2 << 24);
                LengthTableEntry = ((chan.pcmCommonWordLength-1) << 11);
                LengthTableEntry |= (UINT16)( chan.pcmWordsInMinorFrame);
                CSR |= (UINT16)( LengthTableEntry);
                CSR |= MP_PCM_WRITE_STROBE;
                CSR |= MP_PCM_TABLE_COMPLETE;
                M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_LENGTH_TABLE_OFFSET);

            }
            else
            {
                /*Write the First Table Entry which is the Sync word*/
                CSR = ((chan.pcmSyncLength-1) << 11);
                CSR |= MP_PCM_WRITE_STROBE;
                M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_LENGTH_TABLE_OFFSET);

                /*Write the Second Table Entry*/
                CSR = (1<<16);
                CSR |= (1 << 24);
                LengthTableEntry = ((chan.pcmCommonWordLength-1) << 11);
                LengthTableEntry |= (UINT16)( chan.pcmWordsInMinorFrame - 1);
                CSR |= (UINT16)( LengthTableEntry);

                CSR |= MP_PCM_WRITE_STROBE;
                CSR |= MP_PCM_TABLE_COMPLETE;
                M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_LENGTH_TABLE_OFFSET);
                CSR |= (UINT16)( chan.pcmWordsInMinorFrame - 1);
            }
        }
        else
        {
            /*Write the First Table Entry which is the Sync word*/
            CSR = ((chan.pcmSyncLength-1) << 11);
            CSR |= MP_PCM_WRITE_STROBE;
            M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_LENGTH_TABLE_OFFSET);

            /*Write the Second Table Entry*/
            CSR = (1<<16);
            CSR |= (1 << 24);
            LengthTableEntry = ((chan.pcmCommonWordLength - 1) << 11);
            LengthTableEntry |= (UINT16)( chan.pcmWordsInMinorFrame - 1);
            CSR |= (UINT16)( LengthTableEntry);
            CSR |= MP_PCM_WRITE_STROBE;
            CSR |= MP_PCM_TABLE_COMPLETE;
            M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_LENGTH_TABLE_OFFSET);
        }
#endif

        /*Write the Filter Table at 0x134*/
        //Case 1 Filtering is Not Enabled
        if(chan.PCM_FilteringEnabled == FILTERING_NONE)
        {
            for(i = 0; i < 32;i++)
            {
                M23_mp_write_csr(device,BAR1,0x0,(MP_CHANNEL_OFFSET * channel) + MP_PCM_FILTER_TABLE_WRITE_OFFSET);
                CSR = (i << 16);
                CSR |= MP_PCM_WRITE_STROBE;
                if(i == 31)
                {
                    CSR |= MP_PCM_TABLE_COMPLETE;
                }
                M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_FILTER_TABLE_OFFSET);
            }
        }
        else if(chan.PCM_FilteringEnabled == INCLUDE)
        {
            //Case 2 Filtering is INCLUDE,All other message are not filtered only the ones in this list;
            for(i = 0;i< 32;i++)
            {
                CSR = 0x0;
                for(j = 0;j < 32;j++)
                {
                    if(chan.PCM_WordNumber[j + (i*32)] != 0) //Include this message
                    {
                        CSR |= (1<<j);
                    }
                }

                M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_FILTER_TABLE_WRITE_OFFSET);

                CSR = (i << 16);
                CSR |= MP_PCM_WRITE_STROBE;
                if(i == 31)
                    CSR |= MP_PCM_TABLE_COMPLETE;
                M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_FILTER_TABLE_OFFSET);
            }
        }
        else if(chan.PCM_FilteringEnabled == EXCLUDE)
        {
            //Case 3 Filtering is EXCLUDE,All messages are filtered except the ones in the list;
            for(i = 0;i< 32;i++)
            {
                CSR = 0xFFFFFFFF;
                for(j = 0;j < 32;j++)
                {
                    if(chan.PCM_WordNumber[j + (i*32)] != 0) //Include this message
                    {
                        CSR &= ~(1<<j);
                    }
                }

                M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_FILTER_TABLE_WRITE_OFFSET);

                CSR = (i << 16);
                CSR |= MP_PCM_WRITE_STROBE;
                if(i == 31)
                    CSR |= MP_PCM_TABLE_COMPLETE;
                M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_FILTER_TABLE_OFFSET);
            }
        }


        /*We can go ahead and set the Bit Rate Modulus here*/
        RealValue = (float)180000000/(float)chan.pcmBitRate;

        if((RealValue - (int)RealValue) < 0.5)
        {
            Nominal = (int)RealValue;
        }
        else
        {
            Nominal = (int)RealValue + 1;
        }

        RealValue = (float)Nominal;
        Upper = Nominal + (UINT32)(RealValue * 0.20);

        RealValue = (float)Nominal;
        Lower = Nominal - (UINT32)(RealValue * 0.20);

        if((Nominal - Lower) < 2)
            Lower = Nominal - 2;

        M23_mp_write_csr(device,BAR1,Lower,(MP_CHANNEL_OFFSET * channel) + MP_PCM_LOWER_BIT_RATE_MODULUS);
        M23_mp_write_csr(device,BAR1,Upper,(MP_CHANNEL_OFFSET * channel) + MP_PCM_UPPER_BIT_RATE_MODULUS);
        M23_mp_write_csr(device,BAR1,Nominal,(MP_CHANNEL_OFFSET * channel) + MP_PCM_NOMINAL_BIT_RATE_MODULUS);


        /*Set up the Precision MASK*/
        Nominal = 2*(chan.pcmBitRate/100);
        //Lower = myLog2Ceil(Nominal);
        Lower = myLog2(Nominal);
        Upper = CEILING_POS(Lower);
        Nominal = 1;
        for(i = 0; i < Upper;i++)
        {
            Nominal = Nominal * 2;
        }

        Nominal = Nominal - 1;
        if(debug)
            printf("Writing 0x%x to 0x%x\r\n",Nominal,(MP_CHANNEL_OFFSET * channel) + MP_PCM_CLOCK_PRECISION_MASK);
        M23_mp_write_csr(device,BAR1,Nominal,(MP_CHANNEL_OFFSET * channel) + MP_PCM_CLOCK_PRECISION_MASK);

        /*Write the expected bit rate*/
        M23_mp_write_csr(device,BAR1,chan.pcmBitRate,(MP_CHANNEL_OFFSET * channel) + MP_PCM_EXPECTED_BIT_RATE);


        /*Set the DAC Values*/
        M23_GetPCM_DAC(channel - 1,&Clock,&Data);
        M23_MP_SetDAC( device,channel,Data,Clock);


       if(chan.PCM_RecordEnable == FALSE)
       {
           CSR = MP_PCM_DISABLE_RECORD;
       if(debug)
           printf("PCM Pausing Channel %d\r\n", chan.chanID);
           M23_SetHealthBit(chan.chanID,M23_PCM_CHANNEL_PAUSED);
       }
       else
       {
           CSR = 0x0;
           M23_ClearHealthBit(chan.chanID,M23_PCM_CHANNEL_PAUSED);
       }
       if(debug)
           printf("Writing 0x%x to 0x%x\r\n",CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_RECORD_ENABLE);

       M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_RECORD_ENABLE);

    }
    else
    {
        /*Write The channel ID*/
        M23_mp_write_csr(device,BAR1,(chan.chanID | BROADCAST_MASK),(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ID_OFFSET);

        /*Set the Enable Bit*/
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
        M23_mp_write_csr(device,BAR1,CSR | MP_CHANNEL_ENABLE | MP_LOAD_RELATIVE_TIME,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);

        CSR = MP_PCM_422;
        CSR |= MP_PCM_TRANSFER_LSB;
        CSR |= (MP_PCM_UART << 24);
        CSR |= MP_PCM_INTRA_PKT_HDR;
        if(debug)
            printf("Writing 0x%x to 0x%x\r\n",CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_FRAME_TYPE_OFFSET);

        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_PCM_FRAME_TYPE_OFFSET);

        /*Set the Data Bits, Offset 0x180*/
        CSR = 0;
        if(chan.UARTDataBits == 7)
        {
            CSR = MP_UART_7_BIT;
        }

        if(chan.UARTParity == PARITY_EVEN)
        {
            CSR |= (MP_UART_PARITY_EVEN << 4);
        }
        else if(chan.UARTParity == PARITY_ODD)
        {
            CSR |= (MP_UART_PARITY_ODD << 4);
        }

        switch(chan.UARTBaudRate)
        {
            case 300:
                CSR |= MP_UART_BAUD_RATE_300;
                break;
            case 1200:
                CSR |= MP_UART_BAUD_RATE_1200;
                break;
            case 2400:
                CSR |= MP_UART_BAUD_RATE_2400;
                break;
            case 4800:
                CSR |= MP_UART_BAUD_RATE_4800;
                break;
            case 9600:
                CSR |= MP_UART_BAUD_RATE_9600;
                break;
            case 19200:
                CSR |= MP_UART_BAUD_RATE_19200;
                break;
            case 38400:
                CSR |= MP_UART_BAUD_RATE_38400;
                break;
            case 57600:
                CSR |= MP_UART_BAUD_RATE_57600;
                break;
            case 115200:
                CSR |= MP_UART_BAUD_RATE_115200;
                break;
            case 230400:
                CSR |= MP_UART_BAUD_RATE_230400;
                break;
            case 460800:
                CSR |= MP_UART_BAUD_RATE_460800;
                break;
            default:
                CSR |= MP_UART_BAUD_RATE_300;
                break;
        }
if(debug)
printf("UART Writing 0x%x to 0x%x\r\n",CSR,(MP_CHANNEL_OFFSET * channel) + MP_UART_CONFIG_OFFSET);

            M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_UART_CONFIG_OFFSET);

    }
}

int M23_mp_write_csr(int device_number,int bar,int Value,int offset)
{
    int rv;
    struct mp_buf_S buf;

    if(MP_Device_Exists[device_number])
    {

        buf.ptr = &Value;         //Value To Write 
        buf.size = 4;             // bytes to read 
        buf.offset = offset;             // starting offset 
        buf.access_size = 4;        // b/w/l == 1/2/4
        buf.increment = 1;          // same address or increment by access size
        buf.bar = bar;              // BAR to read [0 -> 5]
        rv = ioctl( MP_Device_Handles[device_number], MP_IOC_WRITE, &buf );

        if( rv == -1 ) 
        {
            perror( "ioctl" );
            return(-1);
        }

    }

    return(0);

}


int M23_mp_read(int device_number,int size,int bar,int offset)
{
    int i;
    int count = 0;
    int rv;
    int lines = 0;
    struct mp_buf_S buf;

    memset( tmpbuf, 0, sizeof(tmpbuf) );

    if(MP_Device_Exists[device_number])
    {
        buf.ptr = &tmpbuf[0];       // buffer for results
        buf.size = size;             // bytes to read 
        buf.offset = offset;             // starting offset 
        buf.access_size = 4;        // b/w/l == 1/2/4
        buf.increment = 1;          // same address or increment by access size
        buf.bar = bar;              // BAR to read [0 -> 5]
        rv = ioctl( MP_Device_Handles[device_number], MP_IOC_READ, &buf );

        if( rv == -1 ) 
        {
            perror( "ioctl" );
            return(-1);
        }
        else
        {
            printf("%02x - ",(lines *4) + offset);
            lines += 8;
            for(i=0;i<size;i+=4)
            {
                printf("%08x ", *(unsigned long*)&tmpbuf[i]);
                if( count == 7)
                {
                    printf("\r\n%02x - ",(lines*4) + offset);
                    lines += 8;
                    count = 0;
                }
                else
                    count++;
            }
            if(count < 7)
                printf("\r\n");
        }

    }

     return(0);

}

int M23_mp_read_csr(int device_number,int bar,int offset)
{

    int rv;
    struct mp_buf_S buf;
    int CSR;

    if(MP_Device_Exists[device_number])
    {
        memset( tmpbuf, 0, sizeof(tmpbuf) );

        buf.ptr = &CSR;       // buffer for results
        buf.size = 4;             // bytes to read 
        buf.offset = offset;             // starting offset 
        buf.access_size = 4;        // b/w/l == 1/2/4
        buf.increment = 1;          // same address or increment by access size
        buf.bar = bar;              // BAR to read [0 -> 5]
        rv = ioctl( MP_Device_Handles[device_number], MP_IOC_READ, &buf );

        if( rv == -1 ) 
        {
            perror( "ioctl" );
            return(-1);
        }

        return(CSR);

    }
 
    return(0);

}

BOOL M23_Wait_Till_Burned(int device,UINT32 address)
{
    unsigned long i,j;
    UINT32 CSR = 0;

    for(i = 0; i < TIMEOUT;i++)
    {
        j=i;
        CSR = M23_mp_read_csr(device,BAR1,MP_FLASH_READ_OFFSET);
        if( !(CSR & 0x100) )
        {
            break;
        }
    }

    if(j != i)
    {
      printf("Never Completed: 0x%x\n",CSR);
      return(FALSE);
    }

    return(TRUE);


}

void M23_MP_UnlockFlash(int device,UINT32 address,int bypass)
{
    unsigned long Value = 0;;

    /*Load The address*/
    address = 0;
    //Value = address << 8;

    /*Set the Write Bit*/
    Value |= WRITE_COMMAND;

    /*Set the First Unlock value*/
    Value |= 0x000000AA;

    //Value = SWAP_FOUR(Value);
    //printf("First Unlocl = 0x%x\n",Value);
    M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);

    /*Set the Second Unlock value*/

    /*First Clear the previous Value*/
    //Value = SWAP_FOUR(Value);
    Value &= 0xFFFFFF00;

    Value |= 0x00000055;

    //Value = SWAP_FOUR(Value);
    //printf("Second Unlock = 0x%x\n",Value);
    M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);

    if(bypass)
    {
        /*Set the Write Bit*/
        Value = WRITE_COMMAND;

        /*Set the First Unlock value*/
        Value |= 0x00000020;
        M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);
    }

}

void M23_MP_Validate(int device,int count)
{
    UINT32        address;
    UINT8         byte;
    int           Value;
    int           i;
    FILE          *fp;

    printf("Validate %d bytes\n",count);
    fp = fopen("validate.bin","wb");
    for(i = 0; i < count;i++)
    {
        Value = 0;
        address = (START_PROGRAM_SECTOR_ADDRESS + i);
        //printf("Read from Address 0x%x\n",address);
        /*Load The address*/
        Value = address << 8;
        //Value = SWAP_FOUR(Value);
        M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);

        Value = M23_mp_read_csr(device,BAR1,MP_FLASH_READ_OFFSET);
        byte = Value;
        fwrite(&byte,1,1,fp);
    }

    if(fp)
        fclose(fp);

}

void M23_MP_ValidateFile(int device,int count,UINT32 address,UINT8 *Buffer)
{
    UINT8         byte;
    int           Value;
    int           i;

    printf("Validating\t");
    for(i = 0; i < count;i++)
    {
        Value = 0;
        //address = (START_PROGRAM_SECTOR_ADDRESS + i);
        Value = address << 8;
        M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);
        Value = M23_mp_read_csr(device,BAR1,MP_FLASH_READ_OFFSET);
        byte = *(Buffer + i);
        if (byte != Value) {
            printf("Validate failed at 0x%x: Flash=0x%x, File=0x%x\n",count,Value,byte);
            break;
        }
        if ( i % SIZE_OF_SECTORS == 0)
        {
            fflush(stdout);
            printf(".");
        }
        address++;
    }
    printf(" Complete.\r\n");
}

BOOL M23_MP_EraseSectors(int device)
{

    BOOL          Ok = FALSE;
    UINT32        address = START_PROGRAM_SECTOR_ADDRESS;
    int           Value;
    int           i;

    printf("\r\nErase Begin:Starting Sector 0x%x\r\n",address);

    for(i = 0; i < NUMBER_OF_PROGRAMMABLE_SECTORS;i++)
    {
        address = START_PROGRAM_SECTOR_ADDRESS + (i * SIZE_OF_SECTORS);

        /*Unlock the FLASH*/
        M23_MP_UnlockFlash(device,address,0);

        /*Set the Third Cycle Value*/
        /*Load The address*/
        Value = 0;

        /*Set the Write Bit*/
        Value |= WRITE_COMMAND;

        /*Set the First Unlock value*/
        Value |= 0x00000080;

        M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);

        /*This will set the fourth and Fifth cycles*/
        M23_MP_UnlockFlash(device,address,0);

        /*Now Set the sector erase command*/

        Value = address << 8;
        Value |= WRITE_COMMAND;                         // Added LWB 2/14/2005
        Value |= 0x00000030;

        M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);

        sleep(1);

        /*Wait for Completion*/
        Ok = M23_Wait_Till_Burned(device,address);
        printf(".");
        fflush(stdout);
    }

    printf("\nErase %d sectors\r\n",i);

    return Ok;



}


BOOL M23_MP_Erase(int device, unsigned long start, unsigned int size)
{
    BOOL          Ok = FALSE;
    UINT32        address = start;
    int           Value;
    int           i;

    printf("\nErasing\t\t");
    //for(i = 0; i < 1;i++)
    for(i = 0; i < size;i++)
    {
        address = start + (i * SIZE_OF_SECTORS);

        /*Unlock the FLASH*/
        M23_MP_UnlockFlash(device,address,0);

        /*Set the Third Cycle Value*/
        /*Load The address*/
        Value = 0;

        /*Set the Write Bit*/
        Value |= WRITE_COMMAND;

        /*Set the First Unlock value*/
        Value |= 0x00000080;

        //Value = SWAP_FOUR(Value);
        //printf("Third Cycle 0x%x\n",Value);
        M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);

        /*This will set the fourth and Fifth cycles*/
        M23_MP_UnlockFlash(device,address,0);

        /*Now Set the sector erase command*/

        Value = address << 8;
        Value |= WRITE_COMMAND;                         // Added LWB 2/14/2005
        Value |= 0x00000030;

        //Value = SWAP_FOUR(Value);
        //printf("Set the Erase 0x%x\n",Value);
        M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);

        usleep(900000);

        /*Wait for Completion*/
        Ok = M23_Wait_Till_Burned(device,address);
        printf(".");
        fflush(stdout);
    }
    printf(" Complete.\r\n");

    return Ok;
}


void M23_ResetToRead(int device)
{
    int       Value;

    Value = 0x000000F0;

    /*Set the Write Bit*/
    Value |= WRITE_COMMAND;

    //Value = SWAP_FOUR(Value);
    M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);
}

void M23_MP_ProgramFlash(int device,UINT32 address,UINT32 size,UINT8 *Buffer)
{
    int       i;
    int       Value;
    BOOL      Ok;
    UINT8     byte;
    UINT32    start = address;
    UINT32    NumBytes = size;


    M23_ResetToRead(device);
    printf("Size of file: %d\t", size);
    size = (size / SIZE_OF_SECTORS) + 1;   // Guarantees that enough are erased.
    printf("Number of sectors: %d\r\n", size);

    /*First Erase the Sectors that we will use to program*/
    Ok = M23_MP_Erase(device, address, size);

    M23_MP_UnlockFlash(device,address,1);

    if(Ok == TRUE)
    {

        /*Start the Write*/
        printf("Programming\t");

        for(i = 0;i < NumBytes;i++)
        {
            Value = 0x000000A0;
            /*Set the Write Bit*/
            Value |= WRITE_COMMAND;

           // Value = SWAP_FOUR(Value);
            M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);

            /*Now Program the logic byte by byte*/


            /*Set the Write Bit*/
            Value = WRITE_COMMAND;
            /*Set the Address*/
            Value |= address  << 8;

            /*Read the Byte*/
            byte = *(Buffer + i);

            Value |= byte;

            //Value = SWAP_FOUR(Value);
            M23_mp_write_csr(device,BAR1,Value,MP_FLASH_OFFSET);


            Ok = M23_Wait_Till_Burned(device,address);

            if ( i % SIZE_OF_SECTORS == 0)
            {
                fflush(stdout);
                printf(".");
            }

            if ( Ok == FALSE )
                printf("Failed @ %d\n", i);

            address++;

        }
        printf(" Complete.\r\n");

        M23_ResetToRead(device);
        M23_MP_ValidateFile(device,--i,start,Buffer);
    }
    else
    {
        printf("Sector Erase Failed\r\n");
    }

}


/************************************************************************************************
 * This is where we will Setup The Calculex  Video board
 ***********************************************************************************************/
int M23_AllowVideoI2c()
{
    int NumMPBoards;
    int i;

    UINT32 CSR;

    M23_NumberMPBoards(&NumMPBoards);

    for(i = 0; i < NumMPBoards;i++)
    {

        if(M23_MP_IS_Video(i) )
        {
            CSR = M23_mp_read_csr(i,BAR1,MP_VIDEO_I2C_OFFSET);
            CSR |= MP_VIDEO_ALLOW_I2C;
            M23_mp_write_csr(i,BAR1,CSR,MP_VIDEO_I2C_OFFSET);
        }
    }

    return 0;
}

int M23_RemoveVideoI2c()
{
    int NumMPBoards;
    int i;

    UINT32 CSR;


    M23_NumberMPBoards(&NumMPBoards);

    for(i = 0; i < NumMPBoards;i++)
    {

        if(M23_MP_IS_Video(i) )
        {
            CSR = M23_mp_read_csr(i,BAR1,MP_VIDEO_I2C_OFFSET);
            CSR &= ~MP_VIDEO_ALLOW_I2C;
            M23_mp_write_csr(i,BAR1,CSR,MP_VIDEO_I2C_OFFSET);
        }
    }

    return 0;
}


int M23_InitializeVideoChannels(VIDEO_CHANNEL *p_video,int channel,int index,int device)
{

    int chan;

    //printf("Attribute for Channel %d,%d\n",channel,index);
    /* Retrieve these values from TMATS*/
    if(channel > 4)
    {
        chan = channel - 4;
    }
    else
    {
        chan = channel;
    }


    /*Get the type of Input - Composite or S-VIDEO*/
    if(p_video->videoInput == VIDEO_COMP_VIDINPUT)
    {
        M23_I2C_SetVideoMode(index,0); //set composite
    }
    else
    {
        M23_I2C_SetVideoMode(index,1); //set SVIDEO
    }

    /* Need to Check Actual Bit Rates*/
    switch(p_video->internalClkRate)
    {
        case VIDEO_2_0M_CLOCKRATE :
            M2300_I2C_SetMPEG_Bitrate(index,2000); //In Kb/s 0x1000 = 4096 bit/s
            break;
        case VIDEO_3_0M_CLOCKRATE:
            M2300_I2C_SetMPEG_Bitrate(index,3000); //In Kb/s 0x1000 = 4096 bit/s
            break;
        case VIDEO_4_0M_CLOCKRATE:
            M2300_I2C_SetMPEG_Bitrate(index,4000); //In Kb/s 0x1000 = 4096 bit/s
            break;
        case VIDEO_6_0M_CLOCKRATE:
            M2300_I2C_SetMPEG_Bitrate(index,6000); //In Kb/s 0x1000 = 4096 bit/s
            break;
        case VIDEO_8_0M_CLOCKRATE:
            M2300_I2C_SetMPEG_Bitrate(index,8000); //In Kb/s 0x1000 = 4096 bit/s
            break;
        case VIDEO_10_0M_CLOCKRATE:
            M2300_I2C_SetMPEG_Bitrate(index,10000); //In Kb/s 0x1000 = 4096 bit/s
            break;
        case VIDEO_12_0M_CLOCKRATE:
            M2300_I2C_SetMPEG_Bitrate(index,12000); //In Kb/s 0x1000 = 4096 bit/s
            break;
        case VIDEO_15_0M_CLOCKRATE:
            M2300_I2C_SetMPEG_Bitrate(index,15000); //In Kb/s 0x1000 = 4096 bit/s
            break;
        default:
            M2300_I2C_SetMPEG_Bitrate(index,4000); //In Kb/s 0x1000 = 4096 bit/s
            break;
    }

        /* Defaulting To GOP size of 3 to try and make CH.10 timing*/
    switch(p_video->videoCompression)
    {
        /* define video compression mode */
        case VIDEO_I_VIDCOMP:
            M2300_I2C_SetMPEG_GOP(index,0,1);
            break;
        case VIDEO_IP_15_VIDCOMP:
            M2300_I2C_SetMPEG_GOP(index,1,15);
            //IPBB16M2300_I2C_SetMPEG_GOP(index,3,16);
            break;
        default:
            M2300_I2C_SetMPEG_GOP(index,1,15);
            break;
    }

    if(p_video->isEnabled)
    {
        if(p_video->videoMode == 0)
        {
            M23_I2C_DisableVideo(index);
        }

        if(p_video->audioMode == 0)
        {
            M23_I2C_DisableAudio(index);
        }
    }

    return(0);
}



void M23_MP_StopVideoChannels(int device,int channel)
{
    UINT32 CSR;

    /*Clear the Enable Bit*/
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
    M23_mp_write_csr(device,BAR1,CSR & ~MP_CHANNEL_ENABLE,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
}

void M23_MP_SetAudioTone()
{
    int    num_video;
    int    i;
    int    j;
    UINT32 CSR;
    M23_CHANNEL const  *chan;

    SetupGet( &chan );


    num_video = M23_MP_GetNumVideo();

    for(i = 0;i<num_video;i++)
    {
        for(j = 1;j < 5;j++)
        {
            CSR = M23_mp_read_csr(MP_Video_Board[i],BAR1,(MP_CHANNEL_OFFSET * j) + MP_AUDIO_SETUP);
            if(chan->video_chan[i].audioSourceR == AUDIO_EVENT_TONE)
            {
                CSR &= ~MP_RIGHT_AUDIO_MUTE;
                CSR |= MP_RIGHT_AUDIO_TONE;
            }

            if(chan->video_chan[i].audioSourceL == AUDIO_EVENT_TONE)
            {
                CSR &= ~MP_LEFT_AUDIO_MUTE;
                CSR |= MP_LEFT_AUDIO_TONE;
            }

            M23_mp_write_csr(MP_Video_Board[i],BAR1,CSR ,(MP_CHANNEL_OFFSET * j) + MP_AUDIO_SETUP);
        }

        CSR = M23_mp_read_csr(MP_Video_Board[i],BAR1,MP_CHANNEL_ENABLE_OFFSET);
        M23_mp_write_csr(MP_Video_Board[i],BAR1,CSR | VIDEO_AUDIO_TONE,MP_CHANNEL_ENABLE_OFFSET);
    }

}

void M23_MP_ClearAudioTone()
{
    int    num_video;
    int    i;
    int    j;
    UINT32 CSR;

    M23_CHANNEL const  *chan;

    SetupGet( &chan );


    num_video = M23_MP_GetNumVideo();


    for(i = 0;i<num_video;i++)
    {
        for(j = 1; j < 5;j++)
        {
            CSR = M23_mp_read_csr(MP_Video_Board[i],BAR1,(MP_CHANNEL_OFFSET * j) + MP_AUDIO_SETUP);

            if(chan->video_chan[i].audioSourceR == AUDIO_EVENT_TONE)
            {
                CSR |= MP_RIGHT_AUDIO_MUTE;
                CSR &= ~MP_RIGHT_AUDIO_TONE;
            }

            if(chan->video_chan[i].audioSourceL == AUDIO_EVENT_TONE)
            {
                CSR &= ~MP_LEFT_AUDIO_TONE;
                CSR |= MP_LEFT_AUDIO_MUTE;
            }

            M23_mp_write_csr(MP_Video_Board[i],BAR1,CSR ,(MP_CHANNEL_OFFSET * j) + MP_AUDIO_SETUP);
        }

        CSR = M23_mp_read_csr(MP_Video_Board[i],BAR1,MP_CHANNEL_ENABLE_OFFSET);
        CSR &= ~VIDEO_AUDIO_TONE;
        M23_mp_write_csr(MP_Video_Board[i],BAR1,CSR,MP_CHANNEL_ENABLE_OFFSET);
    }
}


void M23_MP_Video_StartArbiter(int device)
{
    UINT32 CSR;

    /*Set the Channel 0  Enable Bit, First Clear the FIFO's*/

    /*Reset the Cascase FIFO*/

    CSR = M23_mp_read_csr(device,BAR1,MP_VIDEO_CHAN0_OFFSET);
    M23_mp_write_csr(device,BAR1,MP_VIDEO_CHAN0_OFFSET,CSR | MP_VIDEO_RESET_FIFO);
    CSR = M23_mp_read_csr(device,BAR1,MP_CHANNEL_ENABLE_OFFSET);
    M23_mp_write_csr(device,BAR1,CSR | MP_CHANNEL0_ENABLE, MP_CHANNEL_ENABLE_OFFSET);
    CSR = 0x1;
    M23_mp_write_csr(device,BAR1,CSR,MP_ARBITER_GO_OFFSET);
}

void M23_MP_TurnOnRelativeTimeVideo(int device)
{
    UINT32 CSR = 0x1;

#if 0
    UINT32 read;
    int i;
    for(i=1;i<2;i++)
    {
        //printf("Setting Relative Time to Offset 0x%x,device = %d\r\n",(MP_CHANNEL_OFFSET * i) + MP_PCM_TIME_OFFSET,device);
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * i) + MP_VIDEO_TIME_OFFSET);
        read = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * i) + MP_VIDEO_TIME_OFFSET);
        //printf("Reading Relative Time CSR = 0x%x\r\n",read);
    }
#endif

    M23_mp_write_csr(device,BAR1,CSR,MP_VIDEO_RELATIVE_TIME_GO_OFFSET);

}

void M23_MP_ResetVideoChannel(int device,int channel,int video_index)
{
    int      slot;
    UINT32   CSR;

    //device = M23_GetVideoDevice(channel - 1);

    /*This Clears the Enable Bit for this channel*/
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
    M23_mp_write_csr(device,BAR1,CSR & ~MP_CHANNEL_ENABLE & ~(1<<4),(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);

    /*Setup the channel*/ 
    slot =  (1 << (device + 1) );

    M2300_I2C_Init_VideoChannel(slot,channel);

    /*We now need to Set the Enable bit*/
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
    M23_mp_write_csr(device,BAR1,CSR | MP_CHANNEL_ENABLE,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);

    /*Set the Need sync for this channel*/
    ChannelNeedsSync((channel - 1) + (4 * video_index) );

}

void M23_MP_Initialize_Video_I2c(int device,int video_index)
{
    int     slot;
    int     NumAudio;

    slot =  (1 << (device + 1) );

    TwoAudio[video_index] = M2300_I2C_Get_VideoBoard_Version(slot,device);

    if(TwoAudio[video_index] == 4)
    {
        //printf("Four Audio Video Board\r\n");
        NumAudio = 4;
    }
    else
    {
        //printf("Two Audio Video Board\r\n");
        M23_mp_write_csr(device,BAR1,MP_TWO_AUDIO_BIT,0x20);
        NumAudio = 2;
    }

    M2300_I2C_Init_VideoBoard(slot,device,video_index,NumAudio);

}
void M23_MP_Set_Video_Encode(int device,int video_index)
{
    int slot;

    slot =  (1 << (device + 1) );

 M2300_I2C_VideoBoard_SetEncode(slot,device,video_index);
}

void M23_MPVideoOut(int chan)
{
    M23_mp_write_csr(0,BAR1,chan,0x114);
}



void M23_MPSetSync(int device,int channel)
{
    UINT32 CSR;
    int    debug;

    M23_GetDebugValue(&debug);

    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_UPSTREAM_SELECT);
    CSR |= MP_SYNC_BIT;
    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_UPSTREAM_SELECT);

#if 0
    if(debug)
        printf("Setting Sync -> 0x%x @ 0x%x\r\n",CSR,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_UPSTREAM_SELECT);
#endif

    /*Turn Off Verbose*/
    CSR = M23_mp_read_csr(device,BAR1,0x24);
    if(CSR & MP_VIDEO_VERBOSE)
    {
        //printf("Writing 0x%x to 0x24\r\n",CSR);
        M23_mp_write_csr(device,BAR1,CSR,0x24);
    }

}

void M23_MPClearSync(int device,int channel)
{
    UINT32 CSR;

    channel++;
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_UPSTREAM_SELECT);
    CSR &= ~MP_SYNC_BIT;
    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_UPSTREAM_SELECT);
}

void M23_MPSetReSync(int device,int channel)
{
    UINT32 CSR;

    channel++;
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_UPSTREAM_SELECT);
    CSR |= MP_RESYNC_BIT;
    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_UPSTREAM_SELECT);
}

void M23_MPClearReSync(int device,int channel)
{
    UINT32 CSR;

    channel++;
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_UPSTREAM_SELECT);
    CSR &= ~MP_RESYNC_BIT;
    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_UPSTREAM_SELECT);
}

/******************************************************************************************
 *
 *  This will handle to operations when a Slip is detected
 *
 ******************************************************************************************/
void M23_MPHandleSlip(int channel)
{
    int device;
    int count = 0;
    int state;
    int slot;


    device = M23_GetVideoDevice(channel);
    
    slot = (1 << (device + 1) );

    //Stop the Compressor
     M23_I2C_StopCompressor(channel,slot);

    //Wait until compressor has stopped
    while(1)
    {
        state = M23_I2C_GetCompressorState(channel,slot);

        if(state & 0x4)  //Compressor is stopped
        {
            break;
        }
        else
        {
            count++;
 
            if(count == 20)
            {
                break;
            }
        }
   
        usleep(100);
    }

    //reconfig the Compressor
    M23_I2C_ReconfigCompressor(channel,slot);

    //Wait until compressor to go back to idle
    while(1)
    {
        state = M23_I2C_GetCompressorState(channel,slot);

        if(state & 0x1)  //Compressor is idle
        {
            break;
        }
        else
        {
            count++;
 
            if(count == 20)
            {
                break;
            }
        }
   
        usleep(100);
    }

    //Start the Compressor
     M23_I2C_StopCompressor(channel,slot);

#if 0
    //Clear Sync Bit
    M23_MPClearSync(device,channel);
#endif

    //Set Resync Bit
    M23_MPSetReSync(device,channel);

    //Clear Resync Bit
    M23_MPClearReSync(device,channel);



}

void M23_MP_Reset_Video_Channel(int device,int channel)
{
    int    count;
    int    debug;
    UINT32 CSR;
   
    M23_GetDebugValue(&debug);

    /*Write The channel ID*/
    M23_mp_write_csr(device,BAR1,MP_VIDEO_RESET,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_CHANNEL_RESET);

    count = 0;
 
    while(1)
    {
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_CHANNEL_RESET);
        if(CSR & MP_VIDEO_RESET) //Reset Not Complete
        {
            if(count > 100)
            {
                if(debug)
                    printf("Video Reset for Channel %d never cleared\r\n",channel);

                break;
            }
            else
            {
                usleep(100000);
                count++;
            }
        }
        else
        {
            if(debug)
                printf("Reset Complete for Channel %d,0x%x\r\n",channel,CSR);

            break;
        }
    }

}

void M23_MP_Initialize_Video_Channel(int device,int channel,VIDEO_CHANNEL chan,int index)
{
    UINT32 CSR;
    int    setsync = 0;

    int    Left;
    int    Right;

    int    debug;


    M23_CHANNEL  const *config;


    SetupGet(&config);

    M23_GetDebugValue(&debug);

    /*Write The channel ID*/
    M23_mp_write_csr(device,BAR1,(chan.chanID | BROADCAST_MASK),(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ID_OFFSET);

    /*Set the Enable Bit*/
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);
    M23_mp_write_csr(device,BAR1,CSR | MP_CHANNEL_ENABLE | MP_LOAD_RELATIVE_TIME,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_ENABLE_OFFSET);



    if(chan.videoMode == 0)
    {
        if(chan.audioMode == 1)
        {
            M23_mp_write_csr(device,BAR1,MP_AUDIO_ONLY,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_UPSTREAM_SELECT);
        }
    }

    CSR = MP_RIGHT_AUDIO_MUTE | MP_LEFT_AUDIO_MUTE;
    if(TwoAudio[index] == 4)
    {
        CSR = channel;
        CSR |= LEFT_LEFT_RIGHT_RIGHT;
        CSR |= 0x110000;

        if( chan.audioSourceL == AUDIO_NONE)
        {
            CSR |= MP_LEFT_AUDIO_MUTE;
        }

        if( chan.audioSourceR == AUDIO_NONE)
        {
            CSR |= MP_RIGHT_AUDIO_MUTE;
        }
    }
    else
    {

        if( (chan.audioSourceL == AUDIO_LOCAL) && (chan.audioSourceR == AUDIO_LOCAL) )
        {
            if(channel == 2)
                CSR = 1;
            else if(channel == 4)
                CSR = 3;
            else
            CSR = channel;
            
            //CSR = channel;
            CSR |= LEFT_LEFT_RIGHT_RIGHT;
	    CSR |= 0x110000;

            if( chan.audioSourceL == AUDIO_NONE)
            {
                CSR |= MP_LEFT_AUDIO_MUTE;
            }

            if( chan.audioSourceR == AUDIO_NONE)
            {
                CSR |= MP_RIGHT_AUDIO_MUTE;
            }

        }
        else
        {
            if(config->NumConfiguredVoice == 0)
            {
                Left  = 0xFF;
                Right = 0xFF;
            }
            else if(config->NumConfiguredVoice == 1)
            {
                //Left  = config->voice_chan[0].chanID;
                //Right = Left;

                Right  = config->voice_chan[0].chanID;
                Left = Right;
            }
            else
            {
                //Left   = config->voice_chan[0].chanID;
                //Right  = config->voice_chan[1].chanID;

                Right   = config->voice_chan[0].chanID;
                Left    = config->voice_chan[1].chanID;
            }
        
            if( (chan.audioSourceL == Left) && (chan.audioSourceR == Right) )
            {
                CSR = LEFT_LEFT_RIGHT_RIGHT;
            }
            else if( (chan.audioSourceL == Left) && (chan.audioSourceR == Left) )
            {
                CSR = LEFT_LEFT_RIGHT_LEFT;
            }
            else if( (chan.audioSourceL == Right) && (chan.audioSourceR == Left) )
            {
                CSR = LEFT_RIGHT_RIGHT_LEFT;
            }
            else if( (chan.audioSourceL == Right) && (chan.audioSourceR == Right) )
            {
                CSR = LEFT_RIGHT_RIGHT_RIGHT;
            }
            else if( (chan.audioSourceL == Left) && (chan.audioSourceR == AUDIO_NONE) )
            {
                CSR = LEFT_LEFT_RIGHT_RIGHT;
                CSR |= MP_RIGHT_AUDIO_MUTE;
            }
            else if( (chan.audioSourceL == Right) && (chan.audioSourceR == AUDIO_NONE) )
            {
                CSR = LEFT_RIGHT_RIGHT_LEFT;
                CSR |= MP_RIGHT_AUDIO_MUTE;
            }
            else if( (chan.audioSourceL == AUDIO_NONE) && (chan.audioSourceR == Right) )
            {
                CSR = LEFT_LEFT_RIGHT_RIGHT;
                CSR |= MP_LEFT_AUDIO_MUTE;
            }
            else if( (chan.audioSourceL == AUDIO_NONE) && (chan.audioSourceR == Left) )
            {
                CSR = LEFT_RIGHT_RIGHT_LEFT;
                CSR |= MP_LEFT_AUDIO_MUTE;
            }
        }
    }

//printf("Writing 0x%x to 0x%x,Left %d, Right %d\r\n",CSR,(MP_CHANNEL_OFFSET * channel)+MP_AUDIO_SETUP,chan.audioSourceL,chan.audioSourceR);

    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_AUDIO_SETUP);

    CSR = 0;
    /*Set Overlay Parameters*/
    if(chan.OverlaySize == LARGE_FONT)
    {
        CSR |= MP_VIDEO_LARGE_CHAR;
    }


    if(chan.Overlay_Time == TIME_ONLY)
    {
        CSR |= MP_VIDEO_TIME_ONLY;
        CSR |= MP_VIDEO_NO_MS;
    }
    else if(chan.Overlay_Time == TIME_DAY)
    {
        CSR |= MP_VIDEO_NO_MS;
    }
    else if(chan.Overlay_Time == TIME_MS)
    {
        CSR |= MP_VIDEO_TIME_ONLY;
    }
  

    if(chan.Overlay_Format == WHITE_TRANS)
    {
        CSR |= MP_VIDEO_CLEAR_BACK;
        CSR &= ~MP_VIDEO_BLACK_ON_WHITE;

    }
    else if(chan.Overlay_Format == BLACK_TRANS)
    {
        CSR |= MP_VIDEO_CLEAR_BACK;
        CSR |= MP_VIDEO_BLACK_ON_WHITE;
    }
    else if(chan.Overlay_Format == WHITE_BLACK)
    {
        CSR &= ~MP_VIDEO_BLACK_ON_WHITE;
        CSR &= ~MP_VIDEO_CLEAR_BACK;
    }
    else
    {
        CSR |= MP_VIDEO_BLACK_ON_WHITE;
        CSR &= ~MP_VIDEO_CLEAR_BACK;
    }

    /*Add Overlay Trans here*/
    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY);


    CSR = 0;
    CSR = chan.Overlay_X & 0x07FF;
    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY_X);

    CSR = 0;
    CSR = chan.Overlay_Y & 0x03FF;
    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY_Y);

    if(chan.Overlay_Generate == GENERATE_OFF)
    {
        CSR = 0;
    }
    else
    {
        setsync = 1;
        CSR = MP_VIDEO_TEST_PATTERN;
    }



    M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY_GO);

    /*Set the Audio Passthru values*/
    //M23_mp_write_csr(device,BAR1,0x61E,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_BOBBLE_MASK);
    M23_mp_write_csr(device,BAR1,0x21E,(MP_CHANNEL_OFFSET * channel) + MP_VIDEO_BOBBLE_MASK);


    M23_MPSetSync(device,channel);

#if 0
    if(setsync)
    {
        M23_MPSetSync(device,channel);
    }
#endif


}

void M23_SetFilterGo()
{
    int     NumMPBoards;
    int    i;
    int    j;
    int    debug;
    int    video_index = 0;

    UINT32 CSR;

    M23_CHANNEL  const *config;

    SetupGet(&config);

    M23_NumberMPBoards(&NumMPBoards);
    M23_GetDebugValue(&debug);

//printf("Configure M1553 Channels\r\n");
    for(i = 0; i < NumMPBoards;i++)
    {

        if(  M23_MP_IS_Video(i) )
        {

            for(j=0;j<4;j++)            
            {                
                if(config->video_chan[j + (4*video_index)].isEnabled)
                {
                    CSR = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_OVERLAY_GO);

                    CSR |= MP_VIDEO_FILTER_GO;
                    //CSR |= MP_VIDEO_FILTER_BAR;
                   //if(config->VideoOverlayIsEnabled)
                   //{
                    //   CSR |= MP_VIDEO_OVERLAY_GO;
                   //}

                   if(config->video_chan[j + (4*video_index)].OverlayEnable == OVERLAY_ON)
                   {
                       CSR |= MP_VIDEO_OVERLAY_GO;
                   }

#if 0
                    if(debug)
                    {
                        printf("Writing 0x%x to 0x%x\r\n",CSR,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_OVERLAY_GO);
                    }
#endif

                    M23_mp_write_csr(i,BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_OVERLAY_GO);
                }
            }
            video_index++;
        }
    }
}

void M23_ClearFilterGo()
{
    int     NumMPBoards;
    int    i;
    int    j;
    int    debug;
    int    video_index = 0;

    UINT32 CSR;

    M23_CHANNEL  const *config;

    SetupGet(&config);

    M23_NumberMPBoards(&NumMPBoards);
    M23_GetDebugValue(&debug);

//printf("Configure M1553 Channels\r\n");
    for(i = 0; i < NumMPBoards;i++)
    {

        if(  M23_MP_IS_Video(i) )
        {

            for(j=0;j<4;j++)            
            {                
                if(config->video_chan[j + (4*video_index)].isEnabled)
                {
                    CSR = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_OVERLAY_GO);

                    CSR &= ~MP_VIDEO_FILTER_GO;

                   if(config->video_chan[j + (4*video_index)].OverlayEnable == OVERLAY_ON)
                   {
                       CSR |= MP_VIDEO_OVERLAY_GO;
                   }

                    M23_mp_write_csr(i,BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_OVERLAY_GO);
                }
            }
            video_index++;
        }
    }
}

void M23_StartUncompressedVideo(int channel)
{
    int device;
    int numVideo;
    int other;
    int chan;

    UINT32 CSR;

    numVideo =  M23_MP_GetNumVideo();

    if( (channel > 4) && (numVideo < 2) )
    {
        return;
    }
    else if( (channel > 9) && (numVideo < 3) )
    {
        return;
    }
    else if( (channel > 13) && (numVideo < 4) )
    {
        return;
    }
    else if(numVideo != 0)
    {
        CSR = M23_ReadControllerCSR(CONTROLLER_PLAYBACK_CONTROL);
        M23_WriteControllerCSR(CONTROLLER_PLAYBACK_CONTROL,CSR | CONTROLLER_UPSTREAM_DATA |CONTROLLER_UPSTREAM_CLOCK);

        if(channel != 0)
        {
            device = M23_GetVideoDevice(channel - 1);

            if(  (channel > 4) && (channel < 9) )
            {
                chan = channel - 4;
            }
            else
            {
                chan = channel;
            }
            /*Now Write The Video and Audio Out Channel*/
            M23_mp_write_csr(device,BAR1,chan,MP_CHANNEL_VIDEO_OUT);
            //M23_mp_write_csr(device,BAR1,channel,MP_CHANNEL_AUDIO_OUT);

            if(numVideo > 1)
            {
                if(channel > 4)
                {
                    /*Set pass thru on downstream board*/
                    other = 0;
                    device = M23_GetVideoDevice(other);
                    M23_mp_write_csr(device,BAR1,MP_VIDEO_DOWNSTREAM,MP_CHANNEL_VIDEO_OUT);
                    //M23_mp_write_csr(device,BAR1,MP_AUDIO_DOWNSTREAM,MP_CHANNEL_AUDIO_OUT);
                }
            }
            CSR = M23_ReadControllerCSR(0x0);
            if(CSR != 0x354012)
            {

                /*Now Reset the CIRRUS Chip*/
                CSR = M23_ReadControllerCSR(CONTROLLER_PLAYBACK_CONTROL);
                M23_WriteControllerCSR(CONTROLLER_PLAYBACK_CONTROL,CSR | CONTROLLER_RESET_CIRRUS);
                CSR = M23_ReadControllerCSR(CONTROLLER_PLAYBACK_CONTROL);
                M23_WriteControllerCSR(CONTROLLER_PLAYBACK_CONTROL,CSR & ~CONTROLLER_RESET_CIRRUS);
                M23_InitializeControllerRGBI2c();
            }

         
        }
        else //Turn Off Video Out
        {
            other = 0;
            device = M23_GetVideoDevice(other);
            M23_mp_write_csr(device,BAR1,MP_VIDEO_OFF,MP_CHANNEL_VIDEO_OUT);
            //M23_mp_write_csr(device,BAR1,MP_AUDIO_OFF,MP_CHANNEL_AUDIO_OUT);
        }

    }


}

void M23_ReverseVideo()
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    debug;
    int    video_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    M23_GetDebugValue(&debug);

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);


    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_Video(i) )
        {
            for(j=0;j<4;j++)            
            {                
                if(config->video_chan[j + (4*video_index)].isEnabled)
                {
                    if(config->video_chan[j + (4*video_index)].Overlay_Event_Toggle == EVENT_TOGGLE_ON)
                    {
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_OVERLAY);

                        if(CSR & MP_VIDEO_BLACK_ON_WHITE)
                        {
                            CSR &= ~MP_VIDEO_BLACK_ON_WHITE;
                        }
                        else
                        {
                            CSR |= MP_VIDEO_BLACK_ON_WHITE;
                        }

                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_OVERLAY);
                    }
                }
            }
            video_index++;
        }

    }

}

/************************************************************************************************************
 *             The Following will be used to PAUSE Recording of channels                                    *
 ************************************************************************************************************/

void M23_PauseAllVideo()
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    video_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);


    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_Video(i) )
        {
            for(j=0;j<4;j++)            
            {                
                if(config->video_chan[j + (4*video_index)].isEnabled)
                {
                    /*This Clears the Enable Bit for this channel*/
                    CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + 0x24);
                    CSR |= MP_VIDEO_PAUSE;
                    M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + 0x24);
                    M23_SetHealthBit(config->video_chan[j+(4*video_index)].chanID,M23_VIDEO_CHANNEL_CHANNEL_PAUSED);
                }
            }
            video_index++;
        }

    }
}

void M23_ResumeAllVideo()
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    video_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);


    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_Video(i) )
        {
            for(j=0;j<4;j++)            
            {                
                if(config->video_chan[j + (4*video_index)].isEnabled)
                {
                    CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + 0x24);
                    CSR &= ~MP_VIDEO_PAUSE;
                    M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + 0x24);
                    M23_ClearHealthBit(config->video_chan[j+(4*video_index)].chanID,M23_VIDEO_CHANNEL_CHANNEL_PAUSED);
                }
            }
            video_index++;
        }

    }
}


void M23_PauseAllM1553()
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    m1553_4_index = 0;
    int    m1553_8_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_M1553(i) )
        {
            if(M23_MP_M1553_4_Channel(i))
            {
                for(j=0;j<4;j++)
                {
                    if(config->m1553_chan[j + (4*m1553_4_index)].isEnabled)
                    {
                        /*This Clears the Enable Bit for this channel*/
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);

                        CSR |= MP_M1553_PAUSE;
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                        M23_SetHealthBit(config->m1553_chan[j+(4*m1553_4_index)].chanID,M23_M1553_CHANNEL_PAUSED);

                    }
                }
                m1553_4_index++;
            }
            else
            {
                for(j=0;j<8;j++)
                {
                    if(config->m1553_chan[j + (8*m1553_8_index)].isEnabled)
                    {
                        /*This Clears the Enable Bit for this channel*/
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);

                        CSR |= MP_M1553_PAUSE;
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                        M23_SetHealthBit(config->m1553_chan[j+(8*m1553_8_index)].chanID,M23_M1553_CHANNEL_PAUSED);
                    }
                }
                m1553_8_index++;
            }
        }
    }
}

void M23_PauseAllHealth()
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    m1553_4_index = 0;
    int    m1553_8_index = 0;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_M1553(i) )
        {
            if(M23_MP_M1553_4_Channel(i))
            {
                for(j=0;j<4;j++)
                {
                    if(config->m1553_chan[j + (4*m1553_4_index)].isEnabled)
                    {
                        M23_SetHealthBit(config->m1553_chan[j+(4*m1553_4_index)].chanID,M23_M1553_CHANNEL_PAUSED);

                    }
                }
                m1553_4_index++;
            }
            else
            {
                for(j=0;j<8;j++)
                {
                    if(config->m1553_chan[j + (8*m1553_8_index)].isEnabled)
                    {
                        M23_SetHealthBit(config->m1553_chan[j+(8*m1553_8_index)].chanID,M23_M1553_CHANNEL_PAUSED);
                    }
                }
                m1553_8_index++;
            }
        }
    }
}

void M23_ResumeAllM1553()
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    m1553_4_index = 0;
    int    m1553_8_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_M1553(i) )
        {
            if(M23_MP_M1553_4_Channel(i))
            {
                for(j=0;j<4;j++)
                {
                    if(config->m1553_chan[j + (4*m1553_4_index)].isEnabled)
                    {
                        /*This Sets the Enable Bit for this channel*/
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                        CSR &= ~MP_M1553_PAUSE;
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                        M23_ClearHealthBit(config->m1553_chan[j+(4*m1553_4_index)].chanID,M23_M1553_CHANNEL_PAUSED);

                    }
                }
                m1553_4_index++;
            }
            else
            {
                for(j=0;j<8;j++)
                {
                    if(config->m1553_chan[j + (8*m1553_8_index)].isEnabled)
                    {
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                        CSR &= ~MP_M1553_PAUSE;
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR ,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                        M23_ClearHealthBit(config->m1553_chan[j+(8*m1553_8_index)].chanID,M23_M1553_CHANNEL_PAUSED);
                    }
                }
                m1553_8_index++;
            }
        }
    }
}

void M23_ResumeAllHealth()
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    m1553_4_index = 0;
    int    m1553_8_index = 0;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_M1553(i) )
        {
            if(M23_MP_M1553_4_Channel(i))
            {
                for(j=0;j<4;j++)
                {
                    if(config->m1553_chan[j + (4*m1553_4_index)].isEnabled)
                    {
                        M23_ClearHealthBit(config->m1553_chan[j+(4*m1553_4_index)].chanID,M23_M1553_CHANNEL_PAUSED);

                    }
                }
                m1553_4_index++;
            }
            else
            {
                for(j=0;j<8;j++)
                {
                    if(config->m1553_chan[j + (8*m1553_8_index)].isEnabled)
                    {
                        M23_ClearHealthBit(config->m1553_chan[j+(8*m1553_8_index)].chanID,M23_M1553_CHANNEL_PAUSED);
                    }
                }
                m1553_8_index++;
            }
        }
    }
}

void M23_PauseAllAnalog()
{
    int    i;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);

    for(i = 0;i < config->NumConfiguredVoice;i++)
    {
        /*Setup the Voice Channel*/
        if(config->voice_chan[i].isEnabled)
        {
            if(i == 0)
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR);
                CSR |= CONTROLLER_VOICE_PAUSE;
                M23_WriteControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR,CSR);
            }
            else
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR);
                CSR |= CONTROLLER_VOICE_PAUSE;
                M23_WriteControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR,CSR);
            }

            M23_SetHealthBit(config->voice_chan[i].chanID,M23_VOICE_CHANNEL_PAUSED);
        }
    }

}

void M23_ResumeAllAnalog()
{
    int    i;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);

    for(i = 0;i < config->NumConfiguredVoice;i++)
    {
        /*Setup the Voice Channel*/
        if(config->voice_chan[i].isEnabled)
        {
            if(i == 0)
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR);
                CSR &= ~CONTROLLER_VOICE_PAUSE;
                M23_WriteControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR,CSR );
            }
            else
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR);
                CSR &= ~CONTROLLER_VOICE_PAUSE;
                M23_WriteControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR,CSR);
            }

            M23_ClearHealthBit(config->voice_chan[i].chanID,M23_VOICE_CHANNEL_PAUSED);
        }
    }

}

void M23_PauseM1553Channel(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    m1553_4_index = 0;
    int    m1553_8_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_M1553(i) )
        {
            if(M23_MP_M1553_4_Channel(i))
            {
                for(j=0;j<4;j++)
                {
                    if(config->m1553_chan[j + (4*m1553_4_index)].chanID == Id)
                    {
                        if(config->m1553_chan[j + (4*m1553_4_index)].isEnabled)
                        {
                            /*This Clears the Enable Bit for this channel*/
                            CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                            M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR |  MP_M1553_PAUSE,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                            M23_SetHealthBit(config->m1553_chan[j+(4*m1553_4_index)].chanID,M23_M1553_CHANNEL_PAUSED);
                        }
                    }
                }
                m1553_4_index++;
            }
            else
            {
                for(j=0;j<8;j++)
                {
                    if(config->m1553_chan[j + (8*m1553_8_index)].chanID == Id)
                    {
                        if(config->m1553_chan[j + (8*m1553_8_index)].isEnabled)
                        {
                            /*This Clears the Enable Bit for this channel*/
                            CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                            M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR | MP_M1553_PAUSE,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);

                            M23_SetHealthBit(config->m1553_chan[j+(8*m1553_8_index)].chanID,M23_M1553_CHANNEL_PAUSED);
                        }
                    }
                }
                m1553_8_index++;
            }
        }
    }
}

void M23_PauseVideoChannel(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    video_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);


    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_Video(i) )
        {
            for(j=0;j<4;j++)            
            {                
                if(config->video_chan[j + (4*video_index)].chanID == Id)
                {
                    if(config->video_chan[j + (4*video_index)].isEnabled)
                    {
                        /*This Clears the Enable Bit for this channel*/
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + 0x24);
                        CSR |= MP_VIDEO_PAUSE;
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + 0x24);
                    }

                    M23_SetHealthBit(config->video_chan[j+(4*video_index)].chanID,M23_VIDEO_CHANNEL_CHANNEL_PAUSED);
                }
            }
            video_index++;
        }

    }
}

void M23_PauseAnalogChannel(int Id)
{
    int    i;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);

    for(i = 0;i < config->NumConfiguredVoice;i++)
    {
        /*Setup the Voice Channel*/
        if(config->voice_chan[i].chanID == Id)
        {
            if(config->voice_chan[i].isEnabled)
            {
                if(i == 0)
                {
                    CSR = M23_ReadControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR);
                    CSR += CONTROLLER_VOICE_PAUSE;
                    M23_WriteControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR,CSR);
                }
                else
                {
                    CSR = M23_ReadControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR);
                    CSR += CONTROLLER_VOICE_PAUSE;
                    M23_WriteControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR,CSR);
                }

                M23_SetHealthBit(config->voice_chan[i].chanID,M23_VOICE_CHANNEL_PAUSED);
            }
        }
    }

}

void M23_PauseAllPCM(int Id)
{
    int    NumBoards = 0;
    int    i;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_PCM(i) )
        {
            /*This Clears the Enable Bit for this channel*/
            CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,MP_PCM_SET_PAUSE);
            M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,(CSR | MP_PCM_PAUSE),MP_PCM_SET_PAUSE);
        }
    }
}

void M23_ResumeAllPCM(int Id)
{
    int    NumBoards = 0;
    int    i;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_PCM(i) )
        {
            /*This Clears the Enable Bit for this channel*/
            CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,MP_PCM_SET_PAUSE);
            M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,(CSR & ~MP_PCM_PAUSE),MP_PCM_SET_PAUSE);
        }
    }
}

void M23_ResumeAnalogChannel(int Id)
{
    int    i;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);

    for(i = 0;i < config->NumConfiguredVoice;i++)
    {
        /*Setup the Voice Channel*/
        if(config->voice_chan[i].chanID == Id)
        {
            if(config->voice_chan[i].isEnabled)
            {
                if(i == 0)
                {
                    CSR = M23_ReadControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR);
                    CSR &= ~CONTROLLER_VOICE_PAUSE;
                    M23_WriteControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR,CSR );
                }
                else
                {
                    CSR = M23_ReadControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR);
                    CSR &= ~CONTROLLER_VOICE_PAUSE;
                    M23_WriteControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR,CSR);
                }

                M23_ClearHealthBit(config->voice_chan[i].chanID,M23_VOICE_CHANNEL_PAUSED);
            }
        }
    }

}
void M23_ResumeVideoChannel(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    video_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);


    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_Video(i) )
        {
            for(j=0;j<4;j++)            
            {                
                if(config->video_chan[j + (4*video_index)].chanID == Id)
                {
                    if(config->video_chan[j + (4*video_index)].isEnabled)
                    {
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + 0x24);
                        CSR &= ~MP_VIDEO_PAUSE;
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + 0x24);
                        M23_ClearHealthBit(config->video_chan[j+(4*video_index)].chanID,M23_VIDEO_CHANNEL_CHANNEL_PAUSED);
                    }
                }
            }
            video_index++;
        }

    }
}

void M23_ResumeM1553Channel(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    m1553_4_index = 0;
    int    m1553_8_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_M1553(i) )
        {
            if(M23_MP_M1553_4_Channel(i))
            {
                for(j=0;j<4;j++)
                {
                    if(config->m1553_chan[j + (4*m1553_4_index)].chanID == Id)
                    {
                        if(config->m1553_chan[j + (4*m1553_4_index)].isEnabled)
                        {
                            /*This Sets the Enable Bit for this channel*/
                            CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                            CSR &= ~MP_M1553_PAUSE;
                            M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                            M23_ClearHealthBit(config->m1553_chan[j+(4*m1553_4_index)].chanID,M23_M1553_CHANNEL_PAUSED);

                        }
                    }
                }
                m1553_4_index++;
            }
            else
            {
                for(j=0;j<8;j++)
                {
                    if(config->m1553_chan[j + (8*m1553_8_index)].chanID == Id)
                    {
                        if(config->m1553_chan[j + (8*m1553_8_index)].isEnabled)
                        {
                            CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                            CSR &= ~MP_M1553_PAUSE;
                            M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR ,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ENABLE_OFFSET);
                            M23_ClearHealthBit(config->m1553_chan[j+(8*m1553_8_index)].chanID,M23_M1553_CHANNEL_PAUSED);
                        }
                    }
                }
                m1553_8_index++;
            }
        }
    }
}



void PauseChannel(int channel)
{
    M23_PauseAnalogChannel(channel);
    M23_PauseVideoChannel(channel);
    M23_PauseM1553Channel(channel);

}




void ResumeChannel(int channel)
{
    M23_ResumeAnalogChannel(channel);
    M23_ResumeVideoChannel(channel);
    M23_ResumeM1553Channel(channel);

}

/**************************************************************
 *   This will be setting and Clearing the Broadcast Messages */

void M23_StartPCMBroadcast(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;

    int    pcm_4_index = 0;
    int    pcm_8_index = 0;

    UINT32 CSR;

    M23_CHANNEL *Config;

    SetupGet(&Config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_PCM(i) )
        {
            if(M23_MP_PCM_4_Channel(i))
            {
                /*Setup PCM Channels that are enabled*/
                for(j=0;j<4;j++)
                {
                    if(Id == Config->pcm_chan[j + (pcm_4_index * 4)].chanID)
                    {
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR & ~BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    }
                }
                pcm_4_index++;
            }
            else  //This is an 8 channel PCM
            {
                /*Setup PCM Channels that are enabled*/
                for(j=0;j<8;j++)
                {
                    if(Id == Config->pcm_chan[j + (pcm_8_index * 8)].chanID)
                    {
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR & ~BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    }
                }
                pcm_8_index++;
            }

        }
    }
}

void M23_StopPCMBroadcast(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;

    int    pcm_4_index = 0;
    int    pcm_8_index = 0;

    UINT32 CSR;

    M23_CHANNEL *Config;

    SetupGet(&Config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_PCM(i) )
        {
            if(M23_MP_PCM_4_Channel(i))
            {
                /*Setup PCM Channels that are enabled*/
                for(j=0;j<4;j++)
                {
                    if(Id == Config->pcm_chan[j + (pcm_4_index * 4)].chanID)
                    {
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR | BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    }
                }
                pcm_4_index++;
            }
            else  //This is an 8 channel PCM
            {
                /*Setup PCM Channels that are enabled*/
                for(j=0;j<8;j++)
                {
                    if(Id == Config->pcm_chan[j + (pcm_8_index * 8)].chanID)
                    {
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR | BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    }
                }
                pcm_8_index++;
            }

        }
    }
}


void M23_StartDMBroadcast(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;

    UINT32 CSR;

    M23_CHANNEL *Config;

    SetupGet(&Config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_DM(i) )
        {
            for(j=0;j<4;j++)
            {
                if(Id == Config->m1553_chan[j].chanID)
                {
                    CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR & ~BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                }
            }
            if(Id == Config->dm_chan.chanID)
            {
                CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR & ~BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
            }
        }
    }
}

void M23_StopDMBroadcast(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;

    UINT32 CSR;

    M23_CHANNEL *Config;

    SetupGet(&Config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_DM(i) )
        {
            for(j=0;j<4;j++)
            {
                if(Id == Config->m1553_chan[j].chanID)
                {
                    CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR | BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                }
            }
            if(Id == Config->dm_chan.chanID)
            {
                CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR | BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
            }
        }
    }
}

void M23_StartM1553Broadcast(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    m1553_4_index = 0;
    int    m1553_8_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_M1553(i) )
        {
            if(M23_MP_M1553_4_Channel(i))
            {
                for(j=0;j<4;j++)
                {
                    if(config->m1553_chan[j + (4*m1553_4_index)].chanID == Id)
                    {
                        /*This Sets the Enable Bit for this channel*/
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR & ~BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    }
                }
                m1553_4_index++;
            }
            else
            {
                for(j=0;j<8;j++)
                {
                    if(config->m1553_chan[j + (8*m1553_8_index)].chanID == Id)
                    {
                        if(config->m1553_chan[j + (8*m1553_8_index)].isEnabled)
                        {
                            CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                            M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR & ~BROADCAST_MASK ,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                        }
                    }
                }
                m1553_8_index++;
            }
        }
        else if(M23_MP_IS_DM(i)) /*This is a Discrete Board with 4 busses*/
        {
            for(j=0;j<4;j++)
            {
                if(config->m1553_chan[j + (4*m1553_4_index)].chanID == Id)
                {
                    /*This Sets the Enable Bit for this channel*/
                    CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR & ~BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                }
            }
            m1553_4_index++;

        }
    }
}
void M23_StopM1553Broadcast(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    m1553_4_index = 0;
    int    m1553_8_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);

    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_M1553(i) )
        {
            if(M23_MP_M1553_4_Channel(i))
            {
                for(j=0;j<4;j++)
                {
                    if(config->m1553_chan[j + (4*m1553_4_index)].chanID == Id)
                    {
                        /*This Sets the Enable Bit for this channel*/
                        CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                        M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR | BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    }
                }
                m1553_4_index++;
            }
            else
            {
                for(j=0;j<8;j++)
                {
                    if(config->m1553_chan[j + (8*m1553_8_index)].chanID == Id)
                    {
                        if(config->m1553_chan[j + (8*m1553_8_index)].isEnabled)
                        {
                            CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                            M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR | BROADCAST_MASK ,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                        }
                    }
                }
                m1553_8_index++;
            }
        }
        else if(M23_MP_IS_DM(i)) /*This is a Discrete Board with 4 busses*/
        {
            for(j=0;j<4;j++)
            {
                if(config->m1553_chan[j + (4*m1553_4_index)].chanID == Id)
                {
                    /*This Sets the Enable Bit for this channel*/
                    CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR | BROADCAST_MASK,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                }
            }
            m1553_4_index++;

        }
    }
}

void M23_StartVideoBroadcast(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    video_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);


    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_Video(i) )
        {
            for(j=0;j<4;j++)            
            {                
                if(config->video_chan[j + (4*video_index)].chanID == Id)
                {
                    /*This Clears the Enable Bit for this channel*/
                    CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    CSR &= ~BROADCAST_MASK;
                    M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                }
            }
            video_index++;
        }

    }
}

void M23_StopVideoBroadcast(int Id)
{
    int    NumBoards = 0;
    int    i;
    int    j;
    int    video_index = 0;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);
    M23_NumberMPBoards(&NumBoards);


    for(i = 0; i < NumBoards;i++)
    {
        if(M23_MP_IS_Video(i) )
        {
            for(j=0;j<4;j++)            
            {                
                if(config->video_chan[j + (4*video_index)].chanID == Id)
                {
                    /*This Clears the Enable Bit for this channel*/
                    CSR = M23_mp_read_csr((MP_DEVICE_0 + i),BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                    CSR |= BROADCAST_MASK;
                    M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,CSR,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_ID_OFFSET);
                }
            }
            video_index++;
        }

    }
}



void M23_StartAnalogBroadcast(int id)
{
    int    i;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);

    for(i = 0;i < config->NumConfiguredVoice;i++)
    {
        if( id == config->voice_chan[i].chanID)
        {
            if(i == 0)
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_VOICE1_CHANNEL_ID_CSR);
                M23_WriteControllerCSR(CONTROLLER_VOICE1_CHANNEL_ID_CSR,CSR & ~BROADCAST_MASK);
            }
            else
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_VOICE2_CHANNEL_ID_CSR);
                M23_WriteControllerCSR(CONTROLLER_VOICE2_CHANNEL_ID_CSR,CSR & ~BROADCAST_MASK);
            }
        }
    }

}

void M23_StopAnalogBroadcast(int id)
{
    int    i;
    UINT32 CSR;

    M23_CHANNEL *config;

    SetupGet(&config);

    for(i = 0;i < config->NumConfiguredVoice;i++)
    {
        if( id == config->voice_chan[i].chanID)
        {
            if(i == 0)
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_VOICE1_CHANNEL_ID_CSR);
                M23_WriteControllerCSR(CONTROLLER_VOICE1_CHANNEL_ID_CSR,CSR | BROADCAST_MASK);
            }
            else
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_VOICE2_CHANNEL_ID_CSR);
                M23_WriteControllerCSR(CONTROLLER_VOICE2_CHANNEL_ID_CSR,CSR | BROADCAST_MASK);
            }
        }
    }

}

void M23_StartEthernetBroadcast()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(ETHERNET_CHAN_ID_CSR);
    M23_WriteControllerCSR(ETHERNET_CHAN_ID_CSR,CSR & ~BROADCAST_MASK);
}

void M23_StopEthernetBroadcast()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(ETHERNET_CHAN_ID_CSR);
    M23_WriteControllerCSR(ETHERNET_CHAN_ID_CSR,CSR | BROADCAST_MASK);
}

void M23_StartTimeBroadcast()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
    M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR & ~BROADCAST_MASK);
}

void M23_StopTimeBroadcast()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
    M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR | BROADCAST_MASK);
}


void M23_StopAllPublish()
{
    int i;

    for(i = 0; i < 50;i++)
    {
        if(publish.id[i] != 0xFF)
        {
            if(publish.Type[i] == PUB_VIDEO)
            {
                M23_StopVideoBroadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_PCM)
            {
                M23_StopPCMBroadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_1553)
            {
                M23_StopM1553Broadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_DISCRETE)
            {
                M23_StopDMBroadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_UART)
            {
                StopUARTBroadcast();
            }
            else if(publish.Type[i] == PUB_TIME)
            {
                M23_StopTimeBroadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_VOICE)
            {
                M23_StopAnalogBroadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_ETH)
            {
                M23_StopEthernetBroadcast(publish.id[i]);
            }
        }
    }
}

void M23_StopPublish()
{
    int i;

    for(i = 0; i < 50;i++)
    {
        if(stop_publish.id[i] != 0xFF)
        {
            if(stop_publish.Type[i] == PUB_VIDEO)
            {
                M23_StopVideoBroadcast(stop_publish.id[i]);
            }
            else if(stop_publish.Type[i] == PUB_PCM)
            {
                M23_StopPCMBroadcast(stop_publish.id[i]);
            }
            else if(stop_publish.Type[i] == PUB_1553)
            {
                M23_StopM1553Broadcast(stop_publish.id[i]);
            }
            else if(stop_publish.Type[i] == PUB_DISCRETE)
            {
                M23_StopDMBroadcast(stop_publish.id[i]);
            }
            else if(stop_publish.Type[i] == PUB_UART)
            {
                StopUARTBroadcast();
            }
            else if(stop_publish.Type[i] == PUB_TIME)
            {
                M23_StopTimeBroadcast(stop_publish.id[i]);
            }
            else if(stop_publish.Type[i] == PUB_VOICE)
            {
                M23_StopAnalogBroadcast(stop_publish.id[i]);
            }
            else if(stop_publish.Type[i] == PUB_ETH)
            {
                M23_StopEthernetBroadcast(stop_publish.id[i]);
            }
        }
    }
}

void M23_StartAllPublish()
{
    int i;

    for(i = 0; i < 50;i++)
    {
        if(publish.id[i] != 0xFF)
        {
            if(publish.Type[i] == PUB_VIDEO)
            {
                M23_StartVideoBroadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_PCM)
            {
                M23_StartPCMBroadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_1553)
            {
                M23_StartM1553Broadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_DISCRETE)
            {
                M23_StartDMBroadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_UART)
            {
                StartUARTBroadcast();
            }
            else if(publish.Type[i] == PUB_TIME)
            {
                M23_StartTimeBroadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_VOICE)
            {
                M23_StartAnalogBroadcast(publish.id[i]);
            }
            else if(publish.Type[i] == PUB_ETH)
            {
                M23_StartEthernetBroadcast(publish.id[i]);
            }
        }
    }

}


