
#include "M23_Controller.h"
#include "M23_Ssric.h"
#include "M2X_serial.h"
#include "squeue.h"

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>


  //////////////////////////////SSRIC setup///////////////////////////////////////

#define  ACK_CMD_BIT  0x0080
#define  NAK_CMD_BIT  0x8000

#define  SSRIC_TOGGLE_DECLASSIFY    0x01
#define  SSRIC_TOGGLE_ERASE         0x02
#define  SSRIC_TOGGLE_ENABLE        0x04
#define  SSRIC_TOGGLE_IBIT          0x08
#define  SSRIC_TOGGLE_DOWNLOAD      0x10
#define  SSRIC_TOGGLE_CLEAR_ERROR   0x20

#define  SSRIC_ROTARY_MASK          0x0f

#define  SSRIC_OPTO_CMD_RECORD      0x01

typedef struct
{
    UINT8 ToggleSwitch;
    UINT8 RotarySwitch;
    UINT8 OptoInputCommandLow;
    UINT8 OptoInputCommandHigh;
} IP_O_SSRIC_UPDATE_REQ;


#define  SSRIC_AUX_DECLASSIFY_LED     0x01
#define  SSRIC_AUX_ERASE_LED          0x02
#define  SSRIC_AUX_BIT_LED            0x04
#define  SSRIC_AUX_FAULT_LED          0x08
#define  SSRIC_AUX_RECORD_LED         0x10
#define  SSRIC_AUX_COMM_LED           0x20
#define  SSRIC_AUX_RADD_LED           0x40
#define  SSRIC_AUX_DOWNLOAD_ERROR_LED 0x80

#define  SSRIC_TIME_LED             0x01
#define  SSRIC_AUDIO_LED            0x02

#define  SSRIC_FET_STATUS_RECORD    0x01
#define  SSRIC_FET_STATUS_FAULT     0x02

typedef struct
{
    UINT8 DigitLow1;
    UINT8 DigitHigh1;
    UINT8 DigitLow2;
    UINT8 DigitHigh2;

    UINT8 AuxLeds;
    UINT8 M1553Leds;
    UINT8 PcmLeds;
    UINT8 TimeAudioLeds;

    UINT8 FETOutputStatusLow;
    UINT8 FETOutputStatusHigh;
} IP_O_SSRIC_UPDATE_RSP;


static UINT8  mSSRIC_State;
#define  SSRIC_IDLE                 0
#define  SSRIC_PENDING_DECLASSIFY   1
#define  SSRIC_PENDING_ERASE        2
#define  SSRIC_PENDING_IBIT         3
#define  SSRIC_PENDING_RECORD       4
#define  SSRIC_PENDING_DOWNLOAD     5
#define  SSRIC_PENDING_CONFIG       6
#define  SSRIC_DECLASSIFY           7
#define  SSRIC_ERASE                8
#define  SSRIC_IBIT                 9
#define  SSRIC_RECORD              10
#define  SSRIC_DOWNLOAD            11
#define  SSRIC_CONFIG              12
#define  SSRIC_PENDING_RECORD_STOP 13
#define  SSRIC_RESET               14


UINT8  SSRIC_MainCommand;
#define  SSRIC_CMD_NONE             0
#define  SSRIC_CMD_RECORD           1
#define  SSRIC_CMD_STOP             2
#define  SSRIC_CMD_ERASE            3
#define  SSRIC_CMD_DECLASSIFY       4
#define  SSRIC_CMD_IBIT             5
#define  SSRIC_CMD_SELECT_SETUP     6


static BOOL    mSSRIC_EraseTypeSecure;

static BOOL    mSSRIC_EraseSet;
static BOOL    mSSRIC_BitSet;

static UINT32  mSSRIC_PercentUsed;

static UINT32   mSSRIC_FaultCode;
#define   SSRIC_FAULT_CONFIGURATION_INVALID  0x01
#define   SSRIC_FAULT_CANISTER_BAD           0x02
#define   SSRIC_FAULT_PARTITION_FULL         0x04
#define   SSRIC_FAULT_BIT_FAIL               0x08 
#define   SSRIC_FAULT_ERASE_FAIL             0x10 
#define   SSRIC_FAULT_DECLASS_FAIL           0x20 

static UINT8  mDeclassifyStatus;
static UINT8  mEraseStatus;
static UINT8  mBitStatus;
static UINT8  mFaultStatus;
static UINT8  mRecordStatus;
static UINT8  mCommStatus;
static UINT8  mRaddStatus;
static UINT8  mDownloadErrorStatus;
static UINT8  mPcmChannelStatus;
static UINT8  mM1553ChannelStatus;
static UINT8  mTimeCodeChannelStatus;

//static UINT32 mPreviousOptoInputLow;
//static UINT32 mPreviousToggleSwitch;
static UINT8  mPreviousRotarySwitch;

//static UINT8  byResponseDestinationID;
//static UINT8  byResponseDestinationPort;

static UINT8  mDigitLow1, mDigitHigh1;
static UINT8  mDigitLow2, mDigitHigh2;

