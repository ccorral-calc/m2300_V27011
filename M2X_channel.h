/***********************************************************************
 *
 *     Copyright (c) CALCULEX, Inc., 2004
 *
 ***********************************************************************
 *
 *   Filename :   m21_channel.h
 *   Author   :   bdu
 *   Revision :   1.00
 *   Date     :   June 7, 2003
 *   Product  :   Header file for M2100
 *
 *   Revision :
 *
 *   Description:   Header file defining M2100 channel structures
 *
 ***********************************************************************/

#ifndef _M2X_CHANNEL_H_
#define _M2X_CHANNEL_H_


#define MAX_CHANNAME                  32
#define MAX_STRING                   180 
//#define MAX_STRING                  64  
#define MAX_CHANNELS                  40
#define TOTAL_1553_CHANNEL            40
#define TOTAL_VIDEO_CHANNEL           20
#define TOTAL_PCM_CHANNEL             40
#define TOTAL_VOICE_CHANNEL            2 
#define TOTAL_UART_CHANNEL            40
#define TOTAL_DM_CHANNEL              40

#define MAX_PCM_SOURCE_LENGTH               4
#define MAX_PCM_CODE_LENGTH                 6
#define MAX_PCM_POLARITY_LENGTH             2
#define MAX_PCM_PACKING_OPTION_LENGTH       3
#define MAX_PCM_WTO_FIELD_LENGTH            2
#define MAX_PCM_IFH_FIELD_LENGTH            3
#define MAX_PCM_SYNC_PATTERN_LENGTH         33
#define MAX_PCM_SYNC_MASK_LENGTH            33
#define MAX_PCM_IDCM_LENGTH                 33

#define M1553_RT_HOST_CONTROL               0x0
#define M1553_RT_NONE                       0xFF

//typedef unsigned long UINT32;
//typedef unsigned short UINT16;
//typedef unsigned char UINT8;

#define M1553_PACKET_METHOD_AUTO      0
#define M1553_PACKET_METHOD_SINGLE    1

#define INDEX_CHANNEL_TIME            0
#define INDEX_CHANNEL_COUNT           1


typedef struct IndexChannel
{
    int                 IndexType;
    int                 IndexCount;
    int                 IndexTime;
}INDEX_CHANNEL;

#define EXCLUDE 0
#define INCLUDE 1

#define FILTERING_NONE 0xFF
#define EXCLUDE 0
#define INCLUDE 1

typedef struct M1553_Channel
{
    /* generic channel vars */
    int  isEnabled;
    int  m_dataType;
    int  chanNumber;
    char chanName[MAX_CHANNAME];
    int  chanID;
    int  numPackets;
    int  numBuffers;
    int  pktSize;
    int  pktMethod;

    int  included;

    /* 1553 specific vars */
    int  busLoading;
    int  numFilters;
    long int m_WatchWordLockIntervalInMilliseconds;
    long int m_WatchWordPattern;
    long int m_WatchWordMask;
    int  m_ReportErrorTime;
    int  WatchWordEnabled;

    /*TRD Specific parameters*/
    int  NumFiltered;
    int  FilterType;
    int  FilteredRT[100];
    int  FilteredSA[100];
    int  FilteredTR[100];

    char BusLink[32];

    /*Discrete Channel*/
    int  isDiscrete;

} M1553_CHANNEL;


typedef struct Discrete_Channel
{
    int    isEnabled;
    int    m_dataType;              //R-n\DST-m:xxxxx
    char   chanName[MAX_CHANNAME];  //R-n\DSI-m:xxxxx
    int    chanNumber;              //V-n\CLX\CN-m:x
    int    chanID;
    int    DiscreteMask;

} DISCRETE_CHANNEL;


typedef struct VIDEO_Channel
{
    /* generic channel vars */
    int  isEnabled;
    int  m_dataType;
    int  chanNumber;
    char chanName[MAX_CHANNAME];
    int  chanID;
    int  numPackets;
    int  numBuffers;
    int  pktSize;
    int  pktSizeMethod;

    int  included;

    /* video specific vars */
    int  enable_embedded_channel;
    int  internalClkRate;
    int  videoMode;
    int  videoCompression;
    int  videoResolution;
    int  videoFormat;
    int  videoInput;
    int  VideoDelay;
    int  audioMode;
    int  audioRate;
    int  audioGain;
    int  audioSourceL;
    int  audioSourceR;

    int  NumMaps;
    int  Ids[16];

    /*Overlay Variables*/
    int OverlayEnable;
    int OverlaySize;
    int Overlay_X;
    int Overlay_Y;
    int Overlay_Time;
    int Overlay_Format;
    int Overlay_Generate;
    int Overlay_Event_Toggle;

    int VideoOut;
} VIDEO_CHANNEL;

