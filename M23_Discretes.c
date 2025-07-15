//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File:M23_Discretes.c
//    Version: 1.0
//     Author: PaulC
//
//            MONSSTR 2300 Discretes 
//
//
//    Revisions:   
//
#include <stdio.h>
#include <string.h>

#include "M23_Controller.h"
#include "M23_Typedefs.h"
#include "M23_EventProcessor.h"


/***************************************************
    Standard Switches and LEDs
    Previous Eglin Settings , CONFIG = 1
****************************************************/

#define STD_RECORD_SWITCH  0x1
#define STD_BIT_SWITCH     0x2
#define STD_ENABLE_SWITCH  0x4
#define STD_ERASE_SWITCH   0x8
#define STD_DECLASS_SWITCH 0x10
#define STD_EVENT_SWITCH   0x20

#define STD_RECORD_LED     0xFE
#define STD_FAULT_LED      0xFD
#define STD_BINGO_LED      0xFB
#define STD_M1553_LED      0xF7
#define STD_VIDEO_LED      0xEF
#define STD_ERASE_LED      0xDF
#define STD_TIME_LED       0xBF
#define STD_PCM_LED        0x7F

/***************************************************
    TRD (V2) Switches and LEDs
          CONFIG = 0
****************************************************/

#define REMOTE_LOCKED  0x1
#define ERASE_SWITCH   0x2
#define ENABLE_SWITCH  0x4
#define DECLASS_SWITCH 0x8
#define BIT_SWITCH     0x10
#define EVENT_SWITCH   0x20
#define REMOTE_PRES    0x40
#define RECORD_SWITCH  0x80

#define RECORD_LED       0xFE
#define ERASE_LED        0xFD
#define FAULT_LED        0xFB
#define DECLASS_LED      0xF7
#define RECORD2_LED      0xEF
#define M1553_READY_LED  0xDF
#define FAULT_EM_LED     0xBF
#define FAULT_CL_LED     0x7F


/***************************************************
    Lockheed Switches and LEDs
 **************************************************/
#define LOCKHEED_RECORD_SW   0X1 //Record
#define LOCKHEED_EVENT_SW    0x2 //Event Mark
#define LOCKHEED_BIT_SW      0x4 //SBIT
#define LOCKHEED_VIDEO0_SW   0x8 //Bit 0 of Video Select
#define LOCKHEED_VIDEO1_SW   0x10 //Bit 1 of Video Select
#define LOCKHEED_VIDEO2_SW   0x20 //Bit 2 of Video Select
#define LOCKHEED_ERASE_SW    0x40 //Erase Command
#define LOCKHEED_ENABLE_SW   0x80

#define LOCKHEED_RECORD      0xFE
#define LOCKHEED_READY       0xFD
#define LOCKHEED_BINGO       0xFB
#define LOCKHEED_DATA        0xF7  
#define LOCKHEED_DWNLD       0xEF
#define LOCKHEED_ERASE       0xDF
#define LED_DCOUT7      0xBF
#define LED_DCOUT8      0x7F

/***************************************************
    P3 Switches and LEDs
 **************************************************/
#define P3_RECORD_SW   0X1 //Record
#define P3_BIT_SW      0x2 //IBIT
#define P3_ENABLE_SW   0x4 //Enable
#define P3_ERASE_SW    0x8 //Erase
#define P3_EVENT_SW   0x20 //Event Mark

#define P3_RECORD      0xFE
#define P3_FAULT       0xFD
#define P3_BINGO       0xFB
#define P3_ERASE       0xF7
#define P3_READY       0xEF



#define ENABLE_EXTERNAL 0x10000

pthread_t DiscreteThread;

static int stopFaultBlink;
pthread_t FaultBlinkThread;
void StartFaultBlink();

static int stopBingoBlink;
pthread_t BingoBlinkThread;
void StartBingoBlink();


UINT8 InitialSwitches;
UINT8 CurrentSwitches;
UINT8 PreviousSwitches;

static UINT8 Ready;
static UINT8 OtherSet;
static UINT8 Critical;

static UINT8 LEDS;

static int Tail;

static int Reverse;

static int InRecord;

static UINT8  SelectChanged;  

void M23_SetFrom()
{
    CommandFrom = FROM_DISCRETE;
}

void M23_SetReverse()
{
    Reverse = 1;
}

void GetCartridgeDiscretes(int *present, int *latch)
{
    UINT32         NewSwitches;
    int   config;

    M23_GetConfig(&config);

    NewSwitches = M23_ReadControllerCSR(CONTROLLER_DISCRETE_CSR);

    if((config == 1) || (config == 2)  || (config == 3) || (config == 4) || (config == LOCKHEED_CONFIG) || (config == P3_CONFIG) || (config == B1B_CONFIG) || (config == B52_CONFIG) )
    {
        *present = (NewSwitches >> 14) & 0x1;
        *latch  = (NewSwitches >> 15) & 0x1;

    }
    else
    {
        *latch    = (NewSwitches>>8) & REMOTE_LOCKED;
        *present  = (NewSwitches >>8) & REMOTE_PRES;
    }


}

