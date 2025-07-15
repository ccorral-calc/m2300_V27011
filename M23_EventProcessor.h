#ifndef __M23_EVENTPROCESSOR_H__
#define __M23_EVENTPROCESSOR_H__

#include "M23_Controller.h"

#define MAX_EVENT_LOG_ENTRY_LENGTH 52
#define MAX_NUM_EVENTS 999 

#define EVENT_PRIORITY_1       1
#define EVENT_PRIORITY_2       2
#define EVENT_PRIORITY_3       3
#define EVENT_PRIORITY_4       4
#define EVENT_PRIORITY_5       5

#define EVENT_TYPE_EXTERNAL          1
#define EVENT_TYPE_MEAS_DISCRETE     2
#define EVENT_TYPE_MEAS_LIMIT        3
#define EVENT_TYPE_RECORDER          4
#define EVENT_TYPE_OTHER             5

//#pragma pack(1)

#define MAX_NODE_INDEX              5000 

typedef struct{
    UINT32 IntraTimeLSW;
    UINT32 IntraTimeMSW;
    UINT32 AbsTimeLSW;
    UINT32 AbsTimeMSW;
    UINT32 NodeIndexLSW;
    UINT32 NodeIndexMSW;
}NODE_INDEX;

typedef struct{
    UINT32     CSDW;
    NODE_INDEX nodes[5002];
    UINT32     RootIndexLSW;
    UINT32     RootIndexMSW;
}ROOT_INDEX;

ROOT_INDEX  root_packet;
extern int         CurrentNodeIndex;

#define NOT_IN_RECORD 0
#define DURING_RECORD 1

typedef struct{
    UINT32  EventCount;
    UINT32  EventLimit;
    UINT8   EventPriority;
    UINT8   EventType;
    UINT8   CalculexEvent;
    UINT8   EventEnabled;
    UINT8   EventMode;
    UINT8   EventIC;
    UINT8   DualEvent;
    UINT8   PurpleEvent;   //Used For Dual Events;
    UINT8   NoResponseEvent;
    UINT32  CW; //This is to compare No Resonse with AIM Fail
    UINT8   EventName[33];
    UINT8   MeasurementSource[33];
    UINT8   MeasurementLink[33];
    UINT8   DataLink[33];
    UINT8   EventDesc[257];
}EVENT_DESCRIPTION;


EVENT_DESCRIPTION EventTable[MAX_NUM_EVENTS];

typedef struct{
    UINT32 IntraTimeLSW;
    UINT32 IntraTimeMSW;
    UINT32 AbsTimeLSW;
    UINT32 AbsTimeMSW;
    UINT32 EventInfo;
}CH_10_EVENT;

typedef enum {
    EQUAL,
    NOT_EQUAL,
    LESS_EQUAL,
    GREATER_EQUAL,
    LESS,
    GREATER,
    LOR,
    LAND,
    BOR,
    BAND,
    BXOR,
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
    MOD
}EVENT_OPERATORS;
typedef struct PCM_Event
{
    UINT32 ChannelID;
    UINT32 DataWordNum;
    UINT32 Mask;
    UINT32 EventNum;
    UINT32 TableEntry;
    UINT32 BusNumber;
    UINT32 LowerEntry;
    UINT32 UpperEntry;
    UINT32 TotalMask;
    UINT16 ConversionIndex;
    UINT16 DiscreteValue;
    UINT8  EventEnable;
    UINT8  EventMode;
    UINT8  RTEventMode;
    UINT8  EventIC;
    UINT8  EventContinue;
    UINT8  EventType;
    UINT8  Capture1;
    UINT8  NotCapturedYet;
    UINT8  FragPosition;
    UINT8  IsDataConversion;
    UINT8  IsTrigger;
    UINT8  IsHealthEvent;
    UINT8  OnlyHealthEvent;
    UINT8  ConvType;
    UINT8  IsFirst;
    UINT8  MultiEvent;
}PCM_EVENT;

PCM_EVENT PCMEvents[MAX_NUM_EVENTS];

int PCMTableEntryNumber[16];
typedef struct PCM_WordDef
{
    UINT32 WordPos;
    UINT32 Mask;
    UINT8  FragPos;
}PCM_WORDDEF;

typedef struct PCM_Fragments
{
    UINT32      NumFragments;
    PCM_WORDDEF words[8];
}PCM_FRAGMENTS;

typedef struct PCM_EventDef
{
    UINT32        Locations;
    UINT8         IsTrigger;
    UINT8         TransOrder;
    char          MeasName[32];
    PCM_FRAGMENTS fragments[8];
}PCM_EVENTDEF;

typedef struct PCM_Def
{
    int            NumMeas;
    char           DataLink[32];
    PCM_EVENTDEF   Event[128];
}PCM_DEF;
/*PCM Event Definitions*/
extern PCM_DEF *PCMEventDef;


