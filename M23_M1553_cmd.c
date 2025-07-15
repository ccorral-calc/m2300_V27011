/***************************************************************************************
 *
 *    Copyright (C) 2004 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed
 *    for any purpose except as specifically authorized in writing.
 *
 *       File: M23_M1553_cmd.c
 *       Version: 1.0
 *     Author: pcarrion
 *
 *            MONSSTR 2300v1
 *
 *     This will provide the interface with the command and control from 1553.
 *
 *    Revisions:
 ******************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>

#include "M23_Controller.h"
#include "M23_features.h"
#include "M23_Status.h"
#include "M23_M1553_cmd.h"
#include "M23_MP_Handler.h"
#include "M23_Stanag.h"
#include "M23_EventProcessor.h"
#include "M23_Utilities.h"
#include "M2X_time.h"

#include "version.h"


#define  CONDOR_CORE_BASE    0x80002000
#define  CONTROL_REGISTER    (0x0   << 2)
#define  INTERRUPT_Q         (0x120 << 2)
#define  INTERRUPT_LSW       (0x122 << 2)
#define  INTERRUPT_Q_INDEX   (0xF1  << 2)
#define  INTERRUPT_Q_MASK    (0xF2  << 2)
#define  BIT_CMD_REG         (0xF4  << 2)
#define  BIT_STATUS_REG      (0xF5  << 2)
#define  BIT_ERROR_REG       (0xF6  << 2)
#define  FW_STATUS_REG       (0xF8  << 2)
#define  OPN_STATE_REG       (0xF9  << 2)
#define  RT_CONTROL_BUFFERS  (0x270 << 2)
#define  RT_MESSAGE_BUFFERS  (0x280 << 2)
#define  RT_OFFSET           (0x130 << 2) 
#define  RT_ADDRESS_OFFSET   (0x131 << 2) 
#define  RT_FILTER_TABLE     (0x230 << 2) 

#define  INTERRUPT_Q_OFFSET      0x120

#define  RECEIVE_SA1_OFFSET      0x80000270
#define  RECEIVE_R2_OFFSET       0x800002BD
#define  TRANSMIT_SA3_OFFSET     0x800002DD
#define  TRANSMIT_SA4_OFFSET     0x800002FD
#define  TRANSMIT_SA5_OFFSET     0x8000031D
#define  TRANSMIT_SA6_OFFSET     0x8000033D

#define  RECEIVE_SA1_CONTROL     (0x270 << 2)
#define  RECEIVE_R2_CONTROL      (0x2BD << 2)
#define  TRANSMIT_SA3_CONTROL    (0x2DD << 2)
#define  TRANSMIT_SA4_CONTROL    (0x2FD << 2)
#define  TRANSMIT_SA5_CONTROL    (0x31D << 2)
#define  TRANSMIT_SA6_CONTROL    (0x33D << 2)

volatile CONTROL_BUFFER *R_SA1_Control = (CONTROL_BUFFER*)(CONDOR_CORE_BASE + RECEIVE_SA1_CONTROL);
volatile CONTROL_BUFFER *R2_Control    = (CONTROL_BUFFER*)(CONDOR_CORE_BASE + RECEIVE_R2_CONTROL);
volatile CONTROL_BUFFER *T_SA3_Control = (CONTROL_BUFFER*)(CONDOR_CORE_BASE + TRANSMIT_SA3_CONTROL);
volatile CONTROL_BUFFER *T_SA4_Control = (CONTROL_BUFFER*)(CONDOR_CORE_BASE + TRANSMIT_SA4_CONTROL);
volatile CONTROL_BUFFER *T_SA5_Control = (CONTROL_BUFFER*)(CONDOR_CORE_BASE + TRANSMIT_SA5_CONTROL);
volatile CONTROL_BUFFER *T_SA6_Control = (CONTROL_BUFFER*)(CONDOR_CORE_BASE + TRANSMIT_SA6_CONTROL);



//#define  RECEIVE_SA1_BUFFER      0x0280 
#define  RECEIVE_SA1_BUFFER      0x0940 
#define  RECEIVE_R2_BUFFER       0x0800 
#define  TRANSMIT_SA3_BUFFER     0x0840 
#define  TRANSMIT_SA4_BUFFER     0x0880 
#define  TRANSMIT_SA5_BUFFER     0x08C0 
#define  TRANSMIT_SA6_BUFFER     0x0900 

//#define  RECEIVE_SA1_ADDRESS     (0x280 << 2)

/*New Dual-Port RAM address*/
#define  RECEIVE_SA1_ADDRESS     0x8000C500 

#define  RECEIVE_R2_ADDRESS      0x8000C000
#define  TRANSMIT_SA3_ADDRESS    0x8000C100
#define  TRANSMIT_SA4_ADDRESS    0x8000C200
#define  TRANSMIT_SA5_ADDRESS    0x8000C300
#define  TRANSMIT_SA6_ADDRESS    0x8000C400


//volatile MESSAGE_BUFFER *R_SA1_Buffer = (MESSAGE_BUFFER*)(CONDOR_CORE_BASE + RECEIVE_SA1_ADDRESS);

volatile MESSAGE_BUFFER *R_SA1_Buffer = (MESSAGE_BUFFER*)(RECEIVE_SA1_ADDRESS);

volatile MESSAGE_BUFFER *R_R2_Buffer  = (MESSAGE_BUFFER*)(RECEIVE_R2_ADDRESS);
volatile MESSAGE_BUFFER *T_SA3_Buffer = (MESSAGE_BUFFER*)(TRANSMIT_SA3_ADDRESS);
volatile MESSAGE_BUFFER *T_SA4_Buffer = (MESSAGE_BUFFER*)(TRANSMIT_SA4_ADDRESS);
volatile MESSAGE_BUFFER *T_SA5_Buffer = (MESSAGE_BUFFER*)(TRANSMIT_SA5_ADDRESS);
volatile MESSAGE_BUFFER *T_SA6_Buffer = (MESSAGE_BUFFER*)(TRANSMIT_SA6_ADDRESS);

#define  RT_RUN                  (1 << 2)
#define  RT_INTERRUPT            (1 << 3)
#define  INFO_INTERRUPT          (1 << 13)
#define  STATUS_INTERRUPT        (1 << 14)
#define  CONDOR_SETUP            0x0C04

#define LIVE_STATUS             (1 << 0)
#define STATUS_VALID            (1 << 3)
#define ERROR_BIT               (1 << 4)

#define JAM_TIME_DIFF            750


volatile FILTER_TABLE *filter = (FILTER_TABLE*)(CONDOR_CORE_BASE + RT_FILTER_TABLE);
volatile ADDRESS_TABLE *table = (ADDRESS_TABLE*)(CONDOR_CORE_BASE + RT_OFFSET);



pthread_t M1553CmdThread;
pthread_t M1553LoadThread;
pthread_t M1553Time;
pthread_t M1553Event;

static int M1553Device = 0xFF;
static int M1553Channel = 0xFF;

#define LOAD_FILE    0
#define LOAD_EVENT   1
#define LOAD_NONE    0xFF
static int LoadInfo;
static int InfoNumber;
static int EventOccurence;

static int CurrentError;

static int TimeJammed = 0;
static int BusPauseEvent = 0;
static int BusResumeEvent = 0;
static int BusTimeSyncEvent = 0;

static int ValidTFOM = 1;

/*Used for the New B1B Time Algorithm*/
static float  UTC_check_time;
static UINT32 UTC_compare_time;


#define QUEUE_NO_STATUS    0x0
#define QUEUE_PASS         0x2
#define QUEUE_FAIL         0x4
#define QUEUE_IN_PROGRESS  0x6
static int CurrentQueueStatus;

static int LiveVidStatus;

static int days_in_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

void M23_M1553_LoadTimeEvent(int Event);

volatile UINT16 M23_M1553_ReadCondor(int offset);
volatile void M23_M1553_WriteCondor(int offset,int value);

/*Variables used to Jam M1553 Format 1 time*/
INT64    RT1;
UINT32   ST1;
UINT32   ST2;
UINT32   TOD1;
int     DayOfYear;

/*Variables used to Jam time from an RT .time command*/
INT64    RT2;
UINT32   ST3;
UINT32   ST4;
UINT32   TOD2;

static int PlaySpeedStatus;

void M23_SetPauseEvent(int number)
{
    int debug;

    M23_GetDebugValue(&debug);

    BusPauseEvent = number;

    if(debug)
        printf("Bus Pause Event is %d\r\n",BusPauseEvent);
}

void M23_SetResumeEvent(int number)
{
    int debug;

    M23_GetDebugValue(&debug);

    BusResumeEvent = number;

    if(debug)
        printf("Bus Resume Event is %d\r\n",BusResumeEvent);
}

void M23_SetTimeSyncEvent(int number)
{
    int debug;

    M23_GetDebugValue(&debug);

    BusTimeSyncEvent = number;

    if(debug)
        printf("EGI Time Sync  Event is %d\r\n",BusTimeSyncEvent);
}

void M23_SetPlaySpeedStatus(int speed)
{
    PlaySpeedStatus = speed;
}