void M23_WriteDiscretes(UINT8 LEDs)
{
    UINT32 CSR;

    LEDS &= LEDs;

    CSR = ENABLE_EXTERNAL | LEDS | CONTROLLER_DISCRETE_BLINK_ON ; 

    M23_WriteControllerCSR( CONTROLLER_DISCRETE_CSR,CSR);
}

void M23_TurnOffDiscretes(UINT8 LEDs)
{
    UINT32 CSR;

    LEDS |= ~LEDs;

    CSR = ENABLE_EXTERNAL | LEDS | CONTROLLER_DISCRETE_BLINK_ON ; 


    M23_WriteControllerCSR( CONTROLLER_DISCRETE_CSR,CSR);
}

void M23_SetBingoLED()
{
    int   config;

    M23_GetConfig(&config);

    if(config == B52_CONFIG)
        StartBingoBlink();
    else if((config == 1) || (config == 2) || (config == 4) || (config == B1B_CONFIG) )
        M23_WriteDiscretes(STD_BINGO_LED);
    else if(config == LOCKHEED_CONFIG)
        M23_WriteDiscretes(LOCKHEED_BINGO);
    else if(config == P3_CONFIG)
        M23_WriteDiscretes(P3_BINGO);

}

void M23_ClearBingoLED()
{
    int   config;

    M23_GetConfig(&config);

 
    if(config == B52_CONFIG)
        stopBingoBlink = 1;
    else if((config == 1) || (config == 2) || (config == 4) || (config == B1B_CONFIG) )
        M23_TurnOffDiscretes(STD_BINGO_LED);
    else if(config == LOCKHEED_CONFIG)
        M23_TurnOffDiscretes(LOCKHEED_BINGO);
    else if(config == P3_CONFIG)
        M23_TurnOffDiscretes(P3_BINGO);

    
}

void M23_SetFullLED()
{
    M23_WriteDiscretes(STD_BINGO_LED);
}

void M23_ClearFullLED()
{
    M23_TurnOffDiscretes(STD_BINGO_LED);
}

void M23_ClearRecordLED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 3) || (config == 4) || (config == B1B_CONFIG) || (config == B52_CONFIG) )
    {
        M23_TurnOffDiscretes(STD_RECORD_LED);
    }
    else if(config == 0)
    {
        M23_TurnOffDiscretes(RECORD_LED);
        M23_TurnOffDiscretes(RECORD2_LED);
    }
    else if(config == LOCKHEED_CONFIG)
    {
        M23_TurnOffDiscretes(LOCKHEED_RECORD);
    }
    else if(config == P3_CONFIG)
    {
        M23_TurnOffDiscretes(P3_RECORD);
    }

}

void M23_SetRecordLED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 3) || (config == 4) || (config == B1B_CONFIG) || (config == B52_CONFIG) )
    {
        M23_WriteDiscretes(STD_RECORD_LED);
    }
    else if(config == 0)
    {
        M23_WriteDiscretes(RECORD_LED);
        M23_WriteDiscretes(RECORD2_LED);
    }
    else if(config == LOCKHEED_CONFIG)
    {
        M23_WriteDiscretes(LOCKHEED_RECORD);
    }
    else if(config == P3_CONFIG)
    {
        M23_WriteDiscretes(P3_RECORD);
    }

}

void M23_SetPCMLED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 4) )
    {
        M23_WriteDiscretes(STD_PCM_LED);
    }
}

void M23_ClearPCMLED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 4) )
    {
        M23_TurnOffDiscretes(STD_PCM_LED);
    }
}


void M23_SetM1553LED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 4) || (config == B52_CONFIG) )
    {
        M23_WriteDiscretes(STD_M1553_LED);
    }
}

void M23_ClearM1553LED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 4) || (config == B52_CONFIG) )
    {
        M23_TurnOffDiscretes(STD_M1553_LED);
    }
}

void M23_SetVideoLED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 4) || (config == B52_CONFIG) )
    {
        M23_WriteDiscretes(STD_VIDEO_LED);
    }
}

void M23_ClearVideoLED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 4) || (config == B52_CONFIG) )
    {
        M23_TurnOffDiscretes(STD_VIDEO_LED);
    }
}

void M23_SetTimeLED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 4) || (config == B52_CONFIG) )
    {
        M23_WriteDiscretes(STD_TIME_LED);
    }
}

void M23_ClearTimeLED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 4) || (config == B52_CONFIG) )
    {
        M23_TurnOffDiscretes(STD_TIME_LED);
    }
}

void M23_SetEthernetDataPresent()
{
    M23_WriteDiscretes(STD_PCM_LED);
}

void M23_ClearEthernetDataPresent()
{
    M23_TurnOffDiscretes(STD_PCM_LED);
}

