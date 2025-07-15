/****************************************************************************************
*
 *    Copyright (C) 2004 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and 
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
 *    for any purpose except as specifically authorized in writing.
 *
 *    File: M23_HealthProcessor.c
 *
 *    Version: 1.0
 *    Author: pcarrion
 *
 *    MONSSTR 2300
 *
 *    Will query all configured channels in order to obtain the status of each.
 *       
 *
 *    Revisions:  
 ******************************************************************************************/
#include <stdlib.h>
#include <stdio.h>

#include "M23_Controller.h"
#include "M23_Status.h"
#include "M2X_channel.h"
#include "M23_features.h"
#include "M23_Ssric.h"
#include "M23_Utilities.h"
#include "M23_MP_Handler.h"
#include "M2X_Const.h"
#include "M23_EventProcessor.h"

UINT32 NumberOfVoiceChannels;
UINT32  NumberOfPCMChannels;
UINT32  NumberOf1553Channels;
UINT32  NumberOfVideoChannels;
UINT32  NumberOfTimeChannels;

pthread_t HealthStatusThread;


//INT8    FeatureDescriptions[MAX_FEATURES][40];

static UINT32 M23_CriticalMasks[MAX_FEATURES];
//static UINT32 HealthStatusArr[MAX_FEATURES];

static int  M1553_HealthTimes[32];

static int  OneTime;

static int  ClearVideo;

static int  PreviousEthData;


//static int NeedSync[4];
static int NeedSync[16];

static int ResetCount[16];
static int StableCount[16];

void SetNeedSync()
{
    int i;

    for(i = 0; i < 16;i++)
    {
        NeedSync[i] = 1;
    }
}

void ChannelNeedsSync(int channel)
{
    NeedSync[channel] = 1;
}

int ReturnNumberOfCritical()
{
    int i;
    int j;
    int CriticalErrors = 0;


    for( i = 0 ; i < MAX_FEATURES ; i++ )//MSB is not Error
    {
        if(HealthStatusArr[i] & 0x80000000)
        {
            for( j = 0 ; j < 31 ; j++)
            {
                if( ((HealthStatusArr[i] & ( 0x1 << j) ) & M23_CriticalMasks[i]) !=0 )
                {
                    CriticalErrors++;
                }
            }
        }
    }

    return(CriticalErrors);

}


int HealthViewAll(int *number_of_features, UINT32 **health_status)
{

    *number_of_features = NumberOfVoiceChannels + NumberOfPCMChannels + NumberOf1553Channels + NumberOfVideoChannels + 1;

    *health_status = &HealthStatusArr[0];

    if ( RecorderStatus == STATUS_ERROR )
        RecorderStatus = STATUS_IDLE;

    return NO_ERROR;

}



int HealthView(  int feature, UINT32 *mask, char **decoded_masks[32] ,int index)
{
    int               i;


    if(HealthArrayTypes[feature] == NO_FEATURE)
    {

        if ( RecorderStatus == STATUS_IDLE )
            RecorderStatus = STATUS_ERROR;

        return ERROR_INVALID_PARAMETER;
    }


    // Build the list of features dynamically, depending on system configuration
    switch( HealthArrayTypes[feature] )
    {
        case RECORDER_FEATURE:
                        *decoded_masks = RecorderFeatureBits;
                        break;
        case TIMECODE_FEATURE:
                        *decoded_masks = TimeFeatureBits;
                        break;
        case VOICE_FEATURE:
                        *decoded_masks = VoiceFeatureBits;
                        break;
        case PCM_FEATURE:
                        *decoded_masks = PCMFeatureBits;
                        break;
        case M1553_FEATURE:
                        *decoded_masks = m1553FeatureBits;
                        break;
        case VIDEO_FEATURE:
                        *decoded_masks = AudioVideoFeatureBits;
                        break;
        case ETHERNET_FEATURE:
                        *decoded_masks = EthernetFeatureBits;
                        break;
        case UART_FEATURE:
                        *decoded_masks = UARTFeatureBits;
                        break;
        case DISCRETE_FEATURE:
                        *decoded_masks = DiscreteFeatureBits;
                        break;
           default:   
                        *decoded_masks = NULL;
                        break;
    }

    *mask = HealthStatusArr[index];

    // filter errors that do not have description
    //
    for ( i = 0; i < 31; i++ )
    {
        if ( (*decoded_masks)[i][0] == '\0' )
            *mask &= ~( 1 << i );
    }


    if ( RecorderStatus == STATUS_ERROR )
        RecorderStatus = STATUS_IDLE;

    return NO_ERROR;
 

}


void M23_GetHealthArray(UINT32 **statusArray)
{
	*statusArray = &HealthStatusArr[0];
}

void M23_GetCriticalMask(UINT32 **masks)
{
     
    *masks = &M23_CriticalMasks[0];
}

void M23_SetCriticalMask(int feature, UINT32 mask)
{
	M23_CriticalMasks[feature] = mask;
}

void M23_ClearTimeSync()
{
    OneTime = 1;
}

void M23_SetMediaError()
{
    if(!(HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_IO_ERROR))
    {
        M23_SetHealthBit(RECORDER_FEATURE,M23_MEMORY_IO_ERROR);
    }
}

void M23_ClearMediaError()
{
    if((HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_IO_ERROR))
    {       
        M23_ClearHealthBit(RECORDER_FEATURE,M23_MEMORY_IO_ERROR);
    }       
}


int FindCartridge()
{
    int    IsPresent = 0; //Assume Cartridge is not present
    int    Present; 
    int    Switch;
    int    debug;
    UINT32 CSR;

    M23_GetDebugValue(&debug);
    /*Read the Cartidge CSR*/
    CSR = M23_ReadControllerCSR(CONTROLLER_CARTRIDGE_TAIL_CSR);

    if( CSR &  CONTROLLER_CARTRIDGE_NOT_PRESENT)  //Cartridge Not Present
    {
        GetCartridgeDiscretes(&Present,&Switch);
        if(Present == 0)
        {
            IsPresent = 1; //Cartridge Is Present
            M23_SetRemoteValue(1);
            IEEE1394_Bus_Speed = BUS_SPEED_S400;

            if(debug)
                printf("Found Cartridge in Remote Receiver\r\n");
        }
        else
        {
            if(!(HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_NOT_PRESENT))
            {
                M23_SetHealthBit(RECORDER_FEATURE,M23_MEMORY_NOT_PRESENT);
            }

            if(!(HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_LATCH))
            {
                M23_SetHealthBit(RECORDER_FEATURE,M23_MEMORY_LATCH);
            }
        }
    }
    else
    {
        if(debug)
            printf("Found Cartridge in Local Receiver\r\n");

        IsPresent = 1; //Cartridge Is Present
        M23_SetRemoteValue(0);
        IEEE1394_Bus_Speed = BUS_SPEED_S400;
    }

    return(IsPresent);
}



void GetCartridgeHealth(int *present, int *latch)
{
    int    Remote;
    int    Present;
    int    Switch;
    int    config;
    int    ignore;
    UINT32 CSR;

    M23_GetRemoteValue(&Remote);
    M23_GetConfig(&config);
    M23_GetIgnorePresent(&ignore);

#if 0
if(ignore)
{
/*PC REMOVE*/
*present = 0;
*latch = 0;
return;
}
#endif

    if(config == LOCKHEED_CONFIG)
    {
        /*Read the Cartidge CSR*/
        CSR = M23_ReadControllerCSR(CONTROLLER_CARTRIDGE_TAIL_CSR);
        if( CSR & CONTROLLER_LATCH_OPEN )
        {
            *latch = 1;
        }
        else
        {
            *latch = 0;
        }

        if( CSR &  CONTROLLER_CARTRIDGE_NOT_PRESENT)
        {
            *present = 1;
        }
        else
        {
            *present = 0;
        }

        return;
    }


    if(Remote)  //Using Remote with discretes
    {
        GetCartridgeDiscretes(&Present,&Switch);
        *present = Present;
        *latch  = Switch;
    }
    else     //Use Backplane discretes
    {
        /*Read the Cartidge CSR*/
        CSR = M23_ReadControllerCSR(CONTROLLER_CARTRIDGE_TAIL_CSR); 
        if( CSR & CONTROLLER_LATCH_OPEN )
        {
            *latch = 1;
        }
        else
        {
            *latch = 0;
        }

        if( CSR &  CONTROLLER_CARTRIDGE_NOT_PRESENT) 
        {
            *present = 1;
        }
        else
        {
            *present = 0;
        }
    }
}

