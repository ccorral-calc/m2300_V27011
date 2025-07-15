/****************************************************************************************
 *
 *
 *    Copyright (C) 2004 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and 
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
 *    for any purpose except as specifically authorized in writing.
 *
 *    File: M23_Controller.h
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
#ifndef _M23_CONTROLLER_H_
#define _M23_CONTROLLER_H_

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "M23_Typedefs.h"
#include "M2X_time.h"

#define M23_BLOCK_SIZE           512

#define LOCKHEED_CONFIG          5 
#define P3_CONFIG                6
#define B1B_CONFIG               7
#define B52_CONFIG               8

extern pthread_t DataRecoderThread;
extern pthread_t HealthStatusThread;
extern pthread_t DiscreteThread;
extern pthread_t M1553CmdThread;
extern pthread_t ControlThread;
extern pthread_t M1553Time;
extern pthread_t M1553Event;
extern pthread_t CartridgeMonitor;
extern pthread_t EventThread;
extern pthread_t GPSThread;

extern int TimeSource;

extern int RootReceived;

extern int VideoIds[32];

extern int TwoAudio[4];

extern INDEX_CHANNEL indexChannel;

extern int      CH10_TMATS;



/*The Following will be used to determine if we have process TMATS from the Cartridge*/
extern int  TmatsLoaded;
extern int  LoadedTmatsFromCartridge;
extern int  RecordTmats;
extern char TmatsName[40];

extern int  EventChannelInUse;

typedef struct
{
    UINT16    SyncPattern;
    UINT16    ChannelId;
    UINT32    PacketLength;
    UINT32    DataLength;
    UINT8     HeaderVersion;
    UINT8     SeqNumber;
    UINT8     PacketFlags;
    UINT8     DataType;
    UINT32    TimeLower;
    UINT16    TimeUpper;
    UINT16    Checksum;
    UINT32    CSDW;
}PACKETHEADER;

#define PUB_START 1
#define PUB_STOP  2


typedef enum {
    PUB_VIDEO,
    PUB_PCM,
    PUB_1553,
    PUB_DISCRETE,
    PUB_UART,
    PUB_TIME,
    PUB_VOICE,
    PUB_ETH
} PUBLISH_CHAN;

typedef struct Publish_List
{
    char IP[16];
    int  port;
    int  command;
    int  Type[50];
    int  id[50];
}PUBLISH_LIST;

PUBLISH_LIST publish;
PUBLISH_LIST stop_publish;


/*Used to define whether or not to Auto-Record*/
extern int AutoRecord;

#define TIMESOURCE_NONE      0
#define TIMESOURCE_INTERNAL  1
#define TIMESOURCE_EXTERNAL  2
#define TIMESOURCE_CARTRIDGE 3
#define TIMESOURCE_M1553     4

/*The Following are configured using TMATS*/
#define RGB   1
#define NTSC  2

#define RMM_INTERNAL 0
#define RMM_EXTERNAL 1

extern char SystemVersion[7];
extern char M2300Version[13];
extern char ControllerVersion[18];
extern char VideoVersion[13];
extern char M1553Version[13];
extern char PCMVersion[11];

extern int Discrete_Disable;
extern int Internal_Event_Disable;
extern int IEEE1394_Bus_Speed;
extern int IEEE1394_Lockheed_Bus_Speed;
extern int Video_Output_Format;
extern int TimeSet_Command;
extern int TmatsVersion;

extern char   RMM_Version[80];
extern int    RMM_VersionSize;

