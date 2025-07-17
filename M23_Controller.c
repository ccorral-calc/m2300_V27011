/****************************************************************************************
 *
 *    Copyright (C) 2004 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and 
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
 *    for any purpose except as specifically authorized in writing.
 *
 *    File: M23_Controller.c
 *
 *    Version: 1.0
 *    Author: pcarrion
 *
 *    MONSSTR 2300
 *
 *    Defines the parameters that will be used in communicating with the M2300 Controller Board
 *       
 *
 *    Revisions:  
 ******************************************************************************************/
#include <pthread.h>

#include "M23_Controller.h"
#include "M23_Utilities.h"
#include "M23_PCI.h"
#include "M2X_system_settings.h"
#include "M2X_FireWire_io.h"
#include "M23_features.h"
#include "M23_Status.h"
#include "M23_MP_Handler.h"
#include "M23_I2c.h"
#include "M23_Stanag.h"
#include "M23_EventProcessor.h"
#include "M2X_time.h"


char SystemVersion[7];
char M2300Version[13];
char ControllerVersion[18];
char VideoVersion[13];
char M1553Version[13];
char PCMVersion[11];

char RMM_Version[80];
int  RMM_VersionSize;


//extern int TimeSource;
/*The Following are configured using TMATS*/
int Discrete_Disable;
int Internal_Event_Disable;
int IEEE1394_Bus_Speed;
int IEEE1394_Lockheed_Bus_Speed;
int Video_Output_Format;
int TimeSet_Command;
int AutoRecord;

int RootReceived;

/*The Following will be used to determine if we have process TMATS from the Cartridge*/
int  TmatsLoaded;
int  RecordTmats;
int  LoadedTmatsFromCartridge;
char TmatsName[40];

int  EventChannelInUse;



CH10_STATUS RecorderStatus;
UINT32      Play_Percentage;

int CheckBITStatus(int *percent_complete);
int RunBIT(void);
void SaveSetup(int setup);


// static definitions -- ADD STATEMENT TO INIT FUNCTION WHEN ADDING NEW VARIABLES !!!!
//
static int TimeFromCartridgeAlreadySet;


//static CH10_STATUS RecorderStatus;

static INT32 SelectedSetup;

static int   M23_DismountCommand;

static int  StartEther;

static UINT32 BITPercentComplete;

int DismountCommandIssued()
{
    if(M23_DismountCommand == TRUE)
        return(1);
    else
        return(0);
}

void ClearDismountCommand()
{
    M23_DismountCommand = FALSE;
}

/***************************************************************************************************************************************************
*  Name :    M23_Setup1394BLogic()
*
*  Purpose : This function will setup up the Dual Port,Record,Playback and Host ORB Addresses.
*            Also setup the Interrupts.
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ***************************************************************************************************************************************************/
void M23_Setup1394BLogic()
{
    UINT32 CSR;


    /*Setup the Dual Port Address*/
    M23_WriteControllerCSR(IEEE1394B_DPR_ADDRESS,DUAL_PORT_ADDRESS);

    /*Setup the Record and Playback FIFO Address*/
    M23_WriteControllerCSR(IEEE1394B_RECORD_FIFO,RECORD_FIFO_ADDRESS);
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_FIFO,PLAYBACK_FIFO_ADDRESS);

    /*Setup the Host ORB Address*/
    M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,HOST_ORB_ADDRESS);


    /*Setup the Interrupts*/
    M23_WriteControllerCSR(IEEE1394B_INTERRUPTS,IEEE1394B_INTERRUPT_MASK);

    CSR = M23_ReadControllerCSR(IEEE1394B_CONTROL);
    M23_WriteControllerCSR(IEEE1394B_CONTROL,CSR | IEEE1394B_INTERRUPT_ENABLE);

    /*Setup the Record Transfer size*/
    M23_WriteControllerCSR(IEEE1394B_RECORD_NUM_BLOCKS,IEEE1394B_MAX_BLOCKS);

    /*Setup the Playback Transfer size*/
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_STATUS,(IEEE1394B_MAX_PLAY_BLOCKS << 16));


}

/***************************************************************************************************************************************************
*  Name :    M23_Set1394BCommandGo()
*
*  Purpose : This function will set 1394B Host Control GO 
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ***************************************************************************************************************************************************/
void M23_Set1394BCommandGo()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(IEEE1394B_CONTROL);
    M23_WriteControllerCSR(IEEE1394B_CONTROL,CSR | IEEE1394B_COMMAND_GO);

}

/***************************************************************************************************************************************************
*  Name :    M23_Clear1394BCommandGo()
*
*  Purpose : This function will clear 1394B Host Control GO 
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ***************************************************************************************************************************************************/
void M23_Clear1394BCommandGo()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(IEEE1394B_CONTROL);
    M23_WriteControllerCSR(IEEE1394B_CONTROL,CSR & ~IEEE1394B_COMMAND_GO);

}

/***************************************************************************************************************************************************
*  Name :    M23_Clear()
*
*  Purpose : This function will setup the RT on the Controller
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ***************************************************************************************************************************************************/
void M23_ClearRT()
{
    UINT32 CSR;
    UINT32 Address;
    int    config;

    M23_GetConfig(&config);

    if(config != 0)
    {
        if(config != LOCKHEED_CONFIG)
        {
            /*Clear the RT*/
            CSR = 0;
            Address = 0x0;
            M23_M1553_WriteRT(Address,CSR);

            M23_WriteControllerCSR(CONTROLLER_RT_HOST_ADDRESS_CSR,0x1);
        }
    }

#if 0
    Address |= CONTROLLER_RT_WRITE_ENABLE;

    M23_WriteControllerCSR(CONTROLLER_RT_WRITE_DATA,CSR );
    M23_WriteControllerCSR(CONTROLLER_RT_HOST_ADDRESS_CSR,Address );
#endif


}

/***************************************************************************************************************************************************
*  Name :    M23_SetpRT(int RT)
*
*  Purpose : This function will setup the RT on the Controller
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ***************************************************************************************************************************************************/
void M23_SetupRT(int RT)
{
    UINT32 CSR;
    UINT32 Address;
    int    config;

    M23_GetConfig(&config);

    if(config != 0)
    {
        if(config != LOCKHEED_CONFIG)
        {
            /*Clear the RT*/
            M23_ClearRT();
            usleep(10);

            /*Write The RT Address*/
            CSR = (RT << 11);
            Address = 0x4C4;
            M23_M1553_WriteRT(Address,CSR);

            usleep(10);

            /*Write 0x003F to Address 0x130*/
            CSR = 0x003F;
            Address = 0x4C0;
            M23_M1553_WriteRT(Address,CSR);

            /*Write 0x0C04 to Address 0x0000*/
            CSR = 0x0C04;
            Address = 0x0000;
            M23_M1553_WriteRT(Address,CSR);

            M23_WriteControllerCSR(CONTROLLER_RT_HOST_ADDRESS_CSR,0x0);
        }
    }

}

/***************************************************************************************************************************************************
*  Name :    M23_ResetCascadeFIFO()
*
*  Purpose : This function will Reset the Cascade FIFO
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ***************************************************************************************************************************************************/
void M23_ResetCascadeFIFO()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_CASCADE_CSR);
    M23_WriteControllerCSR(CONTROLLER_CASCADE_CSR,CSR | CONTROLLER_USER_RESET );

}
/***************************************************************************************************************************************************
*  Name :    M23_StartControllerHardware()
*
*  Purpose : This function will wait for the Global Ready flag to be set then it will set the Global Go bit
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
***************************************************************************************************************************************************/
int M23_StartControllerHardware(int RecordEthernet)
{
    int                count = 0;
    int                status = 0;
    int                NumMP;
    int                i;
    int                RT;
    int                debug;
    int                config;
    UINT32             Ready;
    M23_CHANNEL const  *p_config;

    SetupGet( &p_config );
 
    M23_GetConfig(&config);

    M23_GetDebugValue(&debug);

    while(1)
    {
        Ready = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);

        if(debug)
            printf("Checking Ready 0x%x\r\n",Ready);

        //Ready = CONTROLLER_GLOBAL_READY;

	if(Ready & CONTROLLER_GLOBAL_READY)
	{

//printf("Reset Cascade FIFO\r\n");
            /*Reset the Cascade FIFO*/
            M23_ResetCascadeFIFO();


            /*Set Global GO And Arbiter*/
            M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,CONTROLLER_GLOBAL_GO);
                           
            /*Set Master Time Go*/
            M23_WriteControllerCSR(CONTROLLER_MASTER_TIME_ENABLE_CSR,CONTROLLER_MASTER_TIME_GO); 

            /*Set Host Go*/
	    M23_WriteControllerCSR(CONTROLLER_HOST_CONTROL_CSR,CONTROLLER_HOST_CHANNEL_GO);


            M23_NumberMPBoards(&NumMP);
            for(i = 0;i < NumMP;i++)
            {
                if(M23_MP_IS_M1553(i))
                {
                    /*Turn On the Relative Time count on the MP board*/
                    M23_MP_TurnOnRelativeTime(MP_DEVICE_0 + i);
                }
                else if(M23_MP_IS_DM(i))
                {
                    /*Turn On the Relative Time count on the MP board*/
                    M23_MP_TurnOnRelativeTime(MP_DEVICE_0 + i);
                    M23_DM_TurnOnRelativeTime(MP_DEVICE_0 + i);
                }
                else if(M23_MP_IS_PCM(i))
                {

                    /*Start the Relative Time Counter on the PCM MP Board*/
                    M23_MP_TurnOnRelativeTimePCM(MP_DEVICE_0 + i);

                    /* Start the Arbiter*/
                    M23_MP_PCM_StartArbiter(MP_DEVICE_0 + i);
                }
                else /*This is a Video Board*/
                {
                    /*Start the Relative Time Counter on the PCM MP Board*/
                    M23_MP_TurnOnRelativeTimeVideo(MP_DEVICE_0 + i);

                    /* Start the Arbiter*/
                    M23_MP_Video_StartArbiter(MP_DEVICE_0 + i);
                }
 
            }

            if(RecordEthernet)
                M23_WriteControllerCSR(ETHERNET_CHANNEL_REL_TIME_GO ,ETHERNET_REL_TIME_GO);
   
            /* Turn On the Relative Time on the Controller */
            M23_WriteControllerCSR(CONTROLLER_TIME_SYNCH_CONTROL_CSR ,CONTROLLER_TIME_GEN_GO | CONTROLLER_TIME_GEN_ENABLE);


	    /*Enable The Cascade Bus*/
	    M23_WriteControllerCSR(CONTROLLER_CASCADE_CONTROL_CSR,CONTROLLER_CASCADE_ENABLE);

	    break;
	}
        else
	{
	    count++;
	    if(count == 5)
	    {
                printf("Controller: Never Ready\r\n");
		status = -1;
	        break;
	    }
	    else
	    {
	        sleep(1);
	    }
	}
    }


    /*Setup the Bus Transfer Manager Buffer Size*/
    //Ready = (64*512); //Set for 32K 
    Ready = (2048*512); //Set for 1M 
    M23_WriteControllerCSR(CONTROLLER_BTM_SIZE_CSR,Ready);

    if(config != 0)
    {
        if(config != LOCKHEED_CONFIG)
        {
            /*Enable The RT Function on the Controller*/
            M23_GetRemoteTerminalValue(&RT);
            if(RT < 0 ) //RT is OFF
            {
                M23_ClearRT();
            }
            else
            {
                M23_SetupRT(RT);
            }
        }
    }

    return(status);

}

