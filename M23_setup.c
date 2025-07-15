
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: setup.c
//    Version: 1.0
//     Author: dm
//
//            MONSSTR 2100 Setup Module
//
//            Coordinates parsing of TMATS files and the configuration
//            of each of the channel types.
//
//    Revisions:   
//
#include <stdio.h>
#include <string.h>
#include "M23_setup.h"
#include "M2X_Const.h"
#include "M2X_Tmats.h"
#include "M23_Controller.h"
#include "M23_Utilities.h"
#include "M23_features.h"
#include "M23_MP_Handler.h"
#include "M23_Ssric.h"

static M23_CHANNEL  Config;

int TimeSource;

void M23_StartLiveVid()
{
    int    i;
    int    state;
    int    live_set = 0;
    int    live;
    UINT32 CSR;

    M23_RecorderState(&state);


    if( (state == STATUS_PLAY) || (state == STATUS_REC_N_PLAY) )
    {
        return;
    }

    for(i = 0; i < Config.NumConfiguredVideo;i++)
    {
        if(Config.video_chan[i].VideoOut)
        {
            M23_SetLiveVideo( i+1 ); //Config.video_chan[i].chanID);

            i = Config.NumConfiguredVideo;
 
            live_set = 1;
        }
    }

    if(live_set == 0)
    {
        M23_GetLiveVideo(&live);

        if(live > 0)
        {
            M23_SetLiveVideo(live);
        }
    }
}

void SetupCondor()
{
    if(Config.M1553_RT_Control != M1553_RT_NONE)
    {
        M23_InitializeCONDOR();
        M23_WriteControllerCSR(CONTROLLER_RT_HOST_ADDRESS_CSR,CONTROLLER_RT_CONTROL);
    }
}

void SetupIndex()
{
    UINT32 CSR;

    M23_WriteControllerCSR(CONTROLLER_INDEX_CHAN_ID,BROADCAST_MASK);

    CSR = M23_ReadControllerCSR(CONTROLLER_INDEX_CHANNEL_GO);
    if( Config.IndexIsEnabled )
    {

        if( indexChannel.IndexType == INDEX_CHANNEL_COUNT)
        {
            /*Enable The Index Channel and Setup for index per packet*/
            M23_WriteControllerCSR(CONTROLLER_INDEX_CHANNEL_GO,CSR | INDEX_CHANNEL_GO | CONTROLLER_INDEX_PER_PACKET);
        }
        else
        {
            /*Enable The Index Channel and Setup for index per packet*/
            M23_WriteControllerCSR(CONTROLLER_INDEX_CHANNEL_GO,CSR | INDEX_CHANNEL_GO);
/*For now all all INDEX per packets*/
            //M23_WriteControllerCSR(CONTROLLER_INDEX_CHANNEL_GO,CSR | INDEX_CHANNEL_GO | CONTROLLER_INDEX_PER_PACKET);
        }


    }
    else
    {
        M23_WriteControllerCSR(CONTROLLER_INDEX_CHANNEL_GO,CSR & ~INDEX_CHANNEL_GO);
    }

    CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR);
    if(Config.EventsAreEnabled)
    {
        M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR,CSR |  CONTROLLER_EVENT_GO);

    }
    else
    {
        M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR,CSR & ~CONTROLLER_EVENT_GO);
    }
}