#if 0
void M23_ClearVideoHealth()
{
    M23_CHANNEL *config;
    int         i;

    if(ClearVideo)
    {
        SetupGet(&config);

        for(i = 0; i < config->NumConfiguredVideo;i++)
        {
            M23_ClearHealthBit(StartOfVideoHealth + i,M2X_VIDEO_CHANNEL_PACKET_DROPOUT);
        }
    }

    ClearVideo = 0;
}

void M23_ClearNoDSPData()
{
    if((HealthStatusArr[RECORDER_FEATURE] & M2X_RECORDER_NO_DSP_DATA))
    {
        M23_ClearHealthBit(RECORDER_FEATURE,M2X_RECORDER_NO_DSP_DATA);
    }

}
void M23_ClearNoRecorderData()
{
    if((HealthStatusArr[RECORDER_FEATURE] & M2X_RECORDER_NO_DATA))
    {
        M23_ClearHealthBit(RECORDER_FEATURE,M2X_RECORDER_NO_DATA);
    }

}

void M23_SetNoRecorderData()
{
    if(!(HealthStatusArr[RECORDER_FEATURE] & M2X_RECORDER_NO_DATA))
    {
       M23_SetHealthBit(RECORDER_FEATURE,M2X_RECORDER_NO_DATA);
    }
 
}
#endif