/***************************************************************************************************************************************************
 *  Name :    M23_SetGo()
 *
 *  Purpose : This function will Set the Go bit and Turn on the Arbiter
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/

void M23_SetGo()
{
    UINT32 CSR;

     M23_AllowVideoI2c();

    /*Set Global GO And Arbiter*/
    CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);
    M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,CSR | CONTROLLER_GLOBAL_GO);
}


/***************************************************************************************************************************************************
 *  Name :    M23_StopRecorder()
 *
 *  Purpose : This function will Stop alll the Data channels in the recorder so that a startup condition is set
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/

void M23_StopRecorder()
{
    int                i;
    int                j;
    UINT32             CSR;
    int                NumMPBoards = 0;
    M23_CHANNEL const  *p_config;

    SetupGet( &p_config );

    M23_NumberMPBoards(&NumMPBoards);

    for(i = 0; i < NumMPBoards;i++)
    {
        if(M23_MP_IS_M1553(i))
        {
            if(M23_MP_M1553_4_Channel(i))
            {
                /* Stop the M1553 Channels that are enabled*/
                for(j = 0; j < 4;j++)
                {
                    if(p_config->m1553_chan[j + (i*4)].isEnabled)
                    {
                        M23_MP_StopChannels((MP_DEVICE_0 + i),j+1);
                    }
                }
            }
            else
            {
                /* Setup the M1553 channels that are enabled*/
                for(j = 0; j < 8;j++)
                {
                    if(p_config->m1553_chan[j].isEnabled)
                    {
                        M23_MP_StopChannels((MP_DEVICE_0 + i),j+1);
                    }
                }
            }
        }
        else if(M23_MP_IS_PCM(i))
        {
            /*Stop the PCM Channels*/ 
            for(j = 0; j < 4;j++)
            {
                if(p_config->pcm_chan[j].isEnabled)
                {
                    M23_MP_StopChannels((MP_DEVICE_0 + i),j+1);
                }
            }
        }
    }



    /*Disable the Timecode Channel*/
    M23_WriteControllerCSR(CONTROLLER_TIMECODE_CSR,0x0);

    /*Clear Global GO And Arbiter*/
    CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);
    M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,CSR & ~CONTROLLER_GLOBAL_GO);

    M23_ClearFilterGo();


}
/***************************************************************************************************************************************************
 *  Name :    M23_SetEthernetBroadcast()
 *
 *  Purpose : This function will set the Ethernet Broadcast bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
*******************************************************************************************/
void M23_SetEthernetBroadcast()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);

    M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,CSR | ETHERNET_BROADCAST);

}

/***************************************************************************************************************************************************
 *  Name :    M23_ClearEthernetBroadcast()
 *
 *  Purpose : This function will clear the Ethernet Broadcast bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *
 *
 *
*******************************************************************************************/
void M23_ClearEthernetBroadcast()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);

    M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,CSR & ~ETHERNET_BROADCAST);

}

/***************************************************************************************************************************************************
 *  Name :    M23_StartDataToDisk()
 *
 *  Purpose : This function will set the Ethernet Broadcast bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
*******************************************************************************************/
void M23_StartDataToDisk()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);

    M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,CSR | RECORD_TO_DISK);

}

/***************************************************************************************************************************************************
 *  Name :    M23_StopDataToDisk()
 *
 *  Purpose : This function will clear the Ethernet Broadcast bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *
 *
 *
*******************************************************************************************/
void M23_StopDataToDisk()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);
    M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,CSR & ~RECORD_TO_DISK);

}





/***************************************************************************************************************************************************
 *  Name :    M23_StartControllerRecord()
 *
 *  Purpose : This function will set the Global Record Bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *
 *  Purpose : This function will set the Global Record Bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
*******************************************************************************************/
void M23_StartControllerRecord()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);

    /*Set The Global Record Bit*/
    M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,CSR | CONTROLLER_GLOBAL_RECORD);

    /*Set the Event Record bit*/
    CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR);
    M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR,CSR | CONTROLLER_EVENT_RECORD);

}

/*****************************************************************************************************
**********************************************
 *  Name :    M23_StopControllerRecord()
 *
 *  Purpose : This function will set the Global Record Bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 *****************************************************************************************************
**********************************************/
void M23_StopControllerRecord()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);

    /*Clear The Global Record Bit*/
    M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,CSR & ~CONTROLLER_GLOBAL_RECORD );

    /*Clear the Event Record bit*/ 
    CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR);
    M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR,CSR & ~CONTROLLER_EVENT_RECORD);

}

/***************************************************************************************************************************************************
 *  Name :    M23_SetHostChannelID()
 *
 *  Purpose : This function will set the Global Record Bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/

void M23_SetHostChannelID()
{
    UINT32 CSR;

    /*Set the Host Channel Id, Both TMATS and Event are channel 0*/
    CSR = M23_ReadControllerCSR(CONTROLLER_HOST_CHANNEL_ID_CSR);
    CSR |= (HOST_CHANNEL_ID | BROADCAST_MASK);
    M23_WriteControllerCSR(CONTROLLER_HOST_CHANNEL_ID_CSR,CSR);

}

/***************************************************************************************************************************************************
 *  Name :    M23_SetHostChannelRecord()
 *
 *  Purpose : This function will set the Global Record Bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/

void M23_SetHostChannelRecord()
{
    UINT32 CSR;

#if 0
    /*Set the Host Channel Id, Both TMATS and Event are channel 0*/
    CSR = M23_ReadControllerCSR(CONTROLLER_HOST_CHANNEL_ID_CSR);
    CSR |= HOST_CHANNEL_ID;
    M23_WriteControllerCSR(CONTROLLER_HOST_CHANNEL_ID_CSR,CSR);
#endif

    CSR = M23_ReadControllerCSR( CONTROLLER_HOST_CONTROL_CSR);

    /*Set The Host Channel Record Bit*/
    M23_WriteControllerCSR(CONTROLLER_HOST_CONTROL_CSR,CSR | CONTROLLER_HOST_RECORD);

}

/***************************************************************************************************************************************************
 *  Name :    M23_StopHostChannelRecord()
 *
 *  Purpose : This function will set the Global Record Bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/

void M23_StopHostChannelRecord()
{
    UINT32 CSR;


    CSR = M23_ReadControllerCSR( CONTROLLER_HOST_CONTROL_CSR);

    /*Set The Host Channel Record Bit*/
    M23_WriteControllerCSR( CONTROLLER_HOST_CONTROL_CSR,CSR & ~CONTROLLER_HOST_RECORD);

}

/***************************************************************************************************************************************************
 *  Name :    M23_SetHostChannelPacketReady()
 *
 *  Purpose : This function will set the Global Record Bit
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/

void M23_SetHostChannelPacketReady()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_HOST_CHANNEL_ID_CSR);

    /*Set The Host Channel Record Bit*/
    M23_WriteControllerCSR(CONTROLLER_HOST_CHANNEL_ID_CSR,CSR | CONTROLLER_HOST_CHANNEL_PKT_READY);

}