void SetupHardware()
{
    UINT8  IrigType;
    UINT32 CSR;
    UINT32 TIMEEXTERN = 0;
    int    i;
    int    j;
    int    k;
    int    year;
    int    debug;
    int    m1553_4_index = 0;
    int    m1553_8_index = 0;
    int    pcm_4_index = 0;
    int    pcm_8_index = 0;
    int    video_index = 0;
    int    dm_index = 0;
    int    NumMPBoards = 0;
    int    NumVideo = 0;
    int    isGPS = 0;

    int    RadarEnet;
    int    OurEnet;
    int    Offset;


    M23_GetDebugValue(&debug);


    /*Setup the Timecode Channel*/
    if(Config.timecode_chan.isEnabled)
    {
        /*Set the Channel ID*/
        CSR = (UINT16)(Config.timecode_chan.chanID | BROADCAST_MASK);

        /*Set the IRIG type*/
        switch(Config.timecode_chan.Format)
        {
            case 'A':
                IrigType = 0x1;
                if( Config.IndexIsEnabled )
                {
                    M23_WriteControllerCSR(CONTROLLER_TIME_PER_ROOT,0x0A);
                }
                break;
            case 'B':
                IrigType = 0x0;
                if( Config.IndexIsEnabled )
                {
                    M23_WriteControllerCSR(CONTROLLER_TIME_PER_ROOT,0x01);
                }
                break;
            case 'G':
                IrigType = 0x2;
                if( Config.IndexIsEnabled )
                {
                    M23_WriteControllerCSR(CONTROLLER_TIME_PER_ROOT,0x64);
                }
                break;
            case 'N':
            case 'U':
                IrigType = 0x0;
                isGPS = 1;
                if( Config.IndexIsEnabled )
                {
                    M23_WriteControllerCSR(CONTROLLER_TIME_PER_ROOT,0x01);
                }
                break;
            default:
                IrigType = 0x0;
                if( Config.IndexIsEnabled )
                {
                    M23_WriteControllerCSR(CONTROLLER_TIME_PER_ROOT,0x01);
                }
                break;
        }
        CSR |= (IrigType << 20);

        /*Set the IRIG Source*/
        switch(Config.timecode_chan.Source)
        {
            case TIMESOURCE_IS_INTERNAL:
                TimeSource = TIMESOURCE_INTERNAL;
                break;
            case TIMESOURCE_IS_IRIG:
                TimeSource = TIMESOURCE_EXTERNAL;
                TIMEEXTERN = CONTROLLER_TIMECODE_EXTERNAL;

                if(isGPS == 0)
                {
                    /*This is set so that we can train the DAC*/
                    M23_SetInitHealthBit(TIMECODE_FEATURE,M23_TIMECODE_NO_CARRIER);
                }

                break;
            case TIMESOURCE_IS_RMM:
                TimeSource = TIMESOURCE_CARTRIDGE;
                break;
            case TIMESOURCE_IS_M1553:
                TimeSource = TIMESOURCE_M1553;
                M23_SetInitHealthBit(TIMECODE_FEATURE,M23_TIMECODE_SYNC_ERROR);
                break;
 
        }
        
        CSR |= TIMEEXTERN;
        M23_GetYear(&year);
        if((year% 4) == 0) //This is a leap year
            CSR |= CONTROLLER_TIMECODE_LEAPYEAR;

        M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);


        /*Set the Enable Bit*/
        CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_CSR);
        if(isGPS)
        {
            CSR |= TIME_USE_GPS;
            //CSR |= TIME_INHIBIT_RESEED;
            //CSR |= TIME_INHIBIT_RESYNC;
            M23_SetInitHealthBit(TIMECODE_FEATURE,M23_TIMECODE_GPS_NOT_SYNCED);
        }
        else
        {
            CSR &= ~TIME_USE_GPS;
            CSR &= ~TIME_INHIBIT_RESEED;
            CSR &= ~TIME_INHIBIT_RESYNC;

            SetDontJamGPS();
        }

        M23_WriteControllerCSR(CONTROLLER_TIMECODE_CSR,CSR | CONTROLLER_TIMECODE_ENABLE);


        M23_ClearHealthBit(Config.timecode_chan.chanID,M23_TIMECODE_CONFIG_FAIL);
    }

    /*TRD -- Need to add actual CSR when available*/
    for(i = 0;i < Config.NumConfiguredVoice;i++)
    {
        if(i == 0)
        {
            M23_WriteControllerCSR(CONTROLLER_VOICE1_CHANNEL_ID_CSR,(Config.voice_chan[i].chanID | BROADCAST_MASK));

            M23_WriteControllerCSR(CONTROLLER_VOICE1_CHANNEL_SRC,0x0);
            M23_SetAudioGain(Config.voice_chan[i].Gain,0);
        }
        else
        {
            M23_WriteControllerCSR(CONTROLLER_VOICE2_CHANNEL_ID_CSR,(Config.voice_chan[i].chanID | BROADCAST_MASK));

            M23_WriteControllerCSR(CONTROLLER_VOICE2_CHANNEL_SRC,CONTROLLER_VOICE_LEFT);
            M23_SetAudioGain(Config.voice_chan[i].Gain,1);
        }

        /*Setup the Voice Channel*/
        if(Config.voice_chan[i].isEnabled)
        {
            if(i == 0)
            {
                M23_WriteControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR,CONTROLLER_VOICE_ENABLE);
            }
            else
            {
                M23_WriteControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR,CONTROLLER_VOICE_ENABLE);
            }
        }
        else
        {
            if(i == 0)
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR);
                CSR &= ~CONTROLLER_VOICE_ENABLE;
                M23_WriteControllerCSR(CONTROLLER_VOICE1_CONTROL_CSR,CSR);
            }
            else
            {
                CSR = M23_ReadControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR);
                CSR &= ~CONTROLLER_VOICE_ENABLE;
                M23_WriteControllerCSR(CONTROLLER_VOICE2_CONTROL_CSR,CSR);
            }
        }

        M23_ClearHealthBit(Config.voice_chan[i].chanID,M23_VOICE_CONFIG_FAIL);
    }

    if(Config.NumConfiguredVoice == 1)
    {
        M23_WriteControllerCSR(CONTROLLER_AUDIO_MUX,CONTROLLER_RIGHT_RIGHT);
    }
    else
    {
        M23_WriteControllerCSR(CONTROLLER_AUDIO_MUX,CONTROLLER_LEFT_RIGHT);
    }
  

    M23_NumberMPBoards(&NumMPBoards);
    NumVideo = M23_MP_GetNumVideo();

    for(i = 0; i < NumMPBoards;i++)
    {

        if(M23_MP_IS_M1553(i) )
        {
            M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,0,MP_M1553_RT_SELECTION);
            /* Setup the M1553 channels that are enabled*/
            for(j = 0; j < 8;j++)
            {
                M23_ClearHealthBit(Config.m1553_chan[j + (m1553_8_index*8)].chanID ,M23_M1553_CHANNEL_CONFIGURE_FAIL);
                if(Config.m1553_chan[j + (m1553_8_index*8)].isEnabled)
                {
                    M23_MP_Initialize_M1553_Channel((MP_DEVICE_0 + i),j+1,Config.m1553_chan[j+(m1553_8_index*8)],1);
                    M23_MP_Enable_M1553_Channel((MP_DEVICE_0 + i),j+1);
                }else {
                    M23_MP_Initialize_M1553_Channel((MP_DEVICE_0 + i),j+1,Config.m1553_chan[j+(m1553_8_index*8)],0);
                    M23_ExcludeAll((MP_DEVICE_0 + i),j+1,0);
                }

            }

            m1553_8_index++;

        }
        else if(M23_MP_IS_PCM(i))
        {
            if(M23_MP_PCM_4_Channel(i))
            {
                /*Setup PCM Channels that are enabled*/
                for(j=0;j<4;j++)
                {
                    if(Config.pcm_chan[j + (pcm_4_index * 4)].isEnabled)
                    {     
                        M23_MP_Initialize_PCM_Channel( (MP_DEVICE_0 + i),j+1,Config.pcm_chan[j+(pcm_4_index * 4)]); 
                        M23_ClearHealthBit(Config.pcm_chan[j + (pcm_4_index * 4)].chanID ,M23_PCM_CHANNEL_CONFIGURE_FAIL);
                    }
                }
                pcm_4_index++;
            }
            else  //This is an 8 channel PCM
            {
                /*Setup PCM Channels that are enabled*/
                for(j=0;j<8;j++)
                {
                    if(Config.pcm_chan[j + (pcm_8_index * 8)].isEnabled)
                    {                        
                        M23_MP_Initialize_PCM_Channel( (MP_DEVICE_0 + i),j+1,Config.pcm_chan[j+(pcm_8_index * 8)]);
                        M23_ClearHealthBit(Config.pcm_chan[j + (pcm_8_index * 8)].chanID,M23_PCM_CHANNEL_CONFIGURE_FAIL);
                    }
                }
                pcm_8_index++;
            }

        }
        else if(M23_MP_IS_DM(i)) /*This is a Discrete Board with 4 busses*/
        {
            M23_mp_write_csr((MP_DEVICE_0 + i),BAR1,0,MP_M1553_RT_SELECTION);

            /* Setup the M1553 channels that are enabled*/
            for(j = 0; j < 4;j++)
            {
                M23_MP_Initialize_M1553_Channel((MP_DEVICE_0 + i),j+1,Config.m1553_chan[j + (m1553_4_index*4)]);
                M23_ClearHealthBit(Config.m1553_chan[j + (m1553_4_index*4)].chanID,M23_M1553_CHANNEL_CONFIGURE_FAIL);
                if(Config.m1553_chan[j + (m1553_4_index*4)].isEnabled)
                {
                    M23_MP_Enable_M1553_Channel((MP_DEVICE_0 + i),j+1);
                }
            }

            m1553_4_index++;

            M23_MP_Initialize_DM_Channel((MP_DEVICE_0 + i),j+1,Config.dm_chan);
            M23_ClearHealthBit(Config.dm_chan.chanID ,M23_M1553_CHANNEL_CONFIGURE_FAIL);
            dm_index++;
        }
        else if(M23_MP_IS_Video(i)) //This is a Video Board 
        {
            if(NumVideo > 0)
            {
                SetNeedSync();
                for(j=0;j<4;j++)
                {
                    M23_InitializeVideoChannels(&(Config.video_chan[j +(4*video_index)]),Config.video_chan[j+(4*video_index)].chanNumber,j+(4*video_index),MP_DEVICE_0 + i);
                }

                for(k = 0; k < 2; k++)
                {
                    M23_MP_Initialize_Video_I2c((MP_DEVICE_0 + i) ,video_index);
                    for(j=0;j<4;j++)
                    {
                        if(Config.video_chan[j + (4*video_index)].isEnabled)
                        {
                            M23_MP_Initialize_Video_Channel( (MP_DEVICE_0 + i),j+1,Config.video_chan[j + (4*video_index)],video_index);
                            M23_ClearInitHealthBit(Config.video_chan[j + (4*video_index)].chanID ,M23_VIDEO_CHANNEL_CONFIGURE_FAIL);
                        }
                    } 
                }

                //sleep(1);

                //M23_MP_Set_Video_Encode( (MP_DEVICE_0 + i),video_index);
            }
            video_index++;
        }
    }


    /*Now We check if we are to setup the Ethernet Channel*/
    if(Config.NumConfiguredEthernet > 0)
    {
        if(Config.eth_chan.isEnabled)
        {
            M23_ClearHealthBit(Config.eth_chan.chanID,M23_ETHERNET_CONFIG_FAIL);

            /*Write the Channel ID*/
            M23_WriteControllerCSR(ETHERNET_CHANNEL_ID,(Config.eth_chan.chanID | BROADCAST_MASK));
            M23_WriteControllerCSR(ETHERNET_CHANNEL_ENABLE,ETHERNET_ENABLE);

            //sleep(5);
            Offset = 2048;

            Offset = Offset * -1;
            M23_WriteControllerCSR(ETHERNET_CHANNEL_REL_LSW,Offset & 0xFFFF);
            M23_WriteControllerCSR(ETHERNET_CHANNEL_REL_MSW,(Offset>>16));
            M23_WriteControllerCSR(ETHERNET_CHANNEL_REL_MSW,ETHERNET_REL_TIME_LOAD);


            RadarEnet = Config.eth_chan.Radar_IPLow & 0xFF;
            RadarEnet |= ((Config.eth_chan.Radar_IPMid2 & 0xFF) << 8);
            RadarEnet |= ((Config.eth_chan.Radar_IPMid1 & 0xFF) << 16);
            RadarEnet |= ((Config.eth_chan.Radar_IPHigh & 0xFF) << 24);

            OurEnet = Config.eth_chan.IPLow & 0xFF;
            OurEnet |= ((Config.eth_chan.IPMid2 & 0xFF) << 8);
            OurEnet |= ((Config.eth_chan.IPMid1 & 0xFF) << 16);
            OurEnet |= ((Config.eth_chan.IPHigh & 0xFF) << 24);
            M23_SetHealthBit(Config.eth_chan.chanID,M23_ETHERNET_NOT_READY);
            M23_SetHealthBit(Config.eth_chan.chanID,M23_ETHERNET_NO_DATA_PRESENT);

            if(debug)
                printf("Our Address 0x%x,Radar Address 0x%x, Port %d\r\n",OurEnet,RadarEnet,Config.eth_chan.port);

            M23_ConfigureRadarENET(OurEnet,RadarEnet,Config.eth_chan.port,Config.eth_chan.Mode);

        }else
            M23_WriteControllerCSR(ETHERNET_CONTROL_CSR,ETHERNET_ENABLE | ETHERNET_ENABLE_INT);
    }else
        M23_WriteControllerCSR(ETHERNET_CONTROL_CSR,ETHERNET_ENABLE | ETHERNET_ENABLE_INT);
  

    CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR);

    M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR,CSR | CONTROLLER_EVENT_ENABLE);

    if(Config.NumConfiguredUART > 0 )
    {
        M23_ClearHealthBit(Config.uart_chan.chanID,M23_UART_CONTROLLER_CONFIGURE_FAIL);
        M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR,CSR | CONTROLLER_EVENT_ENABLE);
    }

}