void M23_CheckStatus()
{
    int         i,j,k;
    int         state;
    int         NumEnableM1553 = 0;
    int         NumEnablePCM = 0;
    int         NumMPBoards = 0;
    int         VideosOK = 2;
    int         M1553OK = 1;
    int         PCMOK = 1;
    int         PCMSSRIC;
    int         VIDEOSSRIC;
    int         NumCritical = 0;
    int         Num1553OK = 0;
    int         CheckLock;
    int         ssric;
    int         return_status;
    int         pcm_4_index = 0;
    int         pcm_8_index = 0;
    int         m1553_4_index = 0;
    int         m1553_8_index = 0;
    int         dm_index = 0;
    int         video_index = 0;
    int         notpresent;
    int         notlatched;
    int         debug;
    int         video_no_packets;
    int         IsPresent = 1;
    int         loop;
    int         setup;
    int         pcm_paused = 0;
    int         enet_state;
    int         enet_data = 0;

    UINT32      CSR;
    UINT8       status = 0;;
    UINT8       Ready;
    //UINT32      *CriticalMasks = NULL;
    UINT32      CSR1;
    UINT32      CSR2;
    UINT32      Pause;
    M23_CHANNEL *config;

    static int         SyncLoops = 0;

    //M23_GetCriticalMask(&CriticalMasks);
    M23_GetSsric(&ssric);

    M23_GetDebugValue(&debug);

    M23_RecorderState(&state);

    /*Cartridge Is Not Present Try To find where it will be inserted*/
    if((HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_NOT_PRESENT))
    {
        IsPresent = FindCartridge();
    }
    

    if(IsPresent)
    {
        GetCartridgeHealth(&notpresent,&notlatched);


        M23_GetConfig(&setup);

        /*Check if the Latch is open */
        if(notlatched)
        {
            if(!(HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_LATCH))
            {
                M23_SetHealthBit(RECORDER_FEATURE,M23_MEMORY_LATCH);
            }
        }
        else
        {
            if((HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_LATCH))
            {
                M23_ClearHealthBit(RECORDER_FEATURE,M23_MEMORY_LATCH);
            }
        }

        /*Check if the Cartridge is present */
        if(notpresent)
        {

            CSR = M23_ReadControllerCSR(IEEE1394B_STATUS);
            if(CSR & IEEE1394B_RECORD_FAULT)
            {
                //M23_SetMediaError();
                M23_WriteControllerCSR(IEEE1394B_STATUS, ((CSR & 0xFF) | IEEE1394B_RECORD_FAULT) );
                if(debug)
                   printf("REMOVED - Clearing Record Fault 0x%x - 0x%x\r\n",((CSR & 0xFF) | IEEE1394B_RECORD_FAULT),IEEE1394B_STATUS);

            }

            if(setup != 0)
            {
	        if((state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) || (state == STATUS_PLAY) )
	        {
                    //if(debug)
                     //  printf("Removal Stop Not Present %d,%d-%d\r\n",setup,notlatched,notpresent);

                    /*Stop as much of the record/play as possible*/
                    Removal_Stop();
                    M23_SetHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);
                    M23_SetHealthBit(RECORDER_FEATURE,M23_MEMORY_NOT_PRESENT);

                }
                else
                {
                    if(!(HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_NOT_PRESENT))
                    {
                        M23_SetHealthBit(RECORDER_FEATURE,M23_MEMORY_NOT_PRESENT);
	                if((state != STATUS_RECORD) && (state != STATUS_REC_N_PLAY) && (state != STATUS_PLAY) )
                        {
                            Dismount();
                        }
                    }
                }
            }
            else
            {
	        if((state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) || (state == STATUS_PLAY) )
	        {

                    /*Stop as much of the record/play as possible*/
                    Removal_Stop();

                    M23_StopPlayback(0);
                    M23_CS6651_Normal();

                    M23_SetLiveVideo(0);
                    M23_SetHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);
                    M23_SetHealthBit(RECORDER_FEATURE,M23_MEMORY_NOT_PRESENT);

	        }
                else
                {
                    if(!(HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_NOT_PRESENT))
                    {
                        M23_SetHealthBit(RECORDER_FEATURE,M23_MEMORY_NOT_PRESENT);
	                if((state != STATUS_RECORD) && (state != STATUS_REC_N_PLAY) && (state != STATUS_PLAY) )
                        {
                            Dismount();
                        }
                    }
                }
            }


        }
        else
        {
            if((HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_NOT_PRESENT))
            {
                M23_ClearHealthBit(RECORDER_FEATURE,M23_MEMORY_NOT_PRESENT);

                if(setup == 0)
                {
                    for(loop = 0; loop < 15;loop++)
                    {
                        GetCartridgeHealth(&notpresent,&notlatched);
                        /*Check if the Latch is open */
                        if(notlatched)
                        {
                            if(!(HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_LATCH))
                            {
                                M23_SetHealthBit(RECORDER_FEATURE,M23_MEMORY_LATCH);
                            }
                        }
                        else
                        {
                            if((HealthStatusArr[RECORDER_FEATURE] & M23_MEMORY_LATCH))
                            {
                                M23_ClearHealthBit(RECORDER_FEATURE,M23_MEMORY_LATCH);
                            }
                        }

                        sleep(1);
                    }

                    Mount();
                }
                else
                {
                    sleep(22);
                    Mount();
                }
            }
        }

        /* Now check if the disk is connected*/
        return_status = DiskIsConnected();

        if(return_status)
        {
            /*Disk is Connected*/
            if((HealthStatusArr[RECORDER_FEATURE] & M23_DISK_NOT_CONNECTED))
            {
                return_status = DismountCommandIssued();
                if(return_status == 0)
                {
                    M23_ClearHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);
                }
            }
        }
        else
        {
            /*Disk is not Connected*/
            if(!(HealthStatusArr[RECORDER_FEATURE] & M23_DISK_NOT_CONNECTED))
            {
                M23_SetHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);
            }
        }
    }




    SetupGet(&config);


    /*This will check the MP board 1553 status*/

    M23_NumberMPBoards(&NumMPBoards);

    for(i = 0;i < NumMPBoards;i++)
    {
        if(M23_MP_IS_M1553(i))
        {
            //M23_GetM1553StatusMask(&Mask);

            if(M23_MP_M1553_4_Channel(i))
            {
                for(j = 1; j<5;j++)
                {
                    status = 0;
                    if(config->m1553_chan[(j+(m1553_4_index*4)) -1].isEnabled)
                    {
                        NumEnableM1553++;

                        if(config->m1553_chan[(j+(m1553_4_index*4)) -1].WatchWordEnabled == 1)
                        {
                            /*This will check the watch word bit*/
                            CSR1 = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_WATCH_WORD_EVENT_OFFSET);
                            if(CSR1 == 1)
                            {
                                if( !(HealthStatusArr[config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID] & M23_M1553_WATCH_WORD_ERROR))
                                {
                                    M23_SetHealthBit(config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID ,M23_M1553_WATCH_WORD_ERROR);
                                }
                                M23_mp_write_csr(i,BAR1,0,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_WATCH_WORD_EVENT_OFFSET);
                                //if( ((Mask >> ((j + (i*4)) - 1)) & 0x1) == 0)
                                if(config->m1553_chan[(j+(m1553_4_index*4)) -1].included == TRUE)
                                {

                                    M1553OK = 0;
                                }
                                status = ( 1 << ((j + (m1553_4_index*4)) -1) );
                                if(ssric == 1)
                                    SSRIC_Clear1553Status(status);

                            }
                            else
                            {
                                if( (HealthStatusArr[config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID] & M23_M1553_WATCH_WORD_ERROR))
                                {
                                    M23_ClearHealthBit(config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID,M23_M1553_WATCH_WORD_ERROR);
                                }
                                status = ( 1 << ((j + (m1553_4_index*4)) -1) );
                                if(ssric == 1)
                                   SSRIC_Set1553Status(status);
                                Num1553OK++;

                            }
                        }
                        else
                        {
                            if( (HealthStatusArr[config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID] & M23_M1553_WATCH_WORD_ERROR))
                            {
                                M23_ClearHealthBit(config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID,M23_M1553_WATCH_WORD_ERROR);
                            }

                            Num1553OK++;

                            status = ( 1 << ((j + (m1553_4_index*4)) -1) );
                            if(ssric == 1)
                                SSRIC_Set1553Status(status);
                        }

                        /*This reads the No signal Bit*/
                        CSR1 = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_STATUS_OFFSET);
                        if(CSR1 == 0)
                        {
                            if(M1553_HealthTimes[(j+(m1553_4_index*4)) -1 ] == 4)
                            { 
                                if( !(HealthStatusArr[config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID] & M23_M1553_CHANNEL_NO_SIGNAL))
                                {
                                    M23_SetHealthBit(config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID,M23_M1553_CHANNEL_NO_SIGNAL);
                                }

                                if(config->m1553_chan[(j+(m1553_4_index*4)) -1].WatchWordEnabled == 0)
                                {
                                   // if( ((Mask >> ( (j + (i*4)) - 1)) & 0x1) == 0)
                                    if(config->m1553_chan[(j+(m1553_4_index*4)) -1].included == TRUE)
                                    {
                                        M1553OK = 0;
                                    }
                                    status = ( 1 << ( (j + (m1553_4_index*4)) -1) );
                                    if(ssric == 1)
                                        SSRIC_Clear1553Status(status);
                                }
                            }
                            else
                            {
                                M1553_HealthTimes[(j+(m1553_4_index*4)) -1]++;
                            }
                        }
                        else
                        {
                            Num1553OK++;
                            if( (HealthStatusArr[config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID] & M23_M1553_CHANNEL_NO_SIGNAL))
                            {
                                M23_ClearHealthBit(config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID,M23_M1553_CHANNEL_NO_SIGNAL);
                            }

                            M23_mp_write_csr(i,BAR1,0,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_STATUS_OFFSET);
                            M1553_HealthTimes[(j+(m1553_4_index*4))-1] = 0;

                            if(config->m1553_chan[(j+(m1553_4_index*4)) -1].WatchWordEnabled == 0)
                            {
                                status = ( 1 << ( (j + (m1553_4_index*4)) -1) );
                                if(ssric == 1)
                                    SSRIC_Set1553Status(status);

                            }
                        }
                    }
                }
		m1553_4_index++;
            }
            else
            {

                for(j = 1; j<9;j++)
                {
                    status = 0;
                    if(config->m1553_chan[(j+(m1553_8_index*8)) -1].isEnabled)
                    {
                        NumEnableM1553++;
                        if(config->m1553_chan[(j+(m1553_8_index*8)) - 1].WatchWordEnabled == 1)
                        {
                            /*Check the Watch Word  bit*/
                            CSR1 = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_WATCH_WORD_EVENT_OFFSET);

                            if(CSR1 == 1)
                            {
                                if( !(HealthStatusArr[config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID] & M23_M1553_WATCH_WORD_ERROR))
                                {
                                    M23_SetHealthBit(config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID,M23_M1553_WATCH_WORD_ERROR);
                                }

                                M23_mp_write_csr(i,BAR1,0,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_WATCH_WORD_EVENT_OFFSET);
                                status = ( 1 << ((j+(m1553_8_index*8)) - 1) );
                                if(ssric == 1)
                                    SSRIC_Clear1553Status(status);
                               /// if( ((Mask >> (j - 1)) & 0x1) == 0)
                                if(config->m1553_chan[(j+(m1553_8_index*8)) - 1].included == TRUE)
                                {
                                    M1553OK = 0;
                                }
                            }
                            else
                            {
                                Num1553OK++;
                                if( (HealthStatusArr[config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID] & M23_M1553_WATCH_WORD_ERROR))
                                {
                                    M23_ClearHealthBit(config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID,M23_M1553_WATCH_WORD_ERROR);
                                }

                                status = ( 1 << ((j+(m1553_8_index*8))  - 1) );
                                if(ssric == 1)
                                    SSRIC_Set1553Status(status);

                            }

                        }
                        else
                        {
                            if( (HealthStatusArr[config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID] & M23_M1553_WATCH_WORD_ERROR))
                            {
                                M23_ClearHealthBit(config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID,M23_M1553_WATCH_WORD_ERROR);
                            }
                        }

                        /*Check the No signal bit*/
                        CSR1 = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_STATUS_OFFSET);

                        if(CSR1 == 0)
                        {
                            if(M1553_HealthTimes[(j+(m1553_8_index*8)) - 1] == 4)
                            { 
                                if( !(HealthStatusArr[config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID] & M23_M1553_CHANNEL_NO_SIGNAL))
                                {
                                    M23_SetHealthBit(config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID ,M23_M1553_CHANNEL_NO_SIGNAL);
                                }

#if 0
                                if(config->m1553_chan[(j+(m1553_8_index*8)) - 1].WatchWordEnabled == 0)
                                {
#endif
                                    status = ( 1 << ((j+(m1553_8_index*8)) - 1) );
                                    if(ssric == 1)
                                        SSRIC_Clear1553Status(status);

                                   // if( ((Mask >> (j - 1)) & 0x1) == 0)
                                    if(config->m1553_chan[(j+(m1553_8_index*8)) - 1].included == TRUE)
                                    {
                                        M1553OK = 0;
                                    }
#if 0
                                }
#endif
                            }
                            else
                            {
                                M1553_HealthTimes[(j+(m1553_8_index*8)) -1]++;
                            }

                        }
                        else
                        {
                            Num1553OK++;
                            if( (HealthStatusArr[config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID] & M23_M1553_CHANNEL_NO_SIGNAL))
                            {
                                M23_ClearHealthBit(config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID,M23_M1553_CHANNEL_NO_SIGNAL);
                            }
                            M23_mp_write_csr(i,BAR1,0,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_STATUS_OFFSET);
                            M1553_HealthTimes[(j+(m1553_8_index*8)) - 1] = 0;

                            if(config->m1553_chan[(j+(m1553_8_index*8)) - 1].WatchWordEnabled == 0)
                            {
                                status = ( 1 << ((j+(m1553_8_index*8))  - 1) );
                                if(ssric == 1)
                                    SSRIC_Set1553Status(status);
                            }
                        }
#if 0
                        /*Check if Pause is Set in the Channel*/
                        CSR = M23_mp_read_csr(i,BAR1,0x24);
                        if(CSR & MP_M1553_BOARD_PAUSED)
                        {
                            M23_SetHealthBit(config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID ,M23_M1553_CHANNEL_PAUSED);
                        }
                        else //The Board Pause is Cleared
                        {
                            if( (HealthStatusArr[config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID] & M23_M1553_CHANNEL_PAUSED))
                            {
                            }
                            else
                            {
                                M23_ClearHealthBit(config->m1553_chan[(j+(m1553_8_index*8)) -1].chanID ,M23_M1553_CHANNEL_PAUSED);
                            }
                        }
#endif

                    }
                }
		m1553_8_index++;
            }
        }
        else if( M23_MP_IS_PCM(i))
        {
            Pause = M23_mp_read_csr(i,BAR1,MP_PCM_PAUSE_STATUS);
            if(Pause & MP_PCM_PAUSE)
            {
                pcm_paused = 1;
            }

            if(M23_MP_PCM_4_Channel(i))
            {

                for(j = 1; j<5;j++)
                {
                    CheckLock = 1;
                    if(config->pcm_chan[(j+(pcm_4_index*4))- 1].isEnabled)
                    {
                        if(pcm_paused)
                        {
                            if( !(HealthStatusArr[config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID] & M23_PCM_CHANNEL_PAUSED))
                            {
                                M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_CHANNEL_PAUSED);
                            }
                        }
                        else
                        {
                            if( (HealthStatusArr[config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID] & M23_PCM_CHANNEL_PAUSED))
                            {
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID ,M23_PCM_CHANNEL_PAUSED);
                            }
                        }

                        PCMSSRIC = 1;
                        NumEnablePCM++;
                        CSR = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * j) + MP_PCM_STATUS_OFFSET);

                        /*determine if the UART channel is Enabled and is a PCM_UART*/
                        if(config->pcm_chan[(j+(pcm_4_index*4))-1].pcmCode == PCM_UART)
                        {
                            if(CSR & MP_UART_FRAME)
                            {
                                M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_UART_FRAME);
                            }
                            else
                            {
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_UART_FRAME);
                            }

                            if(CSR & MP_UART_PARITY)
                            {
                                M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_UART_PARITY);
                            }
                            else
                            {
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_UART_PARITY);
                            }

                            if(CSR & MP_UART_PACKET_DROP)
                            {
                                M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID ,M23_UART_DROP_PACKET);
                            }
                            else
                            {
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID ,M23_UART_DROP_PACKET);
                            }

                            if(CSR & MP_UART_NO_PACKET)
                            {
                                M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_UART_NO_PACKET);
                            }
                            else
                            {
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_UART_NO_PACKET);
                            }
                        }
                        else if(config->pcm_chan[(j+(pcm_4_index*4))-1].pcmCode == PCM_PULSE)
                        {
                        }
                        else
                        {
                            if(config->pcm_chan[(j+(pcm_4_index*4))-1].pcmBitSync == FALSE)
                            {
                                if(CSR & MP_PCM_LOST_CLOCK)
                                {                                    
                                    if( (HealthStatusArr[config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID] & M23_PCM_CHANNEL_NO_CLOCK))
                                    {                                        
                                        M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID ,M23_PCM_CHANNEL_NO_CLOCK);
                                    }
                                }
                                else
                                {      
                                    if( !(HealthStatusArr[config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID] & M23_PCM_CHANNEL_NO_CLOCK))
                                    {                                        
                                        M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                    }

                                    CheckLock = 0;

                                    PCMSSRIC = 0;

                                }
                            }
                            else  //Bit Sync enabled
                            {

                                if((!(CSR & MP_PCM_BIT_SYNC_LOCKED)) || ((CSR & MP_PCM_BIT_ERROR_STICKY)))
                                {                                    
                                    if( !(HealthStatusArr[config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID] & M23_PCM_CHANNEL_NO_CLOCK))
                                    {                                        
                                        M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                    }
                                    CheckLock = 0;
                                    PCMSSRIC = 0;
                                }
                                else
                                {                                    
                                    if( (HealthStatusArr[config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID] & M23_PCM_CHANNEL_NO_CLOCK))
                                    {                                        
                                        M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                    }
                                    CheckLock = 1;

                                }
                            }
                            if(CheckLock == 1) //The Clock Is present
                            {
                                if( ((CSR & 0x000000F0) != MP_MAJOR_FRAME_LOCKED)  || (CSR & MP_PCM_LOST_MAJOR_LOCK_STICKY) )
                                {                                    
                                    if( !(HealthStatusArr[config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID] & M23_PCM_MAJOR_LOCK))
                                    {                                        
                                        M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_MAJOR_LOCK);
                                    }
                                    if(config->pcm_chan[(j+(pcm_4_index*4))-1].included == TRUE)
                                    {
                                        PCMOK = 0;
                                    }
                                    PCMSSRIC = 0;
                                }      
                                else
                                {                                    
                                    if( (HealthStatusArr[config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID] & M23_PCM_MAJOR_LOCK))
                                    {                                        
                                        M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_MAJOR_LOCK);
                                    }

                                }
                                if( ((CSR&0x0000000F) != MP_MINOR_FRAME_LOCKED) || (CSR & MP_PCM_LOST_MINOR_LOCK_STICKY))
                                {                                    
                                    if( !(HealthStatusArr[config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID] & M23_PCM_MINOR_LOCK))
                                    {                                        
                                        M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_MINOR_LOCK);
                                    }
                                    if(config->pcm_chan[(j+(pcm_4_index*4))-1].included == TRUE)
                                    {
                                        PCMOK = 0;
                                    }
                                    PCMSSRIC = 0;
                                }
                                else
                                {                                    
                                    if( (HealthStatusArr[config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID] & M23_PCM_MINOR_LOCK))
                                    {                                        
                                        M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_MINOR_LOCK);
                                    }

                                }

                                if(config->pcm_chan[(j+(pcm_4_index*4))-1].pcm_WatchWordOffset > 0)
                                {
                                    if( !(CSR & MP_PCM_WATCH_WORD_LOCKED) || (CSR & MP_PCM_WATCH_WORD_ERROR_STICKY) )
                                    {                                        
                                        PCMSSRIC = 0;
                                        if(config->pcm_chan[(j+(pcm_4_index*4))-1].included == TRUE)
                                        {                                            
                                            PCMOK = 0;
                                        }
                                    }

                                }

                            }
                            else
                            {                                
                                if(config->pcm_chan[(j+(pcm_4_index*4))-1].pcmBitSync == TRUE)
                                {
                                    if( (CSR & 0x000000F0) == MP_MAJOR_FRAME_LOCKED )
                                    {                                    
                                        if( (CSR&0x0000000F) == MP_MINOR_FRAME_LOCKED )
                                        {
                                            M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                        }
                                        else
                                        {
                                            M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                        }
                                    } 
                                    else
                                    {
                                        M23_SetHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                    }
                                }
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_MAJOR_LOCK);
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_4_index*4))- 1].chanID,M23_PCM_MINOR_LOCK);
                                PCMSSRIC = 0;
                                PCMOK = 0;
                            }

                        }

                    }
                    else
                    {                        
                        PCMSSRIC = 0;
                    }

                    /*Add Ssric here*/                    
                    status = ( 1 << ((j+(pcm_4_index*4)) + 3) ); //Bit Position for PCM
                    if(PCMSSRIC)
                    {

                        if(ssric == 1)
                            SSRIC_SetPCMStatus(status);
                    }
                    else
                    {
                        if(ssric == 1)
                            SSRIC_ClearPCMStatus(status);
                    }
                }
                pcm_4_index++;
            }
            else
            {
                for(j = 1; j<9;j++)
                {
                    CheckLock = 1;
                    if(config->pcm_chan[(j+(pcm_8_index*8))- 1].isEnabled)
                    {
                        if(pcm_paused)
                        {
                            if( !(HealthStatusArr[config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID] & M23_PCM_CHANNEL_PAUSED))
                            {
                                M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_CHANNEL_PAUSED);
                            }
                        }
                        else
                        {
                            if( (HealthStatusArr[config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID] & M23_PCM_CHANNEL_PAUSED))
                            {
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID ,M23_PCM_CHANNEL_PAUSED);
                            }
                        }

                        PCMSSRIC = 1;
                        NumEnablePCM++;
                        CSR = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * j) + MP_PCM_STATUS_OFFSET);
                        if(config->pcm_chan[(j+(pcm_8_index*8))-1].pcmCode == PCM_UART)
                        {
                            if(CSR & MP_UART_FRAME)
                            {
                                M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_UART_FRAME);
                            }
                            else
                            {
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_UART_FRAME);
                            }

                            if(CSR & MP_UART_PARITY)
                            {
                                M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_UART_PARITY);
                            }
                            else                            
                            {                                
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_UART_PARITY);
                            }

                            if(CSR & MP_UART_PACKET_DROP)
                            {                                
                                M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_UART_DROP_PACKET);
                            }
                            else
                            {                                
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_UART_DROP_PACKET);
                            }

                            if(CSR & MP_UART_NO_PACKET)
                            {                                
                                M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_UART_NO_PACKET);
                            }
                            else
                            {                                
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_UART_NO_PACKET);
                            }
                        }
                        else if(config->pcm_chan[(j+(pcm_8_index*8))-1].pcmCode == PCM_PULSE)
                        {
                        }
                        else
                        {
                            if(config->pcm_chan[(j+(pcm_8_index*8))-1].pcmBitSync == FALSE)
                            {
                                if(CSR & MP_PCM_LOST_CLOCK)
                                {                                    
                                    if( (HealthStatusArr[config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID] & M23_PCM_CHANNEL_NO_CLOCK))
                                    {                                        
                                        M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                    }
                                }
                                else
                                {                                    
                                    if( !(HealthStatusArr[config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID] & M23_PCM_CHANNEL_NO_CLOCK))
                                    {                                        
                                        M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                    }

                                    CheckLock = 0;

                                    PCMSSRIC = 0;

                                }
                            }
                            else  //Bit Sync enabled
                            {

                                if((!(CSR & MP_PCM_BIT_SYNC_LOCKED)) || ((CSR & MP_PCM_BIT_ERROR_STICKY)))
                                {                                    
                                    if( !(HealthStatusArr[config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID] & M23_PCM_CHANNEL_NO_CLOCK))
                                    {                                       
                                        M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                    }

                                    CheckLock = 0;
                                    PCMSSRIC = 0;
                                }
                                else
                                {                                    
                                    if( (HealthStatusArr[config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID] & M23_PCM_CHANNEL_NO_CLOCK))
                                    {                                        
                                        M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                    }
                                    CheckLock = 1;

                                }
                            }

                            if(CheckLock == 1) //The Clock Is present
                            {
                                if( ((CSR & 0x000000F0) != MP_MAJOR_FRAME_LOCKED)  || (CSR & MP_PCM_LOST_MAJOR_LOCK_STICKY) )
                                {                                    
                                    if( !(HealthStatusArr[config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID] & M23_PCM_MAJOR_LOCK))
                                    {                                        
                                        M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_MAJOR_LOCK);
                                    }
                                    if(config->pcm_chan[(j+(pcm_8_index*8))-1].included == TRUE)
                                    {
                                        PCMOK = 0;
                                    }
                                    PCMSSRIC = 0;
                                }                                
                                else
                                {                                    
                                    if( (HealthStatusArr[config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID] & M23_PCM_MAJOR_LOCK))
                                    {                                        
                                        M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_MAJOR_LOCK);
                                    }

                                }
                                if( ((CSR&0x0000000F) != MP_MINOR_FRAME_LOCKED) || (CSR & MP_PCM_LOST_MINOR_LOCK_STICKY))
                                {                                    
                                    if( !(HealthStatusArr[config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID] & M23_PCM_MINOR_LOCK))
                                    {                                        
                                        M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_MINOR_LOCK);
                                    }
                                    if(config->pcm_chan[(j+(pcm_8_index*8))-1].included == TRUE)
                                    {
                                        PCMOK = 0;
                                    }
                                    PCMSSRIC = 0;
                                }
                                else
                                {                                    
                                    if( (HealthStatusArr[config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID] & M23_PCM_MINOR_LOCK))
                                    {                                        
                                        M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_MINOR_LOCK);
                                    }

                                }
                                if(config->pcm_chan[(j+(pcm_8_index*8))-1].pcm_WatchWordOffset > 0)
                                {
                                    if( !(CSR & MP_PCM_WATCH_WORD_LOCKED) || (CSR & MP_PCM_WATCH_WORD_ERROR_STICKY) )
                                    {                                        
                                        PCMSSRIC = 0;
                                        if(config->pcm_chan[(j+(pcm_8_index*8))-1].included == TRUE)
                                        {                                            
                                            PCMOK = 0;
                                        }
                                    }

                                }

                            }
                            else
                            {                                
                                if(config->pcm_chan[(j+(pcm_8_index*8))-1].pcmBitSync == TRUE)
                                {
                                    if( (CSR & 0x000000F0) == MP_MAJOR_FRAME_LOCKED )
                                    {                                    
                                        if( (CSR&0x0000000F) == MP_MINOR_FRAME_LOCKED )
                                        {
                                            M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                        }
                                        else
                                        {
                                            M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                        }
                                    } 
                                    else
                                    {
                                        M23_SetHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_CHANNEL_NO_CLOCK);
                                    }
                                }
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID ,M23_PCM_MAJOR_LOCK);
                                M23_ClearHealthBit(config->pcm_chan[(j+(pcm_8_index*8))- 1].chanID,M23_PCM_MINOR_LOCK);
                                PCMSSRIC = 0;
                                PCMOK = 0;
                            }

                        }

                    }
                    else
                    {                        
                        PCMSSRIC = 0;
                    }

                    /*Add Ssric here*/                    
                    status = ( 1 << ((j+(pcm_8_index*4)) + 3) ); //Bit Position for PCM
                    if(PCMSSRIC)
                    {

                        if(ssric == 1)
                            SSRIC_SetPCMStatus(status);
                    }
                    else
                    {
                        if(ssric == 1)
                            SSRIC_ClearPCMStatus(status);
                    }
                }
                pcm_8_index++;
            }
        }
        else if( M23_MP_IS_DM(i))
        {
            for(j = 1; j<5;j++)
            {
                status = 0;
                if(config->m1553_chan[(j+(m1553_4_index*4)) -1].isEnabled)
                {
                    NumEnableM1553++;

                    if(config->m1553_chan[(j+(m1553_4_index*4)) -1].WatchWordEnabled == 1)
                    {
                        /*This will check the watch word bit*/
                        CSR1 = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_WATCH_WORD_EVENT_OFFSET);
                        if(CSR1 == 1)
                        {
                            if( !(HealthStatusArr[config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID] & M23_M1553_WATCH_WORD_ERROR))
                            {
                                M23_SetHealthBit(config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID ,M23_M1553_WATCH_WORD_ERROR);
                            }
                            M23_mp_write_csr(i,BAR1,0,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_WATCH_WORD_EVENT_OFFSET);
                            //if( ((Mask >> ((j + (i*4)) - 1)) & 0x1) == 0)
                            if(config->m1553_chan[(j+(m1553_4_index*4)) -1].included == TRUE)
                            {

                                M1553OK = 0;
                            }
                            status = ( 1 << ((j + (m1553_4_index*4)) -1) );
                            if(ssric == 1)
                                SSRIC_Clear1553Status(status);

                        }
                        else
                        {
                            if( (HealthStatusArr[config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID] & M23_M1553_WATCH_WORD_ERROR))
                            {
                                M23_ClearHealthBit(config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID,M23_M1553_WATCH_WORD_ERROR);
                            }
                            status = ( 1 << ((j + (m1553_4_index*4)) -1) );
                            if(ssric == 1)
                               SSRIC_Set1553Status(status);
                            Num1553OK++;

                        }
                    }
                    else
                    {
                        if( (HealthStatusArr[config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID] & M23_M1553_WATCH_WORD_ERROR))
                        {
                            M23_ClearHealthBit(config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID,M23_M1553_WATCH_WORD_ERROR);
                        }

                        Num1553OK++;

                        status = ( 1 << ((j + (m1553_4_index*4)) -1) );
                        if(ssric == 1)
                            SSRIC_Set1553Status(status);
                    }

                    /*This reads the No signal Bit*/
                    CSR1 = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_STATUS_OFFSET);
                    if(CSR1 == 0)
                    {
                        if(M1553_HealthTimes[(j+(m1553_4_index*4)) -1 ] == 4)
                        { 
                            if( !(HealthStatusArr[config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID] & M23_M1553_CHANNEL_NO_SIGNAL))
                            {
                                M23_SetHealthBit(config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID,M23_M1553_CHANNEL_NO_SIGNAL);
                            }

                            if(config->m1553_chan[(j+(m1553_4_index*4)) -1].WatchWordEnabled == 0)
                            {
                               // if( ((Mask >> ( (j + (i*4)) - 1)) & 0x1) == 0)
                                if(config->m1553_chan[(j+(m1553_4_index*4)) -1].included == TRUE)
                                {
                                    M1553OK = 0;
                                }
                                status = ( 1 << ( (j + (m1553_4_index*4)) -1) );
                                if(ssric == 1)
                                    SSRIC_Clear1553Status(status);
                            }
                        }
                        else
                        {
                            M1553_HealthTimes[(j+(m1553_4_index*4)) -1]++;
                        }
                    }
                    else
                    {
                        Num1553OK++;
                        if( (HealthStatusArr[config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID] & M23_M1553_CHANNEL_NO_SIGNAL))
                        {
                            M23_ClearHealthBit(config->m1553_chan[(j+(m1553_4_index*4)) -1].chanID,M23_M1553_CHANNEL_NO_SIGNAL);
                        }

                        M23_mp_write_csr(i,BAR1,0,(MP_CHANNEL_OFFSET * j) + MP_CHANNEL_STATUS_OFFSET);
                        M1553_HealthTimes[(j+(m1553_4_index*4))-1] = 0;

                        if(config->m1553_chan[(j+(m1553_4_index*4)) -1].WatchWordEnabled == 0)
                        {
                            status = ( 1 << ( (j + (m1553_4_index*4)) -1) );
                            if(ssric == 1)
                                SSRIC_Set1553Status(status);

                        }
                    }
                }
            }
	    m1553_4_index++;
        }
        else //This is a Video Board
        {

            for(j =0; j < 4;j++)
            {
                VIDEOSSRIC = 1;

                CSR1 = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_VIDEO_UPSTREAM_SELECT);


                //Change this to read the No packets from the video board*/
                CSR2 = M23_mp_read_csr(i,BAR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_CHANNEL_OVERLAY);
                
                video_no_packets = 0;

                if(!(CSR2 & MP_VIDEO_NO_PACKETS))
                {      
                    video_no_packets = 1;
                }


                if((config->video_chan[j +(4*video_index)].isEnabled) && ((j+(4*video_index)) < config->NumConfiguredVideo))
                {
                
                    if(CSR2 & MP_VIDEO_NO_SIGNAL)
                    {
    
                        if(!(HealthStatusArr[config->video_chan[j +(4*video_index)].chanID] & M23_VIDEO_CHANNEL_NO_VIDEO_SIGNAL))
                        {
                            M23_SetHealthBit(config->video_chan[j +(4*video_index)].chanID,M23_VIDEO_CHANNEL_NO_VIDEO_SIGNAL);
                            CSR1 |= MP_VIDEO_LOSS_SYNC;
                            M23_mp_write_csr(i,BAR1,CSR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_VIDEO_UPSTREAM_SELECT);
                        }

                       // if( (Mask & 0x1) == 0)
                        if(config->video_chan[j + (4*video_index)].included == TRUE)
                        {
                            VideosOK = 0;
                        }

                        VIDEOSSRIC = 0;
                    }
                    else
                    {
                        if(HealthStatusArr[config->video_chan[j +(4*video_index)].chanID] & M23_VIDEO_CHANNEL_NO_VIDEO_SIGNAL)
                        {
                            M23_ClearHealthBit(config->video_chan[j +(4*video_index)].chanID,M23_VIDEO_CHANNEL_NO_VIDEO_SIGNAL);
                            CSR1 &= ~MP_VIDEO_LOSS_SYNC;
                            M23_mp_write_csr(i,BAR1,CSR1,(MP_CHANNEL_OFFSET * (j+1)) + MP_VIDEO_UPSTREAM_SELECT);

                        }
                        VideosOK = 1;
    
                    }

                    if(video_no_packets)
                    {
                        if(!(HealthStatusArr[config->video_chan[j +(4*video_index)].chanID] & M23_VIDEO_CHANNEL_NO_PACKETS))
                        {
                            M23_SetHealthBit(config->video_chan[j +(4*video_index)].chanID,M23_VIDEO_CHANNEL_NO_PACKETS);

                        }

                       // if( (Mask & 0x1) == 0)
                        if(config->video_chan[j+(4*video_index)].included == TRUE)
                        {
                            VideosOK = 0;
                        }

                        VIDEOSSRIC = 0;
                    }
                    else
                    {
                        if(HealthStatusArr[config->video_chan[j +(4*video_index)].chanID] & M23_VIDEO_CHANNEL_NO_PACKETS)
                        {
                            M23_ClearHealthBit(config->video_chan[j +(4*video_index)].chanID,M23_VIDEO_CHANNEL_NO_PACKETS);
                        }

                    }

                    status = ( 1 << j);
                    if(VIDEOSSRIC)
                    {
                        if(ssric == 1)
                            SSRIC_SetVideoStatus(status);
                    }
                    else
                    {
                        if(ssric == 1)
                            SSRIC_ClearVideoStatus(status);
                    }
                }
            }


            if( (VideosOK == 0) || (VideosOK == 2) )
            {
                M23_ClearVideoLED();
            }
            else
            {
                M23_SetVideoLED();
            }
 
            video_index++;
        }
    }


    /*Now check if any M1553 channels enabled were not configured*/
    for(i = 0;i< config->NumConfigured1553;i++)
    {
        if(HealthStatusArr[config->m1553_chan[i].chanID] & M23_M1553_CHANNEL_CONFIGURE_FAIL)
        {
            M1553OK = 0;
        }
    }

