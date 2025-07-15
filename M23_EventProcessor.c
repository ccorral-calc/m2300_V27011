#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <malloc.h>
#include "M23_Typedefs.h"
#include "M23_EventProcessor.h"
#include "M23_Controller.h"

pthread_t EventThread;

int CurrentNodeIndex;

int CurrentNode;
static int CurrentOffset;

static int CurrentStartEvent;
static int NumEvents;

typedef struct
{
    int         EventCount;
    int         RecordEvent;
    UINT8       FirstRecord;
    CH_10_EVENT Event[4096];
}EVENT_LOG;

typedef struct
{
    int    EventInfo;
    UINT32 AbsTimeLSW;
    UINT32 AbsTimeMSW;
    UINT32 Block;
}Nodes;

Nodes *NodeEntries;
//Nodes NodeEntries[20480];

typedef struct
{
    UINT64 Position;
    UINT32 Block;
    UINT32 Len;
}NODE_ENTRIES;

//NODE_ENTRIES node_entries[240];
NODE_ENTRIES *node_entries;
static int NodeCount;

#define MAX_NODE_BLOCKS 938 

unsigned char   *Buffer;

static EVENT_LOG Events;

void M23_Clear_Entries()
{
    int i;

    for(i = 0; i < 180;i++)
    {
        node_entries[i].Position = 0L;
        node_entries[i].Block = 0;
        node_entries[i].Len = 0;
    }
}

void M23_ClearNodes()
{
    M23_Clear_Entries();

}

void M23_ClearRecordNodes()
{
    M23_Clear_Entries(0);
    CurrentNode = 0;
}

void M23_SetFirstRecord()
{
    Events.FirstRecord = 1;
}

/**********************************************************************************************************************/
//                          void M23_InitializeNodes()
//
//    This function is called to Initialize The Node Entry Tables
//
//    Return Value:
//      0 = Passed
//      1 = Failed
//
/**********************************************************************************************************************/
void M23_InitializeNodes()
{

    //printf("Node mallocs %d %d\r\n",240 * sizeof(NODE_ENTRIES) ,32768 * sizeof(Nodes) );
    node_entries = malloc(240 * sizeof(NODE_ENTRIES) );
    NodeEntries  = (Nodes*)malloc( 32768 * sizeof(Nodes) );
    NodeCount    = 32768;

    M23_ClearNodes();

}

void M23_AddMoreNodeEntries()
{
        NodeEntries = (Nodes*)realloc( NodeEntries, (NodeCount * sizeof(Nodes)) + (32768 * sizeof(Nodes)) );
        NodeCount  = NodeCount + 32768;
}


void M23_UpdateRecordBlock(int start)
{
    CurrentOffset = start;
}

