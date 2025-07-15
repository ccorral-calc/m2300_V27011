//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed
//    for any purpose except as specifically authorized in writing.
//
//     Name: M23_MP_Handler.c
//    Version: 1.0
//     Author:paulc
//
//    M2300 MP Board Handler. Provides read and write routines.
//
//    Revisions:
//

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include "M23_MP_Handler.h"
#include "M23_Typedefs.h"
#include "M2X_channel.h"
#include "M23_Utilities.h"
#include "M2X_Const.h"
#include "M23_Controller.h"
#include "M23_EventProcessor.h"
#include "M23_features.h"
#include "cxmp.h"
#include "M23_M1553_cmd.h"

UINT16 M23_M1553_IsDataConversion(UINT8 TableEntry,UINT8 BusNumber,UINT16 *WordNum,UINT16 *index,UINT16 *cw,UINT32 *Event,UINT16 *Mask)
{
    int i;
    int M1553Index = M23_GetM1553Index();

    for(i = 0; i < M1553Index;i++)
    {
        if( (M1553Events[i].TableEntry == (UINT32)TableEntry) && (M1553Events[i].BusNumber == (UINT32)BusNumber) )
        {
            if( M1553Events[i].IsDataConversion == 1 )
            {
                 *WordNum = M1553Events[i].DataWordNum;
                 *Event   = M1553Events[i].EventNum;
                 *index   = M1553Events[i].ConversionIndex;
                 *cw      = M1553Events[i].CommandWord;
                 *Mask    = (UINT16)M1553Events[i].Mask;
                 return(1);
            }
        }
    }

    return(0);
}

UINT16 M23_PCM_IsDataConversion(UINT8 TableEntry,UINT8 BusNumber,UINT16 *WordNum,UINT16 *index,UINT32 *Event,UINT8 *isFirst,UINT16 *Mask)
{
    int i;
    int PCMIndex = M23_GetPCMIndex();

    for(i = 0; i < PCMIndex;i++)
    {
        if( (PCMEvents[i].TableEntry == (UINT32)TableEntry) && (PCMEvents[i].BusNumber == (UINT32)BusNumber) )
        {
            if( PCMEvents[i].IsDataConversion == 1 )
            {
                 *WordNum = PCMEvents[i].DataWordNum;
                 *Event   = PCMEvents[i].EventNum;
                 *index   = PCMEvents[i].ConversionIndex;
                 *isFirst = PCMEvents[i].IsFirst;
                 *Mask    = (UINT16)PCMEvents[i].Mask;
                 return(1);
            }
        }
    }

    return(0);
}

UINT16 M23_M1553_IsTriggerEvent(UINT8 TableEntry,UINT8 BusNumber,UINT16 index)
{
    int i;
    int M1553Index = M23_GetM1553Index();

    for(i = 0; i < M1553Index;i++)
    {
        if( (M1553Events[i].TableEntry == (UINT32)TableEntry) && (M1553Events[i].BusNumber == (UINT32)BusNumber) )
        {
            if( (M1553Events[i].IsTrigger == 1) || ((dataConversion+index)->InitialTrigger == 1) )
            {
                 if((dataConversion+index)->InitialTrigger == 0)
                 {
                     (dataConversion+index)->InitialTrigger = 1;
                 }
                 return(1);
            }
        }
    }

    return(0);
}


UINT16 M23_PCM_IsTriggerEvent(UINT8 TableEntry,UINT8 BusNumber,UINT16 index)
{
    int i;
    int PCMIndex = M23_GetPCMIndex();

    for(i = 0; i < PCMIndex;i++)
    {
        if( (PCMEvents[i].TableEntry == (UINT32)TableEntry) && (PCMEvents[i].BusNumber == (UINT32)BusNumber) )
        {
            if( (PCMEvents[i].IsTrigger == 1) || ((dataConversion+index)->InitialTrigger == 1) )
            {
                 if((dataConversion+index)->InitialTrigger == 0)
                 {
                     (dataConversion+index)->InitialTrigger = 1;
                 }
                 return(1);
            }
        }
    }

    return(0);
}

UINT16 ReturnDataWord(UINT16 DataWord,UINT16 Mask)
{
    int    i;
    int    shift = 0;
    UINT16 DW = 0x0;
    UINT16 Bits;

    Bits = (DataWord & Mask);
    for(i = 0;i < 16;i++)
    {
        if( ((Mask >> i) & 0x1) == 1) //Bit of Interest
        {
            if( Bits & (1 << i) )
                DW |= (1<<shift);
            shift++;
        }
    } 

    return DW;
}

