/***************************************************************************************
 *
 *    Copyright (C) 2006 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed
 *    for any purpose except as specifically authorized in writing.
 *
 *       File: M23_Control.c
 *       Version: 1.0
 *     Author: pcarrion
 *
 *            MONSSTR 2300v2
 *
 *             The following will contain the functions that Need to be updated every second
 *
 *    Revisions:
 ******************************************************************************************/

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory.h>

#include "M23_Controller.h"
#include "M23_Stanag.h"
#include "M23_Status.h"
#include "M23_Utilities.h"
#include "M23_MP_Handler.h"
#include "M23_features.h"

pthread_t ControlThread;

static int EndPlayScan;
static int StartPlayScan;
static int PlayPercent;

static int DeclassState;

static int LastBlock;

void M23_SetDeclassState()
{
    DeclassState = 1;
}


void M23_SetEndOnly()
{
    UINT32 CSR;
    int end;
    int debug;

    end = M23_GetEndBlock(0);
    EndPlayScan = end;

    M23_GetDebugValue(&debug);
if(debug)
    printf("End Scan = %d\r\n",end);

    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_NUM_BLOCKS,end);

    /*Set in Record Bit*/
    CSR = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_STATUS);
    CSR &= ~PLAY_IN_RECORD;
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_STATUS,CSR);


}

#if 0
void M23_SetEndOnly()
{
    int end;

    sCurrentBlock(&end);
    EndPlayScan = end;
}
#endif

void M23_SetStartPlayScan(int start)
{
    StartPlayScan = start;
}

void M23_SetEndPlayScan(int start,int end)
{
    int debug;

    M23_GetDebugValue(&debug);
    if(debug)
        printf("Setting End %d Start %d\r\n",end,start);

    EndPlayScan = end;
    StartPlayScan = start;
    LastBlock = end;
}

void M23_GetPlayPercent(int *percent)
{
    *percent = PlayPercent;
}