void M23_ClearEraseLED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 3) ||(config == 4)  || (config == B52_CONFIG) )
    {
        M23_TurnOffDiscretes(STD_ERASE_LED);
        SSRIC_ClearEraseStatus( );
    }
    else if(config == 0)
    {
        M23_TurnOffDiscretes(ERASE_LED);
    }
    else if(config == LOCKHEED_CONFIG)
    {
        M23_TurnOffDiscretes(LOCKHEED_ERASE);
    }
    else if(config == P3_CONFIG)
    {
        M23_TurnOffDiscretes(P3_ERASE);
    }

}

void M23_SetEraseLED()
{
    int   config;

    M23_GetConfig(&config);

    if((config == 1) || (config == 2) || (config == 3) || (config == 4) || (config == B52_CONFIG) )
    {
        M23_WriteDiscretes(STD_ERASE_LED);
        SSRIC_SetEraseStatus();

    }
    else if(config == 0)
    {
        M23_WriteDiscretes(ERASE_LED);
    }
    else if(config == LOCKHEED_CONFIG)
    {
        M23_WriteDiscretes(LOCKHEED_ERASE);
    }
    else if(config == P3_CONFIG)
    {
        M23_WriteDiscretes(P3_ERASE);
    }

}

void M23_SetDataLED()
{
    int   config;

    M23_GetConfig(&config);

    if(config == LOCKHEED_CONFIG)
    {
        M23_WriteDiscretes(LOCKHEED_DATA);
    }
}

void M23_ClearDataLED()
{
    int   config;

    M23_GetConfig(&config);

    if(config == LOCKHEED_CONFIG)
    {
        M23_TurnOffDiscretes(LOCKHEED_DATA);
    }
}

void M23_ClearDeclassLED()
{
    M23_TurnOffDiscretes(DECLASS_LED);
}

void M23_SetDeclassLED()
{
    M23_WriteDiscretes(DECLASS_LED);
}
 
void M23_SetM1553Ready()
{
    M23_WriteDiscretes(M1553_READY_LED);
}

/*This will need to change for Edwards Configuration*/
void M23_ClearFaultLED(int WhoClear)
{
    int   config;

    M23_GetConfig(&config);

    if(WhoClear == 0)
    {
        OtherSet = FALSE;
    }
    else
    {
        Critical = FALSE;
    }

    if( (OtherSet == FALSE) && (Critical == FALSE) )
    {
        if(config == B52_CONFIG){
            stopFaultBlink = 1;
        }else if((config == 1) || (config == 2)  || (config == B1B_CONFIG))
        {
            M23_TurnOffDiscretes(STD_FAULT_LED);
            SSRIC_ClearFault();
        }
        else if(config == 4) //Edwards is now config 4
        {
            M23_WriteDiscretes(STD_FAULT_LED);
        }
        else if(config == LOCKHEED_CONFIG) 
        {
            M23_WriteDiscretes(LOCKHEED_READY);
        }
        else if(config == P3_CONFIG)
        {
            //M23_WriteDiscretes(P3_READY);
            M23_TurnOffDiscretes(P3_FAULT);
        }
        else
        {
            if(config != 3)
            {
                M23_TurnOffDiscretes(FAULT_LED);
                M23_TurnOffDiscretes(FAULT_EM_LED);
                M23_TurnOffDiscretes(FAULT_CL_LED);
            }
        }

        Ready = TRUE;
    }
}

/*This will need to change for Edwards Configuration*/
void M23_SetFaultLED(int WhoSet)
{
    int   config;

    M23_GetConfig(&config);

    if(config == B52_CONFIG){
        StartFaultBlink();
    }else if((config == 1) || (config == 2) || (config == B1B_CONFIG) )
    {
        M23_WriteDiscretes(STD_FAULT_LED);
        SSRIC_SetFault();
    }
    else if(config == 4) //Edwards is now config 4
    {
        M23_TurnOffDiscretes(STD_FAULT_LED);
    }
    else if(config == LOCKHEED_CONFIG) 
    {
        M23_TurnOffDiscretes(LOCKHEED_READY);
    }
    else if(config == P3_CONFIG)
    {
        //M23_TurnOffDiscretes(P3_READY);
        M23_WriteDiscretes(P3_FAULT);
    }
    else
    {
        if(config != 3)
        {
            M23_WriteDiscretes(FAULT_LED);
            M23_WriteDiscretes(FAULT_EM_LED);
            M23_WriteDiscretes(FAULT_CL_LED);
        }
    }

    Ready = FALSE;

    if(WhoSet == 0)
    {
        OtherSet = TRUE;
    }
    else
    {
        Critical = TRUE;
    }

}


void M23_GetFaultLED(UINT8 *IsReady)
{
    *IsReady = Ready; 
}


void M23_SetReady()
{
    M23_WriteDiscretes(P3_READY);
}



void M23_ReadDiscretes(UINT8 *NewSwitches)
{
    UINT32 Switches;

    /*Read The current Switches*/
    Switches = 0x0;
    Switches = M23_ReadControllerCSR(CONTROLLER_DISCRETE_CSR);

    *NewSwitches = ~((Switches>>8) & 0xFF);
}