/**********************************************************************************************************************/
//                          int M23_FindTimeBlock()
//
//    This function is called to find the closest block to the time provided
//
//    Return Value:
//           int block;
//
/**********************************************************************************************************************/
int M23_FindTimeBlock(GSBTime   time)
{
    int    i;
    int    j;
    int    CurrentBlock = 0;
    int    Nodeblock;
    int    num_blocks;
    int    bytes_read;
    int    file_no;
    int    size;
    int    offset;
    int    debug;

    MicroSecTimeTag Time;

    GSBTime nodeTime;

    INT64 MyTime;
    INT64 IndexTime;
  
    Nodes node;

    M23_GetDebugValue(&debug);
#if 1
    //sGetPlayLocation(&CurrentBlock);

    Time_GSBToTimeTag( &time, &Time);

    MyTime = ((INT64)(Time.Upper) << 32);
    MyTime |= (INT64)Time.Lower;
 
#if 0
    file_no = M23_IsTimeFromStartOfFile(MyTime);
    if(file_no == -2)
    {
        CurrentBlock = M23_GetStartBlock();
        return(CurrentBlock);
    }
#endif

    /*Get the File Number that contains the Time we are searching for*/
    file_no = M23_GetFileFromTime(MyTime);

    if(file_no == -1)
    {
        /* The time is in the Current file*/
        for(i = 0; i < CurrentNode; i++)
        {
            if(debug)
                printf("Current Node - MSW 0x%x, LSW 0x%x,block %d\r\n",NodeEntries[i].AbsTimeMSW,NodeEntries[i].AbsTimeLSW,NodeEntries[i].Block);

            RecordBCD_To_GSB(NodeEntries[i].AbsTimeMSW,NodeEntries[i].AbsTimeLSW,&nodeTime);
            Time_GSBToTimeTag( &nodeTime, &Time);
            IndexTime = ((INT64)(Time.Upper) << 32);
            IndexTime |= (INT64)Time.Lower;
            if(MyTime <= IndexTime)
            {
                if(i != 0)
                {
                    CurrentBlock = NodeEntries[i-1].Block;
                } 
                else
                {
                    CurrentBlock = NodeEntries[i].Block;
                }
                i = CurrentNode;
             }
        }
    }
    else
    {
        /*Get The Node information for this file*/
        memset(Buffer,0x0,MAX_NODE_BLOCKS * M23_BLOCK_SIZE);

        /*We will now read the block that contains the Node Block*/
        Nodeblock = M23_GetNodeBlock(file_no);

        if(debug)
            printf("the Node Block is at %d,file %d\r\n",Nodeblock,file_no);

        if(Nodeblock < 128)
            return(0);

        for(j = 0; j < 10;j++)
        {
            DiskReadSeek(Nodeblock);
            bytes_read = DiskRead(Buffer,512);
            if(bytes_read == 0)
            {
                return(0);
            }

            offset = 12;
            if(strncmp(Buffer,"NODEFILE",8) == 0)                   
            {                       
                j = 10;
                memcpy(&num_blocks,(Buffer+8),4);                       
                size = (num_blocks * sizeof(Nodes) ) + 12;

                if(size > M23_BLOCK_SIZE)
                {
                    size -= M23_BLOCK_SIZE;
                    if(size % M23_BLOCK_SIZE)
                    {
                        size = (size/M23_BLOCK_SIZE) + 1;
                    }
                    else
                    {
                        size = size/M23_BLOCK_SIZE;
                    } 
                }

                for(i = 0; i< size;i++)
                {
                    //DiskReadSeek(Nodeblock + ((i+1)*M23_BLOCK_SIZE) );
                    DiskReadSeek(Nodeblock + (i+1));
                    bytes_read = DiskRead((Buffer + ((i+1) * M23_BLOCK_SIZE) ), M23_BLOCK_SIZE);
                    if(bytes_read == 0)
                    {
                        return(0);
                    }
                }
            }
            else
            {
                if(j > 8)
                    return(CurrentBlock);
            }
        }

        if(debug)
            printf("the Node Block is at %d,file %d,size %d,num blocks %d\r\n",Nodeblock,file_no,size,num_blocks);
        /*Default to the current play point*/
        Nodeblock = CurrentBlock;

        for(i = 0; i < num_blocks;i++)
        {
            memcpy(&node,(Buffer + offset),sizeof(Nodes));
            if(debug)
               printf("Node - MSW 0x%x, LSW 0x%x,block %d\r\n",node.AbsTimeMSW,node.AbsTimeLSW,node.Block);
        
            RecordBCD_To_GSB(node.AbsTimeMSW,node.AbsTimeLSW,&nodeTime);
            Time_GSBToTimeTag( &nodeTime, &Time);
            IndexTime = ((INT64)(Time.Upper) << 32);
            IndexTime |= (INT64)Time.Lower;
            if(MyTime <= IndexTime)
            {
                if(i != 0)
                {
                    CurrentBlock = Nodeblock;
                } 
                else
                {
                    CurrentBlock = node.Block;
                }
                i = num_blocks;
             }

             Nodeblock = node.Block;
             offset += sizeof(Nodes);
        }
    }

#endif

    return(CurrentBlock);
}
/**********************************************************************************************************************/
//                          int M23_FindEventBlock()
//
//    This function is called to find the closest block to the Event provided
//
//    Return Value:
//           int block;
//
/**********************************************************************************************************************/
int M23_FindEventBlock(int event,int occur)
{
    int i;
    int info;
    int event_num;
    int event_count;
    int CurrentBlock = 0;
    int debug;

    M23_GetDebugValue(&debug);
    GSBTime nodeTime;

    sGetPlayLocation(&CurrentBlock);

    for(i = 0; i < Events.EventCount;i++)
    {
        info = Events.Event[i].EventInfo;
        event_num   = (info & 0xFFF );
        //event_num = event_num + 1; //Events are stored (event - 1)
        event_num = event_num; //Events are stored (event - 1)
        event_count = ( (info >> 12) & 0xFFFF );

        if( (event == event_num) && (occur == event_count) )
        {
            RecordBCD_To_GSB(Events.Event[i].AbsTimeMSW,Events.Event[i].AbsTimeLSW,&nodeTime);
if(debug)
    printf("Event %d Occur is at 0x%x 0x%x\r\n",event_num,event_count,Events.Event[i].AbsTimeMSW,Events.Event[i].AbsTimeLSW);
            CurrentBlock = M23_FindTimeBlock(nodeTime);
        }
    }

    return(CurrentBlock);
}


/**********************************************************************************************************************/
//                          int M23_InitializeEventTable()
//
//    This function is called to see if this event is setup as a priority 2 event
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
void M23_QueueEvent()
{
    int fileStartBlock;

    fileStartBlock = M23_FindFirstInLastFile();
}

/**********************************************************************************************************************/
//                          int M23_InitializeEventTable()
//
//    This function is called to see if this event is setup as a priority 2 event
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
void M23_InitializeEventTable()
{
    int i;

    for(i = 0; i < MAX_NUM_EVENTS;i++)
    {
        EventTable[i].CalculexEvent = 0;
        EventTable[i].EventEnabled  = 0;
        EventTable[i].EventLimit    = 0;
        EventTable[i].EventCount    = 0;
        memset(EventTable[i].MeasurementSource,0x0,33);
        memset(EventTable[i].MeasurementLink,0x0,33);
        memset(EventTable[i].DataLink,0x0,33);

    }
}

/**********************************************************************************************************************/
//                          int M23_GetNumADCPEvents()
//
//    This function Will get the ADCP Events (2-32)
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/

int M23_GetNumADCPEvents()
{
    int i;
    int count = 0;

    for(i = 2; i < 33;i++) /*Check 2-32*/
    {
        if(EventTable[i].EventEnabled)
           count++;
    }

    return(count);
}

/**********************************************************************************************************************/
//                          int M23_SetLastRecordEvent()
//
//    This function is called to see if this event is setup as a priority 2 event
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
void M23_SetLastRecordEvent()
{
    Events.RecordEvent = Events.EventCount;
    //printf("The Last Record Event = %d\r\n",Events.RecordEvent);
}