// Summary of commands implemented:  
// [Taken from the IRIG-106-03, Chapter 10 document that was published separately pending 
//  the republication of the entire Telemetry Standards document.  
//  Refer to http://jcs.mil/RCC/ for current information.]
//
//  .BIT                        Runs all of the built-in-tests
//  .CRITICAL [n [mask]]        Specify and view masks that determine which of the .HEALTH status bits are critical warnings
//  .DECLASSIFY                 Secure erases the recording media
//  .DISMOUNT                   Unloads the recording media
//  .DUB [location]             Same as .PLAY but with internal clock
//  .ERASE                      Erases the recording media
//  .EVENT [text string]        Display event table or add event to event table
//  .FILES                      Displays information about each recorded file.
//  .FIND [value [mode]]        Display current locations or find new play point
//  .HEALTH [feature]           Display detailed status of the recorder system
//  .HELP                       Displays table of "dot" commands
//  .LOOP                       Starts record and play in read-after-write mode
//  .MEDIA                      Displays media usage summary
//  .MOUNT                      Powers and enables the recording media
//  .PLAY [location]            Reproduce recorded data starting at [location] using external clock
//  .RECORD [filename]          Starts a recording at the current end of data
//  .REPLAY [endpoint [mode]]   Same as .SHUTTLE but with internal clock
//  .RESET                      Perform software initiated system reset
//  .SETUP [n]                  Displays or selects 1 of 16 (0-15) pre-programmed data recording formats
//  .SHUTTLE [endpoint [mode]]  Play data repeatedly from current location to the specified endpoint location using external clock
//  .STATUS                     Displays the current system status
//  .STOP [mode]                Stops the current recording, playback, or both
//  .TIME [start-time]          Displays or sets the internal system time
//  .TMATS <mode> [n]           Write, Read, Save, or Get TMATS file



// useful defines
//

typedef enum {
    STATUS_FAIL,
    STATUS_IDLE,
    STATUS_BIT,
    STATUS_ERASE,
    STATUS_DECLASSIFY,
    STATUS_RECORD,
    STATUS_PLAY,
    STATUS_REC_N_PLAY,
    STATUS_FIND,
    STATUS_BUSY,
    STATUS_ERROR,
    STATUS_DECLASS_FAIL,
    STATUS_DECLASS_PASS,
    STATUS_RTEST,
    STATUS_PTEST,
    STATUS_P_R_TEST
} CH10_STATUS;

extern CH10_STATUS  RecorderStatus;

typedef enum {
    ERROR_INVALID_COMMAND,
    ERROR_INVALID_PARAMETER,
    ERROR_INVALID_MODE,
    ERROR_NO_MEDIA,
    ERROR_MEMORY_FULL,
    ERROR_COMMAND_FAILED,
    ERROR_MAX_ERROR,
    ERROR_RECORD_SWITCH,
    NO_ERROR
} CH10_ERROR;



#define MAX_NUM_SETUPS  16
//#define SYSTEM_BLOCKS_RESERVED  2048 // 1 MB
//#define SYSTEM_BLOCKS_RESERVED  131072 
//#define SYSTEM_BLOCKS_RESERVED  32768  //Save 16MB for Root and Nodes
//#define SYSTEM_BLOCKS_RESERVED  65536  //Save 16MB for Root and Nodes
#define SYSTEM_BLOCKS_RESERVED  131072 

#define CONTROL_SWAP_FOUR(x)    (   ( ((x) & 0x000000ff) << 24 ) | \
                                ( ((x) & 0x0000ff00) <<  8 ) | \
                                ( ((x) & 0x00ff0000) >>  8 ) | \
                                ( ((x) & 0xff000000) >> 24 )    )




#define NO_COMMAND       0
#define FROM_COM         1
#define FROM_DISCRETE    2
#define FROM_M1553       3
#define FROM_SSRIC       4
#define FROM_AUTO        5

extern int CommandFrom;

// setup and auxilliary functions
//
int M23_ControllerInitialize( void );
int M23_DisableVideoChannels();
int M23_EnableVideoBoardChannels();

/*Stuff to be used with Declassify*/
extern char StatResponse[80];
extern int  CartState;
extern int  CartPercent;
extern int  DeclassErrors;



// commands
//