void LoadTransmitData()
{
    int       i;
    int       number_of_features = 0;
    int       return_status;
    int       major;
    int       minor;
    int       tmp;
    int       state;
    int       nonCrit;
    int       Crit;
    int       percom;
    int       year;
    int       month;
    int       day;
    int       chan;
    UINT16    DataW16;
    UINT32    *health_status = NULL;
    UINT32    total_block_count = 1;
    UINT32    block_use_count = 0;
    UINT32    MSW;
    UINT32    LSW;


    UINT64    blocks;
    UINT64    percent;

    /*First Load Event Data*/
    for(i = 0 ; i < 32;i++)
    {
        T_SA3_Buffer->Data[i] = (UINT16)EventTable[i+2].EventCount;
    }


    /*Now Load The Health Data*/
    HealthViewAll( &number_of_features, &health_status);
    for (i = 0; i < 32; i++ )
    {
        if(HealthArrayTypes[i] != NO_FEATURE)
        {
            DataW16 = health_status[i] & 0x000000FF;
            if((health_status[i] & 0x80000000))//see if status is ON
            {
                T_SA4_Buffer->Data[i] = DataW16;
            }
            else
            {
                T_SA4_Buffer->Data[i] = 0x0;
            }
        }
    }

    /*Now Load the Status Data*/
    return_status = DiskIsConnected();
    if(return_status)
    {
        sdisk( &total_block_count, &block_use_count );
        //printf("total %d, used = %d\r\n",total_block_count,block_use_count);
    }


    blocks = (UINT64)block_use_count * 100;
    percent = blocks/(UINT64)total_block_count;
    percent = (99 - percent);

    if( percent > 99)
    {
        percent = 99;
    }

    /*Get the RMM size in Giga Bytes*/
    blocks = ((UINT64)total_block_count * 512)/ONE_GIG;
    Status(&state,&nonCrit,&Crit,&percom);


    T_SA6_Buffer->Data[2] = (UINT16)blocks;

    //T_SA6_Buffer->Data[3] = (UINT16)(percent << 8);
    T_SA6_Buffer->Data[3] = (UINT16)(percent);

    return_status = DiskIsConnected();

   if(return_status)
   {
        DataW16 = GetNumberOfFiles();
        /*If the TMATS was loaded from the cartridge, do count it, start with the second file*/
        if(LoadedTmatsFromCartridge == 1)
        {
            DataW16--;
        }
   }
   else
   {
       DataW16 = 0;
   }

    //T_SA6_Buffer->Data[4] = (DataW16 << 7);
    T_SA6_Buffer->Data[4] = DataW16;

    //T_SA6_Buffer->Data[1] = (percom << 8);
    T_SA6_Buffer->Data[1] = (UINT16)(percom << 8);
    DataW16 = state;

    M23_GetAssignedChan(&chan);


    /****** Get The System Time ***********/
    tmp = 0;

    /*Read the Upper Time*/
    MSW = M23_ReadControllerCSR(CONTROLLER_TIMECODE_BCD_UPPER_CSR);

    /*Read the Lower Time*/
    LSW = M23_ReadControllerCSR(CONTROLLER_TIMECODE_BCD_LOWER_CSR);

    tmp |= ((LSW >> 12) & 0xF) << 12; // Tens of Seconds
    tmp |= ((LSW >>8) & 0xF) << 8;         // Units of Seconds
    T_SA6_Buffer->Data[5] = (UINT16)tmp;

    tmp =  ((LSW >> 28) & 0xF) << 12; // Tens of Hours
    tmp |= ((LSW >> 24) & 0xF) << 8;  // Hours
    tmp |= ((LSW >> 20) & 0xF) << 4;  // Tens Of Minutes
    tmp |= ((LSW >> 16) & 0xF);        // Units of Minutes
    T_SA6_Buffer->Data[6] = (UINT16)tmp;

    tmp = 100 * ((MSW>>8) & 0x000F);
    tmp += 10*  ((MSW >> 4) & 0x000F);
    tmp +=  (MSW & 0x000F);


    Time_GetDayMonthYear(tmp,&day,&month,&year);
    tmp =  (month/10) << 12;      // Tens of Months
    tmp |= (month%10) << 8;       // Months
    tmp |= (day/10) << 4;         // Tens of Days
    tmp |= (day%10);              // Units of Days
    T_SA6_Buffer->Data[7] = (UINT16)tmp;

    tmp =  (year/1000) << 12;              // Thousands of Years
    tmp |= ((year%1000)/100) << 8;         // Hundreds of Years
    tmp |= (((year%1000)%100)/10) << 4;    // Tens of Years
    tmp |= (((year%1000)%100)%10);         // Units of Years
    T_SA6_Buffer->Data[8] = (UINT16)tmp;

    /****** Get The Version ***********/
    sscanf(SystemVersion,"%d.%d",&major,&minor);
    DataW16 = (UINT16)major;
    DataW16 = (DataW16 << 8) & 0xFF00;
    DataW16 |= (UINT16)minor;
    T_SA6_Buffer->Data[14] = DataW16;

    DataW16 = TmatsVersion;
    T_SA6_Buffer->Data[15] = DataW16;

    /****** Get The Amount of ADCP Events ***********/
    T_SA6_Buffer->Data[13] = (UINT16)M23_GetNumADCPEvents();

    /*Now Get the Playback Time and Return it*/
    M23_WriteControllerCSR(PLAYBACK_TIME_MSW,PLAYBACK_TIME_REQ);

    tmp = 0;

    /*Read the Upper Time*/
    MSW = M23_ReadControllerCSR(PLAYBACK_TIME_MSW);

    /*Read the Lower Time*/
    LSW = M23_ReadControllerCSR(PLAYBACK_TIME_LSW);

    tmp |= ((LSW >> 12) & 0xF) << 12; // Tens of Seconds
    tmp |= ((LSW >>8) & 0xF) << 8;         // Units of Seconds
    T_SA6_Buffer->Data[9] = (UINT16)tmp;

    tmp =  ((LSW >> 28) & 0xF) << 12; // Tens of Hours
    tmp |= ((LSW >> 24) & 0xF) << 8;  // Hours
    tmp |= ((LSW >> 20) & 0xF) << 4;  // Tens Of Minutes
    tmp |= ((LSW >> 16) & 0xF);        // Units of Minutes
    T_SA6_Buffer->Data[10] = (UINT16)tmp;

    tmp = 100 * ((MSW>>8) & 0x000F);
    tmp += 10*  ((MSW >> 4) & 0x000F);
    tmp +=  (MSW & 0x000F);


    Time_GetDayMonthYear(tmp,&day,&month,&year);
    tmp =  (month/10) << 12;      // Tens of Months
    tmp |= (month%10) << 8;       // Months
    tmp |= (day/10) << 4;         // Tens of Days
    tmp |= (day%10);              // Units of Days
    T_SA6_Buffer->Data[11] = (UINT16)tmp;

    tmp =  (year/1000) << 12;              // Thousands of Years
    tmp |= ((year%1000)/100) << 8;         // Hundreds of Years
    tmp |= (((year%1000)%100)/10) << 4;    // Tens of Years
    tmp |= (((year%1000)%100)%10);         // Units of Years
    T_SA6_Buffer->Data[12] = (UINT16)tmp;

    DataW16 = ((UINT16)(state <<12)) | ((UINT16)(chan <<5)) | ((UINT16)(PlaySpeedStatus << 8) | (UINT16)(CurrentError)| (UINT16)(STATUS_VALID)| (UINT16)(CurrentQueueStatus) | (UINT16)(LiveVidStatus));

    T_SA6_Buffer->Data[0] = DataW16;

}


/*******************************************************************************************************
*  Name :    ProcessAndRespondToInot(int Mode,int Number)
*
*  Purpose : This function will process the .Info command and respond with the data
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
int ProcessAndRespondToInfo(int Mode, int Number)
{
    int     day;
    int     month;
    int     year;
    UINT32  MSW;
    UINT32  LSW;
    int     Cmd;
    int     tmp;
    int     FileNum;
    int     EventNum;
    int     NumFiles;
    int     ret_status;
    int     status = M1553_COMMAND_SUCCESS;

    STANAG_FILE_ENTRY      FileEntry;

    M23_CHANNEL  const *config;

    SetupGet(&config);

    Cmd = (Mode >> 15);
    EventNum = Mode & 0x03FF;

    NumFiles = GetNumberOfFiles();

    switch(Cmd)
    {
        case 0x00:
            FileNum = Mode & 0x03FF;

 
            /*If the TMATS was loaded from the cartridge, do count it, start with the second file*/
            if(LoadedTmatsFromCartridge == 1)
            {
                if(NumFiles == 1)
                {
                    CurrentError = ERROR_BIT;
                    return 0;
                }
                else
                {
                    FileNum++;
                    if(FileNum > NumFiles)
                    {
                        CurrentError = ERROR_BIT;
                        return 0;
                    }
                }
            }
            else
            {
                if(FileNum > NumFiles)
                {
                    CurrentError = ERROR_BIT;
                    return 0;
                }
            }

            ret_status = GetFileEntry(&FileEntry,FileNum);


            if(ret_status == 1)
            {

                /**************************Start Time***************************************/
                tmp =  (FileEntry.FILE_CreateTime[4] & 0x000F) << 12; //Tens of Seconds
                tmp |= (FileEntry.FILE_CreateTime[5] & 0x000F) << 8;  //Seconds
                tmp |= (FileEntry.FILE_CreateTime[6] & 0x000F) << 4;  //Hundreds of Milliseconds
                tmp |= FileEntry.FILE_CreateTime[7] & 0x000F;         //Tens of Milliseconds
                T_SA5_Buffer->Data[1] = tmp;

                tmp =  (FileEntry.FILE_CreateTime[0] & 0x0003) << 12; //Tens of Hours
                tmp |= (FileEntry.FILE_CreateTime[1] & 0x000F) << 8;  //Hours
                tmp |= (FileEntry.FILE_CreateTime[2] & 0x0007) << 4;  //Tens of Minutes
                tmp |= (FileEntry.FILE_CreateTime[3] & 0x000F);       //Units of Minutes
                T_SA5_Buffer->Data[2] = tmp;

                tmp =  (FileEntry.FILE_CreateDate[2] & 0x0001) << 12; //Tens of Months
                tmp |= (FileEntry.FILE_CreateDate[3] & 0x000F) << 8;  //Months
                tmp |= (FileEntry.FILE_CreateDate[0] & 0x000F) << 4;  //Tens of Days
                tmp |= (FileEntry.FILE_CreateDate[1] & 0x000F);       //Units of Days
                T_SA5_Buffer->Data[3] = tmp;

                tmp =  (FileEntry.FILE_CreateDate[4] & 0x0003) << 12; //Thousands of Years
                tmp |= (FileEntry.FILE_CreateDate[5] & 0x000F) << 8;  //Hundreds of Years
                tmp |= (FileEntry.FILE_CreateDate[6] & 0x000F) << 4;  //Tens of Years
                tmp |= (FileEntry.FILE_CreateDate[7] & 0x000F);       //Units of Years
                T_SA5_Buffer->Data[4] = tmp;

                /**************************Stop Time***************************************/
                tmp =  (FileEntry.FILE_CloseTime[4] & 0x000F) << 12; //Tens of Seconds
                tmp |= (FileEntry.FILE_CloseTime[5] & 0x000F) << 8;  //Seconds
                tmp |= (FileEntry.FILE_CloseTime[6] & 0x000F) << 4;  //Hundreds of Milliseconds
                tmp |= FileEntry.FILE_CloseTime[7] & 0x000F;         //Tens of Milliseconds
                T_SA5_Buffer->Data[5] = tmp;

                tmp =  (FileEntry.FILE_CloseTime[0] & 0x0003) << 12; //Tens of Hours
                tmp |= (FileEntry.FILE_CloseTime[1] & 0x000F) << 8;  //Hours
                tmp |= (FileEntry.FILE_CloseTime[2] & 0x0007) << 4;  //Tens of Minutes
                tmp |= (FileEntry.FILE_CloseTime[3] & 0x000F);       //Units of Minutes
                T_SA5_Buffer->Data[6] = tmp;

                tmp =  (FileEntry.FILE_CreateDate[2] & 0x0001) << 12; //Tens of Months
                tmp |= (FileEntry.FILE_CreateDate[3] & 0x000F) << 8;  //Months
                tmp |= (FileEntry.FILE_CreateDate[0] & 0x000F) << 4;  //Tens of Days
                tmp |= (FileEntry.FILE_CreateDate[1] & 0x000F);       //Units of Days
                T_SA5_Buffer->Data[7] = tmp;

                tmp =  (FileEntry.FILE_CreateDate[4] & 0x0003) << 12; //Thousands of Years
                tmp |= (FileEntry.FILE_CreateDate[5] & 0x000F) << 8;  //Hundreds of Years
                tmp |= (FileEntry.FILE_CreateDate[6] & 0x000F) << 4;  //Tens of Years
                tmp |= (FileEntry.FILE_CreateDate[7] & 0x000F);       //Units of Years
                T_SA5_Buffer->Data[8] = tmp;

                if(LoadedTmatsFromCartridge == 1)
                {
                    /*Now Write the FileNum and clear the Invalid*/
                    T_SA5_Buffer->Data[0] = ((FileNum - 1) | 0x8000);
                }
                else
                {
                    /*Now Write the FileNum and clear the Invalid*/
                    T_SA5_Buffer->Data[0] = FileNum | 0x8000;
                }

                T_SA5_Buffer->RT_InterruptEnable2 = INFO_INTERRUPT | 0x1;
            }
            break;
        case 0x01:
            EventNum = Mode & 0x03FF;

            T_SA5_Buffer->Data[10] = Number;

            MSW = 0;
            LSW = 0;
            m23_GetEventTime(EventNum + 1,Number, &MSW, &LSW );
            if( (MSW == 0) && (LSW == 0) )
            {
                status = M1553_COMMAND_FAILED;
                T_SA5_Buffer->Data[11] = 0x0;
                T_SA5_Buffer->Data[12] = 0x0;
                T_SA5_Buffer->Data[13] = 0x0;
                T_SA5_Buffer->Data[14] = 0x0;
            }
            else
            {
                tmp = 0;
                tmp |= ((LSW >> 12) & 0xF) << 12; // Tens of Seconds
                tmp |= ((LSW >>8) & 0xF) << 8;         // Units of Seconds
                T_SA5_Buffer->Data[11] = tmp;

                tmp =  ((LSW >> 28) & 0xF) << 12; // Tens of Hours
                tmp |= ((LSW >> 24) & 0xF) << 8;  // Hours
                tmp |= ((LSW >> 20) & 0xF) << 4;  // Tens Of Minutes
                tmp |= ((LSW >> 16) & 0xF);        // Units of Minutes
                T_SA5_Buffer->Data[12] = tmp;

                tmp = 100 * ((MSW>>8) & 0x000F);
                tmp += 10*  ((MSW >> 4) & 0x000F);
                tmp +=  (MSW & 0x000F);


                Time_GetDayMonthYear(tmp,&day,&month,&year);
                tmp =  (month/10) << 12;      // Tens of Months
                tmp |= (month%10) << 8;       // Months
                tmp |= (day/10) << 4;         // Tens of Days
                tmp |= (day%10);              // Units of Days
                T_SA5_Buffer->Data[13] = tmp;

                tmp =  (year/1000) << 12;              // Thousands of Years
                tmp |= ((year%1000)/100) << 8;         // Hundreds of Years
                tmp |= (((year%1000)%100)/10) << 4;    // Tens of Years
                tmp |= (((year%1000)%100)%10);         // Units of Years
                T_SA5_Buffer->Data[14] = tmp;
            }

            T_SA5_Buffer->Data[9] = EventNum | 0x8000;
            T_SA5_Buffer->RT_InterruptEnable2 = INFO_INTERRUPT | 0x1;

            break;
    }

    return(status);

}



