/***********************************************************************
 *
 *     Copyright (c) CALCULEX, Inc., 2004
 *
 ***********************************************************************
 *
 *   Filename :   M21_Const.h
 *   Author   :   bdu
 *   Revision :   1.00
 *   Date     :   June 7, 2003
 *   Product  :   Header file for M2100 main controller
 *                                                      
 *   Revision :   
 *
 *   Description:   Constants for M2100 controller
 *                                                                        
 ***********************************************************************/

#define PKTHDR_SIZE  24          /* ch10 packet header size = 24 bytes */
#define BYTESIZE     8           /* 8 bit */
#define BLOCKSIZE    512         /* 512 bytes */


// error code
#define M23_SUCCESS                      0
#define M23_ERROR                       -1

#define M23VIDEO_BUSY                  30
#define M23VIDEO_IDLE                  31
#define M23WRITE_FILE_FAIL             50
#define M23DATA_PIPE_ERROR             51




// 1553 block status word
#define BUS_ID                      (1<<13)
#define MSG_ERROR                   (1<<12)
#define RT_RT_TRANS                 (1<<11)
#define FORMAT_ERROR                (1<<10)
#define TIME_OUT                    (1<<9)
#define WORD_CNT_ERROR              (1<<5)
#define SYNC_TYPE_ERROR             (1<<4)
#define INVALID_WORD_ERROR          (1<<3)


/*********************************
 *      define health status     *
 *********************************/

#define M23_FEATURE_ENABLED                     0x80000000  // applies to all features


/*****RECORDER FEATURE***********/

#define M23_RECORDER_BIT_FAIL                   0x00000001
#define M23_RECORDER_CONFIGURATION_FAIL         0x00000002
#define M23_RECORDER_OPERATION_FAIL             0x00000004
#define M23_MEMORY_MEDIA_BUSY                   0x00000008
#define M23_MEMORY_NOT_PRESENT                  0x00000010
#define M23_MEMORY_IO_ERROR                     0x00000020
#define M23_MEMORY_BINGO_REACHED                0x00000040
#define M23_MEMORY_FULL                         0x00000080
#define M23_MEMORY_LATCH                        0x00000100
#define M23_DISK_NOT_CONNECTED                  0x00000200

#if 0

#define M2X_RECORDER_STATUS_UPDATE_FAIL         0x00000004
#define M2X_RECORDER_SETUP_FILE_CHECKSUM_ERR    0x00000008
#define M2X_RECORDER_RESET_FAIL                 0x00000010
#define M2X_RECORDER_SETTINGS_FILE_ERR          0x00000020
#define M2X_RECORDER_CONTROLLER_FAIL            0x00000040
#define M2X_RECORDER_VIDEO_BOARDS_MISSING       0x00008000
#define M2X_RECORDER_PCM_BOARDS_MISSING         0x00010000
#define M2X_RECORDER_1553_BOARDS_MISSING        0x00020000
#define M2X_RECORDER_NO_DSP_DATA                0x00100000
#define M2X_RECORDER_NO_DATA                    0x00200000

#define M2X_DISK_NOT_CONNECTED                  0x00000040
#define M2X_MEMORY_BIT_FAIL                     0x80000000

#endif


/*****TIMECODE FEATURE***********/
#define M23_TIMECODE_BIT_FAIL                   0x00000001
#define M23_TIMECODE_CONFIG_FAIL                0x00000002
#define M23_TIMECODE_NO_CARRIER                 0x00000004
#define M23_TIMECODE_BAD_CARRIER                0x00000008
#define M23_TIMECODE_SYNC_ERROR                 0x00000010
#define M23_TIMECODE_FLYWHEEL                   0x00000100
#define M23_TIMECODE_GPS_NOT_SYNCED             0x00000200

#if 0
#define M2X_TIMECODE_COUNT_STABLE               0x00000008
#define M2X_TIMECODE_OVERFLOW                   0x00000010
#define M2X_TIMECODE_FLYWHEEL                   0x00000020
#endif