//PutResponse(0,"f");
    if( (M1553OK == 0) || (NumEnableM1553 == 0) ||(Num1553OK == 0) )
    {
        M23_ClearM1553LED();
    }
    else
    {
        M23_SetM1553LED();
    }

    /*Now check if any PCm channels enabled were not configured*/
    for(i = 0;i< config->NumConfiguredPCM;i++)
    {
        if(HealthStatusArr[config->pcm_chan[i].chanID] & M23_PCM_CHANNEL_CONFIGURE_FAIL)
        {
            PCMOK = 0;
        }
    }
    if( (PCMOK == 0) || (NumEnablePCM == 0) )
    {
        M23_ClearPCMLED();
    }
    else
    {
        M23_SetPCMLED();
    }

    if(config->timecode_chan.Source == TIMESOURCE_IS_IRIG)
    {
        /*This will retrieve the Time code Health Status*/
        CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR); 

        if( (config->timecode_chan.Format == 'U') || (config->timecode_chan.Format == 'N') )
        {


            if( CSR & CONTROLLER_TIMECODE_IN_SYNC)
            {
                if(GPSJammed())
                    M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_FLYWHEEL);
                else
                    M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_FLYWHEEL);

                M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_SYNC_ERROR);
                M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_NO_CARRIER);
            }
            else
            {
                M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_SYNC_ERROR);
                M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_NO_CARRIER );
                M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_FLYWHEEL);
            }
        }
        else
        {
 
            if( CSR & CONTROLLER_TIMECODE_LOSS_CARRIER)
            {
                /*This checks if the No Carrier status is already set*/
                if(!(HealthStatusArr[TIMECODE_FEATURE] & M23_TIMECODE_NO_CARRIER))
                {
                    M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_NO_CARRIER );
                    M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_FLYWHEEL);
                }
                if(ssric == 1)
                    SSRIC_SetTimeStatus();
                M23_ClearTimeLED();
            }
            else if( !(CSR & CONTROLLER_TIMECODE_CARRIER))
            {
                /*This checks if the No Carrier status is already set*/
                if(!(HealthStatusArr[TIMECODE_FEATURE] & M23_TIMECODE_NO_CARRIER))
                {
                    M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_NO_CARRIER);
                    M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_FLYWHEEL);

                }
                if(ssric == 1)
                    SSRIC_ClearTimeStatus();
                M23_ClearTimeLED();
            }
            else
            {
                /*This checks if the No Carrier status is already set*/
                if(HealthStatusArr[TIMECODE_FEATURE] & M23_TIMECODE_NO_CARRIER)
                {
                    /*Train the Time Code Circuit*/
                    M23_TrainTimeCode();

                    M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_NO_CARRIER);
                    M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_FLYWHEEL);
                    if(ssric == 1)
                        SSRIC_SetTimeStatus();
                    M23_SetTimeLED();
                }
            }

            if( !(CSR & CONTROLLER_TIMECODE_IN_SYNC))
            {
                /*This checks if the No Carrier status is already set*/
                if(!(HealthStatusArr[TIMECODE_FEATURE] & M23_TIMECODE_SYNC_ERROR))
                {
                    M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_SYNC_ERROR);
                }

                if( CSR & CONTROLLER_TIMECODE_CARRIER)
                {
                    if(SyncLoops > 50)
                    {
                        /*Train the Time Code Circuit*/
                        M23_TrainTimeCode();
                        SyncLoops = 0;
                    }
                    else
                    {
                        SyncLoops++;
                    }
                }
           
                if(ssric == 1)
                    SSRIC_ClearTimeStatus();
                M23_ClearTimeLED();
            }
            else
            {
                /*This checks if the No Carrier status is already set*/
                if(HealthStatusArr[TIMECODE_FEATURE] & M23_TIMECODE_SYNC_ERROR)
                {
                    M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_SYNC_ERROR);

                    CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
                    CSR &= ~TIMECODE_M1553_JAM;
                    CSR &= ~TIMECODE_RMM_JAM;
                    M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);

                }

                if(ssric == 1)
                    SSRIC_SetTimeStatus();
                M23_SetTimeLED();
                SyncLoops = 0;
            }
        }
    }
    else
    {

        if(config->timecode_chan.Source == TIMESOURCE_IS_RMM)
        {
            M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_NO_CARRIER);
            M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_SYNC_ERROR);

            CSR = Is_RMM_Jammed();
            if(CSR)
            {
                M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_FLYWHEEL);

                M23_SetTimeLED();
                if(ssric == 1)
                    SSRIC_SetTimeStatus();
            }
            else
            {
                M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_FLYWHEEL);
            }
        }
        else if(config->timecode_chan.Source == TIMESOURCE_IS_M1553)
        {
            M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_NO_CARRIER);
            CSR = M23_M1553_IsTimeJammed();
            if(CSR)
            {
                M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_SYNC_ERROR);
                M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_FLYWHEEL);
                M23_SetTimeLED();
                if(ssric == 1)
                    SSRIC_SetTimeStatus();
            }
            
        }
        else //Internal Flywheel 
        {
            M23_SetHealthBit(TIMECODE_FEATURE,M23_TIMECODE_FLYWHEEL);
            M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_NO_CARRIER);
            M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_SYNC_ERROR);
            M23_SetTimeLED();
            if(ssric == 1)
                SSRIC_SetTimeStatus();
        }

    }


    if(config->eth_chan.isEnabled)
    {
        M23_RadarENETStatus(&enet_state,&enet_data);

        if(enet_state != 6)
            M23_SetHealthBit(config->eth_chan.chanID,M23_ETHERNET_NOT_READY);
        else
            M23_ClearHealthBit(config->eth_chan.chanID,M23_ETHERNET_NOT_READY);

        if( enet_data != PreviousEthData)
            M23_ClearHealthBit(config->eth_chan.chanID,M23_ETHERNET_NO_DATA_PRESENT);
        else
            M23_SetHealthBit(config->eth_chan.chanID,M23_ETHERNET_NO_DATA_PRESENT);

        PreviousEthData = enet_data; //Used to check if Ethernet data is flowing

    }



    /*Fill in Health when given the Data*/
    if(config->uart_chan.isEnabled)
    {
    }

    M23_GetFaultLED(&Ready);

    NumCritical = ReturnNumberOfCritical();
    if( NumCritical)
    {
        M23_SetFaultLED(1);
    }
    else
    {
        M23_ClearFaultLED(1); 
    }