/**********************************************************************************************************************/
//                          int M23_IsEventPriority2(int event)
//
//    This function is called to see if this event is setup as a priority 2 event
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
int M23_IsEventPriority2(int event)
{
    int state;
    int return_status = 1;

    M23_RecorderState(&state);
   
    if( (state != STATUS_RECORD) && (state != STATUS_REC_N_PLAY) )
    {
        if(EventTable[event].EventLimit == 0)
        {
            return_status = 0;
        }
        else
        {
            EventTable[event].EventLimit--;
        }
    }
    return(return_status);
}

/**********************************************************************************************************************/
//                          int M23_IsEventPriority3(int event)
//
//    This function is called to see if this event is setup as a priority 3 event
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
int M23_IsEventPriority3(int event)
{
    int state;
    int return_status = 1;

    M23_RecorderState(&state);
   
    if( (state != STATUS_RECORD) && (state != STATUS_REC_N_PLAY) )
    {
        return_status = 0;
    }

    return(return_status);
}

/**********************************************************************************************************************/
//                          int M23_IsEventPriority4(int event)
//
//    This function is called to see if this event is setup as a priority 4 event
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
int M23_IsEventPriority4(int event)
{
    int return_status = 1;

    if(EventTable[event].EventLimit == 0)
    {
        return_status = 0;
    }
    else
    {
        EventTable[event].EventLimit--;
    }

    return(return_status);
}

/**********************************************************************************************************************/
//                          int M23_IsEventPriority5(int event)
//
//    This function is called to see if this event is setup as a priority 5 event
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
int M23_IsEventPriority5(int event)
{
    int state;
    int return_status = 1;

    M23_RecorderState(&state);
   
    if((state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) )
    {
        if(EventTable[event].EventLimit == 0)
        {
            return_status = 0;
        }
        else
        {
            EventTable[event].EventLimit--;
        }
    }
    else
    {
        return_status = 0;
    }

    return(return_status);
}

/**********************************************************************************************************************/
//                          int m23_InitializeEvents()
//
//    This function is called to set all Event Variables to an Initial State
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
int m23_InitializeEvents()
{

    /*We begin events at every startup*/
    Events.EventCount  = 0;
 
    /*Log events until a Record is issued*/
    Events.RecordEvent = 0;

    /*Do Not Log Events until the First Recording*/
    Events.FirstRecord = 0;

    /*This is for Root Index Packet*/
    CurrentNodeIndex = 0;

    CurrentNode = 0;


    CurrentStartEvent = 0; 
    NumEvents = 0;

    Buffer = (unsigned char *)malloc(20480*25);

    return(0);
}

/**********************************************************************************************************************/
//                          int M23_EraseEvents()
//
//    This function is called to set all Event Variables to an Initial State
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
int m23_EraseEvents()
{
    int i;

    for(i = 0; i < MAX_NUM_EVENTS;i++)
    {
        EventTable[i].EventCount = 0;
    }

    /*We begin events at every startup*/
    Events.EventCount  = 0;
 
    /*Log events until a Record is issued*/
    Events.RecordEvent = 0;

    /*Do Not Log Events until the First Recording
    Events.FirstRecord = 0;*/

    /*This is for Root Index Packet*/
    CurrentNodeIndex = 0;

    CurrentNode = 0;

    CurrentStartEvent = 0;

    NumEvents = 0;

    M23_Clear_Entries();

    return(0);
}


/***************************************************************************************************************
*******/
//                          int m23_WriteEventToHost()
//
//    This function Will write a single Event Packet to Host
//
//
//    Return Value:
//      0 = Passed
//      1 = Failed
//
/***************************************************************************************************************
*******/
void m23_WriteEventToHost(CH_10_EVENT *event )
{
    int            length;
    int            i = 0;
    int            count = 0;
    UINT16         *ptmp16;
    UINT32         CSR;


    ptmp16 = (UINT16 *)event;
    length = sizeof(CH_10_EVENT);

    //printf("Length of Event Struct = %d\r\n",length);
    length += length % 2;
    /*Write the length to Host Channel CSR*/
    M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_LEN_CSR,length);

    /*Write the CSDW*/
    M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_CSDW,0x1);

    while(1)
    {
        /*Check the FIFO Empty Flag*/
        CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR);
        if(CSR & CONTROLLER_EVENT_CHANNEL_FIFO_EMPTY)
        {
           for(i = 0; i < length/2; i++)
           {
                M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR,*ptmp16);
                //M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR,SWAP_TWO(*ptmp16));
                //SWAP_TWO(*ptmp16);
                ptmp16++;
            }

             CSR = CONTROLLER_EVENT_CHANNEL_PKT_READY;
             M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_ID_CSR,CSR);

            break;
        }
        else
        {
            count++;
            if(count > 50)
            {
                //printf("Event Never Ready\r\n");
                break;
            }
            else
            {
                usleep(1000);
            }
        }
    }

    //m23_UpdateNodes(event->EventInfo);

}