void M23_M1553_FillInDataWord(UINT16 index,UINT16 WordNum,UINT16 CW,UINT16 DataWord,UINT16 Mask)
{
    int    i;
    int    shift;
    int    debug;
    UINT32 EventIndex = (dataConversion+index)->EventIndex;

    UINT16 CW_1;
    UINT16 CW_2;
    UINT16 DW;

    M23_GetDebugValue(&debug);

    CW_1 = ( (dataConversion+index)->Msg1.RT << 11) ;
    CW_1 |= ((dataConversion+index)->Msg1.TransRcv << 10);
    CW_1 |= ((dataConversion+index)->Msg1.SubAddr << 5);
    CW_1 |= ((dataConversion+index)->Msg1.WordCount);

    CW_2 = ( (dataConversion+index)->Msg2.RT << 11) ;
    CW_2 |= ((dataConversion+index)->Msg2.TransRcv << 10);
    CW_2 |= ((dataConversion+index)->Msg2.SubAddr << 5);
    CW_2 |= ((dataConversion+index)->Msg2.WordCount);

//printf("index %d, CW 0x%x, CW1 0x%x,CW2 0x%x,WordNum %d, Data Word %d\r\n",index,CW,CW_1,CW_2,WordNum,DataWord);
    if(CW == CW_1)
    {
        for(i = 1; i < (dataConversion+index)->Msg1.Event[EventIndex].NumWords+1;i++)
        {
            if(WordNum == (dataConversion+index)->Msg1.Event[EventIndex].WordNumber[i])
            {
                DW = ReturnDataWord(DataWord,Mask);

                if(debug)
                    printf("[%d] CW 0x%x Fill In Current Value 1[%d] with 0x%x,Original DW 0x%x, Mask 0x%x\r\n",index,CW,(dataConversion+index)->Msg1.Event[EventIndex].Position[i],DW,DataWord,Mask);

                (dataConversion+index)->CurrentValue_1[(dataConversion+index)->Msg1.Event[EventIndex].Position[i]] = DW;
            }
        }
    }

    if(CW == CW_2)
    {
        for(i = 1; i < (dataConversion+index)->Msg2.Event[EventIndex].NumWords+1;i++)
        {
            if(WordNum == (dataConversion+index)->Msg2.Event[EventIndex].WordNumber[i])
            {

                DW = ReturnDataWord(DataWord,Mask);
                if(debug)
                    printf("[%d] CW 0x%x Fill In Current Value 2[%d] with 0x%x,Original DW 0x%x, Mask 0x%x\r\n",index,CW,(dataConversion+index)->Msg2.Event[EventIndex].Position[i],DW,DataWord,Mask);
                (dataConversion+index)->CurrentValue_2[(dataConversion+index)->Msg2.Event[EventIndex].Position[i]] = DW;
            }
        }
    }
   
}

UINT16 M23_PCMIsRVDT(UINT16 WordNum,UINT16 DataWord)
{
    UINT16 NewValue;
    BOOL   Comp = FALSE;

    if( (WordNum >= 34) && (WordNum <= 37) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 40) && (WordNum <= 43) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 46) && (WordNum <= 49) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 52) && (WordNum <= 55) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 58) && (WordNum <= 61) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 64) && (WordNum <= 67) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 70) && (WordNum <= 73) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 76) && (WordNum <= 79) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 82) && (WordNum <= 85) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 88) && (WordNum <= 91) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 94) && (WordNum <= 97) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 100) && (WordNum <= 103) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 106) && (WordNum <= 109) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 112) && (WordNum <= 115) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 118) && (WordNum <= 121) )
    {
        Comp = TRUE;
    }
    else if( (WordNum >= 124) && (WordNum <= 127) )
    {
        Comp = TRUE;
    }

    if(Comp == TRUE)
    {
        NewValue = (~DataWord) + 1;
        NewValue = -NewValue; 
    }
    else
    {
        NewValue = DataWord;
    }

    return(NewValue);

}