void M23_M1553_Process_PauseResume(int event)
{

    if(event == PAUSE_EVENT)
    {
        M23_PauseAllHealth();
        M23_PauseAllPCM();
    }
    else //This is a Resume/Start Event
    {
        M23_ResumeAllHealth();
        M23_ResumeAllPCM();
    }
}

int  M23_M1553_IsTimeJammed()
{
    return(TimeJammed);
}

void M23_M1553_SetTiming(int device,int channel)
{
    M1553Device  = device;
    M1553Channel = channel;
}

int ProcessR0_Event(int device,int channel)
{
    int    i;
    int    debug;
    int    count = 0;
    int    status;
    UINT32 CSR;
    UINT32 TimeWords[32];

    M23_GetDebugValue(&debug);

    while(1)
    {
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_OFFSET);
        if(CSR & MP_M1553_CAPTURE_ONE)
        {
            usleep(1000);
            count++;
            if(count > 1000)
            {
                if(debug)
                {
                    printf("Process R0 -> Capture Never Cleared 0x%x\r\n",CSR);
                }
                status = -1;
                break;
            }
        }
        else
        {
            break;
        }
    }

    if(status == -1)
    {
        return(status);
    }

    i = 0;
    while(1)
    {
        CSR = 0;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_MESSAGE_FIFO);
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_MESSAGE_FIFO);
        //if( (CSR >> 16) & 0x1FF)  //Still Data in FIFO
        if(CSR & 0x10000000)
        {
            TimeWords[i++] = CSR & 0xFFFF;
            if(i > 16)
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

        /*Set the M1553 Relative Time*/
    RT1 = TimeWords[0];
    RT1 |= ( ((INT64)TimeWords[1]) << 16);
    RT1 |= ( ((INT64)TimeWords[2]) << 32);
    RT1 |= ( ((INT64)TimeWords[3]) << 48);

    ST1 = TimeWords[8];

    return(0);

}

void ProcessT09_Event(int device,int channel)
{
    int    i;

    UINT32 CSR;
    UINT32 TimeWords[34];


    while(1)
    {
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_OFFSET);
        if(CSR & MP_M1553_CAPTURE_ONE)
        {
            usleep(1000);
        }
        else
        {
            break;
        }
    }

    i = 0;
    while(1)
    {
        CSR = 0;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_MESSAGE_FIFO);
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_MESSAGE_FIFO);
        //if( (CSR >> 16) & 0x1FF)  //Still Data in FIFO
        if(CSR & 0x10000000)
        {
            TimeWords[i++] = CSR & 0xFFFF;
            if(i > 32)
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    ST2 = TimeWords[11];

    TOD1 = TimeWords[30] &  0xFFFF;
    TOD1 |= (TimeWords[29] << 16);

}

void M23_M1553_ProcessF15_Time(int event)
{
    static int state = 0;
    int        status;
    int        debug;

    M23_GetDebugValue(&debug);

    switch(state)
    {
        case 0:
            if(event == T20_EVENT)
            { 
                status = M23_M1553_F15_Time(M1553Device,M1553Channel,state);
                if(status == 1)
                {
                    M23_M1553_LoadTimeEvent(R0_EVENT);
                    state = 1;
                }
                else
                {
                    M23_M1553_LoadTimeEvent(T20_EVENT);
                }
            }
            break;
        case 1:
            if(event == R0_EVENT)
            {
                status = ProcessR0_Event(M1553Device,M1553Channel);
                if(status == 0)
                {
                    M23_M1553_LoadTimeEvent(T9_EVENT);
                    state = 2;
                }
            }
            break;
        case 2:
            if(event == T9_EVENT)
            {
                ProcessT09_Event(M1553Device,M1553Channel);
                M23_M1553_LoadTimeEvent(T20_EVENT);
                state = 3;
            }
            break;
        case 3:
            if(event == T20_EVENT)
            { 
                status = M23_M1553_F15_Time(M1553Device,M1553Channel,state);
                if(status == -3)
                {
                    M23_M1553_LoadTimeEvent(R0_EVENT);
                    state = 1;
                }
                else
                {
                    state = 4;
                }
            }
            break;
        default:
            break;
    }
}

int M23_Process_B1B_FOM(int device,int channel)
{
    int    i;
    int    debug;
    int    count = 0;
    int    status;
    int    valid;
    UINT32 CSR;
    UINT32 TimeWords[48];

    M23_GetDebugValue(&debug);

    while(1)
    {
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_OFFSET);
        if(CSR & MP_M1553_CAPTURE_ONE)
        {
            usleep(1000);
            count++;
            if(count > 1000)
            {
                if(debug)
                {
                    printf("Process FOM -> Capture Never Cleared 0x%x\r\n",CSR);
                }
                status = -1;
                break;
            }
        }
        else
        {
            break;
        }
    }

    if(status == -1)
    {
        return(status);
    }

    i = 0;
    while(1)
    {
        CSR = 0;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_MESSAGE_FIFO);
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_MESSAGE_FIFO);
        //if( (CSR >> 16) & 0x1FF)  //Still Data in FIFO
        if(CSR & 0x10000000)
        {
            TimeWords[i++] = CSR & 0xFFFF;
            if(i > 48)
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    valid = ( (TimeWords[38] & 0x0F00) >> 8);
    if( (valid > 0) && (valid < 8) )
    {
        DayOfYear = TimeWords[39];
        return(1);
    }

    return(0);
}

int ProcessT17_Event(int device,int channel)
{
    int    i;
    int    debug;
    int    count = 0;
    int    status;
    UINT32 CSR;
    UINT32 TimeWords[48];

    M23_GetDebugValue(&debug);

    while(1)
    {
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_OFFSET);
        if(CSR & MP_M1553_CAPTURE_ONE)
        {
            usleep(1000);
            count++;
            if(count > 1000)
            {
                if(debug)
                {
                    printf("Process T17 -> Capture Never Cleared 0x%x\r\n",CSR);
                }
                status = -1;
                break;
            }
        }
        else
        {
            break;
        }
    }

    if(status == -1)
    {
        return(status);
    }
    i = 0;
    while(1)
    {
        CSR = 0;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_MESSAGE_FIFO);
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_MESSAGE_FIFO);
        //if( (CSR >> 16) & 0x1FF)  //Still Data in FIFO
        if(CSR & 0x10000000)
        {
            TimeWords[i++] = CSR & 0xFFFF;
            if(i > 16)
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

        /*Set the M1553 Relative Time*/
    RT1 = TimeWords[0];
    RT1 |= ( ((INT64)TimeWords[1]) << 16);
    RT1 |= ( ((INT64)TimeWords[2]) << 32);
    RT1 |= ( ((INT64)TimeWords[3]) << 48);

    ST1 = TimeWords[8];

    return(0);
}

void M23_M1553_ProcessB1B_Time(int event)
{
    static int state = 0;
    int        status;
    int        debug;

    M23_GetDebugValue(&debug);

    switch(state)
    {
        case 0:
            if(event == B1_T16_EVENT)
            {
                status = M23_Process_B1B_FOM(M1553Device,M1553Channel);
                if(status == 1)
                {
                    M23_M1553_LoadTimeEvent(B1_T17_EVENT);
                    state = 1;
                }
                else
                {
                    M23_M1553_LoadTimeEvent(B1_T16_EVENT);
                }
            }
            break;
        case 1:
            if(event == B1_T17_EVENT)
            {
                status = ProcessT17_Event(M1553Device,M1553Channel);
                if(status == 0)
                {
                    M23_M1553_LoadTimeEvent(B1_T9_EVENT);
                    state = 2;
                }
            }
            break;
        case 2:
            if(event == B1_T9_EVENT)
            {
                ProcessT09_Event(M1553Device,M1553Channel);
                status = M23_M1553_B1B_Time(M1553Device,M1553Channel,state);
                if(status == -3)
                {
                }
                else
                {
                    state = 3;
                }
            }
            break;
        default:
            break;
    }
}

volatile void M23_M1553_WriteRT(int offset,int value)
{
    int      debug;
    volatile UINT16 *CSR;

    M23_GetDebugValue(&debug);
    if(debug)
        printf("RT - Writing 0x%x to 0x%x\r\n",value,CONDOR_CORE_BASE + offset);

    //CSR = (int*)(CONDOR_CORE_BASE + offset);
    CSR = (volatile UINT16*)(CONDOR_CORE_BASE + offset);

    *CSR = (UINT16)value; 

    usleep(1);
}

volatile void M23_M1553_WriteCondor(int offset,int value)
{
    //int *CSR;
    volatile UINT16 *CSR;

    int debug;

    M23_GetDebugValue(&debug);

    CSR = (volatile UINT16*)(CONDOR_CORE_BASE + offset);

    *CSR = (UINT16)value; 

}

volatile UINT16 M23_M1553_ReadCondor(int offset)
{
    volatile UINT16 *CSR;
    UINT16 value;

    CSR = (volatile UINT16*)(CONDOR_CORE_BASE + offset);

    value = *CSR;

#if 0
    usleep(1);
#endif

    return(value);
}

