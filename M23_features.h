//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: M23_features.h
//    Version: 1.0
//     Author: pc 
//
//            MONSSTR 2300v1 TRD features
//
//
//    Revisions:   
//

#ifndef _M23_FEATURES_H
#define _M23_FEATURES_H


/*The following are the indexes into the health/critical array*/
#define  RECORDER_FEATURE          0
#define  TIMECODE_FEATURE          1
#define  M1553_FEATURE             2
#define  VIDEO_FEATURE             3
#define  VOICE_FEATURE             4
#define  PCM_FEATURE               5
#define  ETHERNET_FEATURE          6
#define  UART_FEATURE              7
#define  DISCRETE_FEATURE          8

#define  NO_FEATURE                0xFF

extern int      StartOfPCMHealth;
extern int      StartOf1553Health;
extern int      StartOfVideoHealth;
extern int      StartOfVideoOut;

#define START_OF_VIDEO_FEATURE 3
#define START_OF_M1553_FEATURE 2
#define START_OF_PCM_FEATURE   5 

extern INT32              ControllerHealthTemps[2];
extern INT32              VideoHealthTemps[6][2];
extern INT32              PCMHealthTemps[6][2];
extern INT32              M1553HealthTemps[6][2];
extern INT32              PowerSupplyHealthTemps[2];
extern INT32              FirewireHealthTemps[2];

extern INT32              ControllerPower;
extern INT32              FirewirePower;
extern INT32              VideoPower[5];
extern INT32              M1553Power[5];
extern INT32              PCMPower[5];

#define  NUMBER_STANDARD_FEATURES  1

#define  MAX_FEATURES     50

INT8  FeatureDescriptions[MAX_FEATURES][40];

UINT32 HealthStatusArr[MAX_FEATURES];
INT32 HealthArrayTypes[MAX_FEATURES];

UINT32 M1553_WatchWord[8];


//#pragma pack(1)
//static char *RecorderFeatureBits[32] = {
static char *RecorderFeatureBits[32] = {
           "BIT Failure"                            // 0
          ,"Setup Failure"                          // 1
          ,"Operation Failure"                      // 2
          ,"Media Busy Unable to Accept Command"    // 3 
          ,"No Media"                               // 4
          ,"Media I/O Failure"                      // 5
          ,"Media Almost Full"                      // 6 
          ,"Media Full"                             // 7
          ,"Media Door/Latch Open"                  // 8 --Start of Vendor Specific
          ,"Media Not Connected"                    // 9
          ,""                                       // 10
          ,""                                       // 11
          ,""                                       // 12
          ,""                                       // 13
          ,""                                       // 14
          ,""                                       // 15
          ,""                                       // 16
          ,""                                       // 17
          ,""                                       // 18
          ,""                                       // 19
          ,""                                       // 20
          ,""                                       // 21
          ,""                                       // 22
          ,""                                       // 23
          ,""                                       // 24
          ,""                                       // 25
          ,""                                       // 26
          ,""                                       // 27
          ,""                                       // 28
          ,""                                       // 29
          ,""                                       // 30
          ,""                                       // 31
};

static char *TimeFeatureBits[32] = {
           "BIT Failure"           // 0
          ,"Setup Failure"         // 1
          ,"No External Signal"    // 2
          ,"Bad External Signal"   // 3
          ,"Synchronize Failure"   // 4
          ,""                      // 5
          ,""                      // 6
          ,""                      // 7
          ,"FlyWheeling"           // 8
          ,"GPS NOT Synced"        // 9
          ,""                      // 10
          ,""                      // 11
          ,""                      // 12
          ,""                      // 13
          ,""                      // 14
          ,""                      // 15
          ,""                      // 16
          ,""                      // 17
          ,""                      // 18
          ,""                      // 19
          ,""                      // 20
          ,""                      // 21
          ,""                      // 22
          ,""                      // 23
          ,""                      // 24
          ,""                      // 25
          ,""                      // 26
          ,""                      // 27
          ,""                      // 28
          ,""                      // 29
          ,""                      // 30
          ,""                      // 31
};

static char *VoiceFeatureBits[32] = {
           "BIT Failure"                // 0
           "Setup Failure"              // 1
          ,"No Analog Signal Error"     // 2
          ,"Bad Analog Singal Error"    // 3
          ,""                           // 4
          ,""                           // 5
          ,"Channel Paused"             // 6
          ,""                           // 7
          ,""                           // 8
          ,""                           // 9
          ,""                           // 10 
          ,""                           // 11
          ,""                           // 12
          ,""                           // 13
          ,""                           // 14
          ,""                           // 15
          ,""                           // 16
          ,""                           // 17
          ,""                           // 18
          ,""                           // 19
          ,""                           // 20 
          ,""                           // 21
          ,""                           // 22
          ,""                           // 23
          ,""                           // 24
          ,""                           // 25
          ,""                           // 26
          ,""                           // 27
          ,""                           // 28
          ,""                           // 29
          ,""                           // 30 
          ,""                           // 31
};