/*****Voice/Analog FEATURE***********/
#define M23_VOICE_BIT_FAIL                      0x00000001
#define M23_VOICE_CONFIG_FAIL                   0x00000002
#define M23_VOICE_NO_SIGNAL                     0x00000004
#define M23_VOICE_BAD_SIGNAL                    0x00000008
#define M23_VOICE_CHANNEL_PAUSED                0x00000040



/*****Video FEATURE***********/
#define M23_VIDEO_BIT_FAIL                      0x00000001
#define M23_VIDEO_CHANNEL_CONFIGURE_FAIL        0x00000002
#define M23_VIDEO_CHANNEL_NO_VIDEO_SIGNAL       0x00000004
#define M23_VIDEO_CHANNEL_BAD_VIDEO_SIGNAL      0x00000008
#define M23_VIDEO_CHANNEL_NO_AUDIO_SIGNAL       0x00000010
#define M23_VIDEO_CHANNEL_BAD_AUDIO_SIGNAL      0x00000020
#define M23_VIDEO_CHANNEL_CHANNEL_PAUSED        0x00000040
#define M23_VIDEO_CHANNEL_NO_PACKETS            0x00000100

#if 0 /*Not Used in TRD for now*/
#define M2X_VIDEO_CHANNEL_OVERRUN               0x00000004
#define M2X_VIDEO_CHANNEL_RESET_FAIL            0x00000010
#define M2X_VIDEO_CHANNEL_NO_PACKET             0x00000020
#define M2X_VIDEO_CHANNEL_PACKET_DROPOUT        0x00000040
#endif


/*****M1553 FEATURE***********/
#define M23_M1553_BIT_FAIL                      0x00000001
#define M23_M1553_CHANNEL_CONFIGURE_FAIL        0x00000002
#define M23_M1553_CHANNEL_RESPONSE_ERROR        0x00000004
#define M23_M1553_CHANNEL_FORMAT_ERROR          0x00000008
#define M23_M1553_CHANNEL_SYNC_ERROR            0x00000010
#define M23_M1553_CHANNEL_WORD_COUNT_ERROR      0x00000020
#define M23_M1553_CHANNEL_PAUSED                0x00000040
#define M23_M1553_WATCH_WORD_ERROR              0x00000080
#define M23_M1553_CHANNEL_NO_SIGNAL             0x00000100



/*****PCM FEATURE***********/
#define M23_PCM_BIT_FAIL                        0x00000001
#define M23_PCM_CHANNEL_CONFIGURE_FAIL          0x00000002
#define M23_PCM_CHANNEL_NO_CLOCK                0x00000004
#define M23_PCM_CHANNEL_BAD_DATA                0x00000008
#define M23_PCM_MINOR_LOCK                      0x00000010
#define M23_PCM_MAJOR_LOCK                      0x00000020
#define M23_PCM_BIT_SYNC_ERROR                  0x00000040
#define M23_PCM_WATCH_WORD_ERROR                0x00000080

#define M23_UART_FRAME                          0x00000100
#define M23_UART_PARITY                         0x00000200
#define M23_UART_NO_PACKET                      0x00000400
#define M23_UART_DROP_PACKET                    0x00000800

#define M23_PCM_CHANNEL_PAUSED                  0x00001000

/*****UART FEATURE***********/
#define M23_UART_CONTROLLER_BIT_FAIL            0x00000001
#define M23_UART_CONTROLLER_CONFIGURE_FAIL      0x00000002
#define M23_UART_CONTROLLER_FRAME               0x00000004
#define M23_UART_CONTROLLER_PARITY              0x00000008
#define M23_UART_CONTROLLER_NO_PACKET           0x00000010
#define M23_UART_CONTROLLER_DROP_PACKET         0x00000020