//PutResponse(0,"i\r\n");
//PutResponse(0,"6");
//PutResponse(0,"&");
}


int M23_CheckTimecodeSync()
{
    int         status = 0;
    UINT32      CSR;
    M23_CHANNEL *config;

    SetupGet(&config);

    if(config->timecode_chan.Source == TIMESOURCE_IS_IRIG)
    {

        /*This will retrieve the Time code Health Status*/
        CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);

        if( CSR & CONTROLLER_TIMECODE_LOSS_CARRIER)
        {
            status = 1;
        }

        if( !(CSR & CONTROLLER_TIMECODE_IN_SYNC))
        {
            status = 1;
        }
    }
    else
    {
        status = 1;
    }

    return(status) ;

}


void M23_InitializeHealth()
{
    int i;

    ClearVideo = 0;

    for(i = 0; i < 16;i++)
    {
        ResetCount[i]  = 0;
        StableCount[i] = 0;
    }
    /*Set the timeouts for M1553 Status*/
    for(i = 0; i < 32;i++)
    {
        M1553_HealthTimes[i] = 0;
    }

    // 'CriticalMasks' settings are taken from non-volatile settings file below
    //

    for(i = 0; i < MAX_FEATURES; i++)
    {
        HealthArrayTypes[i]    = NO_FEATURE;
        HealthStatusArr[i]     = 0x0;
    }
    
    memset(FeatureDescriptions,0x0,sizeof(FeatureDescriptions));


    // Recorder Feature
    // 
    HealthStatusArr[RECORDER_FEATURE] |= M23_FEATURE_ENABLED;  // always enabled
    HealthArrayTypes[RECORDER_FEATURE] = RECORDER_FEATURE;
    strncpy(FeatureDescriptions[RECORDER_FEATURE],"Recorder",8);

}