int BuiltInTest( void );
int CriticalViewAll( int *number_of_features, UINT32 **masks );
int CriticalView( int feature, UINT32 *mask, char **decoded_masks[32] );
int CriticalSet( int feature, UINT32 mask );
int Declassify( void );
int Dismount( void );
int Erase( void );
int EventSet( int event );
int FindViewCurrent( UINT32 *play_point, UINT32 *record_point );
int FindPlayPointByKeyword( char *keyword, int mode );
int FindPlayPointByValue( int value, int mode );
int HealthViewAll(int *number_of_features, UINT32 **health_status);
int HealthView(int feature, UINT32 *mask, char **decoded_masks[32] ,int index);
int Media( UINT32 *bytes_per_block, UINT32 *blocks_used, UINT32 *blocks_remaining );
int Mount( void );
int Record( void );
int RecordFile( char *file_name );
int Reset( void );
int SetupViewCurrent( int *setup );
int SetupSet( int setup ,int initial);
int Status( int *state, int *non_critical_warnings, int *critical_warnings, int *percent_complete );
int Stop( int mode );
int TimeSet( GSBTime time );
int TmatsWrite( char *TmatsData  );
//int TmatsRead( char *TmatsData );
int TmatsRead();
int TmatsSave( int setup );
int TmatsGet( int setup );


// not implemented, no play capability
//
//  int Dub( UINT32 location );
//  int Loop( void );
//  int PlayCurrent( void );
//  int PlayBlock( UINT32 start_block );
//  int PlayFileOffset( UINT32 file_name, UINT32 start_offset );     
//  int Replay( void );
//  int Shuttle( void );


// implemented by the calling module -- this is the only exception to the rule of
// letting this module do all the work.
//
//  int Help( char **help_on_commands );



#define HOST_CHANNEL_ID  0x0


// non-chapter 10 commands
//
int DciConfigSet( int config );
int DciConfigView( int *p_config );
int SaveSettings( void );


/*Define the CSR Offsets*/

#define CONTROLLER_VIDEO_DEVICE  0
#define VIDEO_DEVICE_1           1
#define VIDEO_DEVICE_2           5
#define VIDEO_DEVICE_3           9
#define VIDEO_DEVICE_4           13

#define CONTROLLER_BAR           0

#define CONTROLLER_CSR_OFFSET   0x80008000

#define CONTROLLER_COMPILE_TIME                         0x8

#define CONTROLLER_CONTROL_CSR                          0x20 
#define CONTROLLER_DAC_REF_VOLTAGE_CSR                  0x24 
#define CONTROLLER_DISCRETE_CSR                         0x30 
#define CONTROLLER_CARTRIDGE_TAIL_CSR                   0x38 
#define CONTROLLER_BTM_CSR                              0x40 
#define CONTROLLER_BTM_SIZE_CSR                         0x44 
#define CONTROLLER_BTM_FILL_CSR                         0x48 
#define CONTROLLER_RT_HOST_ADDRESS_CSR                  0x60
#define CONTROLLER_RT_WRITE_DATA                        0x64 
#define CONTROLLER_RT_READ_DATA                         0x68
#define CONTROLLER_TIME_SYNCH_CONTROL_CSR               0x70 
#define CONTROLLER_MASTER_TIME_ENABLE_CSR               0x74 
#define CONTROLLER_MASTER_TIME_UPPER                    0x80
#define CONTROLLER_MASTER_TIME_LOWER                    0x84

#define ROOT_INDEX_TIME_MSW                             0x78 
#define ROOT_INDEX_TIME_LSW                             0x7C 

#define M1553_REL_TIME_MSW                              0x88 
#define M1553_REL_TIME_LSW                              0x8c 

#define SYSTEM_RESET                                    (1<<16)
#define CONTROLLER_DISCRETE_EXTERNAL                    (1<<16)
#define CONTROLLER_DISCRETE_BLINK_ON                    (1<<20)

#define CONTROLLER_RT_CONTROL                           (1<<0)

#define CONTROLLER_CASCADE_CONTROL_CSR                  0x420 
#define CONTROLLER_CASCADE_CSR                          0x424