/*****ETHERNET FEATURE***********/
#define M23_ETHERNET_BIT_FAIL                   0x00000001
#define M23_ETHERNET_CONFIG_FAIL                0x00000002
#define M23_ETHERNET_NO_DATA_PRESENT            0x00000004
#define M23_ETHERNET_NOT_READY                  0x00000100

#if 0
#define M23_PCM_CHANNEL_OVERRUN                 0x00000004
#define M23_UART_FRAME                          0x00100000
#define M23_UART_PARITY                         0x00200000
#define M23_UART_DROP_PACKET                    0x00400000
#define M23_UART_NO_PACKET                      0x00800000
#define M23_UART_FRAME                          0x00100000
#define M23_UART_PARITY                         0x00200000
#define M23_UART_NO_PACKET                      0x00400000
#define M23_UART_DROP_PACKET                    0x00800000


#define M23_CONTROLLER_TEMP1                    0x00000002
#define M23_CONTROLLER_TEMP2                    0x00000004
#define M23_VIDEO1_TEMP1                        0x00000040
#define M23_VIDEO1_TEMP2                        0x00000080
#define M23_VIDEO2_TEMP1                        0x00000100
#define M23_VIDEO2_TEMP2                        0x00000200
#define M23_VIDEO3_TEMP1                        0x00000400
#define M23_VIDEO3_TEMP2                        0x00000800
#define M23_VIDEO4_TEMP1                        0x00001000
#define M23_VIDEO4_TEMP2                        0x00002000

#define M23_M15531_TEMP1                        0x00004000
#define M23_M15531_TEMP2                        0x00008000
#define M23_M15532_TEMP1                        0x00010000
#define M23_M15532_TEMP2                        0x00020000
#define M23_M15533_TEMP1                        0x00040000
#define M23_M15533_TEMP2                        0x00080000
#define M23_M15534_TEMP1                        0x00100000
#define M23_M15534_TEMP2                        0x00200000

#define M23_PCM1_TEMP1                          0x00400000
#define M23_PCM1_TEMP2                          0x00800000
#define M23_PCM2_TEMP1                          0x01000000
#define M23_PCM2_TEMP2                          0x02000000
#define M23_PCM3_TEMP1                          0x04000000
#define M23_PCM3_TEMP2                          0x08000000
#define M23_PCM4_TEMP1                          0x10000000
#define M23_PCM4_TEMP2                          0x20000000


#define M23_VIDEOOUT_NO_SIGNAL                  0x00000002
#define M23_VIDEOOUT_DISABLED                   0x40000000

/*Define the Power Feature Bits*/
#define M23_CONTROLLER_POWER                    0x00000002 
#define M23_FIREWIRE_POWER                      0x00000004 

#define M23_VIDEO1_POWER                        0x00000010 
#define M23_VIDEO2_POWER                        0x00000020 
#define M23_VIDEO3_POWER                        0x00000040 
#define M23_VIDEO4_POWER                        0x00000080 
#define M23_VIDEO5_POWER                        0x00000100 
#define M23_VIDEO6_POWER                        0x00000200 
#define M23_VIDEO7_POWER                        0x00000400 
#define M23_VIDEO8_POWER                        0x00000800 

#define M23_M15531_POWER                        0x00001000 
#define M23_M15532_POWER                        0x00002000 
#define M23_M15533_POWER                        0x00004000 
#define M23_M15534_POWER                        0x00008000 

#define M23_PCM1_POWER                        0x00100000 
#define M23_PCM2_POWER                        0x00200000 
#define M23_PCM3_POWER                        0x00400000 
#define M23_PCM4_POWER                        0x00800000 

#endif

/* Remove the SPIDR Name from the types*/
/* define channel type */
#define INPUT_M1553                         0
#define INPUT_VIDEO                         1
#define INPUT_TIMECODE                      2
#define INPUT_PCM                           3
#define INPUT_VOICE                         4
#define INPUT_ETHERNET                      5
#define INPUT_UART                          6