/***************************************************************************************************************************************************
 *  Name :    M23_WriteFactor()
 *
 *  Purpose : This function will write the Time Code Offset when using time from Cartridge
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/
void M23_WriteFactor()
{
    int                factor;
    M23_CHANNEL  const *config;

    SetupGet(&config);

    if(config->timecode_chan.Format == 'A')
    {
        M23_GetFactor(&factor,1);
    }
    else if(config->timecode_chan.Format == 'B')
    {
        M23_GetFactor(&factor,0);
    }
    else if(config->timecode_chan.Format == 'G')
    {
        M23_GetFactor(&factor,2);
    }
    else //Use Default
    {
        M23_GetFactor(&factor,0);
    }

    M23_WriteControllerCSR(CONTROLLER_TIME_STABILITY,factor);
}

/***************************************************************************************************************************************************
 *  Name :    M23_WriteTimeCodeOffset()
 *
 *  Purpose : This function will write the Time Code Offset when using time from Cartridge
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/
void M23_WriteTimeCodeOffset()
{
    int                offset;
    int                mult;
    M23_CHANNEL  const *config;

    SetupGet(&config);

    offset = 50000000;		//LWB 2007-06-18
   
    if(config->timecode_chan.Format == 'A')
    {
        offset = offset/10;
    }
    else if(config->timecode_chan.Format == 'G')
    {
        offset = offset / 100;
    }

    /*Write The Time Code Offset*/
    M23_WriteControllerCSR(CONTROLLER_TIMECODE_OFFSET_CSR,offset);


    /*set the Timecode Mask*/
    M23_GetTimeMask(&offset);
    if(offset == 0)
    {
        offset = 0x7FFFC00;
    }

    M23_WriteControllerCSR(CONTROLLER_TIME_MASK_CSR,offset);
}

/***************************************************************************************************************************************************
 *  Name :    M23_ControllerInitializeTime()
 *
 *  Purpose : This function will Initialize the system Time
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/

void M23_ControllerInitializeTime( int setup )
{
    int     status;
    int     year;
    int     ssric;
    int     debug;
    GSBTime CartridgeTime;
    char    time[120];
    char    milli[4];

    UINT32  CSR;

    M23_GetDebugValue(&debug);

    if( (TimeSource == TIMESOURCE_CARTRIDGE) || (TimeSource == TIMESOURCE_EXTERNAL) )
    {

        CartridgeTime.Days    = 1;
        CartridgeTime.Hours   = 0;
        CartridgeTime.Minutes = 0;
        CartridgeTime.Seconds = 0;

        M23_GetYear(&year);

        memset(milli,0x0,4);
        memset(time,0x0,120);


        status = DiskIsConnected();

        if(status)
        {
            if(TimeSource == TIMESOURCE_EXTERNAL)
            {
                if(TimeSet_Command == 0)
                {
                    return;
                }
            }

            status = M2x_GetCartridgeTime(time);
            if(status == 0)
            {
                M23_SetSystemTime(time); 

                CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
                CSR &= ~TIMECODE_M1553_JAM;
                CSR |= TIMECODE_RMM_JAM;
                M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);

                M23_SetTimeLED();
            }
        }
        else
        {
            SetTime(CartridgeTime);
            M23_ClearTimeLED();
        }

        M23_GetSsric(&ssric);
        if(ssric == 1)
        {
            SSRIC_SetTimeStatus();
        }
    }
    else if(TimeSource == TIMESOURCE_M1553)
    {

        status = M23_M1553_IsTimeJammed();
        if(status == 0)
        {
            CartridgeTime.Days    = 1;
            CartridgeTime.Hours   = 0;
            CartridgeTime.Minutes = 0;
            CartridgeTime.Seconds = 0;

            M23_GetYear(&year);

            memset(time,0x0,120);


            status = DiskIsConnected();

            if(status)
            {
                if(TimeSet_Command == 0)
                {
                    return;
                }

                status = M2x_GetCartridgeTime(time);
                if(status == 0)
                {
                    M23_SetSystemTime(time); 

                    CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
                    CSR &= ~TIMECODE_M1553_JAM;
                    CSR |= TIMECODE_RMM_JAM;
                    M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);

                    M23_SetTimeLED();
                }
            }
            else
            {
                SetTime(CartridgeTime);
                M23_ClearTimeLED();
            }
        }

    }
    else //if(TimeSource == TIMESOURCE_INTERNAL)
    {
        CartridgeTime.Days    = 1;
        CartridgeTime.Hours   = 0;
        CartridgeTime.Minutes = 0;
        CartridgeTime.Seconds = 0;
        SetTime(CartridgeTime);

        M23_GetYear(&year);

    }

    //M23_WriteTimeCodeOffset();

    //SelectedSetup = setup;
    SaveSetup(setup);
    RecorderStatus = STATUS_IDLE;

}

/***************************************************************************************************************************************************
 *  Name :    M23_ControllerInitialize()
 *
 *  Purpose : This function will Initialize all Controller functions
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/

int M23_ControllerInitialize( void )
{
    int err = NO_ERROR;
    int status;
    int init_status = -1;
    int config;
    int ssric;
    int script;
    int fault = 0;
    int numfiles = 0;
    int numboards;
    int total;
    int i;
    int setup;
    int debug;
    int setup_fault = 0;
    int IsPresent;
    int GoSet = 0;

    UINT32 total_blocks;
    UINT32 blocks_used;
    UINT32 version;

    M23_CHANNEL  const *chans;

    char File[72];
    FILE *fp;

    TimeSource = TIMESOURCE_INTERNAL;

    M23_DismountCommand = FALSE;

    RecorderStatus = STATUS_IDLE;
    SelectedSetup = 0;      // this is set from non-volatile settings file below
    RootReceived = 0;
    StartEther = 0;

    TmatsLoaded = 0;
    RecordTmats = 0;
    LoadedTmatsFromCartridge = 0;

    EventChannelInUse = 0;

    memset(TmatsName,0x0,40);

    /*Get the System Version from File*/
    memset(SystemVersion,0x0,7);
    memset(M2300Version,0x0,13);
    memset(ControllerVersion,0x0,18);
    memset(VideoVersion,0x0,13);
    memset(M1553Version,0x0,13);
    memset(PCMVersion,0x0,11);
    memset(File,0x0,72);

    fp = fopen( "SystemVersions", "rb" );

    if ( !fp )
    {
        strncpy(SystemVersion,"00.000",6);
        strncpy(M2300Version,"M2300 000000",12);
        strncpy(ControllerVersion,"Controller 000000",17);
        strncpy(VideoVersion,"Video 000000",12);
        strncpy(M1553Version,"M1553 000000",12);
        strncpy(PCMVersion,"PCM 000000",10);
         
    }
    else
    {
        fread(File,1,72,fp);
        if(fp)
            fclose(fp);

        strncpy(SystemVersion,&File[1],6);
        if(SystemVersion[5] =='M') //This read is incorrect , we need to do it the old way*/
        {
            memset(SystemVersion,0x0,7);
            fseek( fp, 0, SEEK_SET);
            strncpy(SystemVersion,&File[1],5);
            strncpy(M2300Version,"&File[6]",12);
            strncpy(ControllerVersion,&File[18],17);
            strncpy(VideoVersion,&File[35],12);
            strncpy(M1553Version,&File[47],12);
            strncpy(PCMVersion,&File[59],10);
        }
        else
        {
            strncpy(M2300Version,"&File[7]",12);
            strncpy(ControllerVersion,&File[19],17);
            strncpy(VideoVersion,&File[36],12);
            strncpy(M1553Version,&File[48],12);
            strncpy(PCMVersion,&File[60],10);
        }
    }

    
    /*Initialize the Data Buffer*/
    M23_InitializeDataBuffer();
    M23_AllocateTmats();

    M23_AllocateBusEvents();

    /*Setup the Standard Health Features*/
    M23_InitializeHealth();

    /*Change where this is accomplished, Changing where settings are set and when 
      channels are configured based on the setup flags and the SSRIC setup */
    // restore settings
    //


    status = settings_Initialize();

    M23_GetDebugValue(&debug);
    M23_GetConfig(&config);

    if(config == LOCKHEED_CONFIG)
    {
        printf("\r\nStarting M2300 V2-F16\r\n");
        M23_StopLEDBlink();
    }
    else if(config == 0)
    {
        printf("\r\nStarting M2300-F15\r\n");
    }
    else
    {
        printf("\r\nStarting M2300\r\n");
    }

    if(debug)
        printf("Initialize COM PORTS\r\n");

    // Init the serial interface
    CmdAsciiInitialize();

    if((config == 1) || (config == 2) || (config == 4) || (config == B52_CONFIG) )
    {
        /*Initialize the SSRIC*/
        if(debug)
            printf("Initialize SSRIC\r\n");

        m23_ssric_initialize();
    }
    else
    {
        if( (config == 0) || (config == P3_CONFIG) )
        {
            system("/sbin/insmod /lib/modules/am_cs6651.ko");
            M23_InitializeCS6651();
        }
    }

    /*Initialize the Controller and Video Boards. This must be done before any accesses occur to the board*/

    if(debug)
        printf("Initialize Controller Board\r\n");

    M23_SetExternalDiscrete();

    if(debug)
        printf("Done Initialize Controller Board\r\n");

    M23_MP_Initialize();

    if(debug)
        printf("Initialize MP Boards\r\n");

    for(i=0;i<5;i++)
    {
        /*Open the MP device ,At this time we only have 1 device*/
        status = M23_mp_open(i);
        if(status < 0)
        {
            i = 5;
        }
    }


    TimeFromCartridgeAlreadySet = 0;

    if(debug)
        printf("Initialize I2C\r\n");

    /*Check the I2c HERE */
    M23_FixI2cInitialization();

    /*Set Some Default Values*/
    M23_SetI2cDefault();

    M23_InitializeControllerAudioI2c();

    /*Remove for now, may have to put this back in if Test Unit Ready does not work*/
    //sleep(10);
    sleep(2);


    M23_Setup1394BLogic();


    IsPresent = FindCartridge();
    if(IsPresent)
    {
        init_status = sinit();
    }
    else
    {
        M23_SetHealthBit(RECORDER_FEATURE, M23_DISK_NOT_CONNECTED);
        fault = 1;
        err = -1;
    }

    M23_GetSsric(&ssric);

    if(TmatsLoaded == 0)
    {

        if( (config == 1) || (config == 2) || (config == 4) || (config == B52_CONFIG) )
        {
            /*Now to initially Determine who is to setup the recorder*/
            if(ssric == 0)
            {
                M23_GetSetup(&setup);

                if(debug)
                    printf("Process Setup\r\n");

                status = ProcessSetup(setup);
                if ( status != M23_SUCCESS)
                {
                    SSRIC_SetFault();
                    setup_fault = 1;
                }
                else
                {
                    M23_ClearHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
                }

                i = 11;

                if(debug)
                    printf("Done Process Setup\r\n");
            }
            else
            {
                for(i = 0;i< 10;i++)
                {
                    //status = m23_ssric_thread();
                    status = M23_GetInitialSsricSetting();


                    if(status == 1)
                    {
                        SetupViewCurrent( &setup );
                        status = ProcessSetup(setup);
                        if ( status != M23_SUCCESS)
                        {
                            SSRIC_SetFault();
                            setup_fault = 1;
                        }
                        else
                        {
                            M23_ClearHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
                        }

                        i = 11;
                    }
                    sleep(1);
                }
            }

            if(i == 10)
            {
                M23_GetSetup(&setup);
                status = ProcessSetup(setup);
                if ( status != M23_SUCCESS)
                {
                    SSRIC_SetFault();
                    setup_fault = 1;
                }
                else
                {
                    M23_ClearHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
                }
            }
        }
        else
        {
            M23_GetSetup(&setup);

            if(debug)
                printf("Process Setup %d\r\n",setup);

            status = ProcessSetup(setup);
            if ( status != M23_SUCCESS)
            {
                SSRIC_SetFault();
                setup_fault = 1;
            }
            else
            {
                M23_ClearHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
            }

            if(debug)
                printf("Done Process Setup\r\n");
        }
    }

    if(TmatsLoaded == 0)
    {
        SetupCondor();

        M23_WriteTimeCodeOffset();


        if(status == M23_SUCCESS)
        {
            if(debug)
                printf("Setup Channels\r\n");

            SetupChannels();
            TmatsLoaded = 1;
            SaveSetup(setup);
        }
        else //Aug 20 2007, If no setup exists, Setup Up Video I2c
        {
            M23_SetupVideoI2c_Init();
        }
    }
    else
    {
        GoSet = 1;
    }


    version = M23_ReadControllerCSR(0x0);

    if(debug)
        printf("Starting Controller I2c Devices,verion 0x%x\r\n",version);

    if(version == 0x354012)
    {
        M23_InitializeControllerVideoI2c(0x88);
    }
    else
    {
        M23_InitializeControllerRGBI2c();
    }

    M23_WriteFactor();

    SetupGet(&chans);


    if( (chans->NumConfiguredEthernet == 0 ) || (chans->eth_chan.isEnabled == 0) ){
        if(StartEther ==  0)
        {
            if(debug)
                printf("Starting Ethernet\r\n");

            M23_StartEthernet(0x0014,0x5223,0x0000,0x1);

            StartEther = 1;
        }
    }

    if(TmatsLoaded)
    {

        if(GoSet == 0)
        {
            status = M23_StartControllerHardware(chans->NumConfiguredEthernet);
            /*We can now set Filter Sync*/

            M23_SetFilterGo();
        }
    }


 
    SetupIndex();

    /*Set the Host Channel ID*/
    M23_SetHostChannelID();


    if(debug)
        printf("Clear Time Sync\r\n");

    /*Clear the External Time Sync Variable*/
    M23_ClearTimeSync();



    /*First Find out if cartridge is connected and is Powered on*/

    if(init_status != 0 )
    {
        M23_SetHealthBit(RECORDER_FEATURE, M23_DISK_NOT_CONNECTED);
        fault = 1;
        err = -1;
    }
    else /*The cartridge is OK, now initialize the Events*/
    {
        M23_ClearHealthBit(RECORDER_FEATURE, M23_DISK_NOT_CONNECTED);
        /*Check to see if Cartridge is Full*/
        if(debug)
            printf("Get Blocks Used\r\n");

        sdisk(&total_blocks,&blocks_used);
        if( ((total_blocks - blocks_used) < SYSTEM_BLOCKS_RESERVED) || (blocks_used >= total_blocks) )
        {
            fault = 1;
            M23_SetHealthBit(RECORDER_FEATURE, M23_MEMORY_FULL);
            M23_ClearHealthBit(RECORDER_FEATURE, M23_MEMORY_BINGO_REACHED);
        }

        if(debug)
            printf("Get Cartridge Version\r\n");
        memset(RMM_Version,0x0,80);
        RMM_VersionSize = M23_RetrieveCartridgeVersion(RMM_Version);
    }

    /*Move Discrete Setup Here so that we can determine if a cartridge is present*/
    if(debug)
        printf("Setup Discretes\r\n");

    /*Inialize the Discretes*/
    M23_SetupDiscretes();


    M23_ProgramTimeValues();

    SetupViewCurrent(&setup);