#define CONTROLLER_TIMECODE_CSR                         0x820 
#define CONTROLLER_TIMECODE_ID_CSR                      0x824 
#define CONTROLLER_TIMECODE_OFFSET_CSR                  0x830 
#define CONTROLLER_DRIFTCAL_CSR                         0x834 
#define CONTROLLER_TIME_STABILITY                       0x838 
#define CONTROLLER_TIME_MASK_CSR                        0x83C 
#define CONTROLLER_TIMECODE_ADJUST_CSR                  0x840 
#define CONTROLLER_TIMECODE_MARK_CLOCK                  0x844 
#define CONTROLLER_TIMECODE_HOST_REL_LOW                0x848 
#define CONTROLLER_TIMECODE_HOST_REL_HIGH               0x84C 
#define CONTROLLER_TIMECODE_LOAD_BCD_UPPER_CSR          0x850 
#define CONTROLLER_TIMECODE_LOAD_BCD_LOWER_CSR          0x854 
#define CONTROLLER_TIMECODE_BCD_UPPER_CSR               0x858 
#define CONTROLLER_TIMECODE_BCD_LOWER_CSR               0x85C 
#define CONTROLLER_JAM_TIME_REL_LOW                     0x860 
#define CONTROLLER_JAM_TIME_REL_HIGH                    0x864 

#define TIME_JAM_BUSY_BIT                               (1<<31)

#define TIME_USE_GPS                                    (1<<16)
#define TIME_INHIBIT_RESEED                             (1<<24)
#define TIME_INHIBIT_RESYNC                             (1<<28)

#define CONTROLLER_VOICE1_CONTROL_CSR                    0xC20 
#define CONTROLLER_VOICE1_CHANNEL_ID_CSR                 0xC24 
#define CONTROLLER_VOICE1_CHANNEL_PKT_CNT                0xC28 
#define CONTROLLER_VOICE1_CHANNEL_SRC                    0xC2C 

#define CONTROLLER_VOICE2_CONTROL_CSR                    0xC60 
#define CONTROLLER_VOICE2_CHANNEL_ID_CSR                 0xC64 
#define CONTROLLER_VOICE2_CHANNEL_PKT_CNT                0xC68 
#define CONTROLLER_VOICE2_CHANNEL_SRC                    0xC6C 

#define CONTROLLER_VOICE_LEFT                            (1<<0)
#define CONTROLLER_VOICE_PAUSE                           (1<<12)

#define CONTROLLER_HOST_CONTROL_CSR                     0x1020 
#define CONTROLLER_HOST_CHANNEL_ID_CSR                  0x1024 
#define CONTROLLER_HOST_CHANNEL_DATA_LEN_CSR            0x1028 
#define CONTROLLER_HOST_FIFO_CSR                        0x102C 

#define CONTROLLER_INDEX_CHANNEL_GO                     0x1420
#define CONTROLLER_INITIAL_ROOT_OFFSET                  0x1424
#define CONTROLLER_TIME_PER_ROOT                        0x1428
#define CONTROLLER_INDEX_CHAN_ID                        0x1430
#define CONTROLLER_HOST_FIFO                            0x1440
#define CONTROLLER_HOST_FIFO_DATA                       0x1444

#define CONTROLLER_ROOT_FIFO                            0x1448
#define CONTROLLER_ROOT_COUNT                           0x144C

#define ROOT_READ_STROBE                                (1<<0)
#define CONTROLLER_ROOT_COUNT_READY                     (1<<3)
#define CONTROLLER_ROOT_DATA_READY                      (1<<3)
#define CONTROLLER_INDEX_PER_PACKET                     (1<<8)

#define CONTROLLER_EVENT_CHANNEL_CSR                    0x1820
#define CONTROLLER_EVENT_CHANNEL_ID_CSR                 0x1824
#define CONTROLLER_EVENT_CHANNEL_LEN_CSR                0x1828
#define CONTROLLER_EVENT_CHANNEL_WRITE_CSR              0x182C
#define CONTROLLER_EVENT_CHANNEL_CSDW                   0x1838

#define CONTROLLER_EVENT_GO                             (1<<0)
#define CONTROLLER_EVENT_RECORD                         (1<<4)
#define CONTROLLER_EVENT_ENABLE                         (1<<8)

#define EVENT_TYPE                                      0x2
#define CONTROLLER_UART_TYPE                            0x50
#define EVENT_UART_CHANNEL_BUSY                         (1<<7)


#define CONTROLLER_READ_MASTER_TIME                     (1<<31)