#define  SSRIC_SEG_A          0x01
#define  SSRIC_SEG_B          0x02
#define  SSRIC_SEG_C          0x04
#define  SSRIC_SEG_D          0x08
#define  SSRIC_SEG_E          0x10
#define  SSRIC_SEG_F          0x20
#define  SSRIC_SEG_G          0x40
#define  SSRIC_SEG_H          0x80
//---  Segments J through P  & Decimal point is high part
#define  SSRIC_SEG_J          0x01
#define  SSRIC_SEG_K          0x02
#define  SSRIC_SEG_L          0x04
#define  SSRIC_SEG_M          0x08
#define  SSRIC_SEG_N          0x10
#define  SSRIC_SEG_P          0x20
#define  SSRIC_SEG_DP         0x40


//---  Use to index into the array SegmentDisplayTable[]
//--- This array contains codes for characters shown
//--- on 14-segment displays.
#define   SSRIC_CHAR_0    0
#define   SSRIC_CHAR_1    1
#define   SSRIC_CHAR_2    2
#define   SSRIC_CHAR_3    3
#define   SSRIC_CHAR_4    4
#define   SSRIC_CHAR_5    5
#define   SSRIC_CHAR_6    6
#define   SSRIC_CHAR_7    7
#define   SSRIC_CHAR_8    8
#define   SSRIC_CHAR_9    9
#define   SSRIC_CHAR_ASTERISK 10
#define   SSRIC_CHAR_MINUS    11
#define   SSRIC_CHAR_CLEAR    12
#define   SSRIC_CHAR_o        13
#define   SSRIC_CHAR_k        14
#define   SSRIC_CHAR_B        15
#define   SSRIC_CHAR_T        16
#define   SSRIC_CHAR_C        17
#define   SSRIC_CHAR_N        18
#define   SSRIC_CHAR_F        19
#define   SSRIC_CHAR_U        20
#define   SSRIC_CHAR_E        21
#define   SSRIC_CHAR_R        22
#define   SSRIC_CHAR_D        23
#define   SSRIC_CHAR_L        24
#define   SSRIC_CHAR_S        25
#define   SSRIC_CHAR_MAX      26

#define SEG_CHAR_0_L  (SSRIC_SEG_A | SSRIC_SEG_B | SSRIC_SEG_C | \
                       SSRIC_SEG_D | SSRIC_SEG_E | SSRIC_SEG_F)
#define SEG_CHAR_0    (SEG_CHAR_0_L)

#define SEG_CHAR_1_L  (SSRIC_SEG_B | SSRIC_SEG_C)
#define SEG_CHAR_1    (SEG_CHAR_1_L)

#define SEG_CHAR_2_L  (SSRIC_SEG_A | SSRIC_SEG_B | SSRIC_SEG_D | \
                       SSRIC_SEG_E)
#define SEG_CHAR_2_H  (SSRIC_SEG_K | SSRIC_SEG_P)
#define SEG_CHAR_2    ((SEG_CHAR_2_H << 8) |  SEG_CHAR_2_L)

#define SEG_CHAR_3_L  (SSRIC_SEG_A | SSRIC_SEG_B | SSRIC_SEG_C | \
                       SSRIC_SEG_D)
#define SEG_CHAR_3_H  (SSRIC_SEG_K | SSRIC_SEG_P)
#define SEG_CHAR_3    ((SEG_CHAR_3_H << 8) |  SEG_CHAR_3_L)


#define SEG_CHAR_4_L  (SSRIC_SEG_B | SSRIC_SEG_C | SSRIC_SEG_F)
#define SEG_CHAR_4_H  (SSRIC_SEG_K | SSRIC_SEG_P)
#define SEG_CHAR_4    ((SEG_CHAR_4_H << 8) |  SEG_CHAR_4_L)

#define SEG_CHAR_5_L  (SSRIC_SEG_A | SSRIC_SEG_C | SSRIC_SEG_D | \
                       SSRIC_SEG_F)
#define SEG_CHAR_5_H  (SSRIC_SEG_K | SSRIC_SEG_P)
#define SEG_CHAR_5    ((SEG_CHAR_5_H << 8) |  SEG_CHAR_5_L)

#define SEG_CHAR_6_L  (SSRIC_SEG_A | SSRIC_SEG_C | SSRIC_SEG_D | \
                       SSRIC_SEG_E | SSRIC_SEG_F)
#define SEG_CHAR_6_H  (SSRIC_SEG_K | SSRIC_SEG_P)
#define SEG_CHAR_6    ((SEG_CHAR_6_H << 8) |  SEG_CHAR_6_L)

#define SEG_CHAR_7_L  (SSRIC_SEG_A | SSRIC_SEG_B | SSRIC_SEG_C )
#define SEG_CHAR_7_H  0
#define SEG_CHAR_7    ((SEG_CHAR_7_H << 8) |  SEG_CHAR_7_L)

#define SEG_CHAR_8_L  (SSRIC_SEG_A | SSRIC_SEG_B | SSRIC_SEG_C | \
                       SSRIC_SEG_D | SSRIC_SEG_E | SSRIC_SEG_F)