void M23_SetExternalDiscrete()
{
    UINT32  Switches = 0x100FF;
    UINT32  CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_DISCRETE_CSR);
    CSR |= Switches;
    M23_WriteControllerCSR(CONTROLLER_DISCRETE_CSR,CSR);

}

void M23_StartLEDBlink()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_DISCRETE_CSR);
    CSR &= ~CONTROLLER_DISCRETE_EXTERNAL;
    M23_WriteControllerCSR(CONTROLLER_DISCRETE_CSR,CSR);
}

void M23_StopLEDBlink()
{
    UINT32 CSR;

    CSR = M23_ReadControllerCSR(CONTROLLER_DISCRETE_CSR);
    CSR |= CONTROLLER_DISCRETE_EXTERNAL;
    M23_WriteControllerCSR(CONTROLLER_DISCRETE_CSR,CSR | CONTROLLER_DISCRETE_BLINK_ON);
}

void M23_SetupDiscretes()
{
    UINT32  Switches;
    UINT32  CSR;
    int     config;
    int     debug;

    M23_GetDebugValue(&debug);

    Reverse = 0;
    InRecord = 0;

    Ready    = FALSE;
    OtherSet = FALSE;
    Critical = FALSE;

    /*Set the External Discrete Path*/
    Switches = CONTROLLER_DISCRETE_EXTERNAL;
    M23_WriteControllerCSR(CONTROLLER_DISCRETE_CSR,Switches);

    sleep(1);
    Switches = 0x100FF | CONTROLLER_DISCRETE_BLINK_ON;
    M23_WriteControllerCSR(CONTROLLER_DISCRETE_CSR,Switches);
    LEDS = 0xFF;

    /*Read The current Switches*/
    CSR = M23_ReadControllerCSR(CONTROLLER_DISCRETE_CSR);
    CurrentSwitches = (CSR >> 8) & 0xFF;

    M23_GetConfig(&config);
    if((config == 1) || (config == 2) || (config == B52_CONFIG) )
    {
        Tail= M23_ReadControllerCSR(CONTROLLER_CARTRIDGE_TAIL_CSR);
        usleep(1000);
        Tail= M23_ReadControllerCSR(CONTROLLER_CARTRIDGE_TAIL_CSR);

        Tail = ( (Tail >> 2) & 0x1f); //Use bits 6-2
        M23_SetTailIndex(Tail);
    }
    else if(config == LOCKHEED_CONFIG)
    {
 
        SelectChanged = FALSE;
    }

    if(config == 0)
    {
        PreviousSwitches = 0xFF;
    }
    else
    {
        if(config == LOCKHEED_CONFIG)
        {
            PreviousSwitches = 0xC6;
        }
        else
        {
            PreviousSwitches = 0xFE;
        }
    }
    
}