#define INDEX_CHANNEL_GO                                (1<<0)
#define INDEX_CHANNEL_READY                             (1<<3)
#define INDEX_CHANNEL_RECORD                            (1<<4)
#define INDEX_CHANNEL_BUSY                              (1<<7)
#define INDEX_ROOT_PACKET_READY                         (1<<20)
#define INDEX_ENABLE_ROOT_PACKET                        (1<<24)
#define INDEX_FIFO_EMPTY                                (1<<31)

#define CLEAR_INDEX_PACKET_COUNT                        (1<<4)



#define CONTROLLER_GLOBAL_GO                            (1<<0)
#define CONTROLLER_GLOBAL_READY                         (1<<3)
#define CONTROLLER_GLOBAL_RECORD                        (1<<4)
#define CONTROLLER_GLOBAL_BUSY                          (1<<7)
#define CONTROLLER_TIME_PACKET_OK                       (1<<8)
#define CONTROLLER_DSP_PACKETS                          (1<<11)
#define CONTROLLER_CAPTURE_STATUS                       (1<<12)

#define CONTROLLER_USER_RESET                           (1<<28)


#define CONTROLLER_TIMECODE_GO                          (1<<0)
#define CONTROLLER_TIMECODE_READY                       (1<<3)
#define CONTROLLER_TIMECODE_RECORD                      (1<<4)
#define CONTROLLER_TIMECODE_BUSY                        (1<<7)
#define CONTROLLER_TIMECODE_ENABLE                      (1<<8)
#define CONTROLLER_TIMECODE_MATCHED                     (1<<10)
#define CONTROLLER_TIMECODE_OVERFLOW                    (1<<15)
#define CONTROLLER_TIMECODE_CARRIER                     (1<<25)
#define CONTROLLER_TIMECODE_IN_SYNC                     (1<<27)
#define CONTROLLER_TIMECODE_LEAPYEAR                    (1<<28)
#define CONTROLLER_TIMECODE_DEBUG                       (1<<28)
#define CONTROLLER_TIMECODE_LOSS_CARRIER                (1<<30)

#define CONTROLLER_TIMECODE_EXTERNAL                    (1<<24)

#define TIMECODE_M1553_JAM                              (1<<16)
#define TIMECODE_RMM_JAM                                (1<<17)

#define CONTROLLER_TIMECODE_PACKET_CNT_CLEAR            (1<<4)
#define CONTROLLER_TIMECODE_CALIBRATION                 (1<<8)
#define CONTROLLER_TIMECODE_LOAD_BCD                    (1<<12)


#define CONTROLLER_HOST_CHANNEL_GO                      (1<<0)
#define CONTROLLER_HOST_CHANNEL_READY                   (1<<3)
#define CONTROLLER_HOST_RECORD                          (1<<4)
#define CONTROLLER_HOST_CHANNEL_BUSY                    (1<<7)
#define CONTROLLER_WAITING_FOR_BUF_TO_CLEAR             (1<<11)
#define CONTROLLER_WAITING_FOR_END_ACK                  (1<<12)
#define CONTROLLER_WAITING_FOR_START                    (1<<13)
#define CONTROLLER_HOST_CHANNEL_FIFO_EMPTY              (1<<31)
#define CONTROLLER_HOST_CHANNEL_PKT_READY               (1<<24)

#define CONTROLLER_BTM_GO                               (1<<0)
#define CONTROLLER_BTM_READY                            (1<<3)
#define CONTROLLER_BTM_BUSY                             (1<<7)
#define CONTROLLER_BTM_OVERFLOW                         (1<<31)

#define CONTROLLER_EVENT_CHANNEL_PKT_READY              (1<<24)
#define CONTROLLER_EVENT_CHANNEL_FIFO_EMPTY             (1<<31)



#define CONTROLLER_HOST_CHANNEL_PACKET_CNT_CLEAR        (1<<4)

#define CONTROLLER_CASCADE_GO                           (1<<0)
#define CONTROLLER_CASCADE_READY                        (1<<3)
#define CONTROLLER_CASCADE_RECORD                       (1<<4)
#define CONTROLLER_CASCADE_BUSY                         (1<<7)
#define CONTROLLER_CASCADE_ENABLE                       (1<<8)