/* error code for TMATS parsing */
#define TMATS_PARSE_SUCCESS                 0
#define TMATS_M1553_ERROR_1                 10
#define TMATS_M1553_ERROR_2                 11
#define TMATS_M1553_ERROR_3                 12
#define TMATS_M1553_ERROR_4                 13
#define TMATS_M1553_ERROR_5                 14
#define TMATS_M1553_ERROR_6                 15

#define TMATS_VIDEO_ERROR_1                 20
#define TMATS_VIDEO_ERROR_2                 21
#define TMATS_VIDEO_ERROR_3                 22
#define TMATS_VIDEO_ERROR_4                 23
#define TMATS_VIDEO_ERROR_5                 24
#define TMATS_VIDEO_ERROR_6                 25

#define TMATS_PCM_ERROR_1                 30
#define TMATS_PCM_ERROR_2                 31
#define TMATS_PCM_ERROR_3                 32
#define TMATS_PCM_ERROR_4                 33
#define TMATS_PCM_ERROR_5                 34
#define TMATS_PCM_ERROR_6                 35


/* define packet sizing method */
#define PM_PACKET_SIZE                      100         /* size by packet size */
#define PM_INT_RATE                         101         /* size by int rate */
#define PM_MAJOR_FRAME                      102         /* size by number of major frame */
#define PM_PACKET_MESSAGE                   103         /* size by min # of message in packet */
#define PM_PACKET_SAMPLE                    104         /* size by samples in packet */

/* define video channel speed */
#define VIDEO_2_0M_CLOCKRATE          0
#define VIDEO_3_0M_CLOCKRATE          1
#define VIDEO_4_0M_CLOCKRATE          2
#define VIDEO_6_0M_CLOCKRATE          3
#define VIDEO_8_0M_CLOCKRATE          4
#define VIDEO_10_0M_CLOCKRATE         5
#define VIDEO_12_0M_CLOCKRATE         6
#define VIDEO_15_0M_CLOCKRATE         7


/* define video compression mode */
#define VIDEO_I_VIDCOMP               0
#define VIDEO_IP_5_VIDCOMP            1
#define VIDEO_IP_15_VIDCOMP           2
#define VIDEO_IP_45_VIDCOMP           3
#define VIDEO_IPB_5_VIDCOMP           4
#define VIDEO_IPB_15_VIDCOMP          5
#define VIDEO_IPB_45_VIDCOMP          6


/* define video resolution */
#define VIDEO_FULL_VIDRES             0
#define VIDEO_HALF_VIDRES             1
#define VIDEO_SIFHALF_VIDRES          2


/* define video format, NTSC/PAL */
#define VIDEO_NTSC_VIDFORMAT          0
#define VIDEO_PAL_VIDFORMAT           1


/* define video input, Composite/S-Video */
#define VIDEO_COMP_VIDINPUT           0
#define VIDEO_SVID_VIDINPUT           1


/* define audio rate */
#define VIDEO_64_AUDIORATE            0
#define VIDEO_96_AUDIORATE            1
#define VIDEO_192_AUDIORATE           2


/* define audio gain */
#define VIDEO_0_AUDIOGAIN             0
#define VIDEO_1_5_AUDIOGAIN           1
#define VIDEO_3_0_AUDIOGAIN           2
#define VIDEO_4_5_AUDIOGAIN           3
#define VIDEO_6_0_AUDIOGAIN           4
#define VIDEO_7_5_AUDIOGAIN           5
#define VIDEO_9_0_AUDIOGAIN           6
#define VIDEO_10_5_AUDIOGAIN          7
#define VIDEO_12_0_AUDIOGAIN          8
#define VIDEO_13_5_AUDIOGAIN          9
#define VIDEO_15_0_AUDIOGAIN          10
#define VIDEO_16_5_AUDIOGAIN          11
#define VIDEO_18_0_AUDIOGAIN          12
#define VIDEO_19_5_AUDIOGAIN          13
#define VIDEO_21_0_AUDIOGAIN          14
#define VIDEO_22_5_AUDIOGAIN          15