void M23_InitializeChannelHealth()
{

    int i;
    int pcm_index = 0;
    int m1553_index = 0;
    int video_index = 0;
    int voice_index = 0;
    M23_CHANNEL const *p_config;


    SetupGet( &p_config );

    for(i = 1 ; i< MAX_FEATURES;i++)
    {
        HealthStatusArr[i] = 0;

        switch(HealthArrayTypes[i])
        {
            case TIMECODE_FEATURE:
                // Timecode Feature  -
                if(p_config->timecode_chan.isEnabled)
                {
                    HealthStatusArr[i] = M23_FEATURE_ENABLED | M23_TIMECODE_CONFIG_FAIL;
                }
                strncpy(FeatureDescriptions[i],p_config->timecode_chan.chanName,strlen(p_config->timecode_chan.chanName) -1);
                break;
            case VOICE_FEATURE:
                // Voice Feature
                if(p_config->voice_chan[voice_index].isEnabled)
                {
                    HealthStatusArr[i] = M23_FEATURE_ENABLED | M23_VOICE_CONFIG_FAIL;
                }
                strncpy(FeatureDescriptions[i],p_config->voice_chan[voice_index].chanName,strlen(p_config->voice_chan[voice_index].chanName) - 1);
                voice_index++;
                break;
            case PCM_FEATURE:
                if(p_config->pcm_chan[pcm_index].isEnabled)
                {
                    HealthStatusArr[i] = M23_FEATURE_ENABLED | M23_PCM_CHANNEL_CONFIGURE_FAIL;
                }
                strncpy(FeatureDescriptions[i],p_config->pcm_chan[pcm_index].chanName,strlen(p_config->pcm_chan[pcm_index].chanName) - 1);
                pcm_index++;
                break;
            case M1553_FEATURE:
                if(p_config->m1553_chan[m1553_index].isEnabled)
                {
                    HealthStatusArr[i] = M23_FEATURE_ENABLED | M23_M1553_CHANNEL_CONFIGURE_FAIL;
                }
                strncpy(FeatureDescriptions[i],p_config->m1553_chan[m1553_index].chanName,strlen(p_config->m1553_chan[m1553_index].chanName) -1);
                m1553_index++;
                break;
            case VIDEO_FEATURE:
                if(p_config->video_chan[video_index].isEnabled)
                {
                    HealthStatusArr[i] = M23_FEATURE_ENABLED | M23_VIDEO_CHANNEL_CONFIGURE_FAIL;
                }
                strncpy(FeatureDescriptions[i],p_config->video_chan[video_index].chanName,strlen(p_config->video_chan[video_index].chanName) - 1);
                video_index++;
                break;
            case ETHERNET_FEATURE:
                // Ethernet Feature 
                if(p_config->eth_chan.isEnabled)
                {
                    HealthStatusArr[i] = M23_FEATURE_ENABLED | M23_ETHERNET_CONFIG_FAIL;
                }
                strncpy(FeatureDescriptions[i],p_config->eth_chan.chanName,strlen(p_config->eth_chan.chanName) -1);
                break;
            case UART_FEATURE:
                // Ethernet Feature 
                if(p_config->uart_chan.isEnabled)
                {
                    HealthStatusArr[i] = M23_FEATURE_ENABLED | M23_UART_CONTROLLER_CONFIGURE_FAIL;
                }
                strncpy(FeatureDescriptions[i],p_config->uart_chan.chanName,strlen(p_config->uart_chan.chanName) -1);
                break;
            case DISCRETE_FEATURE:
                if(p_config->dm_chan.isEnabled)
                {
                    HealthStatusArr[i] = M23_FEATURE_ENABLED | M23_UART_CONTROLLER_CONFIGURE_FAIL;
                }
                strncpy(FeatureDescriptions[i],p_config->dm_chan.chanName,strlen(p_config->dm_chan.chanName) -1);
                break;
        }
    }


}