void M23_M1553_ReadCondorMem(int offset,int size)
{
    int i;
    int num;
    int count = 0;
    int lines = 0;
    //int *CSR;
    UINT16 *CSR;

    num = size/2;
    if(size % 2)
    {
        num++;
    }

    printf("%03x - ",(lines *2) + offset);
    lines += 8;

    for(i = 0; i < num;i++)
    {
        //CSR = (int*)(CONDOR_CORE_BASE + ((offset + i) << 2 ) );
        CSR = (UINT16*)(CONDOR_CORE_BASE + ((offset + i) << 2 ) );
        //printf("%08x ", *(unsigned long*)CSR);
        printf("%04x ", *(UINT16*)CSR);
        if( count == 7)
        {
            printf("\r\n%03x - ",(lines*2) + offset);
            lines += 8;
            count = 0;
        }
        else
            count++;


    }

    if(count < 7)
        printf("\r\n");

}


/*******************************************************************************************************
*  Name :    M23_M1553_GetCommand(int buffer)
*
*  Purpose : This function will Read the command from the M1553, it will execute the command 
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/

void M23_M1553_GetCommand()
{
    int       state;
    int       return_status;
    int       i;
    int       Cmd;
    int       DataW1;
    int       DataW2;
    int       CommandW;
    int       NumChannels;
    int       month;
    int       day;
    int       TimeWords[8];
    int       RefWords[3];
    int       PlayPoint;
    int       assign;
    int       milli;
    int       debug;
    int       InputChan;
    int       OutputChan;
    int       change = 0;
    int       sync;
    int       live_vid;

    static int QueueReceived = 0;
    static int speed_received = 0;
    static int InitialSpeed = 0;


    UINT16    event;
    UINT16    DataW16;
    UINT16    PlayType;
    UINT16    PlaySpeed;

    UINT32    CSR;

    INT64     tmp;
    GSBTime   time;

    char      Time[32];


    int       chan;

    M23_GetDebugValue(&debug);

    M23_RecorderState(&state);

    //Cmd = M23_M1553_ReadCondor(MESSAGE_RECEIVE_DATA1);
    Cmd = R_SA1_Buffer->Data[0];

    switch(Cmd)
    {
        case M1553_CMD_ASSIGN:
if(debug)
    printf("CMD_ASSIGN\r\n");

            /*Read Ouput Channel Number*/
            DataW1 = R_SA1_Buffer->Data[1];
            DataW2 = R_SA1_Buffer->Data[2];
            
            InputChan = VideoIds[DataW2-1];
            M23_PerformAssign(OutputChan,InputChan);
            M23_SetAssign(DataW2);
            CurrentError = 0x0;
            if(LiveVidStatus == LIVE_STATUS)
            {
                M23_SetLiveVideo(DataW2);
            }

            break;
        case M1553_CMD_BIT:
if(debug)
    printf("CMD_BIT\r\n");
            return_status = BuiltInTest();
            if(return_status == NO_ERROR)
            {
                CurrentError = 0x0;
            }
            else
            {
                CurrentError = ERROR_BIT;
            }

            break;

        case M1553_CMD_DECLASSIFY:
if(debug)
    printf("CMD_DECLASS\r\n");

            return_status = DiskIsConnected();

            if(return_status)
            {
                return_status = Declassify();
                if(return_status == NO_ERROR)
                {
                    M23_SetDeclassSent();
                    M23_SetDeclassLED();
                    CurrentError = 0x0;
                }
                else
                {
                    CurrentError = ERROR_BIT;
                }
            }
            else
            {
                CurrentError = ERROR_BIT;
            }

            break;

        case M1553_CMD_ERASE:
if(debug)
    printf("CMD_ERASE\r\n");

            return_status = DiskIsConnected();
            
            if(return_status)
            {
                if( (state == STATUS_ERROR) || (state == STATUS_IDLE) )
                {

                    return_status = Erase();
                    if(return_status == NO_ERROR)
                    {
                        if(debug)
                            printf("M1553 CMD_ERASE setting to IDLE\r\n");
                        M23_SetRecorderState(STATUS_IDLE);
                        M23_SetEraseLED();
                        CurrentError = 0x0;
                    }
                    else
                    {
                        CurrentError = ERROR_BIT;
                         M23_SetRecorderState(STATUS_ERROR);
                    }

                    if(LoadedTmatsFromCartridge == 1)
                    {
                        M23_ClearEraseLED();
                    }

                }

            }
            else
            {
                CurrentError = ERROR_BIT;
            }

            break;

        case M1553_CMD_EVENT:
if(debug)
    printf("CMD_EVENT\r\n");
            DataW1 = R_SA1_Buffer->Data[1];

            //event  = (UINT16)(DataW1 >> 11);
            event  = (UINT16)(DataW1 & 0x1F);
            return_status = EventSet(event+1);
            if(return_status == NO_ERROR)
            {
                CurrentError = 0x0;
            }
            else
            {
                CurrentError = ERROR_BIT;
            }

            break;

        case M1553_CMD_INFO:
if(debug)
    printf("CMD_INFO\r\n");
            DataW1 = R_SA1_Buffer->Data[1];
            DataW2 = 0xFF;

            if( (DataW1 >> 15) == 0x1 ) /*Get Event Occurence*/
            {
                /*Read  Occurence Number*/
                DataW2 = R_SA1_Buffer->Data[2];
            }

             CurrentError = 0x0;
             ProcessAndRespondToInfo(DataW1,DataW2);

            break;

        case M1553_CMD_PAUSE:
if(debug)
    printf("CMD_PAUSE\r\n");
            //DataW1    = M23_M1553_ReadCondor(MESSAGE_RECEIVE_DATA2);
            DataW1 = R_SA1_Buffer->Data[1];

            DataW16 = ((DataW1 >> 12) & 0x7 );
            if(DataW16 == 0x0)
            {
                M23_PauseAllVideo();
            }
            else if(DataW16 == 0x1)
            {
                M23_PauseAllM1553();
            }
            else if(DataW16 == 0x2)
            {
                M23_PauseAllAnalog();
            }
            else if(DataW16 == 0x6)
            {
                /*We need to get the Number of Channels*/
                //CommandW = M23_M1553_ReadCondor(MESSAGE_RECEIVE_CW);
                CommandW = R_SA1_Buffer->RT_CommandWord;
                NumChannels = (CommandW & 0xFF);

                for(i = 0; i < NumChannels;i++)
                {
                    //DataW2 = M23_M1553_ReadCondor(MESSAGE_RECEIVE_DATA3);
                    DataW2 = R_SA1_Buffer->Data[i+2];
                    DataW16 = (DataW2 & 0xF);
                    if(DataW16 > 0)
                    {
                        PauseChannel(DataW16);
                    }
                }
            }
            else if(DataW16 == 0x7)
            {
                M23_PauseAllVideo();
                M23_PauseAllM1553();
                M23_PauseAllAnalog();
            }

            CurrentError = 0x0;

            break;
        case M1553_CMD_REPLAY:
if(debug)
    printf("CMD_REPLAY\r\n");

            LiveVidStatus = 0;
            DataW16 = GetNumberOfFiles();
            if(LoadedTmatsFromCartridge == 1)
            {
                if(DataW16 == 1)
                {
                    CurrentError = ERROR_BIT;
                    return;
                }
            }
            else
            {
                if(DataW16 == 0)
                {
                    CurrentError = ERROR_BIT;
                    return;
                }
            }

            //DataW1    = M23_M1553_ReadCondor(MESSAGE_RECEIVE_DATA2);
            DataW1    = R_SA1_Buffer->Data[1];

            PlayType  = (DataW1 >>12);
            PlaySpeed = ((DataW1 >>8) & 0x0F);

            CurrentError = 0x0;

            return_status = DiskIsConnected();
            
            if(return_status)
            {
                M23_GetAssign(&assign);
                if(assign == 1)
                {
                    if(PlaySpeed == 0x0)
                    {
                        M23_CS6651_Pause(0);
                        PlaySpeedStatus = 0;
                        speed_received = 1; 
                    }
                    else
                    {
  
                        if(PlayType == 0)
                        {
                            if( (state == STATUS_IDLE) || (state == STATUS_ERROR) || (state == STATUS_RECORD) )
                            {
                                change = 1;
                            }
                        }
                        else
                        {
                            change = 1;
                        }


                        if(PlayType == 0x0)
                        {
                            if( (state == STATUS_IDLE) || (state == STATUS_ERROR) || (state == STATUS_RECORD) )
                            {
                                //TimeWords[0] = M23_M1553_ReadCondor(MESSAGE_RECEIVE_DATA3); 
                                TimeWords[0]   = R_SA1_Buffer->Data[2];
                                //TimeWords[1] = M23_M1553_ReadCondor(MESSAGE_RECEIVE_DATA4); 
                                TimeWords[1]   = R_SA1_Buffer->Data[3];
                                //TimeWords[2] = M23_M1553_ReadCondor(MESSAGE_RECEIVE_DATA5); 
                                TimeWords[2]   = R_SA1_Buffer->Data[4];
                                //TimeWords[3] = M23_M1553_ReadCondor(MESSAGE_RECEIVE_DATA6); 
                                TimeWords[3]   = R_SA1_Buffer->Data[5];

                                /*Get Microseconds*/
                                time.Microseconds = (TimeWords[0] & 0x000F) * 10; //Tens of milliseconds
                                time.Microseconds += (TimeWords[0]>>4) & 0x000F; //Hundreds of milliseconds
                                time.Microseconds = time.Microseconds * 1000;

                                /*Get Seconds*/
                                time.Seconds = (TimeWords[0]>>8) & 0x000F; //Units of seconds
                                time.Seconds += ( (TimeWords[0] >> 12) & 0x000F) * 10; //Tens of seconds

                                /*Get Minutes*/
                                time.Minutes = TimeWords[1] & 0x000F; //Units of Minutes
                                time.Minutes += ( (TimeWords[1] >> 4) & 0x000F) * 10; //Tens of minutes

                                /*Get Hours*/
                                time.Hours = (TimeWords[1]>>8) & 0x000F; //Units of Hours
                                time.Hours += ( (TimeWords[1] >> 12) & 0x000F) * 10; //Tens of Hours

                                /*Get Months*/
                                month = (TimeWords[2]>>8) & 0x000F; //Units of Months
                                month += ( (TimeWords[2] >> 12) & 0x000F) * 10; //Tens of Months

                                /*Get Years*/
                                time.Years    = TimeWords[3] & 0x000F; //Units of Years
                                time.Years    += ( (TimeWords[3] >> 4) & 0x000F) * 10; //Tens of Years
                                time.Years    += ( (TimeWords[3] >> 8) & 0x000F) * 100; //Hundreds of Years
                                time.Years    += ( (TimeWords[3] >> 12) & 0x000F) * 1000; //Thousands of Years

                                if( (time.Years %4) == 0)
                                {
                                    days_in_month[1] = 29;
                                }
                                else
                                {
                                    days_in_month[1] = 28;
                                }

                                /*Get Days*/
                                time.Days = TimeWords[2] & 0x000F; //Units of Days
                                time.Days += ((TimeWords[2]>> 4) & 0x000F) * 10; //Tens of Days
                                day = time.Days;
                                for(i = 0; i < (month - 1);i++)
                                {
                                    time.Days += days_in_month[i];
                                }

                                PlayPoint = M23_FindTimeBlock(time);
if(debug)
    printf("Replay Time->  %03ld-%02ld:%02ld:%02ld.%03ld -> %d\r\n",time.Days,time.Hours,time.Minutes,time.Seconds,time.Microseconds,PlayPoint);
                                if(PlayPoint > (START_OF_DATA - 1) )
                                {
                                    M23_StartPlayback(PlayPoint);
                                }
                                else
                                {
                                    CurrentError = ERROR_BIT;
                                }
                            }
                            else
                            {
                                CurrentError = ERROR_BIT;
                                change = 0;
                            }
                        }
                        else if(PlayType == 0x1) /*Live Video Playback*/
                        {
                            if((state == STATUS_PLAY) || (state == STATUS_REC_N_PLAY) )
                            {
                                M23_StopPlayback(0);
                                M23_CS6651_Normal();
                            }

                            M23_GetAssignedChan(&live_vid);
                            M23_SetLiveVideo(live_vid);
                            LiveVidStatus = LIVE_STATUS;

                        }
                        else if(PlayType == 0x2)
                        {
                            if((state == STATUS_PLAY) || (state == STATUS_REC_N_PLAY) )
                            {
                                //M23_CS6651_Pause(1);
                            }
                            else
                            {
                                if( (state == STATUS_IDLE) || (state == STATUS_ERROR) || (state == STATUS_RECORD) )
                                {
                                    sGetPlayLocation(&PlayPoint);
                                    M23_StartPlayback(PlayPoint);
                                }
                            }
                        }

                        if(change)
                        {
                            if(PlaySpeed == 0x1)
                            {
                                if(speed_received == 1)
                                {
if(debug)
 printf("Play at Normal Speed\r\n");
                                    M23_CS6651_Normal();
                                    speed_received = 0;
                                }

                                PlaySpeedStatus = 0x1;
                            }
                            else if(PlaySpeed == 0x2)
                            {
                                if(debug)
                                    printf("Play at Greater than 2x Speed\r\n");

                                if(InitialSpeed == 0)
                                {
                                    usleep(500000);
                                    InitialSpeed = 1;
                                }

                                //M23_CS6651_Normal();
                                M23_CS6651_03X();
                                PlaySpeedStatus = 0x2;
                                speed_received = 1; 
                            }
                            else if(PlaySpeed == 0xE)
                            {
                                if(debug)
                                    printf("Play at .5x Speed\r\n");

                                if(InitialSpeed == 0)
                                {
                                    usleep(500000);
                                    InitialSpeed = 1;
                                }

                                //M23_CS6651_Normal();
                                M23_CS6651_SlowX(2);
                                PlaySpeedStatus = 0xE;
                                speed_received = 1; 
                            }
                            else if(PlaySpeed == 0xF)
                            {
                                if(debug)
                                    printf("Play at .030x Speed\r\n");

                                if(InitialSpeed == 0)
                                {
                                    usleep(500000);
                                    InitialSpeed = 1;
                                }

                                //M23_CS6651_Normal();
                                M23_CS6651_SlowX(30);
                                PlaySpeedStatus = 0xF;
                                speed_received = 1; 
                            }
                        }

                    }

                }
                else
                {
                    CurrentError = ERROR_BIT;
                }
            }
            else
            {
                if(TmatsLoaded == 0)
                {
                    CurrentError = ERROR_BIT;
                }
                else
                {
                    M23_GetAssign(&assign);
                    if(assign == 1)
                    {
                        if(PlayType == 0x1) /*Live Video Playback*/
                        {
                            if((state == STATUS_PLAY) || (state == STATUS_REC_N_PLAY) )
                            {
                                M23_StopPlayback(0);
                                M23_CS6651_Normal();
                            }

                            M23_GetAssignedChan(&live_vid);
                            M23_SetLiveVideo(live_vid);
                            LiveVidStatus = LIVE_STATUS;
                        }
                        else
                        {
                            CurrentError = ERROR_BIT;
                        }
                    }
                    else
                    {
                        CurrentError = ERROR_BIT;
                    }
                }
            }

            break;
        case M1553_CMD_PUBLISH:
            break;
        case M1553_CMD_QUEUE:
if(debug)
    printf("CMD_QUEUE\r\n");

            CurrentQueueStatus = QUEUE_IN_PROGRESS;

            DataW16 = GetNumberOfFiles();
            if(LoadedTmatsFromCartridge == 1)
            {
                if(DataW16 == 1)
                {
                    CurrentError = ERROR_BIT;
                    CurrentQueueStatus = QUEUE_FAIL;
                    return;
                }
            }
            else
            {
                if(DataW16 == 0)
                {
                    CurrentError = ERROR_BIT;
                    CurrentQueueStatus = QUEUE_FAIL;
                    return;
                }
            }

            //DataW1 = M23_M1553_ReadCondor(MESSAGE_RECEIVE_DATA2);
           if( (state == STATUS_IDLE) || (state == STATUS_ERROR) || (state == STATUS_RECORD) )
           {
                return_status = DiskIsConnected();
            
                if(return_status)
                {
                    CurrentError = 0x0;
                    DataW1    = R_SA1_Buffer->Data[1];
           
                    if( DataW1 & 0x8000) //Queue Event
                    {
                        //event = ((DataW1 >> 7) & 0x7F);
                        event = (DataW1 & 0x3FF);
                        DataW16 = R_SA1_Buffer->Data[2];
                        QueueReceived  = 1;
                        PlayPoint = M23_FindEventBlock(event,DataW16);
                        if(PlayPoint > (START_OF_DATA - 1) )
                        {
                            DiskPlaybackSeek(PlayPoint);
                            CurrentQueueStatus = QUEUE_PASS;
                        }
                        else
                        {
                            CurrentError = ERROR_BIT;
                            CurrentQueueStatus = QUEUE_FAIL;
                        }
                        if(debug)
                            printf("Queued Event %d Occurence %d is at %d\r\n",event,DataW16,PlayPoint);
                    }
                    else  //Queue File
                    {
                        //DataW16 = (DataW1 >> 7);
                        DataW16 = (DataW1 & 0x3FF);

                        /*If the TMATS was loaded from the cartridge, do count it, start with the second file*/
                        if(LoadedTmatsFromCartridge == 1)
                        {
                            DataW16++;
                        }

                        return_status = M23_GetBlockFromFileNumber(DataW16,&PlayPoint);

                        if(return_status == 0)
                        {
                            DiskPlaybackSeek(PlayPoint);
                            CurrentQueueStatus = QUEUE_PASS;
                        }
                        else
                        {
                            CurrentError = ERROR_BIT;
                            CurrentQueueStatus = QUEUE_FAIL;
                        }

                        if(debug)
                            printf("Queued File %d,Block = %d\r\n",DataW16,PlayPoint);

                        QueueReceived  = 0;
                    }
                }
                else
                {
                    CurrentError = ERROR_BIT;
                    CurrentQueueStatus = QUEUE_FAIL;
                }
            }
            else
            {
                CurrentError = ERROR_BIT;
                CurrentQueueStatus = QUEUE_FAIL;
            }


            break;
        case M1553_CMD_RECORD:
if(debug)
    printf("CMD_RECORD\r\n");

            return_status = DiskIsConnected();

            if(return_status)
            {
                return_status = Record();
                if(return_status == NO_ERROR)
                {
                    CommandFrom = FROM_M1553;
                    CurrentError = 0x0;
                }
                else
                {
                    CurrentError = ERROR_BIT;
                }
            }
            else
            {
                CurrentError = ERROR_BIT;
            }

            break;
        case M1553_CMD_RESET:
            M23_ResetSystem();
            break;
        case M1553_CMD_RESUME:
