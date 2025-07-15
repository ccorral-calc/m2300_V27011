//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//    File: M23_MP_Handler.h
//    Version: 1.0
//    Author: Paul C 
//
//            MONSSTR 2300 MP Handler
//
//    Revisions:   
//

#ifndef __M23_MP_HANDLER_H
#define __M23_MP_HANDLER_H

int MP_Device_Exists[8];

#define READ_COMMAND  0x00000000
#define WRITE_COMMAND 0x80000000

#define NUMBER_OF_PROGRAMMABLE_SECTORS 16 

#define SIZE_OF_SECTORS  0x10000

#define START_PROGRAM_SECTOR_ADDRESS  0x400000


#define ENDIAN_TEST 0x87654321

#define BAR1 1


#define MP_VIDEO_DEVICE 1
#define MP_M1553_DEVICE 2
#define MP_PCM_DEVICE   3
#define MP_DM_DEVICE    4


#define MP_DEVICE_0   0
#define MP_DEVICE_1   1
#define MP_DEVICE_2   2
#define MP_DEVICE_3   3
#define MP_DEVICE_4   4

//#define M1553_MAX_PACKET_SIZE    1950
#define M1553_MAX_PACKET_SIZE    1300
/*Board Specific Offsets*/
#define MP_FLASH_OFFSET          0x100
#define MP_FLASH_READ_OFFSET     0x104
#define MP_SERIAL1_CHAIN_SELECT  0x110
#define MP_SERIAL2_CHAIN_SELECT  0x114
#define MP_MODE_SELECT           0x120

#define MP_CHANNEL_OFFSET                      0x400

#define MP_CSR_OFFSET                          0x0
#define MP_COMPILE_TIME_OFFSET                 0x8

#define MP_BOARD_TYPE_OFFSET                   0x1C

#define MP_CHANNEL_ENABLE_OFFSET               0x20
#define MP_CHANNEL_ENABLE_LOAD_REL_TIME        0x00010000

#define MP_CHANNEL_ID_OFFSET                   0x28


#define MP_TIME_LSB_OFFSET                     0x30
#define MP_TIME_MSB_OFFSET                     0x34

#define MP_TIME_LSB_READ                       0x38
#define MP_TIME_MSB_READ                       0x3C

#define MP_CONDOR_OFFSET                       0x100

#define MP_CONDOR_READ_OFFSET                  0x104
#define MP_CONDOR_PACKET_OFFSET                0x108

#define MP_CHANNEL_STATUS_OFFSET               0x10C

#define MP_CHANNEL_WATCH_WORD_OFFSET           0x110
#define MP_CHANNEL_WATCH_WORD_TIMEOUT_OFFSET   0x114

#define MP_CHANNEL_WATCH_WORD_EVENT_OFFSET     0x118

#define MP_CHANNEL_FILTER_OFFSET               0x11C
#define MP_CHANNEL_FILTER_INCLUDE              (1<<16)
#define MP_CHANNEL_LOGICAL_FILTER_INCLUDE      (1<<17)
#define MP_CHANNEL_POD_FILTER_INCLUDE          (1<<18)
#define MP_CHANNEL_PROC_FILTER_INCLUDE         (1<<19)
#define MP_CHANNEL_FILTER_ERRORS               (1<<20)
#define MP_CHANNEL_FILTER_WRITE_ENABLE         (1<<31)



/*M1553 Time sync/capture */
#define MP_M1553_CAPTURE_OFFSET                0x120
#define MP_M1553_CAPTURE_MESSAGE_FIFO          0x124
#define MP_M1553_CAPTURE_READ_FIFO             0x128


#define MP_M1553_CAPTURE_ONE                   (1<<16)
#define MP_M1553_TIME_SEARCH                   (1<<17)

#define MP_M1553_RT_SELECT                     0x130

#define MP_M1553_RT_SELECTION                  0x120
#define MP_M1553_RT2_SELECTION                 0x130