#define SEG_CHAR_8_H  (SSRIC_SEG_K | SSRIC_SEG_P)
#define SEG_CHAR_8    ((SEG_CHAR_8_H << 8) |  SEG_CHAR_8_L)

#define SEG_CHAR_9_L  (SSRIC_SEG_A | SSRIC_SEG_B | SSRIC_SEG_C | \
                       SSRIC_SEG_D | SSRIC_SEG_F)
#define SEG_CHAR_9_H  (SSRIC_SEG_K | SSRIC_SEG_P)
#define SEG_CHAR_9    ((SEG_CHAR_9_H << 8) |  SEG_CHAR_9_L)

#define SEG_CHAR_ASTERISK_L  (SSRIC_SEG_G | SSRIC_SEG_H)
#define SEG_CHAR_ASTERISK_H  (SSRIC_SEG_J | SSRIC_SEG_L | SSRIC_SEG_M | \
                              SSRIC_SEG_N)
#define SEG_CHAR_ASTERISK    ((SEG_CHAR_ASTERISK_H << 8) |  SEG_CHAR_ASTERISK_L)

#define SEG_CHAR_MINUS_H  (SSRIC_SEG_K | SSRIC_SEG_P)
#define SEG_CHAR_MINUS    (SEG_CHAR_MINUS_H << 8)

#define  SEG_CLEAR       0

#define SEG_CHAR_B_L  (SSRIC_SEG_A | SSRIC_SEG_B | SSRIC_SEG_C | SSRIC_SEG_D| SSRIC_SEG_H) 
#define SEG_CHAR_B_H  (SSRIC_SEG_K | SSRIC_SEG_M)
#define SEG_CHAR_B    ((SEG_CHAR_B_H << 8) |  SEG_CHAR_B_L)

#define SEG_CHAR_T_L  (SSRIC_SEG_A | SSRIC_SEG_H) 
#define SEG_CHAR_T_H  (SSRIC_SEG_M)
#define SEG_CHAR_T    ((SEG_CHAR_T_H << 8) |  SEG_CHAR_T_L)

#define SEG_CHAR_C_L  (SSRIC_SEG_A | SSRIC_SEG_F| SSRIC_SEG_E| SSRIC_SEG_D) 
#define SEG_CHAR_C_H  0
#define SEG_CHAR_C    ((SEG_CHAR_C_H << 8) |  SEG_CHAR_C_L)

#define SEG_CHAR_N_L  (SSRIC_SEG_E | SSRIC_SEG_F | SSRIC_SEG_C | SSRIC_SEG_B | SSRIC_SEG_G) 
#define SEG_CHAR_N_H  (SSRIC_SEG_L)
#define SEG_CHAR_N    ((SEG_CHAR_N_H << 8) |  SEG_CHAR_N_L)

#define SEG_CHAR_F_L  (SSRIC_SEG_A | SSRIC_SEG_F | SSRIC_SEG_E) 
#define SEG_CHAR_F_H  (SSRIC_SEG_P | SSRIC_SEG_K)
#define SEG_CHAR_F    ((SEG_CHAR_F_H << 8) |  SEG_CHAR_F_L)

#define SEG_CHAR_U_L  (SSRIC_SEG_F | SSRIC_SEG_E | SSRIC_SEG_D | SSRIC_SEG_C | SSRIC_SEG_B) 
#define SEG_CHAR_U_H  0
#define SEG_CHAR_U    ((SEG_CHAR_U_H << 8) |  SEG_CHAR_U_L)

#define SEG_CHAR_E_L  (SSRIC_SEG_A | SSRIC_SEG_F| SSRIC_SEG_E| SSRIC_SEG_D) 
#define SEG_CHAR_E_H  (SSRIC_SEG_P)
#define SEG_CHAR_E    ((SEG_CHAR_E_H << 8) |  SEG_CHAR_E_L)

#define SEG_CHAR_R_L  (SSRIC_SEG_A | SSRIC_SEG_F | SSRIC_SEG_E| SSRIC_SEG_B) 
#define SEG_CHAR_R_H  (SSRIC_SEG_P | SSRIC_SEG_K| SSRIC_SEG_L)
#define SEG_CHAR_R    ((SEG_CHAR_R_H << 8) |  SEG_CHAR_R_L)

#define SEG_CHAR_D_L  (SSRIC_SEG_A | SSRIC_SEG_B | SSRIC_SEG_C | SSRIC_SEG_D| SSRIC_SEG_H) 
#define SEG_CHAR_D_H  (SSRIC_SEG_M)
#define SEG_CHAR_D    ((SEG_CHAR_D_H << 8) |  SEG_CHAR_D_L)

#define SEG_CHAR_L_L  (SSRIC_SEG_F | SSRIC_SEG_E | SSRIC_SEG_D) 
#define SEG_CHAR_L_H  (0)
#define SEG_CHAR_L    ((SEG_CHAR_L_H << 8) |  SEG_CHAR_L_L)

#define SEG_CHAR_o_L  (SSRIC_SEG_C | SSRIC_SEG_D | SSRIC_SEG_E)
#define SEG_CHAR_o_H  (SSRIC_SEG_K | SSRIC_SEG_P)
#define SEG_CHAR_o    ((SEG_CHAR_o_H << 8) |  SEG_CHAR_o_L)