typedef struct M1553_Event
{
    UINT32 ChannelID;
    UINT32 DataWordNum;
    UINT32 Mask;
    UINT32 TotalMask;
    UINT32 EventNum;
    UINT32 TableEntry;
    UINT32 BusNumber;
    UINT32 LowerEntry;
    UINT32 UpperEntry;
    UINT16 CommandWord;
    UINT16 ConversionIndex;
    UINT16 DiscreteValue;
    UINT8  CW_Only;
    UINT8  EventEnable;
    UINT8  EventMode;
    UINT8  EventIC;
    UINT8  EventContinue;
    UINT8  EventType;
    UINT8  Capture1;
    UINT8  NotCapturedYet;
    UINT8  FragPosition;
    UINT8  IsDataConversion;
    UINT8  IsTrigger;
    UINT8  ConvType;
    UINT8  MultiEvent;

}M1553_EVENT;

M1553_EVENT M1553Events[MAX_NUM_EVENTS];

int TableEntryNumber[16];

typedef struct M1553_EventDef
{
    int  NumWords;
    int  IsTrigger;
    int  CW_Only;
    int  TransOrder[8];
    int  Mask[8];
    int  Position[8];
    int  WordNumber[8];
    char MeasName[32];
}M1553_EVENTDEF;

typedef struct M1553_Def
{
    int            NumMeas;
    int            TransRcv;
    int            SubAddr;
    int            RT;
    int            WordCount;
    M1553_EVENTDEF Event[16];
}M1553_DEF;
typedef struct M1553_Bus
{
    int            NumMsgs;
    char           BusName[32];
    M1553_DEF      Msg[32];
}M1553_BUS;

/*M1553 Event Definitions*/
extern M1553_BUS *EventDef;


typedef struct TimeMsgDef
{
    UINT16 Rel1;
    UINT16 Rel2;
    UINT16 Rel3;
    UINT16 Data[32];
}TIME_EVENT_MSG;

TIME_EVENT_MSG TimeEvent;


typedef struct EventMsgDef
{
    UINT16 Rel1;
    UINT16 Rel2;
    UINT16 Rel3;
    UINT16 TableEntry;
    UINT16 BusNumber;
    INT16  DataWord;
    UINT8  IsDataWord;
}EVENT_MSG;

/*C-d Specs used for Limit and Discrete Events*/
typedef struct Data_Conversion
{
    int             CW_Only;
    INT32           CurrentValue_1[8];
    INT32           CurrentValue_2[8];
    INT32           HighLimit;
    INT32           LowLimit;
    UINT32          ChanID1;
    UINT32          ChanID2;
    UINT32          EventIndex;
    UINT16          NumBeforeValid;
    UINT16          NumOperands;
    char            MeasName_1[32];
    char            MeasName_2[32];
    char            TriggerName[32];
    char            DerivedName[32];
    EVENT_OPERATORS operator;
    UINT8           IsPCM;
    UINT8           ConvType;
    UINT8           EventType;
    UINT8           EventIC;
    UINT8           EventMode;
    UINT8           InitialTrigger;
    INT32           HighThreshold;
    INT32           LowThreshold;
    M1553_DEF       Msg1;
    M1553_DEF       Msg2;
    PCM_DEF         pcm1;
    PCM_DEF         pcm2;
}DATA_CONVERSION;

/*Data Conversion Event Definitions*/
extern DATA_CONVERSION *dataConversion;



 
/******************************************************************************/
//                          int m23_InitializeEvents()
//
//    This function is called when after the Cartridge is connected
//    and STANAG is initialized. It loades the STANAG in memory to get the file information.
//    Once the information is loaded it performs three tasks
//    1. Reads the event table to EventLog structure if there is one at the end of last file
//    2. IF there is no event table at the end look for it in previous files untill it findes one
//       and store it to the EventLog
//    3. After the EventLog is loaded if the file is not the last one scan through 
//       all the remaining files and load remaining events
//         
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/******************************************************************************/
int m23_InitializeEvents(void);

/******************************************************************************/
//                          int m23_UpdateEvent()
//
//    This function is called when a new event is issued, it appends an event entry
//    to the EventLog. It should be passed Block_Number where event occured, GSBTime and
//    event string 
//         
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/******************************************************************************/
int m23_UpdateEvent( int event);

/******************************************************************************/
//                          int m23_EraseEvent()
//
//    This function is called to erase EventLog,
//         
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/******************************************************************************/
int m23_EraseEvent( void );

/******************************************************************************/
//                          int m23_DisplayEvent()
//
//    This function is called to display Events in EventLog
//         
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/******************************************************************************/
int m23_DisplayEvent( void );

#endif