/***************************************************************************************************************
*******/
//                          void m23_WriteEventsToHost()
//
//    This function Will write a single Event Packet to Host
//
//
//    Return Value:
//      0 = Passed
//      1 = Failed
//
/***************************************************************************************************************
*******/
void m23_WriteEventsToHost(int start, int end)
{
    int            length;
    int            i = 0;
    int            count = 0;
    int            readycount = 0;
    int            bytes_to_write;
    int            debug;
    UINT16         *ptmp16;
    UINT32         CSR;

    M23_GetDebugValue(&debug);

    ptmp16 = (UINT16 *)&Events.Event[start];
    length = (end - start) * sizeof(CH_10_EVENT);

    if(length < 1000)
    {
        bytes_to_write = length;
    }
    else
    {
        bytes_to_write = 1000;
    }

    CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);

    if( CSR & CONTROLLER_TIME_PACKET_OK)
    {
        /*Check the Busy Flag, We are sharing this channel with the CONTROLLER UART Channel*/
        CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR);

        while(1)
        {
            CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR);
            if( !(CSR & EVENT_UART_CHANNEL_BUSY) && (EventChannelInUse == 0) )
            {
                EventChannelInUse = 1;
                break;
            }
            else
            {
                usleep(100);
            }
    
        }

        while(1)
        {
            /*Check the FIFO Empty Flag*/
            CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR);
            if(CSR & CONTROLLER_EVENT_CHANNEL_FIFO_EMPTY)
            {
                /*Upper Word*/
                M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_LEN_CSR,length);

                /*Write the CSDW*/
                M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_CSDW,(end-start));

                /*Write The Channel ID and The Channel Type*/
                CSR = BROADCAST_MASK;  
                CSR |= (EVENT_TYPE << 16);
                M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_ID_CSR,CSR);

                /*Write The  Data*/
if(debug)
    printf("EVENT - Writing %d bytes to Event Channel\r\n",bytes_to_write);
                for(i = 0; i < bytes_to_write/2; i++)
                {
                    M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR,*ptmp16);
                    ptmp16++;
                }

                if(count == 0)
                {
                    CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_ID_CSR);
                    CSR |= CONTROLLER_EVENT_CHANNEL_PKT_READY;
                    M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_ID_CSR,CSR);
if(debug)
    printf("EVENT - Writing Packet Ready 0x%x to 0x%x\r\n",CSR,CONTROLLER_EVENT_CHANNEL_ID_CSR);
                }

                count += (i*2);

                if( count >= length) /*all the data has been written*/
                {
                    break;
                }
                else
                {
                    if( (length - count) < 1000)
                    {
                        bytes_to_write = length - count;
                    }
                    else
                    {
                        bytes_to_write = 1000;
                    }
                }

            }
            else
            {
                readycount++;

                if(readycount > 100)
                {
                    if(debug)
                        printf("Event FIFO Never Ready\r\n");
                    break;
                }
                else
                {
                    usleep(100);
                }
            }
        }

        EventChannelInUse = 0;
    }

}

/**********************************************************************************************************************/
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
/**********************************************************************************************************************/
int m23_UpdateEvent(int event)
{
    int         state;
    int         debug;
    UINT32      CSR;
    M23_CHANNEL  const *config;

    SetupGet(&config);

    M23_GetDebugValue(&debug);

    if( config->EventsAreEnabled == 0 )
    {
        return ERROR_INVALID_COMMAND;
    }

    if(Events.FirstRecord == 0)
    {
        return ERROR_INVALID_COMMAND;
    }


    /*If we get here, Rule #2 also is satisfied which is allow Priority 1 Events*/
    /*Now Check for Priorities- We only need to check pri. 4 and 5*/
    if(EventTable[event].EventPriority == EVENT_PRIORITY_2)
    {
        if( !M23_IsEventPriority2(event))
        {
            return(ERROR_INVALID_COMMAND); 
        }
    }
    else if(EventTable[event].EventPriority == EVENT_PRIORITY_3)
    {
        if( !M23_IsEventPriority3(event))
        {
            return(ERROR_INVALID_COMMAND); 
        }
    }
    else if(EventTable[event].EventPriority == EVENT_PRIORITY_4)
    {
        if( !M23_IsEventPriority4(event))
        {
            return(ERROR_INVALID_COMMAND); 
        }
    }
    else if(EventTable[event].EventPriority == EVENT_PRIORITY_5)
    {
        if( !M23_IsEventPriority5(event))
        {
            return(ERROR_INVALID_COMMAND); 
        }
    }

    EventTable[event].EventCount++;

    M23_WriteControllerCSR(CONTROLLER_MASTER_TIME_UPPER,CONTROLLER_READ_MASTER_TIME);
    Events.Event[Events.EventCount].IntraTimeMSW = M23_ReadControllerCSR(CONTROLLER_MASTER_TIME_UPPER);
    Events.Event[Events.EventCount].IntraTimeLSW = M23_ReadControllerCSR(CONTROLLER_MASTER_TIME_LOWER);

//printf("MSW 0x%x, LSW 0x%x\r\n",Events.Event[Events.EventCount].IntraTimeMSW,Events.Event[Events.EventCount].IntraTimeLSW);

   /*Lower*/
   Events.Event[Events.EventCount].AbsTimeLSW =  M23_ReadControllerCSR(CONTROLLER_TIMECODE_BCD_LOWER_CSR);

   /*Upper*/
    Events.Event[Events.EventCount].AbsTimeMSW= M23_ReadControllerCSR(CONTROLLER_TIMECODE_BCD_UPPER_CSR);

if(debug)
    printf("Updating Event %d\r\n",event -1);

    Events.Event[Events.EventCount].EventInfo = (event - 1);
    //Events.Event[Events.EventCount].EventInfo = event;
    Events.Event[Events.EventCount].EventInfo |= (EventTable[event].EventCount << 12);
        
    M23_RecorderState(&state);
    if( (state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) )
    {
            
        Events.Event[Events.EventCount].EventInfo |= (DURING_RECORD << 28);
        //m23_WriteEventToHost(&Events.Event[Events.EventCount]);
    }
    else
    {
        Events.Event[Events.EventCount].EventInfo |= (NOT_IN_RECORD << 28);
    }


    Events.EventCount++;
    NumEvents++;

    if(Events.EventCount > 4095)
    {
        Events.EventCount = 0;
    }

    return(NO_ERROR);
}
/**********************************************************************************************************************/
//                          int m23_Update_M1553_Event()
//
//    This function is called when a new M1553 event is issued, it appends an event entry
//    to the EventLog. It should be passed Block_Number where event occured, GSBTime and
//    event string 
//         
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
int m23_Update_M1553_Event(int event,UINT32 REL_Upper,UINT32 REL_Lower,UINT32 ABS_Upper,UINT32 ABS_Lower)
{
    int         state;
    UINT32      CSR;
    M23_CHANNEL const *config;

    SetupGet(&config);

    if( config->EventsAreEnabled == 0 )
    {
        return 0;
    }
#if 0
    if(Events.FirstRecord == 0)
    {
        return 0;
    }
#endif

    if(event < 1)
    {
        return 0;
    }

    /*If we get here, Rule #2 also is satisfied which is allow Priority 1 Events*/
    /*Now Check for Priorities- We only need to check pri. 4 and 5*/
    if(EventTable[event].EventPriority == EVENT_PRIORITY_2)
    {
        if( !M23_IsEventPriority2(event))
        {
            return(1); 
        }
    }
    else if(EventTable[event].EventPriority == EVENT_PRIORITY_3)
    {
        if( !M23_IsEventPriority3(event))
        {
            return(1); 
        }
    }
    else if(EventTable[event].EventPriority == EVENT_PRIORITY_4)
    {
        if( !M23_IsEventPriority4(event))
        {
            return(1); 
        }
    }
    else if(EventTable[event].EventPriority == EVENT_PRIORITY_5)
    {
        if( !M23_IsEventPriority5(event))
        {
            return(1); 
        }
    }

    EventTable[event].EventCount++;

    Events.Event[Events.EventCount].IntraTimeMSW = REL_Upper;
    Events.Event[Events.EventCount].IntraTimeLSW = REL_Lower;

//printf("MSW 0x%x, LSW 0x%x\r\n",Events.Event[Events.EventCount].IntraTimeMSW,Events.Event[Events.EventCount].IntraTimeLSW);

   /*Lower*/
   Events.Event[Events.EventCount].AbsTimeLSW =  ABS_Lower;

   /*Upper*/
    Events.Event[Events.EventCount].AbsTimeMSW =  ABS_Upper;

    Events.Event[Events.EventCount].EventInfo = (event - 1);
    //Events.Event[Events.EventCount].EventInfo = event;
    Events.Event[Events.EventCount].EventInfo |= (EventTable[event].EventCount << 12);
        
    M23_RecorderState(&state);
    if( (state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) )
    {
            
        Events.Event[Events.EventCount].EventInfo |= (DURING_RECORD << 28);
        //m23_WriteEventToHost(&Events.Event[Events.EventCount]);
    }
    else
    {
        Events.Event[Events.EventCount].EventInfo |= (NOT_IN_RECORD << 28);
    }

    M23_SetReverse();
    Events.EventCount++;
    NumEvents++;
    if(Events.EventCount > 4095)
    {
        Events.EventCount = 0;
    }

    return(0);
}