#define CONTROLLER_CASCADE_PACKET_CNT_CLEAR             (1<<0)


#define CONTROLLER_CASCADE_IN_FIFO_READ                 (1<<0)
#define CONTROLLER_CASCADE_IN_FIFO_ENABLE               (1<<4)
#define CONTROLLER_CASCADE_IN_FIFO_EMPTY                (1<<31)


#define CONTROLLER_VOICE_GO                             (1<<0)
#define CONTROLLER_VOICE_READY                          (1<<3)
#define CONTROLLER_VOICE_RECORD                         (1<<4)
#define CONTROLLER_VOICE_BUSY                           (1<<7)
#define CONTROLLER_VOICE_ENABLE                         (1<<8)
#define CONTROLLER_VOICE_LEFT_SELECT                    (1<<12)
#define CONTROLLER_VOICE_OVERFLOW                       (1<<15)

#define CONTROLLER_VOICE_PACKET_CNT_CLEAR               (1<<0)

#define CONTROLLER_TOTAL_PACKET_CNT_CLEAR               (1<<0)
#define CONTROLLER_DSP_FIFO_ALMOST_FULL                 (1<<15)

#define CONTROLLER_DSP_IN_FIFO_WRITE                    (1<<0)
#define CONTROLLER_DSP_IN_FIFO_ENABLE                   (1<<4)

#define CONTROLLER_TIME_GEN_GO                          (1<<0)
#define CONTROLLER_TIME_GEN_READY                       (1<<3)
#define CONTROLLER_TIME_GEN_ENABLE                      (1<<4)
#define CONTROLLER_TIME_GEN_BUSY                        (1<<7)
#define CONTROLLER_TIME_GEN_LOAD                        (1<<12)

#define CONTROLLER_MASTER_TIME_GO                       (1<<0)
#define CONTROLLER_MASTER_TIME_READY                    (1<<3)
#define CONTROLLER_MASTER_TIME_BUSY                     (1<<7)
#define CONTROLLER_MASTER_TIME_LOAD                     (1<<12)

#define CONTROLLER_INVERT_SERIAL_CLOCK_1                (1<<0)
#define CONTROLLER_INVERT_SERIAL_DATA_1                 (1<<4)
#define CONTROLLER_INVERT_SERIAL_CLOCK_2                (1<<8)
#define CONTROLLER_INVERT_SERIAL_DATA_2                 (1<<12)

#define CONTROLLER_LATCH_OPEN                           (1<<0)
#define CONTROLLER_CARTRIDGE_NOT_PRESENT                (1<<1)

#define CONTROLLER_RT_WRITE_ENABLE                      (1<<15)

/*The Following will be used to store indexes for Events and Time*/
#define CONTROLLER_ENTRY_DATA                           0x1428

#define CONTROLLER_ENTRY_READ_STROBE                    (1<<4)
#define CONTROLLER_ENTRY_READY                          (1<<7)
#define CONTROLLER_ENTRY_REQUEST_TIME                   (1<<8)

/*The Following defines the Playback Interface*/
#define CONTROLLER_PLAYBACK_CONTROL                     0x90
#define CONTROLLER_PLAYBACK_STATUS                      0x94
#define CONTROLLER_PLAYBACK_ID                          0x98
#define CONTROLLER_PLAYBACK_TS_ID                       0x9C

#define CONTROLLER_PLAYBACK_GO                          (1<<0)
#define CONTROLLER_PLAYBACK_GEN                         (1<<4)
#define CONTROLLER_UPSTREAM_DATA                        (1<<8)
#define CONTROLLER_UPSTREAM_CLOCK                       (1<<9)
#define CONTROLLER_RESET_CIRRUS                         (1<<16)

#define TOGGLE_PLAY_CLOCK                               (1<<24)

/*The Following defines the 1394B Interface*/
#define IEEE1394B_MAX_BLOCKS                             512 
#define IEEE1394B_MAX_PLAY_BLOCKS                         64