if(debug)
    printf("CMD_RESUME\r\n");
            CurrentError = 0x0;
            DataW1    = R_SA1_Buffer->Data[1];

            //DataW16 = (DataW1 >> 12);

            DataW16 = ((DataW1 >> 12) & 0x7 );

            if(DataW16 == 0x0)
            {
                M23_ResumeAllVideo();
            }
            else if(DataW16 == 0x1)
            {
                M23_ResumeAllM1553();
            }
            else if(DataW16 == 0x2)
            {
                M23_ResumeAllAnalog();
            }
            else if(DataW16 == 0x6)
            {
                /*We need to get the Number of Channels*/
                //CommandW = M23_M1553_ReadCondor(MESSAGE_RECEIVE_CW);
                CommandW = R_SA1_Buffer->RT_CommandWord;
                NumChannels = (CommandW & 0xFF);

                for(i = 0; i < NumChannels;i++)
                {
                    DataW2 = R_SA1_Buffer->Data[i+2];
                    DataW16 = (DataW2 & 0xF);
                    if(DataW16 > 0)
                    {
                        ResumeChannel(DataW16);
                    }
                }
            }
            else if(DataW16 == 0x7)
            {
                //printf("M1553 Resume Recording\r\n");
                M23_ResumeAllAnalog();
                M23_ResumeAllM1553();
                M23_ResumeAllVideo();
            }

            break;

        case M1553_CMD_STOP:
            CurrentError = 0x0;

            if((state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY)  || (state == STATUS_PLAY) )
            {
                //DataW1 = M23_M1553_ReadCondor(MESSAGE_RECEIVE_DATA2);
                DataW1 = R_SA1_Buffer->Data[1];

                DataW1 = (DataW1 >> 14);

                if( (DataW1 == M1553_STOP_MODE) || (DataW1 == M1553_STOP_AND_REPLAY_MODE) )
                {
                    if((state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) )
                    {
                        Stop(0);
                        CommandFrom = NO_COMMAND;
                        M23_SetEndOnly();
                        
                    }
                }
                if( (DataW1 == M1553_REPLAY_MODE) || (DataW1 == M1553_STOP_AND_REPLAY_MODE) )
                {
                    if( (state == STATUS_PLAY) || (state == STATUS_REC_N_PLAY) )
                    {
                        M23_StopPlayback(0);
                        M23_CS6651_Normal();
                        M23_SetLiveVideo(0);
                    }
                } 
            }
            else
            {
                CurrentError = ERROR_BIT;
            }

            break;
        case M1553_CMD_TIME:

            /*Read The Validity of the time Command*/
            DataW1 = R_SA1_Buffer->Data[1];

            //DataW16 = (DataW1 >> 12);

            //if( DataW16 == 0x4 ) //Set Time
            if( DataW1 & 0x8000 ) //Set Time
            {
                /*Get the Data From the R2 Sync*/
                RefWords[0] = R_R2_Buffer->RT_TimeTag1;
                RefWords[1] = R_R2_Buffer->RT_TimeTag2;
                RefWords[2] = R_R2_Buffer->RT_TimeTag3;
                RT2 = RefWords[0];
                RT2 |= ( ((INT64)RefWords[1]) << 16);
                RT2 |= ( ((INT64)RefWords[2]) << 32);

                ST3 = R_R2_Buffer->Data[0];

                /*Get Time Of Validity*/
                ST4 = R_SA1_Buffer->Data[2];
                
                TimeWords[0]   = R_SA1_Buffer->Data[3];
                TimeWords[1]   = R_SA1_Buffer->Data[4];
                TimeWords[2]   = R_SA1_Buffer->Data[5];
                TimeWords[3]   = R_SA1_Buffer->Data[6];

                //RefWords[0] = R_SA1_Buffer->RT_TimeTag1;
                //RefWords[1] = R_SA1_Buffer->RT_TimeTag2;
                //RefWords[2] = R_SA1_Buffer->RT_TimeTag3;
              
                /*Get Microseconds*/
                time.Microseconds = (TimeWords[0] & 0x000F) * 10; //Tens of milliseconds
                time.Microseconds += (TimeWords[0]>>4) & 0x000F; //Hundreds of milliseconds
                milli              = time.Microseconds;
                time.Microseconds = time.Microseconds * 1000;

                /*Get Seconds*/
                time.Seconds = (TimeWords[0]>>8) & 0x000F; //Units of seconds
                time.Seconds += ( (TimeWords[0] >> 12) & 0x000F) * 10; //Tens of seconds

                /*Get Minutes*/
                time.Minutes = TimeWords[1] & 0x000F; //Units of Minutes
                time.Minutes += ( (TimeWords[1] >> 4) & 0x000F) * 10; //Tens of minutes

                /*Get Hours*/
                time.Hours = (TimeWords[1]>>8) & 0x000F; //Units of Hours
                time.Hours += ( (TimeWords[1] >> 12) & 0x000F) * 10; //Tens of Hours

                /*Get Months*/
                month = (TimeWords[2]>>8) & 0x000F; //Units of Months
                month += ( (TimeWords[2] >> 12) & 0x000F) * 10; //Tens of Months

                /*Get Years*/
                time.Years    = TimeWords[3] & 0x000F; //Units of Years
                time.Years    += ( (TimeWords[3] >> 4) & 0x000F) * 10; //Tens of Years
                time.Years    += ( (TimeWords[3] >> 8) & 0x000F) * 100; //Hundreds of Years
                time.Years    += ( (TimeWords[3] >> 12) & 0x000F) * 1000; //Thousands of Years
 
                /*Set the System Year*/
                M23_SetYear(time.Years);

                if( (time.Years %4) == 0)
                {
                    days_in_month[1] = 29;
                }
                else
                {
                    days_in_month[1] = 28;
                }

                /*Get Days*/
                time.Days = TimeWords[2] & 0x000F; //Units of Days
                time.Days += ((TimeWords[2]>> 4) & 0x000F) * 10; //Tens of Days
                day = time.Days;
                for(i = 0; i < (month - 1);i++)
                {
                    time.Days += days_in_month[i];
                }

                memset(Time,0x0,32);
                sprintf(Time,"%03d %02d:%02d:%02d.%03d",time.Days,time.Hours,time.Minutes,time.Seconds,milli);
#if 0

                RefTime = RefWords[0];
                RefTime |= ( ((INT64)RefWords[1]) << 16);
                RefTime |= ( ((INT64)RefWords[2]) << 32);
#endif

                if ((ST4 > 0xB1E0) && (ST3 <= 0xB1E0))
                    ST3 |= 0x10000;
                else if ((ST4 <= 0xB1E0) && (ST3 > 0xB1E0))
                    ST4 |= 0x10000;

                sync = ST4 - ST3;

                TOD2 = ( ( ( (time.Hours * 60 * 60) + (time.Days * 60) + time.Seconds) -1) << 14);

                tmp = (INT64)(0x4000 - (TOD2 & 0x3FFF) );
                tmp = tmp * 10000000;
                tmp = (tmp >>14);
                RefTime = (INT64)(  (RT2 + ((INT64)(sync * 500) )) + tmp);

                M23_SetSystemTime(Time);
            }

            break;
    }
}
 
int M23_M1553_F15_Time(int device,int channel,int state)
{
    int    i;
    int    debug;

    int    hour;
    int    minutes;
    int    seconds;
    int    sync;
    int    valid;
    int    BlockStatus;

    int    record_state;
    int    in_record = 0;

    int    return_status;
    char   Time[32];

    INT64  M1553Relative;
    INT64  tmp;

    UINT32 TimeWords[20];
    UINT32 CSR;
    UINT32 REL_Upper;
    UINT32 REL_Lower;
    UINT32 ABS_Upper;
    UINT32 ABS_Lower;

    GSBTime         time;


    M23_RecorderState(&record_state);

    while(1)
    {
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_OFFSET);
        if(CSR & MP_M1553_CAPTURE_ONE)
        {
            usleep(1000);
        }
        else
        {
            break;
        }
    }

    i = 0;
    while(1)
    {
        CSR = 0;
        M23_mp_write_csr(device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_MESSAGE_FIFO);
        CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_MESSAGE_FIFO);
        //if( (CSR >> 16) & 0x1FF)  //Still Data in FIFO
        if(CSR & 0x10000000)
        {
            TimeWords[i++] = CSR & 0xFFFF;
            if(i > 16)
            {
                break;
            }
        }
        else
        {
            break;
        }
    }
    /*Check the block Status*/
    BlockStatus = TimeWords[4];
    if(BlockStatus & 0x1FF)
    {
        return 0;
    }

    M23_GetDebugValue(&debug);


    //if( ( (TimeWords[12] & 0xF000) >> 12) > 0)

    valid = ( (TimeWords[12] & 0xF000) >> 12);
    if( (valid > 0) && (valid < 8) )
    {

if(debug)
{
    CSR = M23_mp_read_csr(device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_M1553_CAPTURE_OFFSET);
    printf("Time Valid -> Event CSR is 0x%x\r\n",CSR);
}
        if(state == 0)
        {
            return(1);
        }
        /*Set the M1553 Relative Time*/
        M1553Relative = TimeWords[0];
        M1553Relative |= ( ((INT64)TimeWords[1]) << 16);
        M1553Relative |= ( ((INT64)TimeWords[2]) << 32);
        M1553Relative |= ( ((INT64)TimeWords[3]) << 48);

        /*Get Seconds*/
        time.Seconds =  (TimeWords[10]>>8) & 0x000F; //Units of seconds
        time.Seconds += (( (TimeWords[10]>>12) & 0x000F) * 10); //Tens of seconds

        /*Get Minutes*/
        time.Minutes =  (TimeWords[9]) & 0x000F; //Units of Minutes
        time.Minutes += ( (TimeWords[9]>>4) & 0x000F) * 10; //Tens of minutes

        /*Get Hours*/
        time.Hours =  (TimeWords[9]>>8) & 0x000F; //Units of Hours
        time.Hours += ( (TimeWords[9]>>12) & 0x000F) * 10; //Tens of Hours

        /*Get Julian Day*/
        time.Days =   ((TimeWords[11] >> 12) & 0x000F); //Units of Days
        time.Days +=  (((TimeWords[10]) & 0x000F) * 10); //Tens of Days
        time.Days +=  (((TimeWords[10] >> 4) & 0x000F) * 100); //Hundredss of Days


        hour    = ( ( (TOD1 >> 14) +1)  / 3600);
        minutes = ( ( ( (TOD1 >> 14) +1) % 3600) / 60);
        seconds = ( ( (TOD1 >> 14) +1) % 60);

        if(hour != time.Hours)
        {
            return(-3);
        }

        memset(Time,0x0,32);
        //sprintf(Time,"%03ld %02ld:%02ld:%02ld.100\r\n",time.Days,time.Hours,time.Minutes,time.Seconds);
        //SPrintf(Time,"%03ld %02ld:%02ld:%02ld.000\r\n",time.Days,hour,minutes,seconds);
        sprintf(Time,"%03d %02d:%02d:%02d.000",time.Days,hour,minutes,seconds);

        //sync = ST2 - ST1;

        //LWB 2007-06-18
        //The following code deals with rollover conditions.
        //It also deviates from previous attempts since it handles both conditions
        //Namely that R0 occurs before T9 and the reverse.
        //0xB1E0 is 1s less than the maximum the register can actually hold.  Sorry for the magic numbers.
        if ((ST2 > 0xB1E0) && (ST1 <= 0xB1E0))
            ST1 |= 0x10000;
        else if ((ST2 <= 0xB1E0) && (ST1 > 0xB1E0))
            ST2 |= 0x10000;

        sync = ST2 - ST1;

        //This check is necessary since the above conditions may be so close to the max
        //that they slip through and cause a massive > 1s error.
        if ((sync > 20000) || (sync < -20000))
            return(-3);

#if 0
        sync = ST1 - ST2;
        if(sync < 0)
        {
            sync *=  (-1);
        }
#endif
        tmp = (INT64)(0x4000 - (TOD1 & 0x3FFF) );
        tmp = tmp * 10000000;
        tmp = (tmp >>14);
        M1553Relative = (INT64)(  (RT1 + ((INT64)(sync * 500) )) + tmp);

        //M1553Relative = (INT64)(  (RT1 - ((INT64)(sync * 500) )) + (INT64)(( (0x4000 - (TOD1 & 0x3FFF)) * 10000000 ) >> 14)  );
        RefTime = M1553Relative;



        if(TimeJammed == 0)
        {

            M23_SetSystemTime(Time);
            if(debug)
            {
                printf("1553 MT Time->  %s\r\n",Time);
                printf("sync time Tag 0x%x,utc_time tag 0x%x\r\n",ST1,ST2);
                printf("TOD from T9  0x%x\r\n",TOD1);
                printf("Rel Time  %lld, RT1 %lld\r\n",M1553Relative,RT1);
                printf("Delta  %d\r\n",sync);
            }

            M23_SetTimeLED();

            TimeJammed = 1;

            sleep(1);

            if(record_state == STATUS_RECORD)
            {
                in_record = 1;
                Stop(0);
            }

            while(1)
            {
                /*Check if time has been jammed*/
                CSR = M23_ReadControllerCSR(CONTROLLER_JAM_TIME_REL_HIGH);
                if(CSR & TIME_JAM_BUSY_BIT)
                {
                    usleep(500000);
                }
                else
                {
                    CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
                    CSR &= ~TIMECODE_RMM_JAM;
                    CSR |= TIMECODE_M1553_JAM;
                    M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);

                    if(in_record == 1)
                    {
                        return_status = Record();
                    }
                    break;
                }

            }


            if(BusTimeSyncEvent > 0 )
            {
                REL_Lower = (UINT32)(M1553Relative);
                REL_Upper = (UINT32)(M1553Relative >> 32);

                ABS_Upper  = time.Days;

                ABS_Lower  = 0;
                ABS_Lower |= (hour/10) << 28;
                ABS_Lower |= (hour % 10) << 24;
                ABS_Lower |= (minutes/10) << 20;
                ABS_Lower |= (minutes % 10) << 16;
                ABS_Lower |= (seconds/10) << 12;
                ABS_Lower |= (seconds % 10) << 8;

                m23_Update_M1553_Event(BusTimeSyncEvent,REL_Upper,REL_Lower, ABS_Upper,ABS_Lower);
            }

        }
    }
    else
    {
        return(0);
    }

    return(0);
}