#define MP_M1553_PAUSE                (1<<6)
#define MP_CHANNEL0_ENABLE            (1<<8)
#define MP_CHANNEL_ENABLE             (1<<8)
#define MP_M1553_CHANNEL_PKT_MODE     (1<<28)
#define MP_M1553_SINGLE_MSG_PKT       (1<<0)

#define MP_CTM_OFFSET                 0x28
#define MP_CTM_FIFO_NOT_EMPTY         (1<<31)


/********************************************************
     PCM Definitions
 ********************************************************/
#define MP_PCM_FRAME_TYPE_OFFSET                     0x100
#define MP_PCM_STATUS_OFFSET                         0x104
#define MP_PCM_MAIN_FRAME_WORDS_OFFSET               0x108
#define MP_PCM_SYNC_DEFINITION_OFFSET                0x10C
#define MP_PCM_MAIN_FRAME_SYNC_PATTERN_OFFSET_MSB    0x110
#define MP_PCM_MAIN_FRAME_SYNC_PATTERN_OFFSET        0x114
#define MP_PCM_MAIN_FRAME_SYNC_MASK_OFFSET_MSB       0x118
#define MP_PCM_MAIN_FRAME_SYNC_MASK_OFFSET           0x11C
#define MP_PCM_SUBFRAME_SYNC_PATTERN_OFFSET          0x120
#define MP_PCM_SUBFRAME_SYNC_MASK_OFFSET             0x124
#define MP_PCM_SUBFRAME_1_BIT_OFFSET                 0x128
#define MP_PCM_SUBFRAME_MINOR_MAJOR_OFFSET           0x12C
#define MP_PCM_LENGTH_TABLE_OFFSET                   0x12C
#define MP_PCM_LENGTH_TABLE_READ_OFFSET              0x12C
#define MP_PCM_FILTER_TABLE_OFFSET                   0x134
#define MP_PCM_FILTER_TABLE_WRITE_OFFSET             0x138
#define MP_PCM_FILTER_TABLE_READ_OFFSET              0x13C

#define MP_PCM_MAJOR_FRAMES_PACKET_OFFSET       0x140
#define MP_PCM_BIT_LENGTH_LAST_WORD             0x144
#define MP_PCM_CLOCK_DAC_OFFSET                 0x150
#define MP_PCM_DATA_DAC_OFFSET                  0x154
#define MP_PCM_BIT_RATE_MODULUS_OFFSET          0x160

#define MP_PCM_FRAME_SYNC_THRESHOLD_OFFSET      0x164

#define MP_PCM_WATCH_WORD_SFID_MASK             0x168
#define MP_PCM_WATCH_WORD_OFFSET                0x16C
#define MP_PCM_WATCH_WORD_MINOR_OFFSET          0x170

#define MP_PCM_WATCH_WORD_BIT_OFFSET            0x170

#define MP_PCM_WATCH_WORD_DETECTED_OFFSET       0x170

#define MP_PCM_CLOCK_PRECISION_MASK             0x17C

#define MP_PCM_LOWER_BIT_RATE_MODULUS           0x180
#define MP_PCM_UPPER_BIT_RATE_MODULUS           0x184
#define MP_PCM_NOMINAL_BIT_RATE_MODULUS         0x188

#define MP_PCM_EXPECTED_BIT_RATE                0x18C

#define MP_UART_FRAME                             (1<<18)
#define MP_UART_PARITY                            (1<<19)
#define MP_UART_NO_PACKET                         (1<<30)
#define MP_UART_PACKET_DROP                       (1<<31)

#define MP_ARBITER_GO_OFFSET                    0x120

#define MP_PCM_TIME_OFFSET                      0x40
#define MP_PCM_422                              (1<<0)
#define MP_PCM_TTL                              (1<<1)
#define MP_PCM_INVERT_DATA                      (1<<4)
#define MP_PCM_INVERT_CLOCK                     (1<<8)
#define MP_PCM_BIT_SYNC                         (1<<12)
#define MP_PCM_CRC_CHECK                        (1<<16)
#define MP_PCM_TRANSFER_LSB                     (1<<16)
#define MP_PCM_INTRA_PKT_HDR                    (1<<20)