int ValidateChannelIDs()
{
    int    i;
    int    j;
    int    m1553_index = 0;
    int    video_index = 0;
    int    NumMPBoards = 0;
    int    NumVideo = 0;

    if(Config.timecode_chan.isEnabled)
    {
        if(Config.timecode_chan.chanID != 1)
        {
            return(M23_ERROR);
        }
    }
    else
    {
        return(M23_ERROR);
    }

    if(Config.NumConfiguredVoice != 2)
    {
        return(M23_ERROR);
    }

    for(i = 0;i < Config.NumConfiguredVoice;i++)
    {
        /*Setup the Voice Channel*/
        if(Config.voice_chan[i].isEnabled)
        {
            if(i == 0)
            {
                if(Config.voice_chan[i].chanID != 2)
                {
                    return(M23_ERROR);
                }
            }
            else
            {
                if(Config.voice_chan[i].chanID != 3)
                {
                    return(M23_ERROR);
                }
            }
        }
    }

    M23_NumberMPBoards(&NumMPBoards);
    NumVideo = M23_MP_GetNumVideo();

//printf("Configure M1553 Channels\r\n");
    for(i = 0; i < NumMPBoards;i++)
    {
        if(M23_MP_IS_M1553(i) )
        {
            if(Config.NumConfigured1553 != 6)
            {
                return(M23_ERROR);
            }

            /* Setup the M1553 channels that are enabled*/
            for(j = 0; j < 8;j++)
            {
                if(Config.m1553_chan[j + (m1553_index*8)].isEnabled)
                {
                    if( (Config.m1553_chan[j + (m1553_index*8)].chanID < 4) || 
                        (Config.m1553_chan[j + (m1553_index*8)].chanID > 9) )
                    {
                        return(M23_ERROR);
                    } 
                }

                m1553_index++;
            }

        }
        else //This is a Video Board
        {
            if(NumVideo > 0)
            {
                if(Config.NumConfiguredVideo != 6)
                {
                    return(M23_ERROR);
                }

                for(j=0;j<4;j++)
                {
                    if(Config.video_chan[j + (4*video_index)].isEnabled)
                    {
                        if( (Config.video_chan[j + (4*video_index)].chanID < 10) ||
                            (Config.video_chan[j + (4*video_index)].chanID > 15) )
                        {
                            return(M23_ERROR);
                        }
                    }
                } 
            }

            video_index++;
        }
    }

    return 0;
}