printf("Setup is %d\r\n",setup);
    M23_ControllerInitializeTime(setup);

    
    status = m23_InitializeEvents();
    M23_InitializeNodes();


    status = DiskIsConnected();
    if(status)
    {
        STANAG_GetNumberOfFiles(&numfiles);
        if(numfiles == 0)
        {
            M23_SetEraseLED();
            M23_ClearDataLED();
        }
        else
        {
            M23_ClearEraseLED();
            M23_SetDataLED();
        }

        M23_SetVolume();
    }


    /*Setup the slots that contain boards*/
    for(i = 0;i<7;i++)
    {
        SlotContainsBoard[i] = 0;
    }

    SlotContainsBoard[0] = 1; /*Always have a Controller Board*/
    SlotContainsBoard[5] = 0; /*Don't poll Firewire Board*/

    M23_NumberMPBoards(&numboards);
    total = numboards;
    for(i = 0;i< total;i++)
    {
        SlotContainsBoard[i] = 1;
    }


    M23_StartEventThread();

    M23_StartHealthThread();

    M23_StartControlThread();

    
    if((config == 1) || (config == 2) || (config == 4) || (config == B52_CONFIG) )
    {
        if(ssric == 1)
        {
            M23_StartSsricThread();
        }
    }

    if(setup_fault == 1)
    {
        M23_SetHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
        //M23_SetFaultLED(0);
    }

    
    sleep(1);
    M23_StartM1553EventThread();


    if(config != LOCKHEED_CONFIG)
    {
        if(debug)
            printf("Starting Live Video,config = %d\r\n",config);
        M23_StartLiveVid();
    }

    /*Allow the Video Boards to Control the I2c*/
    M23_AllowVideoI2c();


    if( (chans->NumConfiguredEthernet == 0 ) || (chans->eth_chan.isEnabled == 0) )
        M23_SetEthernetBroadcast();

    M23_StartControllerRecord();

    if( (config == 2) || (AutoRecord == 1) || (RecordSwitchPosition() ) )
    {
        if(debug)
            printf("Starting Record\r\n");
        sleep(1);
        status = Record();

        if(status == NO_ERROR)
        {
            if(RecordSwitchPosition())
                M23_SetFrom();
        }
    }
    else
    {
        /*Write TMATS File at this point*/
        M23_TmatsWriteToHost();
    }

    printf("Init GPS...\r\n");
    M23_StartGPS();

    M23_StartDiscreteThread();


    if( (chans->NumConfiguredEthernet == 0 ) || (chans->eth_chan.isEnabled == 0) ){
        /*The last thing we will do is check if a script exists*/
        M23_GetScript(&script);
        if(script){
            while(1){
                if(GetGPSInit())
                    break;
                else
                    usleep(500000); 
            }
   
            M23_ProcessScript();
        }
    }

    return err;
}



int M23_RecorderState( int *status  )
{
     *status = RecorderStatus;

    return NO_ERROR;
}

int M23_SetRecorderState( int status  )
{
     if(RecorderStatus != STATUS_RECORD)
     {
         RecorderStatus = status;
     }
     else
     {
         if(status == STATUS_REC_N_PLAY)
         {
             RecorderStatus = status;
         }
     }


    return NO_ERROR;

}



// external functions that map directly to IRIG 106, Chapter 10 commands
//

int BuiltInTest( void )
{
    int config;

    M23_GetConfig(&config);

    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) && ( RecorderStatus != STATUS_FAIL ) )
    {
        //printf( "Error: Invalid state for BIT command\r\n" );
        return ERROR_INVALID_MODE;
    }

    /*Make Sure We can read from the Controllers CSR*/

    /*Make Sure we can read from each video board CSR*/


    if(config != LOCKHEED_CONFIG)
    {
        M23_StartLEDBlink();
    }

    BITPercentComplete = 0;

    RecorderStatus = STATUS_BIT;

    return NO_ERROR;

}