#define MP_PCM_NRZL                              0x0
#define MP_PCM_RNRZL                             0x1
#define MP_PCM_BIPHASE                           0x2
#define MP_PCM_REVERSE_RNRZL                     0x3
#define MP_PCM_UART                              0x4
#define MP_PCM_PULSE                             0x5

#define MP_PCM_THROUGHPUT                        0x0
#define MP_PCM_PACKED                            0x1
#define MP_PCM_UNPACKED                          0x2


#define MP_PCM_MINOR_FRAME_ONLY                  (1<<28)
#define MP_PCM_COMMON_WORD_MASK                  (1<<27)
#define MP_PCM_MAJOR_ALIGN                       (1<<28)

#define MP_PCM_TABLE_COMPLETE                    (1<<31)
#define MP_PCM_WRITE_STROBE                      (1<<27)

#define MP_PCM_RECORD_RAW                         (1<<31)
#define MP_PCM_TRANSMIT_RAW                       (1<<30)

#define PCM_RECORD_RAW                           (1<<31)
#define PCM_TRANSMIT_RAW                         (1<<30)
#define MP_PCM_SYNC_ONE_PART                      0x000
#define MP_PCM_SYNC_TWO_PARTS                     0x001
#define MP_PCM_SYNC_THREE_PARTS                   0x011
#define MP_PCM_SYNC_FOUR_PARTS                    0x111


#define MP_PCM_LOST_CLOCK                         (1<<12)
#define MP_PCM_LOST_DATA                          (1<<13)
#define MP_PCM_NO_CLOCK                           (1<<14)
#define MP_PCM_NO_DATA                            (1<<15)
#define MP_PCM_LOST_MAJOR_LOCK_STICKY             (1<<17)
#define MP_PCM_LOST_MINOR_LOCK_STICKY             (1<<16)

#define MP_PCM_WATCH_WORD_LOCKED                  (1<<20)
#define MP_PCM_WATCH_WORD_ERROR_STICKY            (1<<21)

#define MP_PCM_BIT_ERROR_STICKY                   (1<<25)
#define MP_PCM_BIT_SYNC_LOCKED                    (1<<24)
#define MP_PCM_CRC_ERROR                          (1<<28)
#define MP_PCM_NOT_MAKING_PACKETS                 (1<<30)
#define MP_PCM_OVERRUN                            (1<<31)

#define MP_PCM_SET_PAUSE                          0x20
#define MP_PCM_PAUSE_STATUS                       0x24
#define MP_PCM_PAUSE                              (1<<16)
#define MP_PCM_RECORD_ENABLE                       0x24
#define MP_PCM_DISABLE_RECORD                      (1 << 4)
#define MP_MINOR_FRAME_LOCKED                      0x4
#define MP_MAJOR_FRAME_LOCKED                      0x40


/**END PCM**/


#define MP_LOAD_RELATIVE_TIME                    (1<<16)
/*UART Configuration stuff*/
#define MP_UART_CONFIG_OFFSET                     0x194
#define MP_UART_7_BIT                             (1<<8)
#define MP_UART_PARITY_NONE                       0x0
#define MP_UART_PARITY_ODD                        0x1
#define MP_UART_PARITY_EVEN                       0x2

#define MP_UART_BAUD_RATE_300                     0x0
#define MP_UART_BAUD_RATE_1200                    0x1
#define MP_UART_BAUD_RATE_2400                    0x2
#define MP_UART_BAUD_RATE_4800                    0x3
#define MP_UART_BAUD_RATE_9600                    0x4
#define MP_UART_BAUD_RATE_19200                   0x5
#define MP_UART_BAUD_RATE_38400                   0x6
#define MP_UART_BAUD_RATE_57600                   0x7
#define MP_UART_BAUD_RATE_115200                  0x8
#define MP_UART_BAUD_RATE_230400                  0x9
#define MP_UART_BAUD_RATE_460800                  0xA

/*Pulse Counter Configuration*/
#define MP_PCM_PULSE_OFFSET                      0x190