/**********************************************************************************************************************/
//                          int m23_UpdateRecordEvent()
//

/**********************************************************************************************************************/
//                          int m23_UpdateRecordEvent()
//
//    This function is called when a new Record event is issued, it add an event to the table 
//    and will write all the events that have happened since the last record command.
//         
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
int m23_UpdateRecordEvent( int event, GSBTime time)
{
    int         count = 0;
    int         debug;
    UINT32      Upper;
    UINT32      Lower;
    UINT32      CSR;

    M23_CHANNEL const *config;

    SetupGet(&config);

    M23_GetDebugValue(&debug);

    if( config->EventsAreEnabled == 0 )
    {
        return 0;
    }

    Events.FirstRecord = 1; //We have at least 1 record command

    EventTable[event].EventCount++;

    M23_WriteControllerCSR(CONTROLLER_MASTER_TIME_UPPER,CONTROLLER_READ_MASTER_TIME);
    Events.Event[Events.EventCount].IntraTimeMSW = M23_ReadControllerCSR(CONTROLLER_MASTER_TIME_UPPER);
    Events.Event[Events.EventCount].IntraTimeLSW = M23_ReadControllerCSR(CONTROLLER_MASTER_TIME_LOWER);

    Events.Event[Events.EventCount].AbsTimeLSW =  M23_ReadControllerCSR(CONTROLLER_TIMECODE_BCD_LOWER_CSR);
    Events.Event[Events.EventCount].AbsTimeMSW =  M23_ReadControllerCSR(CONTROLLER_TIMECODE_BCD_UPPER_CSR);

    Events.Event[Events.EventCount].EventInfo = (event - 1);
    Events.Event[Events.EventCount].EventInfo |= (EventTable[event].EventCount << 12);
    Events.Event[Events.EventCount].EventInfo |= (DURING_RECORD << 28);

    Events.EventCount++;

if(debug)
    printf("Updating Event %d\r\n",event -1);

    NumEvents++;

    if(Events.EventCount > 4095)
    {
        Events.EventCount = 0;
    }


    return(0);
}

/**********************************************************************************************************************/
//                          int m23_WriteSavedEvents()
//
//    This function is called to write Events That have occured while not in record 
//         
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
int m23_WriteSavedEvents( void )
{
    M23_CHANNEL  const *config;

    SetupGet(&config);

    if( config->EventsAreEnabled == 0 )
    {
        return 0;
    }

    if(Events.EventCount  == 0)
    {
        return 0;
    }

    if(Events.FirstRecord == 1)
    {
        //printf("Write Events for .Stop %d to .Record %d\r\n",Events.RecordEvent,Events.EventCount);
        if(Events.EventCount > Events.RecordEvent)
        {
            m23_WriteEventsToHost(Events.RecordEvent,Events.EventCount);
        }
    }

    Events.FirstRecord = 1; //We have at least 1 record command

}