int FeaturesCriticalView( int *number_of_features, UINT32 **masks )
{
    UINT32 CriticalMasks;

    *number_of_features = NumberOfVoiceChannels + NumberOfPCMChannels + NumberOf1553Channels + NumberOfVideoChannels + 1;

    M23_GetCriticalMask(&CriticalMasks);
    *masks = (UINT32*)CriticalMasks;

#if 0
    if ( RecorderStatus == STATUS_ERROR )
        RecorderStatus = STATUS_IDLE;
#endif

    return NO_ERROR;
}


int CriticalView( int feature, UINT32 *mask, char **decoded_masks[32] )
{
    int i;

    if ( (feature < 0 ) || (HealthArrayTypes[feature] == NO_FEATURE) )
    {
   
#if 0
        if( RecorderStatus == STATUS_IDLE )
            RecorderStatus = STATUS_ERROR;
#endif

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
                        *decoded_masks = RecorderFeatureBits;
                        break;
    }

    *mask = 0;
    for ( i = 0; i < 32; i++ )
    {
        if ( (*decoded_masks)[i][0] != '\0' )
        {
            *mask |= ( 1 << i );
        }
    }

#if 0
    if( RecorderStatus == STATUS_ERROR )
        RecorderStatus = STATUS_IDLE;
#endif


    return NO_ERROR;

}



int CriticalSet( int feature, UINT32 mask )
{
    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) )
    {
        //printf( "Error: Invalid state for Critical command (query only permitted)\r\n" );
        return ERROR_INVALID_MODE;
    }

    if ( feature < 0) 
	{

        RecorderStatus = STATUS_ERROR;
        return ERROR_INVALID_PARAMETER;
    }

    //CriticalMasks[feature] = mask;
    M23_SetCriticalMask(feature, mask);

    RecorderStatus = STATUS_IDLE;
    return NO_ERROR;
}



int Declassify( void )
{
    int status;

    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) )
        return ERROR_INVALID_MODE;


    //printf( "WARNING: Declassify only erases File Table\n" );


    status = M23_Declassify();

    if(status == 0)
    {
        /*This is used so that we can then create a file for secure Erase*/
        M23_SetDeclassState();

        RecorderStatus = STATUS_DECLASSIFY;
        status = NO_ERROR;

        M23_ClearMediaError();
    }
    else
    { 
        M23_SetMediaError();
        status = ERROR_COMMAND_FAILED;
    }
 

    return(status);;

}



int Dismount( void )
{
    int status;
    int config;

    M23_GetConfig(&config);

    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) )
    {
        //printf( "Error: Invalid state for Dismount command\n" );
        return ERROR_INVALID_MODE;
    }

    status = M23_GetCartridgeStatus();
   
    if(status == 0) /*Cartridge is present and logged in*/
    {
        M23_DismountCommand = TRUE;
        M23_SetHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);

#if 0
        if ( RecorderStatus == STATUS_ERROR )
            RecorderStatus = STATUS_IDLE;
#endif


        //M23_SetFaultLED(0); 
        M23_ClearEraseLED(); 
        M23_ClearDataLED();

        M23_ClearHealthBit(RECORDER_FEATURE, M23_MEMORY_FULL);
        M23_ClearHealthBit(RECORDER_FEATURE, M23_MEMORY_BINGO_REACHED);

        if( (config == 1) || (config == 2) || (config == 4) || (config == LOCKHEED_CONFIG) || (config == P3_CONFIG) || (config == B1B_CONFIG) || (config == B52_CONFIG) )
        {
            M23_ClearBingoLED();
        }

    }
    else /*Cartridge Has Been Removed*/
    {
        M23_Clear1394BCommandGo();

        //M23_SetFaultLED(0); 
        M23_ClearEraseLED(); 
        M23_ClearDataLED();
        DiskDisconnect();

        M23_SetHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);

        M23_ClearHealthBit(RECORDER_FEATURE, M23_MEMORY_FULL);
        M23_ClearHealthBit(RECORDER_FEATURE, M23_MEMORY_BINGO_REACHED);


        if( (config == 1) || (config == 2) || (config == 4) || (config == LOCKHEED_CONFIG) || (config == P3_CONFIG) || (config == B1B_CONFIG) || (config == B52_CONFIG)  )
        {
            M23_ClearBingoLED();
        }
    }

    return NO_ERROR;
}

void Declass_Dismount( void )
{

    M23_DismountCommand = TRUE;
    M23_SetHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);
    DiskDisconnect();

}



int Download(int state)
{

    return(0);

}


int Erase( void )
{
    int status;

    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) )
    {
        //printf( "Error: Invalid state for Erase command\n" );
        return ERROR_INVALID_MODE;
    }

    RecorderStatus = STATUS_ERASE;
    serase(0);

    M23_ClearHealthBit(RECORDER_FEATURE,M23_MEMORY_FULL);
    M23_ClearHealthBit(RECORDER_FEATURE, M23_MEMORY_BINGO_REACHED);

    M23_ClearFaultLED(0); 
    M23_ClearBingoLED(); 

    m23_EraseEvents();

    status = NO_ERROR;


    return(status);
}



int EventRecordSet(int event )
{
    int status;

    GSBTime time;

    // determine how much space is used for the recordings already on disk
    //

    if(EventTable[event].EventEnabled)
    {
        TimeView(&time);
        status = m23_UpdateRecordEvent(  event,time);
    }
    else
    {
        status = ERROR_INVALID_COMMAND;
    }

    return(status);

}
int EventSet(int event )
{
    int     status = 1;

    
    if(EventTable[event].EventEnabled)
    {
        status =  m23_UpdateEvent( event);
    }

    return NO_ERROR;

}



int FindViewCurrent( UINT32 *play_point, UINT32 *record_point )
{
    int    status;
    UINT32 TotalBlocks;
    UINT32 CurrentBlock;

    *play_point = 129;
    *record_point = 0;

    status = sdisk(&TotalBlocks,&CurrentBlock);
 
    if(status == 0)
    { 
        if(CurrentBlock != 0)
        {
            *record_point = CurrentBlock;
        }
        else
        {
            *record_point = 129;
        }
        
    }

#if 0
    if ( RecorderStatus == STATUS_ERROR )
        RecorderStatus = STATUS_IDLE;
#endif

    return NO_ERROR;
}



int FindPlayPointByKeyword( char *keyword, int mode )
{
    //RecorderStatus = STATUS_PLAY;

    return NO_ERROR;
}



int FindPlayPointByValue( int value, int mode )
{
    //RecorderStatus = STATUS_PLAY;

    return NO_ERROR;
}







int Media( UINT32 *bytes_per_block, UINT32 *blocks_used, UINT32 *blocks_remaining )
{
    UINT32 total_block_count = 0;
    UINT32 block_use_count = 0;
    UINT32 blocks_remain;

    if( RecorderStatus == STATUS_DECLASSIFY )
    {
        return ERROR_INVALID_MODE;
    }


    // determine how much space is used for the recordings already on disk
    //
    sdisk( &total_block_count, &block_use_count );

    *bytes_per_block = M23_BLOCK_SIZE;
    *blocks_used = block_use_count;

    if(block_use_count >= total_block_count)
    {
        *blocks_remaining = 0;
    }
    else
    {
        blocks_remain = total_block_count - block_use_count;
        if(blocks_remain < (SYSTEM_BLOCKS_RESERVED + 1) )
        {
            blocks_remain = 0;
        }

        *blocks_remaining = blocks_remain;
    }


#if 0
    if ( RecorderStatus == STATUS_ERROR )
        RecorderStatus = STATUS_IDLE;
#endif

    return NO_ERROR;
}

void Declass_Mount()
{
    int     status;

    status = sinit();

    M23_ClearHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);

}