void M23_ControlLoop()
{
    int state;
    int bingo;
    int loops     = 0;
    int Timeloops = 0;
    int Bitloops  = 0;
    int BitCount  = 0;
    int config;
    int ssric;
    int declass = 0;
    int return_status;

    int pkt_len;
    int bingo_set = 0;
    int full_set = 0;
    int inplay = 0;

    M23_CHANNEL  const *Config;

    UINT32 CSR = 0;
    UINT32 total_blocks;
    UINT32 blocks_used;
    UINT32 CurrentPTR;
    UINT64 percent = 0L;
    UINT64 blocks = 0L;
    UINT64 offset;

    int debug;


    while(1)
    {
    
        /* Add Poll Loop At this point*/
        M23_RecorderState(&state);
        M23_GetConfig(&config);
        M23_GetSsric(&ssric);
        M23_GetDebugValue(&debug);

        sdisk(&total_blocks,&blocks_used);
        blocks = (UINT64)blocks_used * 100;
        percent = blocks/(UINT64)total_blocks;

        //Paul Add 4_21_05
        return_status = DiskIsConnected();
        if(return_status)
        {
            if(percent > 99)
            {
                percent = 99;
            }
            if( (config == 1) || (config == 2) || (config == 4) || (config == LOCKHEED_CONFIG) || (config == P3_CONFIG) || (config == B1B_CONFIG) || (config == B52_CONFIG) )
            {
                /*Add BINGO Here*/
                M23_GetBingo(&bingo);
                if( bingo <= (int)percent)
                {
                    /*Light Up Discrete*/
                    M23_SetBingoLED();
                    if(config == LOCKHEED_CONFIG)
                    {
                        if(bingo_set == 0)
                        {
                            bingo_set = 1;
                            M23_SetHealthBit(RECORDER_FEATURE, M23_MEMORY_BINGO_REACHED);
                        }
                    }
                }
                else
                {
                    bingo_set = 0;
                    M23_ClearBingoLED();
                    M23_ClearHealthBit(RECORDER_FEATURE, M23_MEMORY_BINGO_REACHED);
                }
            }
        }
        else
        {
            percent = 0;
        }

        if((state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) )
        {

            /*Check if we have a failure*/
            CSR = M23_ReadControllerCSR(IEEE1394B_STATUS);
            if(CSR & IEEE1394B_RECORD_FAULT)
            {
                M23_WriteControllerCSR(IEEE1394B_STATUS, ((CSR & 0xFF) | IEEE1394B_RECORD_FAULT) );
                if(debug)
                    printf("Record Error 0x%x , Recording Stopping %d,%d,%d\r\n",CSR,total_blocks,blocks_used,CurrentPTR);
                M23_SetMediaError();
                Stop(0);
            }
            else
            {
                M23_ClearMediaError();
            }

            /*If Block Left < 64MB, Stop Recording*/
            CurrentPTR = M23_ReadControllerCSR(IEEE1394B_RECORD_POINTER_OUT);
            //if( ((total_blocks - blocks_used) < SYSTEM_BLOCKS_RESERVED) || (blocks_used >= total_blocks) )

            if( ((total_blocks - CurrentPTR) <= SYSTEM_BLOCKS_RESERVED) )
            {
                if(debug)
                    printf("Memory Full , Recording Stopping %d,%d,%d\r\n",total_blocks,blocks_used,CurrentPTR);

                if(state == STATUS_REC_N_PLAY )
                {
                    inplay = 1;
                }

                Stop(0);

                if(inplay)
                {
                    M23_SetEndOnly();
                    inplay = 0;
                }

                if(config == 3)
                {
                    M23_SetFullLED();
                }
                else
                {
		    //M23_SetFaultLED(0);
                }
                if(full_set == 0)
                {
                    full_set = 1;
                    M23_SetHealthBit(RECORDER_FEATURE, M23_MEMORY_FULL);
                    M23_ClearHealthBit(RECORDER_FEATURE, M23_MEMORY_BINGO_REACHED);
                }
            }
            else
            {
                full_set = 0;
            }
        }
        if( (state == STATUS_PLAY) || (state == STATUS_REC_N_PLAY) )
        {

            if(state == STATUS_PLAY)
            {
                /*Get The Number of Blocks Played*/
                CSR = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_BLOCKS_OUT);
                /*Calculate Percent and use it to determine if we need to stop play*/
                blocks = (UINT64)(CSR * 100);
                percent = blocks/(UINT64)(EndPlayScan - StartPlayScan);
                PlayPercent = (int)percent;
            }
            /*Get The Number of BLocks so far*/
            CSR = M23_ReadControllerCSR(IEEE1394B_STATUS);
            if(CSR & IEEE1394B_PLAY_COMPLETE)
            {

                if(debug)
                    printf("Playback Complete,Start %d ,Current %d End %d\r\n",StartPlayScan,CSR,EndPlayScan);

                if(state == STATUS_PLAY)
                {
                    M23_StopPlayback(1);
                }
                else
                {
                    M23_StopPlayback(0);
                }
                M23_CS6651_Normal();
                M23_SetLiveVideo(0);
                CSR = M23_ReadControllerCSR(IEEE1394B_STATUS);
                M23_WriteControllerCSR(IEEE1394B_STATUS, ((CSR & 0xFF) | IEEE1394B_PLAY_COMPLETE) );

            }

        }
        else if(state == STATUS_DECLASSIFY)
        {
            if(loops == 2)
            {
                loops = 0;
	        if(declass == 1)
	        {
                    if(config == LOCKHEED_CONFIG)
                    {
                        M23_ClearDataLED();
                        M23_ClearEraseLED();
                    }
                    else
                    {
		        M23_ClearDeclassLED();
                    }
		    declass = 0;
                }
	        else
	        {
                    if(config == LOCKHEED_CONFIG)
                    {
                        M23_SetDataLED();
                        M23_SetEraseLED();
                    }
                    else
                    {
	                M23_SetDeclassLED();
                    }

		    declass = 1;
                }

                M2x_SendStatusCommand();
                if( (CartState == STATUS_IDLE) || (CartState == STATUS_DECLASS_PASS ) )
                {
                    //printf("Cartridge Done, Now Start Secure File\r\n");
                    // PC REMOVE CartPercent = 50;
                    /*At this point we need to Erase and write the Secure File*/
                    M23_SetRecorderState(STATUS_IDLE);
                    if(config == LOCKHEED_CONFIG)
                    {
                        M23_SetDataLED();
                        M23_SetEraseLED();
                        M23_SetFaultLED(0);
                    }
                    else
                    {
	                M23_SetDeclassLED();
                    }
                }
                else if( (CartState == STATUS_FAIL) || (CartState == STATUS_DECLASS_FAIL) )
                {
                    M23_SetRecorderState(STATUS_FAIL);
                    if(config == LOCKHEED_CONFIG)
                    {
                        M23_ClearDataLED();
                        M23_ClearEraseLED();
                        M23_SetFaultLED(0);
                    }
                    else
                    {
	                M23_ClearDeclassLED();
                    }
                    DeclassState = 0;
                    CartPercent = 0;
                }
            }
            else
            {
                loops++;
            }
        }
        else if( (state == STATUS_RTEST) || (state == STATUS_P_R_TEST) )
        {
            /*If Block Left < 64MB, Stop Recording*/
            if( (total_blocks - blocks_used) < SYSTEM_BLOCKS_RESERVED)
            {
                M23_PerformRtest(0,0,2);
                M23_ClearRecordLED();
                M23_SetRecorderState(STATUS_IDLE);

            }

            Timeloops++;
            if(Timeloops == 2)
            {

                UpdateStanagAndBlocks();
                Timeloops = 0;
            }
            else
            {
                if(Timeloops > 2)
                {
                    Timeloops = 0;
                }
            }
        }

        if((state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) )
        {
            Timeloops++;
            if(Timeloops == 2)
            {

                SetupGet(&Config);

                if(Config->IndexIsEnabled)
                {
                    M23_GetRoot_UpdateNodes(&offset,&pkt_len);
                    if(offset > 0)
                    {
                        UpdateStanagBlocks(offset,pkt_len);
                        RootReceived = 1;
                    }
                }
                else
                {
                    UpdateStanagAndBlocks();
                }

                Timeloops = 0;
            }
            else
            {
                if(Timeloops > 2)
                {
                    Timeloops = 0;
                }
            }
        }
        else if(state == STATUS_BIT)
        {
            Bitloops++;
            if(Bitloops == 2)
            {
                SetBitStatus(BitCount * 10);
                Bitloops = 0;
                BitCount++;
            }
            if(BitCount == 10)
            {
                SSRIC_SetBitStatus();
                M23_SetRecorderState(STATUS_IDLE);
                if(config == LOCKHEED_CONFIG)
                {
                    M23_ClearFaultLED(0);
                }
                else
                {
                    M23_StopLEDBlink();
                }

                BitCount = 0;
            }


        }
        else if( state == STATUS_PTEST ) 
        {
            /*Get the Number of Blocks recorded*/
            CSR = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_BLOCKS_OUT);

            /*Calculate Percent and use it to determine if we need to stop record*/
            blocks = (UINT64)((StartPlayScan + CSR) * 100);
            percent = blocks/(UINT64)EndPlayScan;
            PTest_Percentage = (int)percent;

            /*Get The Number of BLocks so far*/
            if( (StartPlayScan + CSR) >= EndPlayScan)
            {
                M23_StopPtest();
            }


        }

        SSRIC_UpdateBothDigits((int)percent,FALSE,FALSE);

        //Paul Add 4_21_05
        if(DiskIsConnected() == 0)
        {
            percent = 0;
        }


        usleep(500000);
    }
}

void M23_StartControlThread()
{
    int status;
    int debug;

    M23_GetDebugValue(&debug);

    DeclassState = 0;

    status = pthread_create(&ControlThread, NULL, (void *)M23_ControlLoop, NULL);
    if(debug)
    {
        if(status == 0)
        {
            printf("Control Thread Created Successfully\r\n");
        }
        else
        {
            perror("Control Thread Create\r\n");
        }
    }
}