/**********************************************************************************************************************/
//                          int m23_DisplayEvent()
//
//    This function is called to display Events in EventLog
//         
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
int m23_DisplayEvent( void )
{
    int    i;
    int    event_num;
    int    event_count;
    int    days;
    int    hours;
    int    minutes;
    int    seconds;
    int    info;
    int    start;
    UINT32 Upper;
    UINT32 Lower;
    char tmp[80];



    if(Events.EventCount > 20)
    {
       start = Events.EventCount - 20;
    }
    else
    {
        start = 0;
    }

    PutResponse(0,"                           Events\r\n");
    PutResponse(0,"---------------------------------------------------------------------\r\n");
    PutResponse(0," #   Num  Occur       Time               Description\r\n\r\n");
 
    for(i = start; i < Events.EventCount;i++)
    {
        memset(tmp,0x0,80);
        info = Events.Event[i].EventInfo;
        event_num   = (info & 0xFFF );
        event_num = event_num + 1; //Events are stored event - 1
        //event_num = event_num; 
        event_count = ( (info >> 12) & 0xFFFF );
        Upper = Events.Event[i].AbsTimeMSW;
        Lower = Events.Event[i].AbsTimeLSW;

//printf("Upper 0x%x, Lower 0x%x\r\n",Upper,Lower);
        days    = Upper & 0xF;
        days    += ((Upper>>4) & 0xF) * 10;
        days    += ((Upper>>8) & 0xF) * 100;

        //days    = (Upper *100) + (((Lower >> 28) & 0xF) *10) + ((Lower >> 24) & 0xF);
        hours   = (((Lower >> 28) & 0xf) * 10) + ((Lower >> 24) & 0xF);
        minutes = (((Lower >> 20) & 0xf) * 10) + ((Lower >> 16) & 0xF);
        seconds = (((Lower >> 12) & 0xf) * 10) + ((Lower >>  8)& 0xF);
        sprintf(tmp,"%03d  %03d   %03d   %03d-%02d:%02d:%02d.000  %s\r\n",i+1,event_num,event_count,days,hours,minutes,seconds, EventTable[event_num].EventDesc);

        PutResponse(0,tmp);
    }

    return(0);
}

int m23_GetEventBlock(int event,int occurence,int *block )
{
    int    i;
    int    event_num;
    int    event_count;
    int    days;
    int    hours;
    int    minutes;
    int    seconds;
    int    info;
    UINT32 Upper;
    UINT32 Lower;
    char tmp[80];

    PutResponse(0,"                           Events\r\n");
    PutResponse(0,"---------------------------------------------------------------------\r\n");
    PutResponse(0," #   Num  Occur       Time               Description\r\n\r\n");
 
 
    for(i = 0; i < Events.EventCount;i++)
    {
        memset(tmp,0x0,80);
        info = Events.Event[i].EventInfo;
        event_num   = (info & 0xFFF );
        event_num = event_num + 1; //Events are stored event - 1
        event_count = ( (info >> 12) & 0xFFFF );
        Upper = Events.Event[i].AbsTimeMSW;
        Lower = Events.Event[i].AbsTimeLSW;

//printf("Upper 0x%x, Lower 0x%x\r\n",Upper,Lower);
        days    = Upper & 0xF;
        days    += ((Upper>>4) & 0xF) * 10;
        days    += ((Upper>>8) & 0xF) * 100;

        //days    = (Upper *100) + (((Lower >> 28) & 0xF) *10) + ((Lower >> 24) & 0xF);
        hours   = (((Lower >> 28) & 0xf) * 10) + ((Lower >> 24) & 0xF);
        minutes = (((Lower >> 20) & 0xf) * 10) + ((Lower >> 16) & 0xF);
        seconds = (((Lower >> 12) & 0xf) * 10) + ((Lower >>  8)& 0xF);
        sprintf(tmp,"%03d  %03d   %03d   %03d-%02d:%02d:%02d.000  %s\r\n",i+1,event_num,event_count,days,hours,minutes,seconds, EventTable[event_num].EventDesc);

        PutResponse(0,tmp);
    }

    return(0);
}

/******************************************************************************/
//                      int M23_GetLastRoot(int *offset,int *pkt_len);
//
//    This function will Update the node Index when and event is written or when
//    a time code packet is requested.
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/******************************************************************************/
int M23_GetLastRoot(UINT64 *offset, int *pkt_len)
{
    int     i;
    int     block;
    UINT32  CSR;
    UINT32  PacketLen = 0;
    UINT32  PositionLSW;
    UINT32  PositionMSW;

    UINT64  Block = 0L;

    while(1)
    {

        CSR = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO);

        if(CSR & CONTROLLER_ROOT_DATA_READY)
        {
            /*Read Word Offset LSW*/
            M23_WriteControllerCSR(CONTROLLER_HOST_FIFO, ROOT_READ_STROBE);
            PositionLSW = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO_DATA);

            /*Read Word Offset MSW and LSW of Absolute time*/
            M23_WriteControllerCSR(CONTROLLER_HOST_FIFO, ROOT_READ_STROBE);
            CSR = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO_DATA);
            PositionMSW = (CSR & 0xFFFF);
            NodeEntries[CurrentNode].AbsTimeLSW = ((CSR >> 16) & 0xFFFF);

            /*Read Absolute Time MSW*/
            M23_WriteControllerCSR(CONTROLLER_HOST_FIFO, ROOT_READ_STROBE);
            CSR = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO_DATA);
            NodeEntries[CurrentNode].AbsTimeLSW |= ( (CSR & 0xFFFF) << 16);

            NodeEntries[CurrentNode].AbsTimeMSW |= (CSR >> 16);

            /*Read Packet Lenght [31..0]*/
            M23_WriteControllerCSR(CONTROLLER_HOST_FIFO, ROOT_READ_STROBE);
            CSR = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO_DATA);
            PacketLen = CSR;

            sCurrentBlock(&block);
            Block = PositionLSW;
            Block |= (((UINT64)PositionMSW) << 32);

            NodeEntries[CurrentNode].Block = (UINT32)( (Block/512) + block);
            block = NodeEntries[CurrentNode].Block;
            CurrentNode++;

            *pkt_len = PacketLen; //Change when I get the FIFO operation
            *offset  = Block; //Change when I get the FIFO operation
        }
        else
            break;

    }

    return(0);
}