#define IEEE1394B_CONTROL                               0xA0
#define IEEE1394B_DPR_ADDRESS                           0xA4
#define IEEE1394B_BUS_SPEED                             0xA8
#define IEEE1394B_STATUS                                0xA8
#define IEEE1394B_INTERRUPTS                            0xAC

#define IEEE1394B_COMMAND_GO                            (1<<0)
#define IEEE1394B_INTERRUPT_ENABLE                      (1<<4)
#define IEEE1394B_COMMAND_BUSY                          (1<<7)
#define IEEE1394B_NEXT_CONTEXT                          (1<<8)

#define BUS_PAYLOAD_S100                                0x07
#define BUS_PAYLOAD_S200                                0x08
#define BUS_PAYLOAD_S400                                0x09
#define BUS_PAYLOAD_S800                                0x0A

#define BUS_SPEED_S100                                  0x00
#define BUS_SPEED_S200                                  0x10
#define BUS_SPEED_S400                                  0x20
#define BUS_SPEED_S800                                  0x30

#define IEEE1394B_INTERRUPT_MASK                        0x1FFFE

#define IEEE1394B_HOST_COMPLETE                         (1<<27)
#define IEEE1394B_RECORD_FAULT                          (1<<28)
#define IEEE1394B_PLAY_FAULT                            (1<<29)
#define IEEE1394B_HOST_FAULT                            (1<<30)
#define IEEE1394B_PLAY_COMPLETE                         (1<<31)


#define IEEE1394B_RECORD                                0xB0
#define IEEE1394B_RECORD_FIFO                           0xB4
#define IEEE1394B_RECORD_FIFO_MUX                       0xB8
#define IEEE1394B_RECORD_STATUS                         0xBC

#define IEEE1394B_RECORD_GO                             (1<<0)
#define IEEE1394B_RECORD_BUSY                           (1<<7)

#define IEEE1394B_RECORD_FIFO_EMPTY                     (1<<31)

#define RECORD_MUX_SELECT_CASCADE                       0x00000
#define RECORD_MUX_SELECT_0XBYTE                        0x10000
#define RECORD_MUX_SELECT_0X32                          0x20000
#define RECORD_MUX_SELECT_PSEUDO                        0x30000
#define RECORD_MUX_SELECT_ZERO                          0x40000
#define RECORD_MUX_SELECT_FIVES                         0x50000
#define RECORD_MUX_SELECT_AS                            0x60000
#define RECORD_MUX_SELECT_FS                            0x70000

#define IEEE1394B_RECORD_NUM_BLOCKS                     0xC0
#define IEEE1394B_RECORD_BLOCKS_RECORDED                0xC4
#define IEEE1394B_RECORD_POINTER_IN                     0xC8
#define IEEE1394B_RECORD_POINTER_OUT                    0xCC


#define IEEE1394B_PLAYBACK_CONTROL                      0xD0
#define IEEE1394B_PLAYBACK_FIFO                         0xD4
#define IEEE1394B_PLAYBACK_TEST                         0xD8
#define IEEE1394B_PLAYBACK_STATUS                       0xDC

#define PLAYBACK_GO                                     (1<<0)
#define PLAYBACK_BUSY                                   (1<<7)
#define PLAY_IN_RECORD                                  (1<<24)
#define PLAYBACK_TEST_RESET                             (1<<31)

#define PLAYBACK_LIVE_DATA                              0x00000
#define PLAYBACK_0XBYTE_DATA                            0x10000
#define PLAYBACK_0X32_DATA                              0x20000
#define PLAYBACK_PSEUDO_DATA                            0x30000
#define PLAYBACK_ZERO_DATA                              0x40000
#define PLAYBACK_FIVES_DATA                             0x50000
#define PLAYBACK_AS_DATA                                0x60000
#define PLAYBACK_FS_DATA                                0x70000

#define IEEE1394B_PLAYBACK_NUM_BLOCKS                   0xE0
#define IEEE1394B_PLAYBACK_BLOCKS_OUT                   0xE4
#define IEEE1394B_PLAYBACK_POINTER_IN                   0xE8
#define IEEE1394B_PLAYBACK_POINTER_OUT                  0xEC