int Mount( void )
{
    int     status;
    int     config;
    int     year;
    int     numfiles;
    GSBTime CartridgeTime;
    char    time[120];
    int     debug;

    UINT32  total_blocks;
    UINT32  blocks_used;

    UINT32  CSR;



    M23_CHANNEL  const *chan_config;

    SetupGet(&chan_config);


    M23_GetDebugValue(&debug);

    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) )
    {
        //printf( "Error: Invalid state for Mount command\n" );
        return ERROR_INVALID_MODE;
    }

    status = sinit();
    if(status != 0 )
    {
       // M23_SetInitHealthBit(RECORDER_FEATURE, M23_MEMORY_NOT_PRESENT);
        M23_SetHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);
        //M23_SetFaultLED(0); 
        return ERROR_NO_MEDIA;
    }
    else 
    {
        //M23_ClearInitHealthBit(RECORDER_FEATURE, M23_MEMORY_NOT_PRESENT);
        M23_ClearHealthBit(RECORDER_FEATURE,M23_DISK_NOT_CONNECTED);
        M23_ClearMediaError();

        if(TmatsLoaded)
            M23_ClearFaultLED(0);


        ////M2x_ReadCapacity();
        M23_GetConfig(&config);
        //m23_InitializeEvents();

        memset(RMM_Version,0x0,80);
        RMM_VersionSize = M23_RetrieveCartridgeVersion(RMM_Version);

        /*Check to see if Cartridge is Full*/
        sdisk(&total_blocks,&blocks_used);
        if( ((total_blocks - blocks_used) < SYSTEM_BLOCKS_RESERVED) || (blocks_used >= total_blocks) )
        {

            if(config == 3)
            {
                M23_SetFullLED();
            }
            else
            {
                //M23_SetFaultLED(0);
            }

            M23_SetHealthBit(RECORDER_FEATURE, M23_MEMORY_FULL);
            M23_SetHealthBit(RECORDER_FEATURE, M23_MEMORY_BINGO_REACHED);
            M23_SetDataLED();
        }
        else
        {
            if(config == 3)
            {
                M23_ClearFullLED();
            }

            M23_ClearHealthBit(RECORDER_FEATURE, M23_MEMORY_FULL);
        }

        if(TimeSource == TIMESOURCE_M1553)
        {
            status = M23_M1553_IsTimeJammed();
            if(status == 0)
            {
                CartridgeTime.Days    = 1;
                CartridgeTime.Hours   = 0;
                CartridgeTime.Minutes = 0;
                CartridgeTime.Seconds = 0;

                M23_GetYear(&year);

                status = DiskIsConnected();

                if(status)
                {
                    if(TimeSet_Command == 0)
                    {
                        return 0;
                    }

                    status = M2x_GetCartridgeTime(time);
                    if(status == 0)
                    {
                        M23_SetSystemTime(time);

                        CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
                        CSR &= ~TIMECODE_M1553_JAM;
                        CSR |= TIMECODE_RMM_JAM;
                        M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);

                        SSRIC_SetTimeStatus();
                        M23_SetTimeLED();
                    }
                }
                else
                {
                    M23_ClearTimeLED();
                    SSRIC_ClearTimeStatus();
                    SetTime(CartridgeTime);
                }
            }

        }
        else if( (TimeSource == TIMESOURCE_CARTRIDGE) || (TimeSource == TIMESOURCE_EXTERNAL) )
        {
            CartridgeTime.Days    = 1;
            CartridgeTime.Hours   = 0;
            CartridgeTime.Minutes = 0;
            CartridgeTime.Seconds = 0;

            M23_GetYear(&year);

            status = DiskIsConnected();


            if(status)
            {
                if(TimeSource == TIMESOURCE_EXTERNAL)
                {
                    if(TimeSet_Command != 0)
                    {
                        status = M2x_GetCartridgeTime(time);
                        if(status == 0)
                        {
                            M23_SetSystemTime(time);

                            CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
                            CSR &= ~TIMECODE_M1553_JAM;
                            CSR |= TIMECODE_RMM_JAM;
                            M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);

                            SSRIC_SetTimeStatus();
                            M23_SetTimeLED();
                        }
                    }
                }
            }
            else
            {
                M23_ClearTimeLED();
                SSRIC_ClearTimeStatus();
                SetTime(CartridgeTime);
            }

        }

        STANAG_GetNumberOfFiles(&numfiles);
        if(debug)
            printf("Number of Files %d\r\n",numfiles);
        if(numfiles == 0)
        {
            M23_SetEraseLED();
            M23_ClearDataLED();
        }
        else
        {
            M23_ClearEraseLED();
            M23_SetDataLED();
        }


        RecorderStatus = STATUS_IDLE;

        /*Now Check If Record Switch Is ON*/
        if( (RecordSwitchPosition()) || (AutoRecord == 1) )
        {
            status = Record();

            if(status == NO_ERROR)
            {
                if(RecordSwitchPosition())
                    M23_SetFrom();
                else if(AutoRecord == 1)
                    M23_SetFromAuto();
            }

        }

    }

#if 0
    if ( RecorderStatus == STATUS_ERROR )
        RecorderStatus = STATUS_IDLE;
#endif



    M23_DismountCommand = FALSE;

    return NO_ERROR;
}


int GetRecorderLogicState()
{
    UINT32 CSR;
    UINT32 CSR32;
    char   tmp[20];
    int    i;
    int    NumMPBoards = 0;
    int    status = 0;

    CSR = M23_ReadControllerCSR( CONTROLLER_CONTROL_CSR);

    if( CSR & CONTROLLER_GLOBAL_BUSY)
    {
        PutResponse(0,"Global Busy Asserted -> Cannot Record\r\n");
        status = -1;
    }

    CSR = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO_CSR);
    if(!(CSR & CONTROLLER_HOST_CHANNEL_FIFO_EMPTY))
    {
        PutResponse(0,"Host FIFO Not Empty -> Cannot Record\r\n");
        status = -1;
    }

    M23_NumberMPBoards(&NumMPBoards);

    for(i = 0; i < NumMPBoards;i++)
    {
        if(M23_MP_IS_M1553(i) )
        {
        }
        else
        {
 
            /*Clear the Enable Bit*/
            CSR32 = M23_mp_read_csr(i,BAR1,MP_CTM_OFFSET);

            if(!(CSR32 &MP_CTM_FIFO_NOT_EMPTY) )
            {
                memset(tmp,0x0,20);
                sprintf(tmp,"MP Board %d Cascade FIFO not empty\r\n",i);
                PutResponse(0,tmp);
                status = -1;
            }
        }
       
    }

    return(status);

}

int Record( void )
{
    char   file_name[20];
    int    status;

    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) && (RecorderStatus != STATUS_PLAY) )
    {
        return ERROR_INVALID_MODE;
    }

#if 0 //PC_REMOVE

    M23_GetFaultLED(&Ready);
    if(Ready == FALSE)
    {
        RecorderStatus = STATUS_ERROR;
        return ERROR_INVALID_MODE;
    }
#endif


    file_name[0] = '\0';

    status = RecordFile(file_name);

    return (status);
}



int RecordFile( char *file_name )
{
    int    status;
    int    debug;
    int    count = 0;
    UINT32 CSR;
    UINT32 total_block_count = 0;
    UINT32 block_used_count = 0;

    M23_CHANNEL const  *config;

    M23_GetDebugValue(&debug);

    /*Make sure a TMATS has been loaded*/
    if(TmatsLoaded == 0)
    { 
        return ERROR_INVALID_MODE;
    }

    status = DiskIsConnected();
    if(status == 0)
    {
        return ERROR_NO_MEDIA;
    }

    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) && (RecorderStatus != STATUS_PLAY) )
    {
        //printf( "Error: Invalid state for Record command\n" );
        if((RecorderStatus != STATUS_RTEST) || (RecorderStatus != STATUS_RTEST) )
        {
            RecorderStatus = STATUS_ERROR;
        }

        return ERROR_INVALID_MODE;
    }

    
    M23_Clear_Entries();

    // make sure that there is room for recording more data
    //

    status = sdisk(&total_block_count, &block_used_count );
    if ( status != 0 )
    {
        printf( "Error: No media installed\r\n" );
        RecorderStatus = STATUS_ERROR;
        return ERROR_NO_MEDIA;
    }


    // TODO: figure out exactly how much free space is needed for a recording
    //
    if ( ((total_block_count - block_used_count) <= SYSTEM_BLOCKS_RESERVED ) || (block_used_count >= total_block_count) )
    {
        //printf( "Error: Not enough free space remaining for a recording\r\n" );
        RecorderStatus = STATUS_ERROR;
        return ERROR_MEMORY_FULL;
    }

    M23_StopControllerRecord(); //Stop The Global Packet Making

    /*Wait For Global Busy to Clear*/
    while(1)
    {
        CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);
        if( CSR & CONTROLLER_GLOBAL_BUSY)
        {
            if(count > 50)
            {
                if(debug)
                    printf("Global Busy Never Cleared\r\n");
                break;
            }
            else
            {
                usleep(1000);
                count++;
            }
        }
        else
        {
            break;
        }
    }

    // open a file
    //
    if(*file_name == '\0')
        status = sopen("","W");
    else
        status = sopen(file_name,"W");

    if ( status < 0 )
    {
        //printf( "Error: Unable to open file for recording\r\n" );
        RecorderStatus = STATUS_ERROR;
        return ERROR_COMMAND_FAILED;
    }


    /*Set the MUX select to Cascade*/
    M23_WriteControllerCSR(IEEE1394B_RECORD_FIFO_MUX,RECORD_MUX_SELECT_CASCADE);

    M23_StartDataToDisk();
    M23_StartControllerRecord();

    /*Set the 1394B logic Record to ON*/
    CSR = M23_ReadControllerCSR(IEEE1394B_RECORD);
    M23_WriteControllerCSR(IEEE1394B_RECORD,CSR | IEEE1394B_RECORD_GO);


    /*Set Buffer Transfer Manager Go*/
    M23_WriteControllerCSR(CONTROLLER_BTM_CSR,CONTROLLER_BTM_GO );


    /*Write TMATS File at this point*/
    M23_TmatsWriteToHost();


    M23_ClearRecordNodes();
    M23_SetRecordLED();

    SSRIC_ClearEraseStatus();
    SSRIC_SetRecordStatus();

    M23_ClearEraseLED();
    M23_SetDataLED();

    if(RecorderStatus == STATUS_PLAY)
    {
        RecorderStatus = STATUS_REC_N_PLAY;
    }
    else
    {
        RecorderStatus = STATUS_RECORD;
    }

    SetupGet( &config );
    CSR = M23_ReadControllerCSR(CONTROLLER_INDEX_CHANNEL_GO);
    M23_WriteControllerCSR(CONTROLLER_INDEX_CHANNEL_GO,CSR | INDEX_CHANNEL_RECORD );


    M23_SetFirstRecord();


    return NO_ERROR;
}