#define SEG_CHAR_k_L  (SSRIC_SEG_E | SSRIC_SEG_F)
#define SEG_CHAR_k_H  (SSRIC_SEG_J | SSRIC_SEG_L | SSRIC_SEG_P)
#define SEG_CHAR_k    ((SEG_CHAR_k_H << 8) |  SEG_CHAR_k_L)

#define SEG_CHAR_S_L  (SSRIC_SEG_A | SSRIC_SEG_F | SSRIC_SEG_C | SSRIC_SEG_D) 
#define SEG_CHAR_S_H  (SSRIC_SEG_K | SSRIC_SEG_P)
#define SEG_CHAR_S    ((SEG_CHAR_S_H << 8) |  SEG_CHAR_S_L)


#define SEG_CHAR_MAX_L  (SSRIC_SEG_G)
#define SEG_CHAR_MAX_H  (SSRIC_SEG_J | SSRIC_SEG_L | SSRIC_SEG_N)
#define SEG_CHAR_MAX    ((SEG_CHAR_MAX_H << 8) |  SEG_CHAR_MAX_L)


static UINT16 SegmentDisplayTable[] = {
	              SEG_CHAR_0
	             ,SEG_CHAR_1
	             ,SEG_CHAR_2
	             ,SEG_CHAR_3
	             ,SEG_CHAR_4
	             ,SEG_CHAR_5
	             ,SEG_CHAR_6
	             ,SEG_CHAR_7
	             ,SEG_CHAR_8
	             ,SEG_CHAR_9
	             ,SEG_CHAR_ASTERISK
	             ,SEG_CHAR_MINUS
 	             ,SEG_CLEAR
	             ,SEG_CHAR_o
	             ,SEG_CHAR_k
	             ,SEG_CHAR_B
	             ,SEG_CHAR_T
	             ,SEG_CHAR_C
	             ,SEG_CHAR_N
	             ,SEG_CHAR_F
	             ,SEG_CHAR_U
	             ,SEG_CHAR_E
	             ,SEG_CHAR_R
	             ,SEG_CHAR_D
	             ,SEG_CHAR_L
	             ,SEG_CHAR_S
                 ,SEG_CHAR_MAX 
};


  ////////////////////////////////////////////////////////////////////////////////


static IP_O_SSRIC_UPDATE_RSP Rsp;
static IP_NATIVE_MSG_HEADER Rsp_hdr;

static IP_NATIVE_MSG_HEADER Req_hdr;
static IP_O_SSRIC_UPDATE_REQ Req;

static UINT8 TmpRcvBuffer[MAX_COMMAND_LENGTH];

int ssric_monitor(void);
int ssric_handle_req(void);
int ssric_recieve_req(void);
int ssric_generate_res(void);

pthread_t SsricThread;


void SSRIC_UpdateBothDigits( UINT8 value,  BOOL left_decimal_point, BOOL right_decimal_point );
void SSRIC_UpdateLeftDigit( UINT8 value_index, BOOL decimal_point ); 
void SSRIC_UpdateRightDigit( UINT8 value_index, BOOL decimal_point ); 

static int RecordUsed;

static int InitialSetup;

static int bitcount ;

int M23_GetInitialSsricSetting()
{
    int status;

    status = ssric_monitor();

    return(status);
}

/*-----------------------------------------------------------------------------*/
void M23_ssric_thread()
{
    int status;
    int ssric;

    while(1)
    {
        M23_GetSsric(&ssric);
        if(ssric == 1)
        {
            status = ssric_monitor();
        }

        usleep(500000);
    }

}

/*-----------------------------------------------------------------------------*/

void M23_StartSsricThread()
{
    int status;
    int debug;

    M23_GetDebugValue(&debug);

    status = pthread_create(&SsricThread, NULL, (void *)M23_ssric_thread, NULL);
    if(debug)
    {
        if(status == 0)
        {
            printf("Ssric Thread Created Successfully\r\n");
        }
        else
        {
            perror("Ssric Thread Create\r\n");
        }
    }

}

int m23_ssric_initialize(void)
{
 
    //---  State set to idle
    mSSRIC_State = SSRIC_IDLE;


    SSRIC_MainCommand = SSRIC_CMD_NONE;

    mSSRIC_PercentUsed = 99;

    mSSRIC_FaultCode = (SSRIC_FAULT_CONFIGURATION_INVALID | SSRIC_FAULT_CANISTER_BAD);
                                  
    mSSRIC_EraseTypeSecure = FALSE;

    
	//---  All leds set to off							  
    mDeclassifyStatus      = 0;
    mEraseStatus           = 0;
    mSSRIC_EraseSet        = 0;
    mSSRIC_BitSet        = 0;
    mBitStatus             = 0;
    mRecordStatus          = 0;
    mFaultStatus           = 0;
    mCommStatus            = 0;
    mRaddStatus            = 0;
    mDownloadErrorStatus   = 0;
    mPcmChannelStatus      = 0;
    mM1553ChannelStatus    = 0;
    mTimeCodeChannelStatus = 0;
//    mAudioChannelStatus = 0;

    RecordUsed = 0;

//    mPreviousOptoInputLow = 0;
//    mPreviousToggleSwitch = 0;
    mPreviousRotarySwitch = 0xFF;
    
//    byResponseDestinationPort = (UINT8)-1;

    InitialSetup = 1;


    return(0);
}