/******************************************************************************/
//                      int M23_GetRoot_UpdateNodes(int *offset,int *pkt_len);
//
//    This function will Update the node Index when and event is written or when
//    a time code packet is requested.
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/******************************************************************************/
int M23_GetRoot_UpdateNodes(UINT64 *offset, int *pkt_len)
{
    int     i;
    int     block;
    int     debug;
    int     clear;
    UINT32  CSR;
    UINT32  PacketLen = 0;
    UINT32  PositionLSW;
    UINT32  PositionMSW;

    UINT32  CurrentPTR;

    UINT64  Block = 0L;
    UINT64  TempBlock = 0L;

    static int index = 0;

    M23_CHANNEL  const *config;

    SetupGet(&config);

    M23_GetDebugValue(&debug);

    *offset  = 0; 
    *pkt_len = 0;
 

    while(1)
    {

        CSR = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO);
  
        if(CSR & CONTROLLER_ROOT_DATA_READY)
        {
            /*Read Word Offset LSW*/
            M23_WriteControllerCSR(CONTROLLER_HOST_FIFO, ROOT_READ_STROBE);
            PositionLSW = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO_DATA);

            /*Read Word Offset MSW and LSW of Absolute time*/
            M23_WriteControllerCSR(CONTROLLER_HOST_FIFO, ROOT_READ_STROBE);
            CSR = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO_DATA);
            PositionMSW = (CSR & 0xFFFF);
            NodeEntries[CurrentNode].AbsTimeLSW = ((CSR >> 16) & 0xFFFF);

            /*Read Absolute Time MSW*/
            M23_WriteControllerCSR(CONTROLLER_HOST_FIFO, ROOT_READ_STROBE);
            CSR = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO_DATA);
            NodeEntries[CurrentNode].AbsTimeLSW |= ( (CSR & 0xFFFF) << 16);

            NodeEntries[CurrentNode].AbsTimeMSW |= (CSR >> 16);

            /*Read Packet Lenght [31..0]*/
            M23_WriteControllerCSR(CONTROLLER_HOST_FIFO, ROOT_READ_STROBE);
            CSR = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO_DATA);
            PacketLen = CSR;

            if(index == 240)
            {
                index = 0;
            }

            node_entries[index].Len = PacketLen;

            sCurrentBlock(&block);
            Block = PositionLSW;
            Block |= (((UINT64)PositionMSW) << 32);
            node_entries[index].Position = Block;

            NodeEntries[CurrentNode].Block = (UINT32)( (Block/512) + block);
            node_entries[index].Block = NodeEntries[CurrentNode].Block;
            index++;
          
            clear = 0;
            CurrentPTR = M23_ReadControllerCSR(IEEE1394B_RECORD_POINTER_OUT);

            if(NodeEntries[CurrentNode].Block < CurrentPTR)
            {
                *pkt_len = PacketLen; //Change when I get the FIFO operation
                *offset  = Block; //Change when I get the FIFO operation

#if 0
if(debug)
    printf("Setting PTR %lld,Block %d Len %d, Current Record %d\r\n",Block,NodeEntries[CurrentNode].Block,PacketLen,CurrentPTR);
#endif
                M23_Clear_Entries();
                index = 0;
            }
            else
            {
                for(i = 0; i < index;i++)
                {
                    if((node_entries[i].Block < CurrentPTR) && (node_entries[i].Block != 0) )
                    {
                        *pkt_len = node_entries[i].Len; //Change when I get the FIFO operation
                        *offset  = node_entries[i].Position; //Change when I get the FIFO operation
                        clear = 1;

#if 0
if(debug)
    printf("Prev - Setting PTR %lld,Block %d Len %d, Current Record %d\r\n",node_entries[i].Position,node_entries[i].Block,node_entries[i].Len,CurrentPTR);

#endif
                    }
                }
                if(clear)
                {
                    M23_Clear_Entries();
                    index = 0;
                }
            }