int ProcessSetup(int setup_number)
{
    int  status;
    int  ret;
    int  config;
    int  i;
    int  debug;

    M23_GetConfig(&config);
    M23_GetDebugValue(&debug);

    status = ProcessTmats(setup_number,&Config);

    if(status == 0)
    {
        if(config == 0)
        {
           ret = ValidateChannelIDs();
           if(ret == M23_ERROR)
           {
               for (i = 1; i < MAX_FEATURES; i++ )
               {
                   HealthArrayTypes[i] = NO_FEATURE;
               }

               return(ret);
           }
        }


        ret = M23_SUCCESS;
        M23_ResetIndex();
        M23_DefineM1553Event();
        M23_DefineM1553DataConversion();
        M23_SortM1553Event();
        M23_CreateM1553Mask();

        if(debug)
            M23_PrintEvent();

    }
    else
    {
        M23_GetDebugValue(&debug);

        if(debug)
            PutResponse(0,"Tmats Processing Failed\r\n");

        ret = M23_ERROR;
    }

    return(ret);

}

int SetupChannels( )
{

    //Setup the time Circuits at this point


    //First Update the Health Structure for the configured Channels
    M23_InitializeChannelHealth();

    SetupHardware();


    M23_InitializeControllerAudioI2c(); 


    return M23_SUCCESS;
}