void ssric_clearAll()
{
    mPcmChannelStatus      = 0;
    mM1553ChannelStatus    = 0;
    mTimeCodeChannelStatus = 0;
}

/*-----------------------------------------------------------------------------*/
int ssric_monitor(void)
/*-----------------------------------------------------------------------------*/
{
  if(!ssric_recieve_req())
  {
      ssric_handle_req();
      ssric_generate_res();
      return(1);
  }

   return(0);
}

/*-----------------------------------------------------------------------------*/
int ssric_recieve_req(void)
/*-----------------------------------------------------------------------------*/
{
  int chars_read;



  memset(TmpRcvBuffer,0,sizeof(TmpRcvBuffer));

  chars_read = SerialGetChars(1, &TmpRcvBuffer[0],MAX_COMMAND_LENGTH);
  TmpRcvBuffer[chars_read] = 0;

  if(chars_read == 16 )
  {


     memset(&Req_hdr,0,sizeof(Req_hdr));
     memcpy(&Req_hdr,TmpRcvBuffer+1,sizeof(Req_hdr));

     memset(&Req,0,sizeof(Req));
     memcpy(&Req,TmpRcvBuffer+10,sizeof(Req));

     //***Swap the count and command values in header//***

//     printf("Req_hdr.Count = <%02x>\r\n",Req_hdr.Count);
//     printf("Req.RotarySwitch = <%02x>\r\n",Req.RotarySwitch);
  
     return(0);


  }
  else if((chars_read > 0) && (chars_read != 16))
  {
     //printf("Invalid Packet Size\r\n");   
  }
  else
  {
    if(chars_read < 0 )
    {
      // printf("SerialGetChar: Failed reading\r\n");
    }
    else
    {

    }
  }

  return(1);
}