void M23_SetHealthEvent(int channel,int stat)
{
    int  event = 0;
    int  start;
    int  i;
    int  config;

    M23_CHANNEL const *chan;


    M23_GetConfig(&config);

    if(config != LOCKHEED_CONFIG)
    {
        return;
    }

    SetupGet( &chan );


    if( HealthArrayTypes[channel] == RECORDER_FEATURE )
    {
        if(stat == M23_RECORDER_BIT_FAIL)
        {
            event = 8;
        }
        else if(stat == M23_RECORDER_CONFIGURATION_FAIL)
        {
            event = 9;
        }
        else if(stat == M23_RECORDER_OPERATION_FAIL)
        {
            event = 10;
        }
        else if(stat == M23_MEMORY_MEDIA_BUSY)
        {
            event = 11;
        }
        else if(stat == M23_MEMORY_NOT_PRESENT)
        {
            event = 12;
        }
        else if(stat == M23_MEMORY_IO_ERROR)
        {
            event = 13;
        }
        else if(stat == M23_MEMORY_FULL)
        {
            event = 14;
        }
        else if(stat == M23_MEMORY_BINGO_REACHED)
        {
            event = 15;
        }
        else if(stat == M23_MEMORY_LATCH)
        {
            event = 16;
        }
        else if(stat == M23_DISK_NOT_CONNECTED)
        {
            event = 17;
        }

        //sprintf(tmp,"H %02d %08X Recorder",channel+1,status);
    }
    else if( HealthArrayTypes[channel] == TIMECODE_FEATURE )
    {
        if(stat == M23_TIMECODE_BIT_FAIL)
        {
            event = 18;
        }
        else if(stat == M23_TIMECODE_CONFIG_FAIL)
        {
            event = 19;
        }
        else if(stat == M23_TIMECODE_NO_CARRIER)
        {
            event = 20;
        }
        else if(stat == M23_TIMECODE_BAD_CARRIER)
        {
            event = 21;
        }
        else if(stat == M23_TIMECODE_SYNC_ERROR)
        {
            event = 22;
        }
        else if(stat == M23_TIMECODE_FLYWHEEL)
        {
            event = 23;
        }

        //sprintf(tmp,"H %02d %08X TimeCode",channel+1,status);
    }
    else if( HealthArrayTypes[channel] == VOICE_FEATURE )
    {
        for(i = 0; i < chan->NumConfiguredVoice;i++)
        {
            if(chan->voice_chan[i].chanID == channel)
            {
                if(i == 0)
                {
                    start = 24;
                    break;
                }
                else if(i == 1)
                {
                    start = 27;
                    break;
                }
            } 
        } 

        if(stat == M23_VOICE_BIT_FAIL)
        {
            event = start + 0;
        }
        else if(stat == M23_VOICE_CONFIG_FAIL)
        {
            event = start + 1;
        }
        else if(stat == M23_VOICE_NO_SIGNAL)
        {
            event = start + 2;
        }
        else if(stat == M23_VOICE_BAD_SIGNAL)
        {
            event = start + 3;
        }
        else if(stat == M23_VOICE_CHANNEL_PAUSED)
        {
            event = start + 4;
        }

        //sprintf(tmp,"H %02d %08X Voice %d",channel+1,status,i+1);
    }
    else if( HealthArrayTypes[channel] == VIDEO_FEATURE )
    {
        for(i = 0; i < chan->NumConfiguredVideo;i++)
        {
            if(chan->video_chan[i].chanID == channel)
            {
                if(i == 0)
                {
                    start = 29;
                    break;
                }
                else if(i == 1)
                {
                    start = 37;
                    break;
                }
                else if(i == 2)
                {
                    start = 45;
                    break;
                }
                else if(i == 3)
                {
                    start = 53;
                    break;
                }
                else if(i == 4)
                {
                    start = 61;
                    break;
                }
                else if(i == 5)
                {
                    start = 69;
                    break;
                }
                else if(i == 6)
                {
                    start = 77;
                    break;
                }
                else if(i == 7)
                {
                    start = 85;
                    break;
                }
            } 
        } 

        if(stat == M23_VIDEO_BIT_FAIL)
        {
            event = start + 0;
        }
        else if(stat == M23_VIDEO_CHANNEL_CONFIGURE_FAIL)
        {
            event = start + 1;
        }
        else if(stat == M23_VIDEO_CHANNEL_NO_VIDEO_SIGNAL)
        {
            event = start + 2;
        }
        else if(stat == M23_VIDEO_CHANNEL_BAD_VIDEO_SIGNAL)
        {
            event = start + 3;
        }
        else if(stat == M23_VIDEO_CHANNEL_NO_AUDIO_SIGNAL)
        {
            event = start + 4;
        }
        else if(stat == M23_VIDEO_CHANNEL_BAD_AUDIO_SIGNAL)
        {
            event = start + 5;
        }
        else if(stat == M23_VIDEO_CHANNEL_CHANNEL_PAUSED)
        {
            event = start + 6;
        }
        else if(stat == M23_VIDEO_CHANNEL_NO_PACKETS)
        {
            event = start + 7;
        }

        //sprintf(tmp,"H %02d %08X Video  %d",channel+1,status,i+1);

    }

    if(event != 0)
    {
        if(EventTable[event].CalculexEvent)
        {
            EventSet(event);
        }
    }
}

void M23_SetInitHealthBit(int channel, int status)
{
    HealthStatusArr[channel] |= status;
}

void M23_SetHealthBit(int channel, int status)
{
    HealthStatusArr[channel] |= status;

    M23_SetHealthEvent(channel,status);

}

void M23_ClearInitHealthBit(int channel, int status)
{
    HealthStatusArr[channel] &= ~status;
}

void M23_ClearHealthBit(int channel, int status)
{
    HealthStatusArr[channel] &= ~status;

    //M23_SetHealthEvent(channel,status);

}

void M23_HealthProcessor()
{

    PreviousEthData = 0; //Used to check if Ethernet data is flowing

    while(1)
    {

        /*Get the Status from each configured Channel*/
        M23_CheckStatus();

        usleep(200000);

    }

}

void M23_StartHealthThread()
{
    int status;
    int debug;

    M23_GetDebugValue(&debug);

    status = pthread_create(&HealthStatusThread, NULL,(void *) M23_HealthProcessor, NULL);

    if(debug)
    {
        if(status == 0)
        {
            printf("Health Status Thread Created Successfully\r\n");
        }
        else
        {
            perror("Health Status Create\r\n");
        }
    }
}
                              