#if 0
if(debug)
{
    printf("Block %d ,Current %d Time %x %x\r\n",NodeEntries[CurrentNode].Block,CurrentPTR,NodeEntries[CurrentNode].AbsTimeMSW,NodeEntries[CurrentNode].AbsTimeLSW);


}
#endif

            CurrentNode++;

            if(CurrentNode >= NodeCount)
            {
                M23_AddMoreNodeEntries();
            }


            break;
        }
        else
        {
            break;
        }
    }


    return(0);
}
/******************************************************************************/
//                      int m23_WriteNodesFile();
//
//    This function will Update the node Index when and event is written or when
//    a time code packet is requested.
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/******************************************************************************/
void m23_WriteNodesFile()
{
    int     i;
    int     offset;
    int     block;
    int     size;

    M23_CHANNEL  const *config;


    SetupGet(&config);

    if(config->IndexIsEnabled)
    {
        /*Initialize Node block*/
        sCurrentBlock(&block);
        DiskWriteSeek(block);
        memset(Buffer,0xDEAD,MAX_NODE_BLOCKS * M23_BLOCK_SIZE);
        swrite( Buffer,512,1,0);

        sopenNode();

        memset(Buffer,0x0,MAX_NODE_BLOCKS * M23_BLOCK_SIZE);
        strncpy(Buffer,"NODEFILE",8);
        memcpy((Buffer + 8),&CurrentNode,4);
        offset = 12;

        for(i = 0; i < CurrentNode;i++)
        {
            memcpy((Buffer + offset) ,(UINT8*)&NodeEntries[i],sizeof(Nodes));
            offset += sizeof(Nodes);
        }

        size = (CurrentNode * sizeof(Nodes)) + 12;
        if(size % M23_BLOCK_SIZE)
        {
            size += (M23_BLOCK_SIZE - (size%M23_BLOCK_SIZE) );
        }

        /*Start of the Node File*/
        sUpdateNode(block);
        DiskWriteSeek(block);

        /*We Need to do 512 byte writes*/
        for(i = 0; i < (size/512);i++)
        {
            swrite( (Buffer + (i*512)),512,1,0);
            block++;
            DiskWriteSeek(block);
        }

        scloseNodesFile();
    }

    CurrentNode = 0;
}

/**********************************************************************************************************************/
//                          void m23_GetEventTime()
//
//    This function will return that ABS time from an occurence of a certain event.
//         
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/**********************************************************************************************************************/
void m23_GetEventTime( int EventNum,int Occurence, UINT32 *MSW, UINT32 *LSW )
{
    int    i;
    int    event_num;
    UINT32 info;

    for(i = 0; i < Events.EventCount;i++)
    {
        //PC - We might need to SWAP_FOUR
        info =  Events.Event[i].EventInfo;
        //info = SWAP_FOUR(info);
        event_num   = (info & 0xFFF );
        event_num = event_num + 1; //Events are stored event - 1
        if( (EventNum == event_num) && (Occurence == ( (info >> 12) & 0xFFFF) ) )
        {
            *MSW = Events.Event[i].AbsTimeMSW;
            *LSW = Events.Event[i].AbsTimeLSW;
            break;
        }
    }

}

void M23_EventProcessor()
{
    int count;
    int debug;
    int state;
    int end;
    int set;
    int nextStart;

    UINT32 CSR;


    while(1)
    {
        sleep(1);

        if(NumEvents > 0)
        {
            M23_RecorderState(&state);
            if (  (state == STATUS_RECORD ) || (state == STATUS_REC_N_PLAY))
            {
                while(1)
                {
                    CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);
                    if(CSR & CONTROLLER_TIME_PACKET_OK)
                    {
                        break;
                    }
                    else
                    {
                        count++;
                        if(count > 4)
                        {
                            //printf("Time Code Away Never set\r\n");
                            break;
                        }
                        else
                        {
                            usleep(500000);
                        }
                    }
                }

                if( (CurrentStartEvent+NumEvents) < 4095)
                {
                    M23_GetDebugValue(&debug);

                    if(NumEvents > 20)
                    {
                        end = CurrentStartEvent + 20;
                        set = NumEvents - 20;
                    }
                    else
                    {
                        end = CurrentStartEvent + NumEvents;
                        set = 0;
                    }
                  
                    if(debug)
                        printf("Writing Events : start %d end %d, Num Events %d\r\n",CurrentStartEvent,end,NumEvents);

                    m23_WriteEventsToHost(CurrentStartEvent,end);
                }
                else
                {
                    if( (4095 - CurrentStartEvent) > 20)
                    {
                        end = CurrentStartEvent + 20;
                        set = NumEvents - 20;
                        m23_WriteEventsToHost(CurrentStartEvent,end);
                    }
                    else
                    {
                        if(NumEvents > 20)
                        {
                            m23_WriteEventsToHost(CurrentStartEvent,4095);
                            set = 4095 - CurrentStartEvent;
                            end = 0;
                        }
                    }
                }

                NumEvents = set;
                //CurrentStartEvent = Events.EventCount;
                CurrentStartEvent = end;
                
            }
        }
    }

}


void PrintEventTable()
{
    int i;

    printf("Event Table from TMATS\r\n");
    for(i = 0;i < 10;i++)
    {
        printf("EventTable[%d]->Mode %d \r\n",i,EventTable[i].EventMode);
        printf("EventTable[%d]->Type %d \r\n",i,EventTable[i].EventType);
        printf("EventTable[%d]->EventName %s \r\n",i,EventTable[i].EventName);
        printf("EventTable[%d]->MeasurementSource %s \r\n",i,EventTable[i].MeasurementSource);
        printf("EventTable[%d]->MeasurementLink %s \r\n",i,EventTable[i].MeasurementLink);
        printf("EventTable[%d]->DataLink %s \r\n",i,EventTable[i].DataLink);
    }
}

void M23_StartEventThread()
{
    int status;
    int debug;

    M23_GetDebugValue(&debug);

    status = pthread_create(&EventThread, NULL,(void *) M23_EventProcessor, NULL);

    if(debug)
    {
        if(status == 0)
        {
            printf("Event Thread Created Successfully\r\n");
        }
        else
        {
            perror("Event Processor Create\r\n");
        }
    }
}