int SetupGet( M23_CHANNEL const **hConfig )
{
    *hConfig = &Config;

    return M23_SUCCESS;
}


void PutOverlayInformation()
{
    int i;
    char tmp[80];

    memset(tmp,0x0,80);


    PutResponse(0,"                       Video Overlay Attributes                                  \r\n");
    PutResponse(0,"\r\nChannel   Enabled    Display Type     X      Y     Font size   Generate   Time\r\n");
    PutResponse(0,"-----------------------------------------------------------------------------------\r\n");
    for(i = 0; i < Config.NumConfiguredVideo;i++)
    {
        sprintf(tmp,"  %02d",i+1);
        PutResponse(0,tmp);
        memset(tmp,0x0,80);

        if(Config.video_chan[i].OverlayEnable == OVERLAY_OFF)
        {
            PutResponse(0,"        OFF");
        }
        else
        {
            PutResponse(0,"        ON ");
        }

        if(Config.video_chan[i].Overlay_Format == WHITE_TRANS)
        {
            PutResponse(0,"     WHITE ON TRANS");
        }
        else if(Config.video_chan[i].Overlay_Format == BLACK_TRANS)
        {
            PutResponse(0,"     BLACK ON TRANS");
        }
        else if(Config.video_chan[i].Overlay_Format == WHITE_BLACK)
        {
            PutResponse(0,"     WHITE ON BLACK");
        }
        else if(Config.video_chan[i].Overlay_Format == BLACK_WHITE)
        {
            PutResponse(0,"     BLACK ON WHITE");
        }
        else
        {
            PutResponse(0,"     WHITE ON TRANS");
        }

        sprintf(tmp,"   %04d",(Config.video_chan[i].Overlay_X/2));
        PutResponse(0,tmp);
        memset(tmp,0x0,80);
        sprintf(tmp,"   %04d",Config.video_chan[i].Overlay_Y *2);
        PutResponse(0,tmp);

        if(Config.video_chan[i].OverlaySize == LARGE_FONT)
        {
            PutResponse(0,"     LARGE");
        }
        else
        {
            PutResponse(0,"     SMALL");
        }

        if(Config.video_chan[i].Overlay_Generate == GENERATE_OFF)
        {
            PutResponse(0,"        OFF");
        }
        else if(Config.video_chan[i].Overlay_Generate == GENERATE_BW)
        {
            PutResponse(0,"        BW ");
        }
        else
        {
            PutResponse(0,"        CLR ");
        }

        if(Config.video_chan[i].Overlay_Time == TIME_ONLY)
        {
            PutResponse(0,"    TIME NO-DAY NO-MS");
        }
        else if(Config.video_chan[i].Overlay_Time == TIME_DAY_MS)
        {
            PutResponse(0,"    TIME DAY MS");
        }
        else if(Config.video_chan[i].Overlay_Time == TIME_DAY)
        {
            PutResponse(0,"    TIME DAY NO-MS");
        }
        else if(Config.video_chan[i].Overlay_Time == TIME_MS)
        {
            PutResponse(0,"    TIME MS NO-DAY");
        }
        else
        {
            PutResponse(0,"    TIME AND DAY");
        }

        PutResponse(0,"\r\n");
    }
}