void M23_PCM_FillInDataWord(UINT16 index,UINT16 WordNum,INT16 DataWord,UINT8 isFirst,UINT16 Mask)
{
    int i;
    int j;
    int debug;
   
    INT16 DW;
    UINT16 NewDW;

    UINT32 EventIndex = (dataConversion+index)->EventIndex;


    M23_GetDebugValue(&debug);
    if(isFirst)
    {
        for(i = 1; i < (dataConversion+index)->pcm1.Event[EventIndex].Locations+1;i++)
        {
            for(j = 1; j < (dataConversion+index)->pcm1.Event[EventIndex].fragments[i].NumFragments+1;j++)
            {
                if(WordNum == (dataConversion+index)->pcm1.Event[EventIndex].fragments[i].words[j].WordPos);
                {
                    DW = ReturnDataWord(DataWord,Mask);
                    //DW = M23_PCMIsRVDT(WordNum,NewDW);
 //if(debug)
 // printf("PCM Fill In Current Value 1[%d] with %d,Original DW 0x%x, Mask 0x%x\r\n",(dataConversion+index)->pcm1.Event[EventIndex].fragments[i].words[j].FragPos,DW,NewDW,Mask);

                    (dataConversion+index)->CurrentValue_1[(dataConversion+index)->pcm1.Event[EventIndex].fragments[i].words[j].FragPos] = DW;
                    //(dataConversion+index)->CurrentValue_1[0] = DW;
                }
            }
        }
    }
    else
    {
        for(i = 1; i < (dataConversion+index)->pcm2.Event[EventIndex].Locations+1;i++)
        {
            for(j = 1; j < (dataConversion+index)->pcm2.Event[EventIndex].fragments[i].NumFragments+1;j++)
            {
                if(WordNum == (dataConversion+index)->pcm2.Event[EventIndex].fragments[i].words[j].WordPos);
                {
                    DW = ReturnDataWord(DataWord,Mask);
                    //DW = M23_PCMIsRVDT(WordNum,NewDW);
                    (dataConversion+index)->CurrentValue_2[(dataConversion+index)->pcm2.Event[EventIndex].fragments[i].words[j].FragPos] = DW;
                    //(dataConversion+index)->CurrentValue_2[0] = DW;
                }
            }
        }
    }
}