int M23_M1553_B1B_Time(int device,int channel,int state)
{
    int    i;
    int    debug;

    int    hour;
    int    minutes;
    int    seconds;
    int    sync;
    int    valid;
    int    count = 0;
    int    BlockStatus;

    int    record_state;
    int    in_record;

    int    return_status;
    char   Time[32];

    INT64  M1553Relative;
    INT64  tmp;

    UINT32 TimeWords[20];
    UINT32 CSR;
    UINT32 REL_Upper;
    UINT32 REL_Lower;
    UINT32 ABS_Upper;
    UINT32 ABS_Lower;

    GSBTime         time;

    M23_RecorderState(&record_state);

    hour    = ( ( (TOD1 >> 14) +1)  / 3600);
    minutes = ( ( ( (TOD1 >> 14) +1) % 3600) / 60);
    seconds = ( ( (TOD1 >> 14) +1) % 60);


    memset(Time,0x0,32);
    sprintf(Time,"%03d %02d:%02d:%02d.000",DayOfYear,hour,minutes,seconds);


    //LWB 2007-06-18
    //The following code deals with rollover conditions.
    //It also deviates from previous attempts since it handles both conditions
    //Namely that R0 occurs before T9 and the reverse.
    //0xB1E0 is 1s less than the maximum the register can actually hold.  Sorry for the magic numbers.
    if ((ST2 > 0xC2F7) && (ST1 <= 0xC2F7))  //C2F7 is 65536 - (1000000/64)
        ST1 |= 0x10000;
    else if ((ST2 <= 0xC2F7) && (ST1 > 0xC2F7))
        ST2 |= 0x10000;


    sync = ST2 - ST1;

    //This check is necessary since the above conditions may be so close to the max
    //that they slip through and cause a massive > 1s error.
    if ((sync > 15625) || (sync < -15625)) //15625 is 1000000/64
        return(-3);


    tmp = (INT64)(0x4000 - (TOD1 & 0x3FFF) );
    tmp = tmp * 10000000;
    tmp = (tmp >>14);
    M1553Relative = (INT64)(  (RT1 + ((INT64)(sync * 640) )) + tmp);

    RefTime = M1553Relative;

    if(TimeJammed == 0)
    {
        M23_SetSystemTime(Time);
        if(debug)
        {
            printf("1553 MT Time->  %s\r\n",Time);
            printf("sync time Tag 0x%x,utc_time tag 0x%x\r\n",ST1,ST2);
            printf("TOD from T9  0x%x\r\n",TOD1);
            printf("Rel Time  %lld, RT1 %lld\r\n",M1553Relative,RT1);
            printf("Delta  %d\r\n",sync);
        }


        TimeJammed = 1;
        sleep(1);

        if(record_state == STATUS_RECORD)
        {
            in_record = 1;
            Stop(0);
        }

        while(1)
        {
            /*Check if time has been jammed*/
            CSR = M23_ReadControllerCSR(CONTROLLER_JAM_TIME_REL_HIGH);
            if(CSR & TIME_JAM_BUSY_BIT)
            {
                usleep(500000);
            }
            else
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
                CSR &= ~TIMECODE_RMM_JAM;
                CSR |= TIMECODE_M1553_JAM;
                M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);

                if(in_record == 1)
                {
                    return_status = Record();
                }

                break;
            }
        }
    }
    else
    {
        return(0);
    }
}


/**********************************************
 *         M1553 Time Sync Thread             *
 *********************************************/
void M23_M1553_LoadTimeEvent(int Event)
{
    int    debug;
    UINT32 CSR;
    M23_CHANNEL *config;

    M23_GetDebugValue(&debug);

    /*Get the First Time if it is Enbaled*/
    SetupGet(&config);
    if(config->timecode_chan.Source == TIMESOURCE_IS_M1553)
    {

        if(config->timecode_chan.m1553_timing.Format == F15_EGI)
        {
            CSR = (UINT16)Event;
            CSR |= MP_M1553_CAPTURE_ONE;
            M23_mp_write_csr(M1553Device,1,CSR,(MP_CHANNEL_OFFSET * M1553Channel) + MP_M1553_CAPTURE_OFFSET);
        }
        else if(config->timecode_chan.m1553_timing.Format == B1B_EGI)
        {
            CSR = (UINT16)Event;
            CSR |= MP_M1553_CAPTURE_ONE;
            M23_mp_write_csr(M1553Device,1,CSR,(MP_CHANNEL_OFFSET * M1553Channel) + MP_M1553_CAPTURE_OFFSET);
        }
    }
}

void M23_M1553_Cmd_Processor()
{
    int           debug;
    static int    index = 0;
    int           prev;
    int           CSR;
    int           count = 0;
    int           i;

    int           offset;

    while(1)
    {
        CSR = M23_M1553_ReadCondor(INTERRUPT_Q_INDEX);

        if(CSR != index )
        {

            if(CSR > index)
            {
                count = (CSR - index)/4;
            }
            else //We have rolled over
            {
                if(CSR == index) //We have the max interrupts of 4
                {
                    count = 4;
                }
                else if( (index - CSR) > 8) 
                {
                    count = 1;
                }
                else if( (index - CSR) > 4) 
                {
                    count = 2;
                }
                else if( (index - CSR) > 0) 
                {
                    count = 3;
                }
            }
            prev  = index;
            index = CSR;

            for(i = 0; i < count; i++)
            {
                if( (prev + (i * 4)) > 12)
                {
                    if( (prev + (i*1)) == 16)
                    {
                        offset = 0;
                    }
                    else if( (prev + (i*1)) == 20)
                    {
                        offset = 4;
                    }
                    else if( (prev + (i*1)) == 24)
                    {
                        offset = 8;
                    }
              
                }
                else
                {
                    offset = (prev + (i*4) );
                }

                //CSR = M23_M1553_ReadCondor(INTERRUPT_Q);
                CSR = M23_M1553_ReadCondor( ((INTERRUPT_Q_OFFSET + offset) << 2) );
           

                if(debug)
                    printf("Int Q 0x%x\r\n",CSR);

                if( (CSR & RT_INTERRUPT) && ((CSR & INFO_INTERRUPT) == 0) && ((CSR & STATUS_INTERRUPT) == 0) )//We have an RT interrupt
                {
                    M23_GetDebugValue(&debug);

                    M23_M1553_GetCommand();
                }

                if(CSR & STATUS_INTERRUPT) //We have an User interrupt for Status(SA6)
                {
                   // T_SA6_Buffer->Data[0] = 0x0;
                    T_SA6_Buffer->Data[0] &= ~STATUS_VALID;

                    if(CurrentQueueStatus != QUEUE_IN_PROGRESS)
                    {
                        CurrentQueueStatus = QUEUE_NO_STATUS;
                    }

                }

                if(CSR & INFO_INTERRUPT) //We have an User interrupt
                {
                    T_SA5_Buffer->Data[0] = 0x0; //Invalidate INFO Data 
                    T_SA5_Buffer->Data[9] = 0x0;
                    T_SA5_Buffer->RT_InterruptEnable2 = 0x0;
                }
            }

        //   M23_M1553_WriteCondor(CONTROL_REGISTER,CONDOR_SETUP);
        
        }

        usleep(50000);
    }

}

void M23_M1553_LoadStatus()
{
    int           count = 0;

    while(1)
    {
        if(count > 20)
        {
            LoadTransmitData();
            count = 0;
        }
        else
        {
            count++;
        }

        usleep(50000);
    }
}


/**********************************************
 *         M1553 Event  Thread             *
 *********************************************/