#define TIME_LSB_FIRST  0
#define TIME_MSB_FIRST  1
typedef struct M1553_Timing
{
    int   IsEnabled;
    int   ChannelId;
    int   CommandWord;
    int   NumberOfWords;
    int   WordLocation;
    int   WordOrder;
    int   Format;
}M1553_TIMING;


typedef enum
{
    PCM_422 = 1,
    PCM_TTL
}PCM_INPUT_SOURCE;

typedef enum
{
    PCM_NRZL = 0,
    PCM_RNRZL,
    PCM_BIOL,
    PCM_RNRZL_R,
    PCM_UART,
    PCM_PULSE,
    CONTROLLER_UART
}PCM_CODE;

typedef enum
{
    PCM_NORMAL = 1,
    PCM_INVERTED
}PCM_POLARITY;

typedef enum
{
    PCM_UNPACKED = 1,
    PCM_PACKED,
    PCM_THROUGHPUT
}PCM_PACKING;

typedef enum
{
    PARITY_NONE = 1,
    PARITY_ODD,
    PARITY_EVEN
}PCM_UART_PARITY;

typedef enum
{
    MSB_FIRST = 1,
    LSB_FIRST
}PCM_TRANSFER_ORDER;


#define MAX_FRAME_WORD 1024

#define NO_CRC          0x0
#define MAJOR_CRC       0x1
#define MINOR_CRC       0x2
typedef struct PCM_Channel
{
    /* generic channel vars */
    int                  isEnabled;               //R-n\CHE-m:x
    int                  m_dataType;              //R-n\DST-m:xxxxx
    char                 chanName[MAX_CHANNAME];  //R-n\DSI-m:xxxxx
    char                 LinkName[MAX_CHANNAME];  //P-n\DLN-n:xxxxx
    int                  chanNumber;              //V-n\CLX\CN-m:x
    int                  chanID;                  //R-n\TK1-m:x

    int  included;

    /* PCM specific vars */
    PCM_INPUT_SOURCE     pcmInputSource;         //V-n\CLX\P\D10-m:xxx
    PCM_CODE             pcmCode;                //P-n\D1:xxx
    PCM_POLARITY         pcmDataPolarity;        //P-n\D4:xxx
    PCM_POLARITY         pcmClockPolarity;       //V-n\CLX\P\D9-m:x
    PCM_POLARITY         pcmDataDirection;       //P-n\\D6-m:x
    PCM_PACKING          pcmDataPackingOption;   //R-n\PDP-m:xx
    int                  pcmBitRate;             //P-n\D2:xxxxxx
    PCM_TRANSFER_ORDER   pcmWordTransferOrder;   //P-n\F2:x
    BOOL                 pcmInterFrameHeader;    //V-n\CLX\P\CP4-m:xxx
    int                  pcmInSyncCriteria;      //P-n\SYNC1:xx
    int                  pcmSyncPatternCriteria; //P-n\SYNC2:xx
    int                  pcmNumOfDisagrees;      //P-n\SYNC3:xx
    int                  pcmSyncLength;          //P-n\MF4:xx
    long int             pcmSyncPattern;         //P-n\MF5:xxxxxxxxxxxx...33
    long int             pcmSyncMask;            //V-n\CLX\P\MF6-m:xxxxxx....33
    int                  pcmCommonWordLength;    //P-n\F1:xx
    int                  pcmWordsInMinorFrame;   //P-n\MF1:xx
    int                  pcmBitsInMinorFrame;    //P-n\MF2:xx
    int                  pcmMinorFramesInMajorFrame;  //P-n\MF\N:xx
    int                  pcmSFIDCounterLocation;     //P-n\IDC1-j:x
    int                  pcmSFIDLength;              //P-n\IDC4-j:x
    long int             pcmIdCounterMask;           //V-n\CLX\P\IDCM-m-j:xxxxxxxxx...33
    int                  pcmIdCounterInitialValue;   //P-n\IDC6-j:xx
    BOOL                 pcmBitSync;                 //V-n\CLX\P\CP7
    int                  pcmSFIDBitOffset;            //V-n\CLX\P\IDCB
    long int             pcm_WatchWordPattern;       //R-x\PWWP
    long int             pcm_WatchWordMask;          //R-x\PWWM
    long int             pcm_WatchWordOffset;        //R-x\PWWB
    long int             pcm_WatchWordMinorSFID;     //R-x\PWWF
    int                  pcmDirection;               //V-n\CLX\D1D
    int                  UARTDataBits;               //V-n\CLX\AD1
    int                  UARTBaudRate;               //V-n\CLX\AD2
    int                  OnController;               //V-1\CLX\CUCE
    PCM_UART_PARITY      UARTParity;                 //V-n\CLX\AD3

    /*Filtering Parameters*/
    int                  PCM_FilteringEnabled;             //V-n\CLX\FME;
    int                  PCM_NumFilterd;                   //V-n\CLX\FEC;
    int                  PCM_WordNumber[MAX_FRAME_WORD];   //V-n\CLX\MFW3;
    int                  PCM_CRCEnable;                    //V-n\CLX\CRCE;
    int                  PCM_RecordFilteredSelect;         //V-n\CLX\RFDS;
    int                  PCM_TransmitFilteredSelect;       //V-n\CLX\TFDS;
    int                  PCM_RecordEnable;
} PCM_CHANNEL;