int M23_SetOverlayParameters(int channel)
{
    int    size;
    int    large = 0;
    int    time = 0;
    int    maxx;
    int    maxy;
    int    minx;
    int    miny;
    int    value;
    char   buffer[8];
    int    Device;
    UINT32 CSR;


    Device = M23_GetVideoDevice(channel);
    CSR = M23_mp_read_csr(Device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY);

    memset(buffer,0x0,8);
    PutResponse(0,"\r\nFont Size(0-Large,1-Small)->");
    SerialGetLine(0, buffer,&size);

    if(size > 1)
    {
        if(buffer[0] == 'q')
        {
            return 0;
        }
        else
        {
            if(buffer[0] == '0')
            {
                CSR |= MP_VIDEO_LARGE_CHAR;
                Config.video_chan[channel - 1].OverlaySize = LARGE_FONT;
                large = 1;
            }
            else
            {
                CSR &= ~MP_VIDEO_LARGE_CHAR;
                Config.video_chan[channel - 1].OverlaySize = SMALL_FONT;
                large = 0;
            }
        }
    }
 
    memset(buffer,0x0,8);
    PutResponse(0,"\r\nTime (0-Time Only,1-Include Day)->");
    SerialGetLine(0, buffer,&size);

    if(size > 1)
    {
        if(buffer[0] == 'q')
        {
            return 0;
        }
        else
        {
            if(buffer[0] == '0')
            {
                time = 0;
                CSR |= MP_VIDEO_TIME_ONLY;
                Config.video_chan[channel - 1].Overlay_Time = TIME_ONLY;
            }
            else
            {
                time = 1;
                CSR &= ~MP_VIDEO_TIME_ONLY;
                Config.video_chan[channel - 1].Overlay_Time = TIME_DAY;
            }
        }
    }

    memset(buffer,0x0,8);
    PutResponse(0,"\r\nDisplay (0-WOT,1-BOT,2-WOB,3-BOW)->");
    SerialGetLine(0, buffer,&size);

    if(size > 1)
    {
        if(buffer[0] == 'q')
        {
            return 0 ;
        }
        else
        {
            if(buffer[0] == '0')
            {
                CSR |= MP_VIDEO_CLEAR_BACK;
                CSR &= ~MP_VIDEO_BLACK_ON_WHITE;
                Config.video_chan[channel - 1].Overlay_Format = WHITE_TRANS;
            }
            else if(buffer[0] == '1')
            {
                CSR |= MP_VIDEO_BLACK_ON_WHITE;
                CSR |= MP_VIDEO_CLEAR_BACK;
                Config.video_chan[channel - 1].Overlay_Format = BLACK_TRANS;
            }
            else if(buffer[0] == '2')
            {
                #if 0
                if(large == 0)  //Small Fonts
                {
                    CSR |= MP_VIDEO_BLACK_ON_WHITE;
                    CSR |= MP_VIDEO_BLACK_ON_WHITE;
                    Config.video_chan[channel - 1].Overlay_Format = WHITE_TRANS;
                }
                else
                {
                    CSR &= ~MP_VIDEO_BLACK_ON_WHITE;
                    CSR &= ~MP_VIDEO_CLEAR_BACK;
                    Config.video_chan[channel - 1].Overlay_Format = WHITE_BLACK;
                }
                #endif

                CSR &= ~MP_VIDEO_BLACK_ON_WHITE;
                CSR &= ~MP_VIDEO_CLEAR_BACK;
                Config.video_chan[channel - 1].Overlay_Format = WHITE_BLACK;
            }
            else if(buffer[0] == '3')
            {
                #if 0
                if(large == 0) //Small Font
                {
                    CSR &= ~MP_VIDEO_BLACK_ON_WHITE;
                    CSR |= MP_VIDEO_CLEAR_BACK;
                    Config.video_chan[channel - 1].Overlay_Format = BLACK_TRANS;
                }
                else
                {
                    CSR &= ~MP_VIDEO_BLACK_ON_WHITE;
                    CSR &= ~MP_VIDEO_CLEAR_BACK;
                    Config.video_chan[channel - 1].Overlay_Format = BLACK_WHITE;
                }
                #endif
                CSR |= MP_VIDEO_BLACK_ON_WHITE;
                CSR &= ~MP_VIDEO_CLEAR_BACK;
                Config.video_chan[channel - 1].Overlay_Format = BLACK_WHITE;
            }
        }
    }

    M23_mp_write_csr(Device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY);

    CSR = M23_mp_read_csr(Device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY_X);


    if(large == 1)  //Large Fonts
    {
        if(time == 0) // Time Only
        {
            PutResponse(0,"\r\nX-Axis(min-32,max-624)->");
            maxx = 624;
        }
        else  //Day included
        {
            PutResponse(0,"\r\nX-Axis(min-32,max-576)->");
            maxx = 576;
        }
    }
    else
    {
        if(time == 0) // Time Only
        {
            PutResponse(0,"\r\nX-Axis(min-32,max-628)->");
            maxx = 628;
        }
        else  //Day included
        {
            PutResponse(0,"\r\nX-Axis(min-32,max-608)->");
            maxx = 608;
        }
    }

    minx = 32;
    memset(buffer,0x0,8);
    SerialGetLine(0, buffer,&size);

    if(size > 1)
    {
        if(buffer[0] == 'q')
        {
            return 0;
        }
        else
        {
            value = atoi(buffer);

            if( (value < minx) || (value > maxx))
            {
                PutResponse(0,"Invalid Parameter: Value Not Changed\r\n");
            }
            else
            {
                CSR = value & 0x07FF;
                 Config.video_chan[channel - 1].Overlay_X = value * 2;
            }
        }
    }
    M23_mp_write_csr(Device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY_X);



    CSR = M23_mp_read_csr(Device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY_Y);

    if(large == 1)  //Large Fonts
    {
        PutResponse(0,"\r\nY-Axis(min-16,max-218)->");
        maxy = 218;
    }
    else
    {
        PutResponse(0,"\r\nY-Axis(min-16,max-223)->");
        maxy = 223;
    }

    miny = 16;
    memset(buffer,0x0,8);
    SerialGetLine(0, buffer,&size);

    if(size > 1)
    {
        if(buffer[0] == 'q')
        {
            return 0;
        }
        else
        {
            value = atoi(buffer);

            if( (value < miny) || (value > maxy))
            {
                PutResponse(0,"Invalid Parameter: Value Not Changed\r\n");
            }
            else
            {
                CSR = value & 0x3ff;
                Config.video_chan[channel - 1].Overlay_Y = value;
            }
        }
    }

    M23_mp_write_csr(Device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY_Y);


    CSR = M23_mp_read_csr(Device,BAR1,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY_GO);

    memset(buffer,0x0,8);
    PutResponse(0,"\r\nGenerate(0-OFF,1-ON)->");
    SerialGetLine(0, buffer,&size);

    if(size > 1)
    {
        if(buffer[0] == 'q')
        {
            return 0;
        }
        else
        {
            if(buffer[0] == '0')
            {
                CSR &= ~MP_VIDEO_TEST_PATTERN;
                Config.video_chan[channel-1].Overlay_Generate = GENERATE_OFF;
            }
            else
            {
                CSR |= MP_VIDEO_TEST_PATTERN;
                Config.video_chan[channel-1].Overlay_Generate = GENERATE_BW;
            }
        }
    }

    memset(buffer,0x0,8);
    PutResponse(0,"Overlay (0-OFF,1-ON)->");
    SerialGetLine(0, buffer,&size);

    if(size > 1)
    {
        if(buffer[0] == 'q')
        {
            return 0;
        }
        else
        {
            if(buffer[0] == '0')
            {
                CSR &= ~MP_VIDEO_OVERLAY_GO;
                Config.video_chan[channel -1].OverlayEnable = OVERLAY_OFF;
            }
            else
            {
                CSR |= MP_VIDEO_OVERLAY_GO;
                Config.video_chan[channel -1].OverlayEnable = OVERLAY_ON;
            }
        }
    }

    M23_mp_write_csr(Device,BAR1,CSR,(MP_CHANNEL_OFFSET * channel) + MP_CHANNEL_OVERLAY_GO);

    return 0;
}

void M23_SetDefaultOverlayParameters(int channel)
{
        Config.video_chan[channel].Overlay_Generate = GENERATE_OFF;
        Config.video_chan[channel].Overlay_X        = 64;
        Config.video_chan[channel].Overlay_Y        = 16;
        Config.video_chan[channel].OverlaySize      = LARGE_FONT;
        Config.video_chan[channel].OverlayEnable    = OVERLAY_OFF; //OFF
        Config.video_chan[channel].Overlay_Format   = WHITE_TRANS; //White on Transparent
        Config.video_chan[channel].Overlay_Time     = TIME_ONLY; //No Day
}