static char *TemperatureFeatureBits[32] = {
           ""                          // 0
          ,"Controller Board Temp1"    // 1
          ,"Controller Board Temp2"    // 2
          ,""                          // 3
          ,""                          // 4
          ,""                          // 5
          ,"Video Board 1 Temp 1"      // 6
          ,"Video Board 1 Temp 2"      // 7
          ,"Video Board 2 Temp 1"      // 8
          ,"Video Board 2 Temp 2"      // 9
          ,"Video Board 3 Temp 1"      // 10 
          ,"Video Board 3 Temp 2"      // 11 
          ,"Video Board 4 Temp 1"      // 12 
          ,"Video Board 4 Temp 2"      // 13 
          ,"M1553 Board 1 Temp 1"      // 14 
          ,"M1553 Board 1 Temp 2"      // 15 
          ,"M1553 Board 2 Temp 1"      // 16 
          ,"M1553 Board 2 Temp 2"      // 17 
          ,"M1553 Board 3 Temp 1"      // 18 
          ,"M1553 Board 3 Temp 2"      // 19 
          ,"M1553 Board 4 Temp 1"      // 20 
          ,"M1553 Board 4 Temp 2"      // 21 
          ,"PCM   Board 1 Temp 1"      // 22 
          ,"PCM   Board 1 Temp 2"      // 23 
          ,"PCM   Board 2 Temp 1"      // 24 
          ,"PCM   Board 2 Temp 2"      // 25 
          ,"PCM   Board 3 Temp 1"      // 26 
          ,"PCM   Board 3 Temp 2"      // 26 
          ,"PCM   Board 4 Temp 1"      // 28 
          ,"PCM   Board 4 Temp 2"      // 29 
          ,""                          // 30
          ,""                          // 31
};


static char *PowerFeatureBits[32] = {
           ""                   // 0
          ,"Controller Fail"    // 1
          ,""                   // 2
          ,""                   // 3
          ,"Video 1 Fail"       // 4
          ,"Video 2 Fail"       // 5
          ,"Video 3 Fail"       // 6
          ,"Video 4 Fail"       // 7
          ,""                   // 8
          ,""                   // 9
          ,""                   // 10 
          ,""                   // 11 
          ,"M1553 1 Fail"       // 12 
          ,"M1553 2 Fail"       // 13
          ,"M1553 3 Fail"       // 14
          ,"M1553 4 Fail"       // 15
          ,""                   // 16
          ,""                   // 17
          ,""                   // 18
          ,""                   // 19
          ,"PCM   1 Fail"       // 20 
          ,"PCM   2 Fail"       // 21 
          ,"PCM   3 Fail"       // 22
          ,"PCM   4 Fail"       // 23 
          ,""                   // 24
          ,""                   // 25
          ,""                   // 26
          ,""                   // 27
          ,""                   // 28
          ,""                   // 29
          ,""                   // 30
          ,""                   // 31
};

static char *EthernetFeatureBits[32] = {
           "BIT Failure"             // 0
          ,"Setup Failure"           // 1
          ,"Data Not Present"        // 2
          ,""                        // 3
          ,""                        // 4
          ,""                        // 5
          ,""                        // 6
          ,""                        // 7
          ,"NOT Ready"              // 8
          ,""                        // 9
          ,""                        // 10
          ,""                        // 11
          ,""                        // 12
          ,""                        // 13
          ,""                        // 14
          ,""                        // 15
          ,""                        // 16
          ,""                        // 17
          ,""                        // 18
          ,""                        // 19
          ,""                        // 20
          ,""                        // 21
          ,""                        // 22
          ,""                        // 23
          ,""                        // 24
          ,""                        // 25
          ,""                        // 26
          ,""                        // 27
          ,""                        // 28
          ,""                        // 29
          ,""                        // 30
          ,""                        // 31
};


static char *AudioVideoFeatureBits[32] = {
           "BIT Failure"             // 0
          ,"Setup Failure"           // 1
          ,"No Video Signal Error"   // 2
          ,"Bad Video Signal Error"  // 3
          ,"No Audio Signal Error"   // 4
          ,"Bad Audio Signal Error"  // 5
          ,"Channel Paused"          // 6
          ,""                        // 7
          ,"Not Making Packets"      // 8
          ,""                        // 9
          ,""                        // 10
          ,""                        // 11
          ,""                        // 12
          ,""                        // 13
          ,""                        // 14
          ,""                        // 15
          ,""                        // 16
          ,""                        // 17
          ,""                        // 18
          ,""                        // 19
          ,""                        // 20
          ,""                        // 21
          ,""                        // 22
          ,""                        // 23
          ,""                        // 24
          ,""                        // 25
          ,""                        // 26
          ,""                        // 27
          ,""                        // 28
          ,""                        // 29
          ,""                        // 30
          ,""                        // 31
};