void M23_M1553_Event()
{

    int    i;
    int    debug;
    int    NumEvents;


    UINT8  IsFirst;
    UINT8  EventMode;

    UINT16 EventData;
    UINT16 Sync;
    UINT16 Index;
    UINT16 CW;
    UINT16 WordNum;
    UINT16 Mask;
    UINT16 DisValue;


    UINT32 event;
    UINT32 CSR;
    UINT32 REL_Upper;
    UINT32 REL_Lower;
    UINT32 ABS_Upper;
    UINT32 ABS_Lower;

    UINT32 timeEvent;
    UINT32 DataEvent;



    static int T16_Received;

    EVENT_MSG   Event;

    M23_CHANNEL  const *config;

    SetupGet(&config);


    if(config->timecode_chan.m1553_timing.Format == B1B_EGI)
    {
        if(debug)
            printf("B1B 1553 Time Load Event T16\r\n");
        M23_M1553_LoadTimeEvent(B1_T16_EVENT);
    }
    if(config->timecode_chan.m1553_timing.Format == F15_EGI)
    {
        if(debug)
            printf("F15 1553 Time Load Event T20\r\n");
        M23_M1553_LoadTimeEvent(T20_EVENT);
    }


    while(1)
    {
        M23_GetDebugValue(&debug);

        CSR = M23_ReadControllerCSR(BUS_EVENT_FIFO_CSR);
        if(CSR & M1553_FRAME_AVAILABLE)
        {
            Sync = ReadMem16(M1553_EVENT_FIFO);

            //if(debug)
            //    printf("M1553 Sync = 0x%x\r\n",Sync);

            if(Sync == 0xEB26)
            {
                EventData        = ReadMem16(M1553_EVENT_FIFO);
                Event.IsDataWord = EventData >> 15;
                Event.TableEntry = EventData & 0xFF;
                Event.BusNumber  = ((EventData >> 8) & 0x7);

                Event.Rel1       = ReadMem16(M1553_EVENT_FIFO);
                Event.Rel2       = ReadMem16(M1553_EVENT_FIFO);


                Event.Rel3       = ReadMem16(M1553_EVENT_FIFO);

                Event.DataWord   = ReadMem16(M1553_EVENT_FIFO);

#if 0
                if(debug)
                    printf("Entry %d, Bus Number %d, Data Word 0x%x, Is Data %d\r\n",Event.TableEntry,Event.BusNumber,Event.DataWord,Event.IsDataWord);
#endif

                ABS_Lower =  M23_ReadControllerCSR(CONTROLLER_TIMECODE_BCD_LOWER_CSR);
                ABS_Upper =  M23_ReadControllerCSR(CONTROLLER_TIMECODE_BCD_UPPER_CSR);

                REL_Upper = Event.Rel3;
                REL_Lower = ((UINT32)Event.Rel2 << 16);
                REL_Lower |= Event.Rel1;

                if(Event.IsDataWord == 0x0)
                {
                    event = Event.DataWord;
                    //printf("M1553 Event Received %d\r\n",event);
                }
                else
                {
                    if(M23_IsTimeEvent(Event.TableEntry,Event.BusNumber,&EventData,&timeEvent) == 1)
                    {
                        TimeEvent.Rel1 = Event.Rel1;
                        TimeEvent.Rel2 = Event.Rel2;
                        TimeEvent.Rel3 = Event.Rel3;
                        TimeEvent.Data[EventData] = (UINT16)Event.DataWord;
                        event = timeEvent;
                    }
                    else if(M23_IsMultiEvent(Event.TableEntry,Event.BusNumber,&CW,&NumEvents) == 1)
                    {
                        for(i = 0; i < NumEvents;i++)
                        {
                            if(M1553Events[i].CommandWord == CW)
                            {
                                if(M23_M1553_IsDataConversion(Event.TableEntry,Event.BusNumber,&WordNum,&Index,&CW,&DataEvent,&Mask) == 1)
                                {
                                    event = 0xFFFFFFFF;

                                    //if(debug)
                                    //    printf("Table Entry %d,Bus %d,Wordnum %d,CW 0x%x,Value 0x%x\r\n",Event.TableEntry,Event.BusNumber,WordNum,CW,Event.DataWord);
                                    M23_M1553_FillInDataWord(Index,WordNum,CW,(UINT16)Event.DataWord,Mask);

                                    if(M23_M1553_IsTriggerEvent(Event.TableEntry,Event.BusNumber,Index) == 1)
                                    {
                                        event = M23_DoCalculation(Index,DataEvent,1);
                                    }

                                }
                                else //This is a Discrete Event
                                {
                                    DisValue = M1553Events[i].Mask & Event.DataWord;

                                    if(debug)
                                        printf("Discrete Event 0x%x, Value 0x%x, Mode %d,bus %d %d\r\n",CW,DisValue,M1553Events[i].EventMode,Event.BusNumber,M1553Events[i].BusNumber);
                                    if(M1553Events[i].EventMode == 3) //Match Down
                                    {
                                        if( (M1553Events[i].DiscreteValue > 0) && (DisValue == 0) )
                                        {
                                            m23_Update_M1553_Event(M1553Events[i].EventNum,REL_Upper,REL_Lower, ABS_Upper,ABS_Lower);
                                        }

                                    }
                                    else if(M1553Events[i].EventMode == 2) //Match UP
                                    {
                                        if( (M1553Events[i].DiscreteValue == 0) && (DisValue > 0) )
                                        {
                                            m23_Update_M1553_Event(M1553Events[i].EventNum,REL_Upper,REL_Lower, ABS_Upper,ABS_Lower);
                                        }
                                    }
                                    else if(M1553Events[i].EventMode == 1) //Match UP and Down
                                    {
                                        m23_Update_M1553_Event(M1553Events[i].EventNum,REL_Upper,REL_Lower, ABS_Upper,ABS_Lower);
                                    }

                                    M1553Events[i].DiscreteValue = DisValue;

                                    event = 0xFFFFFFFF;
                                }
                            }
                        }

                    }
                    else
                    {
                        event = 0xFFFFFFFF;
                        if(M23_M1553_IsDataConversion(Event.TableEntry,Event.BusNumber,&WordNum,&Index,&CW,&DataEvent,&Mask) == 1)
                        {

                            //if(debug)
                            //    printf("Table Entry %d,Bus %d,Wordnum %d,CW 0x%x,Value 0x%x\r\n",Event.TableEntry,Event.BusNumber,WordNum,CW,Event.DataWord);
                            M23_M1553_FillInDataWord(Index,WordNum,CW,(UINT16)Event.DataWord,Mask);
                            if(M23_M1553_IsTriggerEvent(Event.TableEntry,Event.BusNumber,Index) == 1)
                            {
                                event = M23_DoCalculation(Index,DataEvent,1);
                            }

                        }
                    }
                }

            }
            else
            {

                event = 0xFFFFFFFF;

                if(debug)
                    printf("M1553 NOT SYNC = 0x%x\r\n",Sync);
            }


            if( (event == B1_T16_EVENT) || (event == B1_T9_EVENT) || (event == B1_T17_EVENT) || (event == B1_G15_EVENT) )
            {
                if(config->timecode_chan.m1553_timing.Format == B1B_EGI)
                {
                    if(T16_Received == 0)
                    {
                        if(debug)
                            printf("B1B 1553 Time Load Event T16\r\n");
                        M23_M1553_LoadTimeEvent(B1_T16_EVENT);
                        UTC_check_time = 0.0;
                        UTC_compare_time = 0;
                        T16_Received = 1;
                    }
                    else
                    {
                        M23_M1553_ProcessB1B_Time(event);
                    }
                }
            }
            else if( (event == B1_INSR_T16_EVENT) || (event == B1_INSR_T9_EVENT) || (event == B1_INSR_T17_EVENT) || (event == B1_INSR_G15_EVENT) )
            {
                M23_M1553_ProcessB1B_Time(event);
                if(config->timecode_chan.m1553_timing.Format == B1B_EGI)
                {
                    if(T16_Received == 0)
                    {
                        if(debug)
                            printf("B1B 1553 Time Load Event T16\r\n");
                        M23_M1553_LoadTimeEvent(B1_INSR_T16_EVENT);
                        UTC_check_time = 0.0;
                        UTC_compare_time = 0;
                        T16_Received = 1;
                    }
                    else
                    {
                        M23_M1553_ProcessB1B_Time(event);
                    }
                }
            }
            else if( (event == PAUSE_EVENT) || (event == RESUME_EVENT) )
            {
                M23_M1553_Process_PauseResume(event);


                if( (event == PAUSE_EVENT) && (BusPauseEvent > 0) )
                {
                    m23_Update_M1553_Event(BusPauseEvent,REL_Upper,REL_Lower, ABS_Upper,ABS_Lower);
                }
                else if( (event == RESUME_EVENT) && (BusResumeEvent > 0) )
                {
                    m23_Update_M1553_Event(BusResumeEvent,REL_Upper,REL_Lower, ABS_Upper,ABS_Lower);
                 }
            }
            else if(event != 0xFFFFFFFF)
            {
                m23_Update_M1553_Event(event,REL_Upper,REL_Lower, ABS_Upper,ABS_Lower);
            }
        }

        usleep(100);
    }
}


void M23_InitializeCONDOR()
{
    int CSR;
    int debug;

    M23_GetDebugValue(&debug);


    /*Initialile the CORE*/
   
    table->RT_Enables = 0x3f;

    usleep(10000);


    filter->Receive[1]  = RECEIVE_SA1_OFFSET;
    usleep(10000);
    //filter->Transmit[2]  = RECEIVE_R2_OFFSET;
    filter->Receive[2]  = RECEIVE_R2_OFFSET;
    usleep(10000);
    filter->Transmit[3]  = TRANSMIT_SA3_OFFSET;
    usleep(10000);
    filter->Transmit[4]  = TRANSMIT_SA4_OFFSET;
    usleep(10000);
    filter->Transmit[5]  = TRANSMIT_SA5_OFFSET;
    usleep(10000);
    filter->Transmit[6]  = TRANSMIT_SA6_OFFSET;
    usleep(10000);

    R_SA1_Control->BufferPtrLSW = RECEIVE_SA1_BUFFER;
    usleep(10000);
    R2_Control->BufferPtrLSW    = RECEIVE_R2_BUFFER;
    usleep(10000);
    T_SA3_Control->BufferPtrLSW = TRANSMIT_SA3_BUFFER;
    usleep(10000);
    T_SA4_Control->BufferPtrLSW = TRANSMIT_SA4_BUFFER;
    usleep(10000);
    T_SA5_Control->BufferPtrLSW = TRANSMIT_SA5_BUFFER;
    usleep(10000);
    T_SA6_Control->BufferPtrLSW = TRANSMIT_SA6_BUFFER;
    usleep(10000);
    
    R_SA1_Control->LegalWCLSW = 0xFFFF;
    usleep(10000);
    R_SA1_Control->LegalWCMSW = 0xFFFF;
    usleep(10000);

    R2_Control->LegalWCLSW = 0xFFFF;
    usleep(10000);
    R2_Control->LegalWCMSW = 0xFFFF;
    usleep(10000);

    T_SA3_Control->LegalWCLSW = 0xFFFF;
    usleep(10000);
    T_SA3_Control->LegalWCMSW = 0xFFFF;
    usleep(10000);

    T_SA4_Control->LegalWCLSW = 0xFFFF;
    usleep(10000);
    T_SA4_Control->LegalWCMSW = 0xFFFF;
    usleep(10000);

    T_SA5_Control->LegalWCLSW = 0xFFFF;
    usleep(10000);
    T_SA5_Control->LegalWCMSW = 0xFFFF;
    usleep(10000);

    T_SA6_Control->LegalWCLSW = 0xFFFF;
    usleep(10000);
    T_SA6_Control->LegalWCMSW = 0xFFFF;
    usleep(10000);
    
    
    R_SA1_Buffer->MessagePtrLSW = RECEIVE_SA1_BUFFER;
    usleep(10000);
    R_R2_Buffer->MessagePtrLSW  = RECEIVE_R2_BUFFER;
    usleep(10000);
    T_SA3_Buffer->MessagePtrLSW = TRANSMIT_SA3_BUFFER;
    usleep(10000);
    T_SA4_Buffer->MessagePtrLSW = TRANSMIT_SA4_BUFFER;
    usleep(10000);
    T_SA5_Buffer->MessagePtrLSW = TRANSMIT_SA5_BUFFER;
    usleep(10000);
    T_SA6_Buffer->MessagePtrLSW = TRANSMIT_SA6_BUFFER;
    usleep(10000);


    //M23_M1553_WriteCondor(MESSAGE_RECEIVE_INT2,0x1);
    R_SA1_Buffer->RT_InterruptEnable2 = 0x1; /*End of Message Interrupt*/
    usleep(10000);

    /*Status Data Has Been Read, Invalidate Data also add End of Message Interrupt*/
    T_SA6_Buffer->RT_InterruptEnable2 = STATUS_INTERRUPT | 0x1;
    usleep(10000);

    table->ResponseTime = 0xA0;
    usleep(10000);
    table->StatusWord   = 0x0;
    usleep(10000);


    M23_M1553_WriteCondor(CONTROL_REGISTER,CONDOR_SETUP);
  
    usleep(10000);

    CSR = M23_M1553_ReadCondor(INTERRUPT_Q_INDEX); 


    CSR = M23_M1553_ReadCondor(BIT_STATUS_REG); 

    CSR = M23_M1553_ReadCondor(BIT_ERROR_REG); 

    CSR = M23_M1553_ReadCondor(FW_STATUS_REG); 

    CSR = M23_M1553_ReadCondor(OPN_STATE_REG); 


}



void M23_StartM1553CmdThread()
{
    int status = 0;
    int debug;

    M23_GetDebugValue(&debug);
    PlaySpeedStatus = 0;
    LoadInfo = LOAD_NONE;
    CurrentError = 0x0;
    CurrentQueueStatus = QUEUE_NO_STATUS;

    LiveVidStatus = 0;

    /*Invalidate the INFO data for both file and Events*/
    T_SA5_Buffer->Data[0] = 0x0;
    T_SA5_Buffer->Data[9] = 0x0;

    /*Invalidate the STATUS data for both file and Events*/
    T_SA6_Buffer->Data[0] = 0x0;

    status = pthread_create(&M1553CmdThread, NULL,(void *) M23_M1553_Cmd_Processor, NULL);

    if(debug)
    {
        if(status == 0)
        {
            PutResponse(0,"M1553 Cmd Thread Created Successfully\r\n");
        }
        else
        {
            PutResponse(0,"M1553 Cmd Create Thread Failed\r\n");
        }
    }

    status = pthread_create(&M1553LoadThread, NULL,(void *) M23_M1553_LoadStatus, NULL);

    if(debug)
    {
        if(status == 0)
        {
            PutResponse(0,"M1553 Load Thread Created Successfully\r\n");
        }
        else
        {
            PutResponse(0,"M1553 Load Create Thread Failed\r\n");
        }
    }
}


void M23_StartM1553EventThread()
{
    int status = 0;
    int debug;

    M23_GetDebugValue(&debug);


    status = pthread_create(&M1553Event, NULL,(void *) M23_M1553_Event, NULL);

    if(debug)
    {
        if(status == 0)
        {
            PutResponse(0,"M1553 Event Thread Created Successfully\r\n");
        }
        else
        {
            PutResponse(0,"M1553 Event Create Thread Failed\r\n");
        }
    }

}