void M23_MonitorDiscretes()
{
    int           status;
    int           ret;
    int           state;
    int           config;
    int           debug;
    UINT8         NewSwitches;
    UINT8         ChangedSwitches;
    UINT8         SwitchedOn;
    UINT8         SwitchedOff;

    /*These are used for Lockheed*/
    int           video;
    static int    start = 0;
    static UINT32 NoSelectCount = 0;          
    static UINT32 CurrentCount = 0;     

    M23_GetDebugValue(&debug);
    M23_ReadDiscretes(&NewSwitches);

    CurrentSwitches = NewSwitches;

    ChangedSwitches = PreviousSwitches ^ CurrentSwitches;

    M23_GetConfig(&config);
    
    if(ChangedSwitches)
    {

        /*Determine Which Switch was Turned ON*/
        SwitchedOn  = (ChangedSwitches & CurrentSwitches);

        /*Determine Which Switch was Turned Off*/
        SwitchedOff = (ChangedSwitches & PreviousSwitches);
        M23_RecorderState(&state);

        if((config == 1) || (config == 2) || (config == 3) || (config == 4) ||(config == B1B_CONFIG) || (config == B52_CONFIG) )  //Eglin V1 Configuration, AirBus, Edwards and B1B
        {
            switch (SwitchedOn)
            {
                case STD_RECORD_SWITCH:

                    if(state != STATUS_RECORD)
                    {
                        ret = DiskIsConnected();
                        if( (ret != -1) && (ret != 0) )
                        {
                            if(AutoRecord == 0)
                            {
		                if( (config == 1) || (config == 3) || (config == 4) || (config == B1B_CONFIG) || (config == B52_CONFIG) )
			        {
                                    usleep(250000);
                                    if(RecordSwitchPosition())
                                    { 
                                        status = Record();

                                        if(status == NO_ERROR)
                                        {
                                            if(EventTable[3].CalculexEvent)
                                            {
                                                EventRecordSet(3);
                                            }
                                            else
                                            {
                                                m23_WriteSavedEvents();
                                            }

                                            InRecord = 1;

                                            CommandFrom = FROM_DISCRETE;
                                        }
                                    }
			        }
                            }
                        }
                    }

                    break;
                case STD_BIT_SWITCH:
                    BuiltInTest();
                    break;
                case STD_EVENT_SWITCH:
                    EventSet(1);
                    M23_MP_SetAudioTone();
                    break;
                default:
                    break;
            }
            if(SwitchedOn & STD_ENABLE_SWITCH)
            {
                if(CurrentSwitches & STD_ERASE_SWITCH)
                {
                    ret = DiskIsConnected();
                    if( (ret != -1) && (ret != 0) )
                    {

                        status = Erase();

                        if(status == NO_ERROR)
                        {
                            M23_WriteDiscretes(STD_ERASE_LED);
                            SSRIC_SetEraseStatus();

                            if(LoadedTmatsFromCartridge == 1)
                            {           
                               // RecordTmats = 1;
                                //RecordTmatsFile();
                                RecordTmats = 0;
                                SSRIC_ClearEraseStatus( );
                                M23_ClearEraseLED();
                            }


                            sleep(1);
                            SSRIC_ClearEraseStatus( );
                            M23_TurnOffDiscretes(STD_ERASE_LED);
                            sleep(1);
                            M23_WriteDiscretes(STD_ERASE_LED);
                            SSRIC_SetEraseStatus();
                            M23_SetRecorderState(STATUS_IDLE);
                        }

                    }
                }
                else if(CurrentSwitches & STD_DECLASS_SWITCH)
                {
                    ret = DiskIsConnected();
                    if( (ret != -1) && (ret != 0) )
                    {
                        status = Declassify();
                        if(status == NO_ERROR)
                        {
                            M23_WriteDiscretes(STD_ERASE_LED);
                        }
                    }
                }
            }
            else if(SwitchedOn & STD_ERASE_SWITCH)
            {
                if(CurrentSwitches & STD_ENABLE_SWITCH)
                {
                    ret = DiskIsConnected();
                    if( (ret != -1) && (ret != 0) )
                    {

                        status = Erase();

                        if(status == NO_ERROR)
                        {
                            M23_WriteDiscretes(STD_ERASE_LED);
                            SSRIC_SetEraseStatus();
                            sleep(1);
                            SSRIC_ClearEraseStatus( );
                            M23_TurnOffDiscretes(STD_ERASE_LED);
                            sleep(1);
                            M23_WriteDiscretes(STD_ERASE_LED);
                            SSRIC_SetEraseStatus();

                            if(LoadedTmatsFromCartridge == 1)
                            {           
                                //RecordTmats = 1;
                                //RecordTmatsFile();
                                RecordTmats = 0;
                                SSRIC_ClearEraseStatus( );
                                M23_ClearEraseLED();
                            }

                            M23_SetRecorderState(STATUS_IDLE);
                        }
                    }
                }

            }
            else if(SwitchedOn & STD_DECLASS_SWITCH)
            {
                if(CurrentSwitches & STD_ENABLE_SWITCH)
                {
                   
                    ret = DiskIsConnected();
                    if( (ret != -1) && (ret != 0) )
                    {
                        status = Declassify();
                        if(status == NO_ERROR)
                        {
                            M23_WriteDiscretes(STD_ERASE_LED);
                        }
                    }
                }
            }
            switch (SwitchedOff)
            {
                case STD_RECORD_SWITCH:
                    if( CommandFrom == FROM_DISCRETE)
                    {
                        M23_RecorderState(&state);
                        if(state == STATUS_RECORD)
                        {
                            if(EventTable[2].CalculexEvent)
                            {
                                EventSet(2);
                            }

                            if(AutoRecord == 0)
                            {
		                if( (config == 1) || (config == 3) || (config == 4) || (config == B1B_CONFIG) || (config == B52_CONFIG) )
		                {
                                    status = Stop(0);
                                    if(status == NO_ERROR)
                                    {
                                        CommandFrom = NO_COMMAND;
                                        InRecord = 0;
                                    }
			        }
                            }
                        }
                        else
                        {
                            CommandFrom = NO_COMMAND;
                        }
                    }
                    break;
                case STD_BIT_SWITCH:
                    break;
                case STD_ENABLE_SWITCH:
                    break;
                case STD_ERASE_SWITCH:
                    break;
                case STD_DECLASS_SWITCH:
                    break;
                case STD_EVENT_SWITCH:
                    M23_MP_ClearAudioTone();
                    break;
                default:
                    break;
            }


        }
        else if(config == 0)  //TRD V2 Configuration
        {

            switch (SwitchedOn)
            {
                case RECORD_SWITCH:
                    ret = DiskIsConnected();
                    if( (ret != -1) && (ret != 0) )
                    {
                        if(AutoRecord == 0)
                        {
                            usleep(250000);
                            if(RecordSwitchPosition())
                            {
                                status = Record();

                                if( status == NO_ERROR)
                                {

                                    CommandFrom = FROM_DISCRETE;
                                    InRecord = 1;
                                }
                            }
                        }
                    }
                    break; 
                case BIT_SWITCH:

                    BuiltInTest();

                    break; 
                case EVENT_SWITCH:
                    EventSet(1);
                    M23_MP_SetAudioTone();
                    M23_ReverseVideo();
                    Reverse = 1;
                    break; 
                default:
                    break;
            }

            if(state != STATUS_RECORD)
            {
                if(SwitchedOn & ENABLE_SWITCH)
                {
                    if(CurrentSwitches & ERASE_SWITCH)
                    {
                        ret = DiskIsConnected();
                        if( (ret != -1) && (ret != 0) )
                        {
                            status = Erase();
                            if(status == NO_ERROR)
                            {
                                M23_WriteDiscretes(ERASE_LED);
                                if(LoadedTmatsFromCartridge == 1)
                                {               
                                    //RecordTmats = 1;
                                    //RecordTmatsFile();
                                    RecordTmats = 0;
                                    M23_ClearEraseLED();
                                }
                                M23_SetRecorderState(STATUS_IDLE);
                            }
                        }
                    }
                    else if(CurrentSwitches & DECLASS_SWITCH)
                    {
                        ret = DiskIsConnected();
                        if( (ret != -1) && (ret != 0) )
                        {
                            status = Declassify();
                            if(status == NO_ERROR)
                            {
                                M23_WriteDiscretes(DECLASS_LED);
                            }
                        }
                    }
                }
                else if(SwitchedOn & ERASE_SWITCH)
                {
                    if(CurrentSwitches & ENABLE_SWITCH)
                    {
                        ret = DiskIsConnected();
                        if( (ret != -1) && (ret != 0) )
                        {
                            status = Erase();
                            if(status == NO_ERROR)
                            {
                                M23_WriteDiscretes(ERASE_LED);
                                if(LoadedTmatsFromCartridge == 1)
                                {               
                                   // RecordTmats = 1;
                                    //RecordTmatsFile();
                                    RecordTmats = 0;
                                    M23_ClearEraseLED();
                                }

                                M23_SetRecorderState(STATUS_IDLE);
                            }
                        }
                    }

                }
                else if(SwitchedOn & DECLASS_SWITCH)
                {
                    if(CurrentSwitches & ENABLE_SWITCH)
                    {
                        ret = DiskIsConnected();
                        if( (ret != -1) && (ret != 0) )
                        {
                            status = Declassify();
                            if(status == NO_ERROR)
                            {
                                M23_WriteDiscretes(DECLASS_LED);
                            }
                        }
                    }
                }
            }

            switch (SwitchedOff)
            {
                case RECORD_SWITCH:
                    if( (CommandFrom == FROM_DISCRETE) || (CommandFrom == FROM_AUTO) )
                    {  
                        if(AutoRecord == 0)
                        {
                            M23_RecorderState(&state);
                            if( (state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) )
                            {
                                status = Stop(0);

                                if(status == NO_ERROR)
                                {
                                    CommandFrom = NO_COMMAND;
                                    InRecord = 0;
                                }
                            }
                        }
                    }
                    break; 
                case BIT_SWITCH:
                    break; 
                case ENABLE_SWITCH:
                    break; 
                case ERASE_SWITCH:
                    break; 
                case DECLASS_SWITCH:
                    break; 
                case EVENT_SWITCH:
                    M23_MP_ClearAudioTone();
                    break; 
                default:
                    break;
            }
        }
        else if(config == LOCKHEED_CONFIG)
        {
            switch (SwitchedOn)
            {
                case LOCKHEED_RECORD_SW:

                    if(start == 0)
                    {
                        sleep(1);
                    }
                    start = 1;
                    if(state != STATUS_RECORD)
                    {
                        ret = DiskIsConnected();
                        if( (ret != -1) && (ret != 0) )
                        {
                            status = Record();

                            if(status == NO_ERROR)
                            {
                                if(EventTable[3].CalculexEvent)
                                {
                                    EventRecordSet(3);
                                }
                                    

                                CommandFrom = FROM_DISCRETE;
                            }
                        }
                    }

                    break;
                case LOCKHEED_BIT_SW:
                    status = BuiltInTest();
                    if(status == NO_ERROR)
                    {
                        if(EventTable[4].CalculexEvent)
                        {
                            EventSet(4);
                        }
                        M23_SetFaultLED(0);
                    }
                    break;
                case LOCKHEED_EVENT_SW:
                    EventSet(1);
                    break;
                    break;
                case LOCKHEED_VIDEO0_SW:
                case LOCKHEED_VIDEO1_SW:
                case LOCKHEED_VIDEO2_SW:
                    SelectChanged = TRUE;
                    break;

                default:
                    break;
            }

            if(state != STATUS_RECORD)
            {

                if(SwitchedOn & LOCKHEED_ENABLE_SW)
                {
                    if(CurrentSwitches & LOCKHEED_ERASE_SW)
                    {
                        ret = DiskIsConnected();
                        if( (ret != -1) && (ret != 0) )
                        {
                            status = Erase();
                            if(status == NO_ERROR)
                            {
                                M23_WriteDiscretes(LOCKHEED_ERASE);
                                sleep(1);
                                M23_TurnOffDiscretes(LOCKHEED_ERASE);
                                sleep(1);
                                M23_WriteDiscretes(LOCKHEED_ERASE);
                                M23_ClearDataLED();
                                M23_SetRecorderState(STATUS_IDLE);
                            }

                        }
                    }
                }
                else if(SwitchedOn & LOCKHEED_ERASE_SW)
                {
                    if(CurrentSwitches & LOCKHEED_ENABLE_SW)
                    {
                        ret = DiskIsConnected();
                        if( (ret != -1) && (ret != 0) )
                        {

                            status = Erase();
                            if(status == NO_ERROR)
                            {
                                M23_WriteDiscretes(LOCKHEED_ERASE);
                                sleep(1);
                                M23_TurnOffDiscretes(LOCKHEED_ERASE);
                                sleep(1);
                                M23_WriteDiscretes(LOCKHEED_ERASE);
                                M23_ClearDataLED();
                                M23_SetRecorderState(STATUS_IDLE);
                            }
                        }
                    }

                }
            }

            switch (SwitchedOff)
            {
                case LOCKHEED_RECORD_SW:
                    if( (CommandFrom == FROM_DISCRETE) || (CommandFrom == FROM_AUTO) )
                    {
                        M23_RecorderState(&state);
                        if(state == STATUS_RECORD)
                        {
                            if(EventTable[2].CalculexEvent)
                            {
                                EventSet(2);
                            }
                            Stop(0);
                           CommandFrom = NO_COMMAND;
                        }
                        else
                        {
                            CommandFrom = NO_COMMAND;
                        }
                    }
                    break;
                case LOCKHEED_VIDEO0_SW:
                case LOCKHEED_VIDEO1_SW:
                case LOCKHEED_VIDEO2_SW:
                    SelectChanged = TRUE;
                    break;

                default:
                    break;
            }


        }
        else if(config == P3_CONFIG)  //P3 Configuration
        {

            switch (SwitchedOn)
            {
                case P3_RECORD_SW:
                    ret = DiskIsConnected();
                    if( (ret != -1) && (ret != 0) )
                    {
                        if(AutoRecord == 0)
                        {
                            usleep(250000);
                            if(RecordSwitchPosition())
                            {
                                status = Record();

                                if( status == NO_ERROR)
                                {
                                    if(EventTable[3].CalculexEvent)
                                    {
                                        EventRecordSet(3);
                                    }
                                    else
                                    {

                                        m23_WriteSavedEvents();
                                    }

                                    CommandFrom = FROM_DISCRETE;
                                    InRecord = 1;
                                }
                            }
                        }
                    }
                    break;
                case P3_BIT_SW:
                    if(EventTable[4].CalculexEvent)
                    {
                        EventSet(4);
                    }
                    BuiltInTest();

                    break;
                case P3_EVENT_SW:
                    EventSet(1);
                    M23_MP_SetAudioTone();
                    M23_ReverseVideo();
                    Reverse = 1;
                    break;
                default:
                    break;
            }
            if(SwitchedOn & P3_ENABLE_SW)
            {
                if(CurrentSwitches & P3_ERASE_SW)
                {
                    ret = DiskIsConnected();
                    if( (ret != -1) && (ret != 0) )
                    {
                        status = Erase();
                        if(status == NO_ERROR)
                        {
                            M23_WriteDiscretes(P3_ERASE);

                            if(LoadedTmatsFromCartridge == 1)
                            {
                                RecordTmats = 0;
                                M23_ClearEraseLED();
                            }
                        }
                    }
                }
            }
            else if(SwitchedOn & P3_ERASE)
            {
                if(CurrentSwitches & P3_ENABLE_SW)
                {
                    ret = DiskIsConnected();
                    if( (ret != -1) && (ret != 0) )
                    {
                        status = Erase();
                        if(status == NO_ERROR)
                        {
                            M23_WriteDiscretes(P3_ERASE);
                            if(LoadedTmatsFromCartridge == 1)
                            {
                                RecordTmats = 0;
                                M23_ClearEraseLED();
                            }
                        }
                    }
                }

            }

            switch (SwitchedOff)
            {
                case P3_RECORD_SW:
                    if( (CommandFrom == FROM_DISCRETE) || (CommandFrom == FROM_AUTO) )
                    {
                        if(AutoRecord == 0)
                        {
                            M23_RecorderState(&state);
                            if( (state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) )
                            {
                                if(EventTable[3].CalculexEvent)
                                {
                                    EventSet(3);
                                }

                                status = Stop(0);

                                if(status == NO_ERROR)
                                {
                                    CommandFrom = NO_COMMAND;
                                    InRecord = 0;
                                }
                            }
                        }
                    }
                    break;
                case P3_BIT_SW:
                    break;
                case P3_ENABLE_SW:
                    break;
                case P3_ERASE_SW:
                    break;
                case P3_EVENT_SW:
                    M23_MP_ClearAudioTone();
                    break;
                default:
                    break;
            }
        }

    }

    if(SelectChanged == TRUE)
    {
        if(CurrentCount > 99)
        {
            CurrentCount = 0;
            NoSelectCount = 0;
            SelectChanged = FALSE;
            video = (CurrentSwitches >> 3) & 0x7;
            if(debug)
                printf("Setting Live video to %d, 0x%x\r\n",video+1,CurrentSwitches);
            M23_SetLiveVideo(video+1);
        }
        else
        {
            CurrentCount++;
        }
    }

    PreviousSwitches = CurrentSwitches;

    start = 1;
}