static char *VideoOutFeatureBits[32] = {
           ""                   // 0
          ,"NoSignal   "        // 1
          ,""                   // 2
          ,""                   // 3
          ,""                   // 4
          ,""                   // 5
          ,""                   // 6
          ,""                   // 7
          ,""                   // 8
          ,""                   // 9
          ,""                   // 10
          ,""                   // 11
          ,""                   // 12
          ,""                   // 13
          ,""                   // 14
          ,""                   // 15
          ,""                   // 16
          ,""                   // 17
          ,""                   // 18
          ,""                   // 19
          ,""                   // 20
          ,""                   // 21
          ,""                   // 22
          ,""                   // 23
          ,""                   // 24
          ,""                   // 25
          ,""                   // 26
          ,""                   // 27
          ,""                   // 28
          ,""                   // 29
          ,"Disabled"           // 30
          ,""                   // 31
};


static char *m1553FeatureBits[32] = {
           "BIT Failure"                        // 0
          ,"Setup Failure"                      // 1
          ,"Response Timeout Error"             // 2
          ,"Format Error"                       // 3
          ,"Sync Type or Invalid Word Error"    // 4
          ,"Word Count Error"                   // 5
          ,"Channel Paused"                     // 6
          ,"Watch Word Error"                   // 7
          ,"No Signal"                          // 8
          ,""                                   // 9
          ,""                                   // 10 
          ,""                                   // 11
          ,""                                   // 12
          ,""                                   // 13
          ,""                                   // 14
          ,""                                   // 15
          ,""                                   // 16
          ,""                                   // 17
          ,""                                   // 18
          ,""                                   // 19
          ,""                                   // 20
          ,""                                   // 21
          ,""                                   // 22
          ,""                                   // 23
          ,""                                   // 24
          ,""                                   // 25
          ,""                                   // 26
          ,""                                   // 27
          ,""                                   // 28
          ,""                                   // 29
          ,""                                   // 30
          ,""                                   // 31
};

static char *PCMFeatureBits[32] = {
           "BIT Failure"               // 0
          ,"Setup Failure"             // 1
          ,"Bad Clock Failure"         // 2
          ,"Bad Data Failure"          // 3
          ,"Minor Frame Sync Failure"  // 4
          ,"Major Frame Sync Failure"  // 5
          ,"Bit Sync Lock Failure"     // 6
          ,"Watch Word Failure"        // 7
          ,"UART Frame Error"          // 8
          ,"UART Parity Error"         // 9
          ,"UART No Packets"           // 10
          ,"UART Dropped Packets"      // 11
          ,"Channel Paused"            // 12
          ,""                          // 13
          ,""                          // 14
          ,""                          // 15
          ,""                          // 16
          ,""                          // 17
          ,""                          // 18
          ,""                          // 19
          ,""                          // 20
          ,""                          // 21
          ,""                          // 22
          ,""                          // 23
          ,""                          // 24
          ,""                          // 25
          ,""                          // 26
          ,""                          // 27
          ,""                          // 28
          ,""                          // 29
          ,""                          // 30
          ,""                          // 31
};

static char *UARTFeatureBits[32] = {
           "BIT Failure"               // 0
          ,"Setup Failure"             // 1
          ,"UART Frame Error"          // 2
          ,"UART Parity Error"         // 3
          ,"UART No Packets"           // 4
          ,"UART Dropped Packets"      // 5
          ,""                          // 6
          ,""                          // 7
          ,""                          // 8
          ,""                          // 9
          ,""                          // 10
          ,""                          // 11
          ,""                          // 12
          ,""                          // 13
          ,""                          // 14
          ,""                          // 15
          ,""                          // 16
          ,""                          // 17
          ,""                          // 18
          ,""                          // 19
          ,""                          // 20
          ,""                          // 21
          ,""                          // 22
          ,""                          // 23
          ,""                          // 24
          ,""                          // 25
          ,""                          // 26
          ,""                          // 27
          ,""                          // 28
          ,""                          // 29
          ,""                          // 30
          ,""                          // 31
};

static char *DiscreteFeatureBits[32] = {
           "BIT Failure"             // 0
          ,"Setup Failure"           // 1
          ,""                        // 2
          ,""                        // 3
          ,""                        // 4
          ,""                        // 5
          ,""                        // 6
          ,""                        // 7
          ,""                        // 8
          ,""                        // 9
          ,""                        // 10
          ,""                        // 11
          ,""                        // 12
          ,""                        // 13
          ,""                        // 14
          ,""                        // 15
          ,""                        // 16
          ,""                        // 17
          ,""                        // 18
          ,""                        // 19
          ,""                        // 20
          ,""                        // 21
          ,""                        // 22
          ,""                        // 23
          ,""                        // 24
          ,""                        // 25
          ,""                        // 26
          ,""                        // 27
          ,""                        // 28
          ,""                        // 29
          ,""                        // 30
          ,""                        // 31
};


#endif