int M23_DoCalculation(UINT16 index,UINT16 Event,UINT8 isBus)
{
    int    i;
    int    j;
    int    debug;
    int    event = 0xFFFFFFFF;

    UINT32 EventIndex = (dataConversion+index)->EventIndex;

    INT32 result = 0;
    INT32 Data_1 = 0;
    INT32 Data_2 = 0;

    M23_GetDebugValue(&debug);
    if(isBus)
    {
        if( (dataConversion+index)->NumOperands == 1)
        {
            if((dataConversion+index)->EventMode == 4)
            {
                if( (dataConversion+index)->CurrentValue_1[0] > (dataConversion+index)->HighLimit)
                {
                    if( (dataConversion+index)->HighThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->HighThreshold = 1; 
                    (dataConversion+index)->LowThreshold = 0; 

                    return(event);
                }
                else
                {
                    (dataConversion+index)->HighThreshold = 0; 
                }

                if( (dataConversion+index)->CurrentValue_1[0] < (dataConversion+index)->LowLimit)
                {
                    if( (dataConversion+index)->LowThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->LowThreshold = 1; 
                    (dataConversion+index)->HighThreshold = 0; 

                    return(event);
                }
                else
                {
                    (dataConversion+index)->LowThreshold = 0; 
                }
            }
            else if((dataConversion+index)->EventMode == 5)
            {
                
                if( (dataConversion+index)->CurrentValue_1[0] > (dataConversion+index)->HighLimit)
                {

                    if( (dataConversion+index)->HighThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->HighThreshold = 1; 

                    return(event);

                }
                else
                {
                    (dataConversion+index)->HighThreshold = 0; 
                }

           
            }
            else if((dataConversion+index)->EventMode == 6)
            {
                if( (dataConversion+index)->CurrentValue_1[0] < (dataConversion+index)->LowLimit)
                {

                    if( (dataConversion+index)->LowThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->LowThreshold = 1; 

                    return(event);

                }
                else
                {
                    (dataConversion+index)->LowThreshold = 0; 
                }
            }
        }
        else
        {
            //if( strcmp((dataConversion+index)->Msg1.Event[EventIndex].MeasName,(dataConversion+index)->Msg2.Event[EventIndex].MeasName) == 0) //This is compare to Constant

            if( strncmp((dataConversion+index)->MeasName_1,(dataConversion+index)->MeasName_2,strlen((dataConversion+index)->MeasName_1) - 1) == 0) //This is compare to Constant
            {
                if( (dataConversion+index)->CurrentValue_1[0] == (dataConversion+index)->HighLimit)
                {
                    if( (dataConversion+index)->HighThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->HighThreshold = 1; 

                     return(event);
                }
                else
                {
                    (dataConversion+index)->HighThreshold = 0; 
                }
                 
            }
            else
            {
                    Data_1 = (dataConversion+index)->CurrentValue_1[0];
                    Data_2 = (dataConversion+index)->CurrentValue_2[0];
                    if(debug)
                        printf("Data 1 %d  Data 2 %d\r\n",Data_1,Data_2);
#if 0
                for(i = 0; i < (dataConversion+index)->Msg1.Event[EventIndex].NumWords;i++)
                {
                    Data_1 |= (((dataConversion+index)->CurrentValue_1[i]) >> i);
                    if(debug)
                       printf("Data_1 -> 0x%x\r\n",Data_1); 
                }

                for(i = 0; i < (dataConversion+index)->Msg2.Event[EventIndex].NumWords;i++)
                {
                    Data_2 |= (((dataConversion+index)->CurrentValue_2[i]) >> i);
                    if(debug)
                        printf("Data_2 -> 0x%x\r\n",Data_2); 
                }
#endif
            }
        }
    }
    else
    {
        if( (dataConversion+index)->NumOperands == 1)
        {
            if((dataConversion+index)->EventMode == 4)
            {
                if( (dataConversion+index)->CurrentValue_1[0] > (dataConversion+index)->HighLimit)
                {
                    if( (dataConversion+index)->HighThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->HighThreshold = 1; 
                    (dataConversion+index)->LowThreshold = 0; 

                    return(event);
                }
                else
                {
                    (dataConversion+index)->HighThreshold = 0; 
                }
                if( (dataConversion+index)->CurrentValue_1[0] < (dataConversion+index)->LowLimit)
                {
                    if( (dataConversion+index)->LowThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->LowThreshold = 1; 
                    (dataConversion+index)->HighThreshold = 0; 

                    return(event);
                }
                else
                {
                    (dataConversion+index)->LowThreshold = 0; 
                }
            }
            else if((dataConversion+index)->EventMode == 5)
            {
                if( (dataConversion+index)->CurrentValue_1[0] > (dataConversion+index)->HighLimit)
                {

                    if( (dataConversion+index)->HighThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->HighThreshold = 1; 

                    return(event);

                }
                else
                {
                    (dataConversion+index)->HighThreshold = 0; 
                }
           
            }
            else if((dataConversion+index)->EventMode == 6)
            {
                if( (dataConversion+index)->CurrentValue_1[0] < (dataConversion+index)->LowLimit)
                {

                    if( (dataConversion+index)->LowThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->LowThreshold = 1; 

                    return(event);

                }
                else
                {
                    (dataConversion+index)->LowThreshold = 0; 
                }
            }
        }
        else
        {
            //if( strcmp((dataConversion+index)->pcm1.Event[EventIndex].MeasName,(dataConversion+index)->pcm2.Event[EventIndex].MeasName) == 0) //This is compare to Constant
            if( strncmp((dataConversion+index)->MeasName_1,(dataConversion+index)->MeasName_2,strlen((dataConversion+index)->MeasName_1) - 1) == 0) //This is compare to Constant
            {
                if( (dataConversion+index)->CurrentValue_1[0] == (dataConversion+index)->HighLimit)
                {
                    if( (dataConversion+index)->HighThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->HighThreshold = 1; 

                     return(event);
                }
                else
                {
                    (dataConversion+index)->HighThreshold = 0; 
                }
                 
            }
            else
            {
                for(i = 1; i < (dataConversion+index)->pcm1.Event[EventIndex].Locations+1;i++)
                {
                    for(j = 1; j < (dataConversion+index)->pcm1.Event[EventIndex].fragments[i].NumFragments+1;j++)
                    {
                        Data_1 |= (((dataConversion+index)->CurrentValue_1[j-1]) >> (j-1));
                    }
                }

                for(i = 0; i < (dataConversion+index)->Msg2.Event[EventIndex].NumWords;i++)
                {
                    for(j = 1; j < (dataConversion+index)->pcm2.Event[EventIndex].fragments[i].NumFragments+1;j++)
                    {
                        Data_2 |= (((dataConversion+index)->CurrentValue_2[j-1]) >> (j-1));
                    }
                }
            }
        }
    }

    if( (Data_1 == 0xdeadbeef) || (Data_2 == 0xdeadbeef) )
        return 0xFFFFFFFF;

    (dataConversion+index)->InitialTrigger = 0xFF;

    switch((dataConversion+index)->operator)
    {
        case EQUAL:
            if(Data_1 == Data_2)
                result = 1;
            else
                result = 0;
            break;

        case NOT_EQUAL:
            if(Data_1 != Data_2)
                result = 1;
            else
                result = 0;
            break;
        case LESS_EQUAL:
            if(Data_1 <= Data_2)
                result = 1;
            else
                result = 0;
            break;
        case GREATER_EQUAL:
            if(Data_1 >= Data_2)
                result = 1;
            else
                result = 0;
            break;
        case LESS:
            //if(debug)
            //    printf("%d < %d\r\n",Data_1,Data_2);
            if(Data_1 < Data_2)
                result = 1;
            else
                result = 0;
            break;
        case GREATER:
            if(Data_1 > Data_2)
                result = 1;
            else
                result = 0L;
            break;
        case LOR:
            //if(debug)
             //   printf("%d || %d,%d\r\n",Data_1,Data_2,result);

            if(Data_1 || Data_2)
                result = 1;
            else
                result = 0;
            break;
        case LAND:
            if(Data_1 && Data_2)
                result = 1;
            else
                result = 0L;
            break;
        case BOR:
            result = Data_1 | Data_2;
            break;
        case BAND:
            result = Data_1 & Data_2;
            break;
        case BXOR:
            result = Data_1 ^ Data_2;
            break;
        case ADD:
            result = Data_1 + Data_2;
            //if(debug)
             //   printf("Result of Addition %d\r\n",result);
            break;
        case SUBTRACT:
            result = Data_1 - Data_2;
          //  if(debug)
           //     printf("Result of Subtract %d\r\n",result);
            break;
        case MULTIPLY:
            result = Data_1 * Data_2;
        //    if(debug)
         //       printf("Result of Multiply %d\r\n",result);
            break;
        case DIVIDE:
            if(Data_2 > 0)
            {
                result = Data_1 / Data_2;
            }
            else
            {
                result = 0;
            }
            //if(debug)
              //  printf("Result of Divide %d\r\n",result);
            break;
        case MOD:
            result = Data_1 % Data_2;
            //if(debug)
             //   printf("Result of MOD %d\r\n",result);
            break;
    }

    switch((dataConversion+index)->operator)
    {
        case EQUAL:
        case NOT_EQUAL:
        case LESS_EQUAL:
        case GREATER_EQUAL:
        case LESS:
        case GREATER:
        case LOR:
        case LAND:
            if((dataConversion+index)->EventMode == 2)
            {
                if(result == 1)
                    event = (int)Event;
            }
            else if((dataConversion+index)->EventMode == 3)
            {
                if(result == 0)
                    event = (int)Event;
            }
            break;
        case BOR:
        case BAND:
        case BXOR:
            if((dataConversion+index)->EventMode == 2)
            {
                if(result)
                    event = (int)Event;
            }
            else if((dataConversion+index)->EventMode == 3)
            {
                if(result == 0)
                    event = (int)Event;
            }
            break;
        case ADD:
        case SUBTRACT:
        case MULTIPLY:
        case DIVIDE:
        case MOD:
            if((dataConversion+index)->EventMode == 5)
            {
                if( result > (dataConversion+index)->HighLimit)
                {
                    if( (dataConversion+index)->HighThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->HighThreshold = 1; 

                }
                else
                {
                    (dataConversion+index)->HighThreshold = 0; 
                }
           
            }
            else if((dataConversion+index)->EventMode == 6)
            {
                if( result < (dataConversion+index)->LowLimit)
                {
                    if( (dataConversion+index)->LowThreshold == 0)
                        event = (int)Event;

                    (dataConversion+index)->LowThreshold = 1; 

                }
                else
                {
                    (dataConversion+index)->LowThreshold = 0; 
                }
           
            }
    }

    return(event);
}