/*-----------------------------------------------------------------------------*/
int ssric_handle_req(void)
/*-----------------------------------------------------------------------------*/
{
    int   status;
    int   ssric;
    int   setup;
    int   config;
    INT32 recorderStatus = 0;
    static int previous_record = 0;
    static int erasecount = 0;

    M23_RecorderState(&recorderStatus);
    M23_GetConfig(&config);

    if((STATUS_IDLE == recorderStatus) || (STATUS_RECORD == recorderStatus))
    {
        
        //---  Process switch setting changes
        //---
	//---  Check Declassify request
        if ((recorderStatus == STATUS_IDLE) && (Req.ToggleSwitch == (SSRIC_TOGGLE_DECLASSIFY | SSRIC_TOGGLE_ENABLE)) )
	{
//---------printf("DECLASSIFY ISSUED\r\n");
           
           status = Declassify();

           if(status == NO_ERROR)
           {
               SSRIC_UpdateLeftDigit( SSRIC_CHAR_ASTERISK, FALSE ); 
               SSRIC_UpdateRightDigit( SSRIC_CHAR_ASTERISK, FALSE ); 

	       mSSRIC_EraseTypeSecure = TRUE;
               mDeclassifyStatus = TRUE;
               mFaultStatus = FALSE;
           }
	}
        else if ((recorderStatus == STATUS_IDLE) && ( Req.ToggleSwitch == (SSRIC_TOGGLE_ERASE | SSRIC_TOGGLE_ENABLE)) )   //-- Check Erase Request
	{
//----------printf("ERASE ISSUED\r\n");
            Erase();

            SSRIC_UpdateLeftDigit( SSRIC_CHAR_ASTERISK, FALSE ); 
            SSRIC_UpdateRightDigit( SSRIC_CHAR_ASTERISK, FALSE ); 


	    mSSRIC_EraseTypeSecure = FALSE;
            mEraseStatus = TRUE;
            mFaultStatus = FALSE;
            M23_SetEraseLED();
            mSSRIC_EraseSet = TRUE;

            if(LoadedTmatsFromCartridge == 1)
            {
                //RecordTmats = 1;
                //RecordTmatsFile();
                RecordTmats = 0;
                M23_ClearEraseLED();
            }

             M23_SetRecorderState(STATUS_IDLE);
	}
       else if ( (recorderStatus == STATUS_IDLE) && (Req.ToggleSwitch == SSRIC_TOGGLE_IBIT ) ) //-- Check Built-In-Test request
       {
/*---------printf("IBIT ISSUED\r\n");*/  

           BuiltInTest();

           SSRIC_UpdateLeftDigit( SSRIC_CHAR_B, FALSE ); 
           SSRIC_UpdateRightDigit( SSRIC_CHAR_T, FALSE ); 
           bitcount  = 0;
           mBitStatus = TRUE;
           mSSRIC_BitSet = TRUE;
           mFaultStatus = FALSE;

	}
        else if (Req.OptoInputCommandLow == SSRIC_OPTO_CMD_RECORD)
        {
            if(previous_record == 1)
            {
                if(STATUS_IDLE == recorderStatus)
                {
		    if(config != 2)
		    {
                        RecordUsed = 1;
                        status = Record();

                        if(status == NO_ERROR)
                        {
                            mEraseStatus = FALSE;
                            mRecordStatus = TRUE;
                            mFaultStatus = FALSE;
                            M23_ClearEraseLED();
                        }
                        else
                        {
                            mRecordStatus = FALSE;
                            mFaultStatus = TRUE;
                        }
		    }
                }
            }
        }
        else if ((STATUS_RECORD == recorderStatus) && (Req.OptoInputCommandLow == 0) )
	{
            if(previous_record == 0)
            {
                if(RecordUsed)
                {
                    RecordUsed = 0;
		    if(config != 2)
		    {
                        status = Stop(0);
                        if(status == NO_ERROR)
                        {
                            mRecordStatus = FALSE;
                            mFaultStatus = FALSE;
                        }
		    }

                 }
            }
        }
        else
        {
           
            if(STATUS_IDLE == recorderStatus)
            {
               if(mSSRIC_BitSet == FALSE)
               {
                   //---  Update the display
                   //SSRIC_UpdateLeftDigit( SSRIC_CHAR_CLEAR, FALSE ); 
                   //SSRIC_UpdateRightDigit( SSRIC_CHAR_CLEAR, FALSE ); 
               }


               //clear all LED's

               if(mSSRIC_EraseSet == TRUE)
               {
                   if(erasecount == 0 )
                   {
                       mEraseStatus = FALSE;
                       mFaultStatus = FALSE;
                       M23_ClearEraseLED();
                       erasecount++;
                   }
                   else if(erasecount == 2)
                   {
                       mEraseStatus = TRUE;
                       M23_SetEraseLED();
                       mSSRIC_EraseSet = FALSE;
                   }
                   else
                   {
                       erasecount++;
                   }
               }

               if(mSSRIC_BitSet == TRUE)
               {
                   if(bitcount == 1 )
                   {
                       mBitStatus = FALSE;
                       bitcount++;
                   }
                   else if(bitcount == 2)
                   {
                       mBitStatus = TRUE;
                       bitcount++;
                   }
                   else if(bitcount == 3)
                   {
                       mBitStatus = FALSE;
                       mSSRIC_BitSet = FALSE;
                       //SSRIC_UpdateLeftDigit( SSRIC_CHAR_CLEAR, FALSE ); 
                       //SSRIC_UpdateRightDigit( SSRIC_CHAR_CLEAR, FALSE ); 
                   }
                   else
                   {
                       bitcount++;
                   }
               }

                //mRecordStatus = FALSE;
                //mDeclassifyStatus = FALSE;
                //mBitStatus = FALSE;
                //mFaultStatus = FALSE;
            }
        
        }

    
    }
    else if ( Req.ToggleSwitch == SSRIC_TOGGLE_CLEAR_ERROR  ) //---  Check Clear Error request
    { 
        mFaultStatus = FALSE;
        M23_SetRecorderState(STATUS_IDLE);
    }
    else
    {
        if(mSSRIC_EraseSet == TRUE)
        {
            //SSRIC_UpdateLeftDigit( SSRIC_CHAR_CLEAR, FALSE ); 
            //SSRIC_UpdateRightDigit( SSRIC_CHAR_CLEAR, FALSE ); 
            //mEraseStatus = FALSE;
            //M23_ClearEraseLED();
            //mSSRIC_EraseSet = FALSE;
        }
      
    
#if 0
        if(Req.ToggleSwitch != 0 )
        {
            printf("System in invalid mode\r\n");
        }
        else
        {
        
        }
#endif
    
    }

    if(mPreviousRotarySwitch != Req.RotarySwitch)
    {
        M23_GetSsric(&ssric);
        if(ssric == 1)
        {
            if(InitialSetup == 0)
            {
                if(recorderStatus != STATUS_RECORD)
                {
                    SetupViewCurrent(&setup);
                    if(setup != Req.RotarySwitch)
                    {
                        //SetupSet(Req.RotarySwitch,0);
                        M23_Setup(Req.RotarySwitch,0);
                        SaveSetup(Req.RotarySwitch);
                    }
                }
            }
            else
            {
                SaveSetup(Req.RotarySwitch);
                InitialSetup = 0;
            }
        }
        mPreviousRotarySwitch = Req.RotarySwitch;
    }

    if (Req.OptoInputCommandLow == 0)
    {
        previous_record = 0;
    } else if (Req.OptoInputCommandLow == SSRIC_OPTO_CMD_RECORD)
    {
        previous_record = 1;
    }

    return(0);
}