#define IEEE1394B_PLAYBACK_TEST_BITS_LSW                0xF0
#define IEEE1394B_PLAYBACK_TEST_BITS_MSW                0xF4
#define IEEE1394B_PLAYBACK_TEST_ERROR_BITS_LSW          0xF8
#define IEEE1394B_PLAYBACK_TEST_ERROR_BITS_MSW          0xFC


#define IEEE1394B_HOST_CONTROL                          0x100
#define IEEE1394B_HOST_DEST_ADDRESS                     0x104
#define IEEE1394B_HOST_ORB_SIZE                         0x108
#define IEEE1394B_HOST_STATUS                           0x10C

#define IEEE1394B_HOST_GO                               (1<<0)
#define IEEE1394B_HOST_TO_TARGET                        (1<<4)
#define IEEE1394B_HOST_BUSY                             (1<<7)

#define IEEE1394B_HOST_CDB_0                            0x110
#define IEEE1394B_HOST_CDB_1                            0x114
#define IEEE1394B_HOST_CDB_2                            0x118


#define DUAL_PORT_ADDRESS                               0x00040000
#define RECORD_FIFO_ADDRESS                             0x00050000
#define PLAYBACK_FIFO_ADDRESS                           0x00050000
#define HOST_ORB_ADDRESS                                0x00040600
#define HOST_ORB_READ_ADDRESS                           0x80040600

#define ETHERNET_HOST_REQUEST                           0x1C14
#define ETHERNET_HOST_WRITE_DATA                        0x1C18
#define ETHERNET_HOST_READ_DATA                         0x1C1C
#define ETHERNET_CONTROL_CSR                            0x1C20
#define ETHERNET_CHAN_ID_CSR                            0x1C24
#define ETHERNET_BASE_ADDR                              0x1C2C
#define ETHERNET_CHAN_MESSAGE_COUNT                     0x1C30

#define ETHERNET_HOST_REQUEST_BIT                       (1<<31)
#define ETHERNET_RESET_BIT                              (1<<28)
#define ETHERNET_HOST_WRITE_BIT                         (1<<4)

#define ETHERNET_ENABLE                                 (1<<8)
#define ETHERNET_NO_CRC                                 (1<<12)
#define ETHERNET_NO_IPH                                 (1<<16)
#define ETHERNET_ENABLE_INT                             (1<<24)


/******************************
 *    Ethernet Recording      *
 ******************************/
#define ETHERNET_CHANNEL_ENABLE				0x2020
#define ETHERNET_CHANNEL_ID				0x2024
#define ETHERNET_CHANNEL_REL_LSW			0x2040
#define ETHERNET_CHANNEL_REL_MSW			0x2044
#define ETHERNET_CHANNEL_REL_TIME_GO			0x2050

#define ETHERNET_REL_TIME_GO				(1<<0)
#define ETHERNET_REL_TIME_LOAD				(1<<16)

#define CONTROLLER_AUDIO_MUX                            0x3C
#define CONTROLLER_LEFT_RIGHT                           0x00
#define CONTROLLER_RIGHT_RIGHT                          0x10
#define CONTROLLER_LOCAL_AUDIO                          0x30


#define PLAYBACK_TIME_MSW                               0x128
#define PLAYBACK_TIME_LSW                               0x12C
#define PLAYBACK_TIME_REQ                               (1<<0)


/*USED For Ethernet Broadcast*/
#define RECORD_TO_DISK                                  (1<<20)
#define ETHERNET_BROADCAST                              (1<<24)
#define LAST_ROOT_IN_FIFO				(1<<28)

#define BROADCAST_MASK                                  0x8000

#define ETHERNET_DUAL_PORT                             0x1c40
#define ETHERNET_DUAL_XMIT                             (1<<31)


/*1553 and PCM Event FIFO*/
#define M1553_EVENT_FIFO                                0x8000C400
#define PCM_EVENT_FIFO                                  0x8000D800
#define BUS_EVENT_FIFO_CSR                              0x40
#define M1553_FRAME_AVAILABLE                           (1<<0)
#define PCM_FRAME_AVAILABLE                             (1<<4)

#endif