int Reset( void )
{

#if 0
    if( RecorderStatus == STATUS_ERROR )
        RecorderStatus = STATUS_IDLE;

    if ( RecorderStatus != STATUS_IDLE )
        Stop( 0 );

#endif
    /*We need to add reset for TRD*/

    // initialize the modules that this subsystem depends on
    //
#if 0
    filesys_Initialize();

    // restore settings
    //
    err = settings_Initialize();

    if ( err < 0 ) 
	{
        // this error is unrecoverable -- can't access settings file.
        // the error flag should stay set until the settings file can
        // be successfully saved
        //
        //printf( "Error: unable to restore system settings\n" );

		M23_SetInitHealthBit(RECORDER_FEATURE, M2X_RECORDER_SETTINGS_FILE_ERR);
    }
#endif


    return NO_ERROR;
}



int SetupViewCurrent( int *setup )
{
    *setup = SelectedSetup;

#if 0
    if( RecorderStatus == STATUS_ERROR )
        RecorderStatus = STATUS_IDLE;
#endif

    return NO_ERROR;
}


void SaveSetup(int setup)
{
    SelectedSetup = setup;
}

int SetupSet( int setup,int initial )
{
    int     status;
    int     debug;
    int     year;
    GSBTime CartridgeTime;
    char    time[120];
    char    milli[4];

    UINT32  CSR;

    M23_CHANNEL  const *chans;

    memset(time,0x0,120);

    M23_GetDebugValue(&debug);

    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) )
    {
        //printf( "Error: Invalid state for Setup command (query only permitted)\n" );
        return ERROR_INVALID_MODE;
    }

    if ( setup >= MAX_NUM_SETUPS )  // setup param is zero based
    {
        //printf( "Error: Invalid setup number\n" );
        RecorderStatus = STATUS_ERROR;
        return ERROR_INVALID_PARAMETER;
    }

    if(debug)
        printf("Stopping Recorder\r\n");

    /*We need to delete Streams*/
    M23_StopRecorder();

    ssric_clearAll();

    M23_RemoveVideoI2c();

    //For TRD
    M23_InitializeHealth();

    
    if(debug)
        printf("Process Setup\r\n");
    status = ProcessSetup(setup);
    if ( status != M23_SUCCESS)
    {
        //printf( "Error: Invalid/missing setup file or missing input board->%d,%d\n",setup,status );
        SSRIC_SetFault();
        //M23_SetFaultLED(0);
        M23_SetHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
        RecorderStatus = STATUS_ERROR;
        return ERROR_COMMAND_FAILED;
    }
    else
    {
        M23_ClearHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);

        if(debug)
            printf("Saving Configuration %d\r\n",setup);

        SaveSetup(setup);

        Setup_Save();

    }


    SetupCondor();
    M23_WriteTimeCodeOffset();

    //sleep(1);

    SetupChannels();

    SetupGet(&chans);

    if( (chans->NumConfiguredEthernet == 0 ) || (chans->eth_chan.isEnabled == 0) ){
        if(StartEther ==  0)
        {
            if(debug)
                printf("Starting Ethernet\r\n");

            M23_StartEthernet(0x0014,0x5223,0x0000,0x1);

            StartEther = 1;
        }
    }

    status = M23_StartControllerHardware(chans->NumConfiguredEthernet);
    /*We can now set Filter Sync*/

    M23_SetFilterGo();

    //M23_SetGo();

    SetupIndex();

    //M23_SetFilterGo();

    if(TimeSource == TIMESOURCE_CARTRIDGE)
    {
        memset(milli,0x0,4);
        CartridgeTime.Days    = 1;
        CartridgeTime.Hours   = 0;
        CartridgeTime.Minutes = 0;
        CartridgeTime.Seconds = 0;

        M23_GetYear(&year);

        status = DiskIsConnected();

        if(status)
        {
            status = M2x_GetCartridgeTime(time);
            if(status == 0)
            {
                M23_SetSystemTime(time);

                CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
                CSR &= ~TIMECODE_M1553_JAM;
                CSR |= TIMECODE_RMM_JAM;
                M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);

                M23_SetTimeLED();
                SSRIC_SetTimeStatus();

            }
        }
        else
        {
            M23_ClearTimeLED();
            SSRIC_ClearTimeStatus();
            SetTime(CartridgeTime);
        }


    }
    else
    {
        /*Clear the External Time Sync Variable*/
        M23_ClearTimeSync();

    }

    //Test Move to Before Setting Up Channels SaveSetup(setup);
    RecorderStatus = STATUS_IDLE;

    return NO_ERROR;
}

int M23_Setup( int setup,int initial )
{
    int     status;
    int     year;
    GSBTime CartridgeTime;
    char    time[120];
    int     debug;
    char    milli[4];

    UINT32  CSR;

    M23_GetDebugValue(&debug);
    memset(time,0x0,120);

    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) )
    {
        //printf( "Error: Invalid state for Setup command (query only permitted)\n" );
        return ERROR_INVALID_MODE;
    }

    if ( setup >= MAX_NUM_SETUPS )  // setup param is zero based
    {
        //printf( "Error: Invalid setup number\n" );
        RecorderStatus = STATUS_ERROR;
        return ERROR_INVALID_PARAMETER;
    }

    M23_StopRecorder();
    ssric_clearAll();

    M23_RemoveVideoI2c();
    sleep(1);


    //For TRD
    M23_InitializeHealth();

    status = ProcessSetup(setup);
    if ( status != M23_SUCCESS)
    {
        //printf( "Error: Invalid/missing setup file or missing input board->%d,%d\n",setup,status );
        SSRIC_SetFault();
        //M23_SetFaultLED(0);
        M23_SetHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
        RecorderStatus = STATUS_ERROR;
        return ERROR_COMMAND_FAILED;
    }
    else
    {
        M23_ClearHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
        TmatsLoaded = 1;
    }

    if(TimeSource != TIMESOURCE_CARTRIDGE)
    {
        M23_WriteTimeCodeOffset();
    }

    sleep(1);

    SetupChannels();

    M23_SetGo();

    SetupIndex();

    M23_SetFilterGo();

    if(TimeSource == TIMESOURCE_CARTRIDGE)
    {
        memset(milli,0x0,4);
        CartridgeTime.Days    = 1;
        CartridgeTime.Hours   = 0;
        CartridgeTime.Minutes = 0;
        CartridgeTime.Seconds = 0;

        M23_GetYear(&year);

        status = DiskIsConnected();

        if(status)
        {
            status = M2x_GetCartridgeTime(time);
            if(status == 0)
            {
                M23_SetSystemTime(time);

                CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
                CSR &= ~TIMECODE_M1553_JAM;
                CSR |= TIMECODE_RMM_JAM;
                M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);

                M23_SetTimeLED();
                SSRIC_SetTimeStatus();
            }
        }
        else
        {
            M23_ClearTimeLED();
            SSRIC_ClearTimeStatus();
            SetTime(CartridgeTime);
        }


    }
    else
    {
        /*Clear the External Time Sync Variable*/
        M23_ClearTimeSync();

    }

    SaveSetup(setup);
   // SelectedSetup = setup;
    RecorderStatus = STATUS_IDLE;

    return NO_ERROR;
}



//CMD_STATUS
int Status( int *p_state, int *p_non_critical_warnings, int *p_critical_warnings, int *p_percent_complete )
{
    int    TotalErrors = 0;
    int    CriticalErrors = 0;
    int    i = 0,j = 0;
    int    return_status;
    UINT32 *HealthStatusArr = NULL;
    UINT32 *CriticalMasks = NULL;
    UINT32 total_block_count = 1;
    UINT32 block_use_count = 0;
    UINT64 blocks;
    UINT64 percent;

    static int declass = 0;

    *p_state = RecorderStatus;
    
    *p_non_critical_warnings = 0;
    *p_critical_warnings = 0;    

    M23_GetHealthArray(&HealthStatusArr);
    M23_GetCriticalMask(&CriticalMasks);

    for( i = 0 ; i < MAX_FEATURES ; i++ )//least sig bit is not Error
    {
        //if(HealthStatusArr[i] & 0x00000001
        if(HealthStatusArr[i] & M23_FEATURE_ENABLED)
        {
            for( j = 0 ; j < 16 ; j++)
            {
                if( ((HealthStatusArr[i] & ( 0x1 << j) ) & CriticalMasks[i]) !=0 )
                    CriticalErrors++;

                if((HealthStatusArr[i] & (0x01 << j)) !=0 )
                    TotalErrors++;
            }
        }
    }


    *p_critical_warnings     = CriticalErrors;
    *p_non_critical_warnings = TotalErrors-CriticalErrors;
    

    if ( (RecorderStatus == STATUS_RECORD) || (RecorderStatus == STATUS_RTEST) || (RecorderStatus == STATUS_REC_N_PLAY) )
    {
        *p_percent_complete = 0;
    
        // determine how much space is used for the recordings already on disk
        //
        return_status = DiskIsConnected();
        if(return_status)
        {

          sdisk( &total_block_count, &block_use_count );
        }


        //*p_percent_complete = (((total_block_count - block_use_count) * 100)/total_block_count);
        blocks = (UINT64)block_use_count * 100;
        percent = blocks/(UINT64)total_block_count;
        if(percent > 99)
        {
            percent = 99;
        }
        //*p_percent_complete = ((block_use_count*100)/total_block_count);
        *p_percent_complete = (UINT32)percent;
    
    }
    else if ( RecorderStatus == STATUS_DECLASSIFY )
    {
        *p_percent_complete = CartPercent;
        *p_critical_warnings = DeclassErrors;
        declass = 1;
    
    }
    else if ( RecorderStatus == STATUS_ERASE )
    {

    }
    else if ( RecorderStatus == STATUS_BIT )
    {
        CheckBITStatus( p_percent_complete );
    
    }
    else if ( RecorderStatus == STATUS_FIND )
    {
        // TODO: determine percent_complete based on current mode
        *p_percent_complete = 0;
    
    }
    else if ( RecorderStatus == STATUS_PLAY )
    {
        // TODO: determine percent_complete based on current mode
        M23_GetPlayPercent(p_percent_complete);
    
    }
    else if ( RecorderStatus == STATUS_PTEST )
    {
        *p_percent_complete = PTest_Percentage;
    }
    else
    {
        *p_percent_complete = 0;
        if(declass == 1)
        {
            *p_critical_warnings = DeclassErrors;
            declass = 0;
        }
    }

/*  This function should not change the status because it is called 
    from other modules on a regular basis and prevents the STATUS_ERROR
    state from being seen when called from the serial user interface.

*/
    if( (RecorderStatus == STATUS_ERROR ) || (RecorderStatus == STATUS_FAIL) )
    {
        RecorderStatus = STATUS_IDLE;
    }

    return NO_ERROR;
}