#define MP_PULSE_COUNT_CLOCK                     (1<<0)
#define MP_PULSE_COUNT_DATA                      (1<<4)


#define MP_VIDEO_TIME_OFFSET                      0x40
#define MP_VIDEO_RELATIVE_TIME_GO_OFFSET          0x48
#define MP_VIDEO_RELATIVE_TIME_LSB                0x4C
#define MP_VIDEO_RELATIVE_TIME_MSB                0x50

#define MP_VIDEO_UPSTREAM_SELECT                  0x100
#define MP_VIDEO_BOBBLE_MASK                      0x104
#define MP_VIDEO_DEBUG                            0x108
#define MP_CHANNEL_OVERLAY                        0x110
#define MP_CHANNEL_OVERLAY_X                      0x114
#define MP_CHANNEL_OVERLAY_Y                      0x118
#define MP_CHANNEL_OVERLAY_GO                     0x11C


#define MP_CHANNEL_AUDIO_OUT                      0x110

#define MP_CHANNEL_VIDEO_OUT                      0x114
#define MP_VIDEO_DOWNSTREAM                       0x0
#define MP_AUDIO_DOWNSTREAM                       0x0
#define MP_VIDEO_OFF                              0x5
#define MP_AUDIO_OFF                              0x5

#define MP_VIDEO_FILTER_GO                        (1<<8)
#define MP_VIDEO_FILTER_BAR                       (1<<12)

#define MP_VIDEO_CHAN0_OFFSET                     0x20
#define MP_VIDEO_RESET_FIFO                      (1<<28)

#define MP_LOCAL_GO                               (1<<0)

#define MP_AUDIO_UPSTREAM                         (1<<0)
#define MP_AUDIO_ONLY                             (1<<4)
#define MP_SYNC_BIT                               (1<<8)
#define MP_RESYNC_BIT                             (1<<12)
#define MP_TWO_AUDIO_BIT                          (1<<16)
#define MP_VIDEO_LOSS_SYNC                        (1<<28)


#define MP_VIDEO_PACKETS_NOT_FLOWING              (1<<16)
#define MP_VIDEO_PACKETS_DROPOUT                  (1<<20)
#define MP_VIDEO_STOP                             (1<<28)


#define MP_VIDEO_4_AUDIO                          (1<<24)

#define MP_VIDEO_VERBOSE                          (1<<20)

#define MP_VIDEO_LARGE_CHAR                       (1<<12)
#define MP_VIDEO_NO_MS                            (1<<9)
#define MP_VIDEO_TIME_ONLY                        (1<<8)
#define MP_VIDEO_CLEAR_BACK                       (1<<4)
#define MP_VIDEO_BLACK_ON_WHITE                   (1<<0)
#define MP_VIDEO_TEST_PATTERN                     (1<<4)
#define MP_VIDEO_OVERLAY_GO                       (1<<0)

#define MP_VIDEO_CHANNEL_RESET                    0x30
#define MP_VIDEO_RESET                            (1<<0)
#define MP_VIDEO_NO_PACKETS                       (1<<16)
#define MP_VIDEO_NO_SIGNAL                        (1<<20)

#define MP_VIDEO_I2C_OFFSET                       0x130
#define MP_VIDEO_ALLOW_I2C                        (1<<0)

#define MP_VIDEO_PAUSE                            (1<<4)

#define MP_AUDIO_SETUP                            0x3C
#define MP_RIGHT_AUDIO_MUTE                       (1<<8)
#define MP_LEFT_AUDIO_MUTE                        (1<<12)
#define MP_RIGHT_AUDIO_TONE                       (1<<16)
#define MP_LEFT_AUDIO_TONE                        (1<<20)

#define VIDEO_AUDIO_TONE                          (1<<20)

#define LEFT_LEFT_RIGHT_RIGHT                     0x00
#define LEFT_RIGHT_RIGHT_RIGHT                    0x10
#define LEFT_LEFT_RIGHT_LEFT                      0x20
#define LEFT_RIGHT_RIGHT_LEFT                     0x30