typedef struct UART_Channel
{
    int                  included;
    int                  isEnabled;               //R-n\CHE-m:x
    int                  m_dataType;              //R-n\DST-m:xxxxx
    char                 chanName[MAX_CHANNAME];  //R-n\DSI-m:xxxxx
    int                  chanNumber;              //V-n\CLX\CN-m:x
    int                  chanID;                  //R-n\TK1-m:x
    PCM_CODE             code;
    int                  UARTDataBits;               //V-n\CLX\AD1
    int                  UARTBaudRate;               //V-n\CLX\AD2
    PCM_UART_PARITY      UARTParity;                 //V-n\CLX\AD3
} UART_CHANNEL;


typedef struct TIMECODE_Channel
{
    int     isEnabled;
    int     m_dataType;
    char    chanName[MAX_CHANNAME];  //R-n\DSI-m:xxxxx
    int     chanNumber;              //V-n\CLX\CN-m:x
    int     chanID;                  //R-n\TK1-m:x
    char    Format;                  //R-x\TFMT-n
    int     Source;                  //R-x\TSRC-n
    /*TRD Stuff*/
    M1553_TIMING        m1553_timing;
}TIMECODE_CHANNEL;

typedef struct VOICE_Channel
{
    int     isEnabled;
    int     m_dataType;
    char    chanName[MAX_CHANNAME];  //R-n\DSI-m:xxxxx
    int     chanNumber;              //V-n\CLX\CN-m:x
    int     chanID;                  //R-n\TK1-m:x
    int     Source;                  //V-n\CLX\A\SC
    int     Gain;                   //V-n\CLX\A\PGA
}VOICE_CHANNEL;

typedef struct ETHERNET_Channel
{
   int     isEnabled;
   char    chanName[MAX_CHANNAME];
   int     chanID;
   int     MacLow;
   int     MacMid;
   int     MacHigh;
   int     IPHigh;
   int     IPMid1;
   int     IPMid2;
   int     IPLow;
   int     Radar_IPHigh;
   int     Radar_IPMid1;
   int     Radar_IPMid2;
   int     Radar_IPLow;
   int     port;
   int     CH10;
   char    Mode;
}ETHERNET_CHANNEL;


typedef struct M2300_Channel
{
    char                ProgramName[32];
    int                 VersionNumber;
    int                 numChannels;
    int                 year;
    int                 month;
    int                 day;
    int                 NumConfigured1553;
    int                 NumConfiguredPCM;
    int                 NumConfiguredVideo;
    int                 NumConfiguredVoice;
    int                 NumConfiguredTime;
    int                 NumConfiguredEthernet;
    int                 NumConfiguredUART;
    int                 NumConfiguredDiscrete;
    int                 NumEvents;
    int                 EventsAreEnabled;
    int                 IndexIsEnabled;
    int                 M1553_RT_Control;
    int                 VideoOverlayIsEnabled;
    int                 NumOverlayChannels;
    int                 AVMappingIsEnabled;
    int                 M1553FilteringIsEnabled;
    int                 M1553NumFiltered;
    int                 NumBuses;
    int                 NumberConversions;
    TIMECODE_CHANNEL    timecode_chan;
    VOICE_CHANNEL       voice_chan[TOTAL_VOICE_CHANNEL];
    M1553_CHANNEL       m1553_chan[TOTAL_1553_CHANNEL];
    VIDEO_CHANNEL       video_chan[TOTAL_VIDEO_CHANNEL];
    PCM_CHANNEL         pcm_chan[TOTAL_PCM_CHANNEL];
    ETHERNET_CHANNEL    eth_chan;
    UART_CHANNEL        uart_chan;
    DISCRETE_CHANNEL    dm_chan;
} M23_CHANNEL;



#endif