/*-----------------------------------------------------------------------------*/
int ssric_generate_res(void)
/*-----------------------------------------------------------------------------*/
{

UINT8 TmpXmtBuffer[MAX_COMMAND_LENGTH];
UINT8 chksum=0;;
int   i = 0;  

  memset(TmpXmtBuffer,0,sizeof(TmpXmtBuffer));

#if(1)
  memset(&Rsp_hdr,0,sizeof(Rsp_hdr));

  // send to the SSRIC 
  Rsp_hdr.Dest = 0x20;
  Rsp_hdr.Src = 0x40;
  Rsp_hdr.MBusAddr = 0x00;
  Rsp_hdr.Port = 0x00;
  Rsp_hdr.Command =  0x21D5;//**ACK_CMD_BIT;
  Rsp_hdr.Count = sizeof( IP_O_SSRIC_UPDATE_RSP );


  Rsp.DigitLow1  = mDigitLow1;
  Rsp.DigitHigh1 = mDigitHigh1;

  Rsp.DigitLow2  = mDigitLow2;
  Rsp.DigitHigh2 = mDigitHigh2;

  Rsp.AuxLeds  = (mDeclassifyStatus ? SSRIC_AUX_DECLASSIFY_LED : 0);
  Rsp.AuxLeds |= (mEraseStatus ? SSRIC_AUX_ERASE_LED : 0);
  Rsp.AuxLeds |= (mBitStatus ? SSRIC_AUX_BIT_LED : 0);
  Rsp.AuxLeds |= (mFaultStatus ? SSRIC_AUX_FAULT_LED : 0);
  Rsp.AuxLeds |= (mRecordStatus ? SSRIC_AUX_RECORD_LED : 0);
  Rsp.AuxLeds |= (mCommStatus ? SSRIC_AUX_COMM_LED : 0);
  Rsp.AuxLeds |= (mRaddStatus ? SSRIC_AUX_RADD_LED : 0);
  Rsp.AuxLeds |= (mDownloadErrorStatus ? SSRIC_AUX_DOWNLOAD_ERROR_LED : 0);


  Rsp.PcmLeds = mPcmChannelStatus;
  Rsp.M1553Leds = mM1553ChannelStatus;
  Rsp.TimeAudioLeds =  (mTimeCodeChannelStatus ? SSRIC_TIME_LED : 0);
  Rsp.TimeAudioLeds |= 0;//(mAudioChannelStatus ? SSRIC_AUDIO_LED : 0);


  Rsp.FETOutputStatusLow  = (mRecordStatus ? SSRIC_FET_STATUS_RECORD : 0);
  Rsp.FETOutputStatusLow |= (mFaultStatus ? SSRIC_FET_STATUS_FAULT : 0);

  Rsp.FETOutputStatusHigh = 0;

  TmpXmtBuffer[0] = 0x02;
  TmpXmtBuffer[1] = 0x20;
  TmpXmtBuffer[2] = 0x40;
  TmpXmtBuffer[3] = 0x00;
  TmpXmtBuffer[4] = 0x00;
  TmpXmtBuffer[5] = 0x21;
  TmpXmtBuffer[6] = 0xD5;
  TmpXmtBuffer[7] = 0x00;
  TmpXmtBuffer[8] = 0x0A;
  TmpXmtBuffer[9] = 0x60;


  TmpXmtBuffer[10] = Rsp.DigitLow1;
  TmpXmtBuffer[11] = Rsp.DigitHigh1;
  TmpXmtBuffer[12] = Rsp.DigitLow2;
  TmpXmtBuffer[13] = Rsp.DigitHigh2;
  TmpXmtBuffer[14] = Rsp.AuxLeds;
  TmpXmtBuffer[15] = Rsp.M1553Leds;
  TmpXmtBuffer[16] = Rsp.PcmLeds;
  TmpXmtBuffer[17] = Rsp.TimeAudioLeds;
  TmpXmtBuffer[18] = Rsp.FETOutputStatusLow;
  TmpXmtBuffer[19] = Rsp.FETOutputStatusHigh;

  for(i = 10 ; i < 20 ; i++)
  {
     chksum = (UINT8)(chksum + TmpXmtBuffer[i]);
  }

  TmpXmtBuffer[20] = chksum;

  TmpXmtBuffer[21] = 0x03;

#endif

                    /*-----TEST PACKET-------

                      TmpXmtBuffer[0] = 0x02;
                      TmpXmtBuffer[1] = 0x20;
                      TmpXmtBuffer[2] = 0x40;
                      TmpXmtBuffer[3] = 0x00;
                      TmpXmtBuffer[4] = 0x00;

                      TmpXmtBuffer[5] = 0x21;
                      TmpXmtBuffer[6] = 0xD5;
                      TmpXmtBuffer[7] = 0x00;
                      TmpXmtBuffer[8] = 0x0A;
                      TmpXmtBuffer[9] = 0x60;

                      TmpXmtBuffer[10] = 0;
                      TmpXmtBuffer[11] = 0;
                      TmpXmtBuffer[12] = 0;
                      TmpXmtBuffer[13] = 0;
                      TmpXmtBuffer[14] = 0;
                      TmpXmtBuffer[15] = 0;
                      TmpXmtBuffer[16] = 0;
                      TmpXmtBuffer[17] = 0;
                      TmpXmtBuffer[18] = 0;
                      TmpXmtBuffer[19] = 0;
                      TmpXmtBuffer[20] = 0x0;
                      TmpXmtBuffer[21] = 0x03;

                    --------------------------*/


  SerialPutChars(1,&TmpXmtBuffer[0]);

  return(0);
}
/*-----------------------------------------------------------------------------*/
void SSRIC_UpdateBothDigits( UINT8 value, BOOL left_decimal_point, BOOL right_decimal_point) 
//**********************************************************************************
//
//
// ... is a function that updates both left & right displays.
//
//
//
// Input(s) -
//      value ........... Binary digit from 0 - 99 inclusive.  Invalid values will be
//                        displayed as asterisks. 
//      decimal_point ... Flag indicating whether the decimal point is turned on.
//
//
// Output(s) - None
//
//
//**********************************************************************************
{
  UINT8  digit_left, digit_right;  

   
   if ( value > 99 )
   {
     //---  Value to large so display two asterisks 
     digit_left = (UINT8)SEG_CHAR_ASTERISK;
     digit_right = (UINT8)SEG_CHAR_ASTERISK;
   }
   else
   if ( value > 9 )
   {
     digit_left = value/10;
     digit_right = value - (digit_left * 10 );
   }
   else
   {
     digit_left = SSRIC_CHAR_CLEAR;
     digit_right = value;
   }


  //---  digit_left & digit_right are used as an index.
  SSRIC_UpdateLeftDigit( digit_left, left_decimal_point);
  SSRIC_UpdateRightDigit( digit_right, right_decimal_point);
  

}  