void M23_DiscreteProcessor()
{
    int loops = 0;

    while(1)
    {
        if(Discrete_Disable == 0)
        {
            M23_MonitorDiscretes();

            if( Reverse == 1)
            {
                if(loops > 99)
                {
                    M23_ReverseVideo();
                    Reverse = 0;
                    loops = 0;
                }
                else
                {
                    loops++;
                }
            }

            usleep(100);
        }
        else
        {
            sleep(2);
        }
    }
}

void M23_StartDiscreteThread()
{
    int status;
    int debug;

    M23_GetDebugValue(&debug);

    stopFaultBlink = 1;
    stopBingoBlink = 1;

    status = pthread_create(&DiscreteThread, NULL, (void *)M23_DiscreteProcessor, NULL);
    if(debug)
    {
        if(status == 0)
        {
            printf("Discrete Thread Created Successfully\r\n");
        }
        else
        {
            perror("Discrete Thread Create\r\n");
        }
    }
}


int RecordSwitchPosition()
{
    UINT8 NewSwitches;
    int   RecordON = 0;
    int   config;
    int   debug;
    int   video;

    static int first = 0;

    UINT32 CSR;

    M23_GetDebugValue(&debug);
    M23_GetConfig(&config);

    M23_ReadDiscretes(&NewSwitches);

    if((config == 1) || (config == 2) || (config == 3) || (config == 4) || (config == B1B_CONFIG)  || (config == B52_CONFIG) )  //Eglin V1 Configuration and Airbus and Edwards and B1B
    {
       if(NewSwitches & STD_RECORD_SWITCH)
       {
           RecordON = 1;
       } 
    }
    else if( config == 0)
    {
       if(NewSwitches & RECORD_SWITCH)
       {
           RecordON = 1;
       } 
    }
    else if( config == P3_CONFIG)
    {
       if(NewSwitches & P3_RECORD_SW)
       {
           RecordON = 1;
       } 
    }
    else if(config == LOCKHEED_CONFIG)
    {
        M23_ReadDiscretes(&NewSwitches);

       if(NewSwitches & LOCKHEED_RECORD_SW)
       {
           RecordON = 1;
       } 

       if(first == 0)
       {
            video = (NewSwitches >> 3) & 0x7;

            CSR = M23_ReadControllerCSR(CONTROLLER_PLAYBACK_CONTROL);
            M23_WriteControllerCSR(CONTROLLER_PLAYBACK_CONTROL,CSR | CONTROLLER_UPSTREAM_DATA |CONTROLLER_UPSTREAM_CLOCK);

            if(debug)
                printf("Setting Live video to %d 0x%x\r\n",video+1,NewSwitches);

            M23_SetLiveVideo(video+1);

            first = 1;
        }
    }

    return(RecordON);
}