void AbortRecord( void)
{
    Stop(0);

    DiskDisconnect();
}

int Emergency_Stop( int mode )
{
    int    verbose;
    int    config;
    int    pkt_len;
    int    count;

    UINT32 CSR;

    UINT64 offset;


    /*We will only do this now since all stops will be emergency stops*/
    if(RecorderStatus == STATUS_REC_N_PLAY)
    {
        RecorderStatus = STATUS_PLAY;
    }
    else
    {
        RecorderStatus = STATUS_IDLE;
    }

    /*Set the Controller to Stop, BIT and locattion TBD*/

    //M23_StopControllerRecord();
    M23_StopDataToDisk();


    count = 0;
    while(1)
    {
        CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);

        if(CSR & LAST_ROOT_IN_FIFO)
        {
            /*Wait here for the Slushware to let me know the last root has been written*/
            M23_GetLastRoot(&offset,&pkt_len);

            if(offset > 0)
            {
                UpdateStanagBlocks(offset,pkt_len);
            }

            break;
        }
        else
        {
            sleep(1);
            count++;
            if(count > 3)
            {
                break;
            }
        }

    }

    M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,CSR | LAST_ROOT_IN_FIFO);


    /*Remove Record BIT*/
    CSR = M23_ReadControllerCSR(IEEE1394B_RECORD);
    M23_WriteControllerCSR(IEEE1394B_RECORD,CSR & ~IEEE1394B_RECORD_GO);

    /*Don't know if we need this - Remove the Host Record Bit
    M23_StopHostChannelRecord();*/

    /* close the file */
    Emergency_Close(0,1);


    M23_ClearRecordLED();
    SSRIC_ClearRecordStatus();

    M23_GetVerbose(&verbose);
    if(verbose)
    {
        PutResponse(0, "\r\nRecording stopped.\r\n" );
    }

    M23_SetLastRecordEvent();

    M23_GetConfig(&config);
    if( (config == 0) || (config == P3_CONFIG) )
    {
        /*Write the Nodes Files*/
        usleep(100000);
        m23_WriteNodesFile();
    }

    return 0;
}

//Recorder_STOP
int Stop( int mode )
{
    int    count = 0;
    int    status;

    M23_WriteControllerCSR(IEEE1394B_RECORD_POINTER_IN,0xFFFFFFFF);

    Emergency_Stop(mode);
    RootReceived = 0;
    status = NO_ERROR;


    return status;


#if 0
    if(RecorderStatus == STATUS_REC_N_PLAY)
    { 
        RecorderStatus = STATUS_PLAY;
    }
    else
    {
        RecorderStatus = STATUS_IDLE;
    }

//printf("Stopping Record, Global\r\n");
    /*Stop the controller record*/
    M23_StopControllerRecord();

    while(1)
    {
        CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);

        if( !(CSR & CONTROLLER_GLOBAL_BUSY))
        {
            break;
        }
        else
        {
            count++;
            if(count > 50)
            {
                printf("Global Busy Never Cleared\r\n");
                break;
            }
           usleep(1000);
        }
    }


//printf("Stopping Host record\r\n");
    /*Remove the Host Record Bit*/
    M23_StopHostChannelRecord();


    count = 0;
    /*Wait for Host Channel to Clear*/
    while(1)
    {
        CSR = M23_ReadControllerCSR(CONTROLLER_HOST_CONTROL_CSR);
        if( !(CSR & CONTROLLER_HOST_CHANNEL_BUSY))
        {
            //printf("Host Channel Clear\r\n");
            break;
        }
        else
        {
            count++;
            if(count > 500)
            {
                printf("Host Channel Still Busy\r\n");
                break;
            }
        }
        usleep(100);
    }

    /*Clear Buffer Transfer Manager Go*/
    CSR = M23_ReadControllerCSR(CONTROLLER_BTM_CSR);
    M23_WriteControllerCSR(CONTROLLER_BTM_CSR, CSR & ~CONTROLLER_BTM_GO);
    CSR = M23_ReadControllerCSR(CONTROLLER_BTM_CSR);

    //usleep(100000);
    sleep(1);

 
//printf("Wait for 1394\r\n");
    loops = 0;
    while(1)
    {
        CSR = M23_ReadControllerCSR(IEEE1394B_RECORD);

        if( (CSR & IEEE1394B_RECORD_BUSY) == 0 )
        {
            break;
        }
        else
        {
            loops++;

            if(loops > 300)
            {
                break;
            }
            else
            {
                usleep(1000);
            }
        }
    }

    /*Remove Record BIT*/
    CSR = M23_ReadControllerCSR(IEEE1394B_RECORD);
    M23_WriteControllerCSR(IEEE1394B_RECORD,CSR & ~IEEE1394B_RECORD_GO);

    CSR = M23_ReadControllerCSR(IEEE1394B_RECORD_BLOCKS_RECORDED);

//printf("Number of Blocks Recorded %d\r\n",CSR);
    UpdateBlocksWritten(CSR);



    /*Get the Fill Amount*/
    CSR = M23_ReadControllerCSR(CONTROLLER_BTM_FILL_CSR);
    /* close the file */
    sclose(0,1,CSR);

    M23_GetVerbose(&verbose);
    if(verbose)
    {
        PutResponse(0, "\r\nRecording stopped.\r\n" );
    }

    M23_ClearRecordLED();
    SSRIC_ClearRecordStatus();

    M23_SetLastRecordEvent();

    M23_GetConfig(&config);
    if(config == 0)
    {
        /*Write the Nodes Files*/
        m23_WriteNodesFile();
    }
#endif



    return NO_ERROR;
}

//Recorder_STOP after removal of Cartridge
int Removal_Stop( )
{
    int    verbose;

    UINT32 CSR;

    /*Set the Controller to Stop, BIT and locattion TBD*/

    M23_StopControllerRecord();

    /*Remove Record BIT*/
    CSR = M23_ReadControllerCSR(IEEE1394B_RECORD);
    M23_WriteControllerCSR(IEEE1394B_RECORD,CSR & ~IEEE1394B_RECORD_GO);

    /*Don't know if we need this - Remove the Host Record Bit
    M23_StopHostChannelRecord();*/

    /* close the file */
    //Emergency_Close(0,1);

    /*We will only do this now since all stops will be emergency stops*/
    if(RecorderStatus == STATUS_REC_N_PLAY)
    {
        RecorderStatus = STATUS_PLAY;
    }
    else
    {
        RecorderStatus = STATUS_IDLE;
    }

    Dismount();

    M23_ClearRecordLED();
    SSRIC_ClearRecordStatus();

    M23_GetVerbose(&verbose);
    if(verbose)
    {
        PutResponse(0, "\r\nRecording stopped.\r\n" );
    }

    return 0;
}

int TimeView( GSBTime *p_time )
{
    UINT32 CSR;


    /*Read the Upper Time*/
    CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_BCD_UPPER_CSR);
    p_time->Days = 100 * ((CSR>>8) & 0x000F);
    p_time->Days += 10*  ((CSR >> 4) & 0x000F);
    p_time->Days +=  (CSR & 0x000F);

    p_time->Microseconds = 0;

    /*Read the Lower Time*/
    CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_BCD_LOWER_CSR);


    p_time->Hours = 10 * ((CSR >> 28) & 0x000F);
    p_time->Hours +=  ((CSR>>24)  & 0x000F);

    /*Read the Lower Time*/
    p_time->Minutes =  10 * ( (CSR >> 20) & 0x000F);
    p_time->Minutes +=  ((CSR >> 16) & 0x000F);

    p_time->Seconds =  10 * ((CSR >> 12) & 0x000F);
    p_time->Seconds +=  ((CSR >> 8)& 0x000F);

    return NO_ERROR;
}

int SaveSettings( void )
{
    if ( settings_Update() )
    {
        //printf( "Error: Unable to save settings\n" );
        RecorderStatus = STATUS_ERROR;
        return ERROR_COMMAND_FAILED;
    }


#if 0
    if( RecorderStatus == STATUS_ERROR )
        RecorderStatus = STATUS_IDLE;
#endif

    return NO_ERROR;
}

void SetBitStatus(int percent)
{
    BITPercentComplete = percent;
}

int CheckBITStatus(int *percent_complete )
{

    *percent_complete = BITPercentComplete;

    return 0;

}

CH10_STATUS GetRecorderStatus()
{
    return (RecorderStatus);
}