#define VIDEO_TEMP_OFFSET                         0x140
#define VIDEO_GET_TEMP                            (1<<16)

#define VIDEO_TS_FRAMES_OFFSET                    0x38
#define VIDEO_SELECT_CONSTANT_TS                  (1<<16)

/*************************************************************************
 *  Add M1553 Discrete Event Definitions                                 * 
 *************************************************************************/

#define MP_M1553_EVENT_TABLE_ADDRESS             0x128
#define MP_M1553_EVENT_DESC_UPPER                0x12C
#define MP_M1553_EVENT_DESC_LOWER                0x130

#define EVENT_TO_CONTROLLER                      (1 << 30)
#define EVENT_TO_PROC                            (1 << 31)

#define M1553_EVENT_DESCRIPTION                  0x128
#define M1553_EVENT_NUMBER                       0x12C
#define M1553_EVENT_NUMBER_REG                   0x130
#define M1553_REL_TIME_HIGH_REG                  0x134
#define M1553_REL_TIME_LOW_REG                   0x138
#define M1553_ABS_TIME_LOW_REG                   0x13C

#define MP_M1553_RESUME_CW                       (1<<14)
#define MP_M1553_PAUSE_CW                        (1<<15)
#define M1553_COMMAND_WORD_ONLY                  (1<<22)
#define M1553_EVENT_ENABLED                      (1<<23)
#define M1553_EVENT_READY                        (1<<24)
#define MP_M1553_EVENT_CONT                      (1<<27)
#define M1553_EVENT_CAPTURE1                     (1<<28)
#define M1553_EVENT_CAPTURED                     (1<<29)
#define M1553_WRITE_TABLE                        (1<<31)

#define MP_M1553_EVENT_TOGGLE         (1<<29)

#define MP_M1553_EVENT_UP_DOWN                   0x1
#define MP_M1553_EVENT_UP                        0x2
#define MP_M1553_EVENT_DOWN                      0x3

#define PCM_EVENT_ENABLED                        (1 << 26)


#define MP_M1553_BOARD_PAUSED                    (1<<8)
#define F15_EGI        1
#define B1B_EGI        2
#define B52_EGI        3


#define STOP_START            0
#define PAUSE_RESUME          1

typedef struct M1553_Pause_Resume
{
    int Enabled;
    int ChanID;
    int Type;
    int PauseCW;
    int ResumeCW;
}M1553_PAUSE_RESUME;


M1553_PAUSE_RESUME PauseResume;

typedef struct M1553_Dual_Events
{
    int Event1;
    int Event1_Occured;
    int Event2;
    int Event2_Occured;
}M1553_DUAL_EVENTS;

M1553_DUAL_EVENTS DualEvents;

#define R0_EVENT    4094
#define T9_EVENT    4093
#define T20_EVENT   4092

#define EGI_VALID_EVENT     4083
#define A1_SRU_EVENT        4082
#define GPS_EVENT           4081
#define TFOM_F15C_EVENT     4080

/*These are the B1B 1553 Time Sync Messages*/
#define B1_T17_EVENT   4091
#define B1_T9_EVENT    4090
#define B1_T16_EVENT   4089
#define B1_G15_EVENT   4088


#define B1_INSR_T17_EVENT   4087
#define B1_INSR_T9_EVENT    4086
#define B1_INSR_T16_EVENT   4085
#define B1_INSR_G15_EVENT   4084

#define PAUSE_EVENT         4079
#define RESUME_EVENT        4078


/*These are the B52 1553 Time Sync Messages*/
#define B52_CC05_EVENT  4070
#define B52_CC29_EVENT  4071
#define B52_CD00_EVENT  4072
#define B52_CC2B_EVENT  4073


/*Use these for Purple Monkey*/
#define PURPLE_START_EVENT 4000

/*************************************************************************
 *  Add Discrete Definitions                                 * 
 *************************************************************************/
#define DMP_DISCRETE_OFFSET                        0x100


#endif /* _M23_MP_HANDLER_H */