void M23_BlinkBingo()
{
    int blink = 0;
    while(1){

        if(stopBingoBlink == 0){
            if(blink == 0){
                M23_WriteDiscretes(STD_BINGO_LED);
                blink = 1;
            }else{
                M23_TurnOffDiscretes(STD_BINGO_LED);
                blink = 0;
            }
        }else
            break;

        sleep(1);
    }

    M23_TurnOffDiscretes(STD_BINGO_LED);
    pthread_exit(NULL);
}

void M23_BlinkFault()
{
    int blink = 0;
    while(1){

        if(stopFaultBlink == 0){
            if(blink == 0){
                M23_WriteDiscretes(STD_FAULT_LED);
                blink = 1;
            }else{
                M23_TurnOffDiscretes(STD_FAULT_LED);
                blink = 0;
            }
        }else
            break;

        sleep(1);

    }

    M23_TurnOffDiscretes(STD_FAULT_LED);
    pthread_exit(NULL);
}


void StartFaultBlink()
{
    int status;

    if(stopFaultBlink) {
        stopFaultBlink = 0;
        status = pthread_create(&FaultBlinkThread, NULL,(void *) M23_BlinkFault, NULL);
    }
}

void StartBingoBlink()
{
    int status;

    if(stopBingoBlink){
        stopBingoBlink = 0;
        status = pthread_create(&BingoBlinkThread, NULL,(void *) M23_BlinkBingo, NULL);
    }
}