void SSRIC_UpdateLeftDigit( UINT8 value_index, BOOL decimal_point ) 
//**********************************************************************************
//
//
// ... is a function updates the left display.
//
//
//
// Input(s) -
//      value_index . ... Index for code table to display characters . 
//      decimal_point ... Flag indicating whether the decimal point is turned on.
//
//
// Output(s) - None
//
//
//**********************************************************************************
{


   if ( value_index > SSRIC_CHAR_MAX )
   {
	   value_index = SSRIC_CHAR_MAX;
	   decimal_point = TRUE;
   }
   
   mDigitLow1  = SegmentDisplayTable[value_index]  & 0xff;
   mDigitHigh1 = SegmentDisplayTable[value_index] >> 8;

   if ( decimal_point )
  	mDigitHigh1 |= SSRIC_SEG_DP;
}  


void SSRIC_UpdateRightDigit( UINT8 value_index, BOOL decimal_point ) 
//**********************************************************************************
//
//
// ... is a function updates the right display.
//
//
//
// Input(s) -
//      value_index . ... Index for code table to display characters . 
//      decimal_point ... Flag indicating whether the decimal point is turned on.
//
//
// Output(s) - None
//
//
//**********************************************************************************
{


    if ( value_index > SSRIC_CHAR_MAX )
	{
	  value_index = SSRIC_CHAR_MAX;
      decimal_point = TRUE;
	}   

   mDigitLow2  = SegmentDisplayTable[value_index]  & 0xff;
   mDigitHigh2 = SegmentDisplayTable[value_index] >> 8;

   if ( decimal_point )
  	mDigitHigh2 |= SSRIC_SEG_DP;
}  

void SSRIC_SetFault(void)
{
    mFaultStatus = TRUE;
}

void SSRIC_ClearFault(void)
{
    mFaultStatus = FALSE;
}

void SSRIC_SetTimeStatus(void) 
{
    mTimeCodeChannelStatus = TRUE;
}
void SSRIC_ClearTimeStatus(void ) 
{
    mTimeCodeChannelStatus = FALSE;
}

void SSRIC_Set1553Status(UINT8 status) 
{
    mM1553ChannelStatus |= status;
}
void SSRIC_Clear1553Status(UINT8 status ) 
{
    mM1553ChannelStatus &= ~status;
}

void SSRIC_SetPCMStatus(UINT8 status) 
{
    mPcmChannelStatus |= status;
}
void SSRIC_ClearPCMStatus(UINT8 status ) 
{
    mPcmChannelStatus &= ~status;
}

void SSRIC_SetVideoStatus(UINT8 status) 
{
    mPcmChannelStatus |= status;
}
void SSRIC_ClearVideoStatus(UINT8 status ) 
{
    mPcmChannelStatus &= ~status;
}

void SSRIC_ClearEraseStatus( ) 
{
    mEraseStatus = FALSE;
    mFaultStatus = FALSE;
}

void SSRIC_SetEraseStatus( ) 
{
    mEraseStatus = TRUE;
    mFaultStatus = FALSE;
}

void SSRIC_SetBitStatus( ) 
{
    SSRIC_UpdateLeftDigit( SSRIC_CHAR_B, FALSE ); 
    SSRIC_UpdateRightDigit( SSRIC_CHAR_T, FALSE ); 

    mBitStatus = TRUE;
    mSSRIC_BitSet = TRUE;
    bitcount  = 0;
}

void SSRIC_ClearBitStatus( ) 
{
    mBitStatus = TRUE;
}

void SSRIC_SetRecordStatus()
{
    mRecordStatus = TRUE;
}

void SSRIC_ClearRecordStatus()
{
    mRecordStatus = FALSE;
}
