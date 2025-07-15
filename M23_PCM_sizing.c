
#include "M23_Typedefs.h"
#include "M23_PCM_sizing.h"


////////////////////////////////////////////////////////////////
// BOOL CalcThroughPutMode_NEW(UINT32 bitRate, UINT32 *noOfMinors)
// input:	bit rate
// output:	No of data words per packet
// return:	status TRUE or FALSE 
////////////////////////////////////////////////////////////////
BOOL CalcThroughPutMode(UINT32 bitRate, UINT32 *noOfDataWordsPerPacket,UINT32 *noOfMinors)
{
    UINT32 Rate;
    UINT32 FPP;
    UINT32 DFW; 

    Rate = bitRate/160;

    if(Rate >= 8192)
    {
        DFW = 8000;
        FPP = 8000/500;
    }
    else
    {
        DFW = Rate;
        DFW = (Rate - (Rate %16));
        FPP = 1;
    }

    *noOfDataWordsPerPacket = DFW;
    *noOfMinors = FPP;


    return TRUE;

}
////////////////////////////////////////////////////////////////
// BOOL CalcPackedMode(UINT32 bitRate,UINT32 wordsPerMinor,  UINT32 minorPerMajor, 
//					BOOL *syncOnBool, UINT32 *majorsPerPacket)
// input:	bit rate, words per minor frame, No of minor frames per major frame 

////////////////////////////////////////////////////////////////
// BOOL CalcThroughPutMode(UINT32 bitRate, UINT32 *noOfDataWordsPerPacket)
// input:	bit rate
// output:	No of data words per packet
// return:	status TRUE or FALSE 
////////////////////////////////////////////////////////////////
BOOL CalcThroughPutMode_Old(UINT32 bitRate, UINT32 *noOfDataWordsPerPacket,UINT32 *noOfMinors)
{
    UINT32 TFW; 
    UINT32 FPP; 
    UINT32 DFW; 
    UINT32 Rate;

#if 0
    if(bitRate >= 200000)
    {
        TFW = 1024;
        FPP = (bitRate / 200000);
        if(FPP > 7)
        {
            FPP = 7;
        }

        DFW = TFW * FPP;
    }
    else
    {
        DFW = (bitRate * 0.8)/160;
        FPP = 1;
    }

    *noOfDataWordsPerPacket = DFW;
    *noOfMinors = FPP;
#endif

    if(bitRate >= 500000)
    {
        FPP = 8000/500;

        DFW = 8000;
    }
    else
    {
       // DFW = (bitRate * 0.8)/160;
        Rate = bitRate/10;
        DFW = (Rate - (Rate %16));
        FPP = 1;
    }

    *noOfDataWordsPerPacket = DFW;
    *noOfMinors = FPP;

    return TRUE;

}
////////////////////////////////////////////////////////////////
// BOOL CalcPackedMode(UINT32 bitRate,UINT32 wordsPerMinor,  UINT32 minorPerMajor, 
//					BOOL *syncOnBool, UINT32 *majorsPerPacket)
// input:	bit rate, words per minor frame, No of minor frames per major frame 
// output:	sync status: ON or OFF, No of major frames per packet,
// return:	status TRUE or FALSE 
////////////////////////////////////////////////////////////////
BOOL CalcPackedMode(UINT32 bitRate,UINT32 wordsPerMinor,  UINT32 minorPerMajor,UINT32 commonWordLength, 
					BOOL *MFE, UINT32 *TotalFrameWords,UINT32 *TotalMajorFrameWords,UINT32 *MFW,UINT32 SyncLength)
{
    UINT32 MaBL;
    UINT32 UWPMi;
    UINT32 TFW;
    UINT32 TMaW = 0;
    UINT32 WIMES;
 
   
    UINT32 Temp;
 
    float tempFloat; 


    if(SyncLength <= 16)
    {
        WIMES = wordsPerMinor;
    }
    else
    {
        WIMES = wordsPerMinor - 1;
    }

    MaBL = (SyncLength + (WIMES * commonWordLength) ) * minorPerMajor;

    UWPMi = (WIMES * commonWordLength)/16;

    if( (SyncLength  == 16) || (SyncLength == 32) )
    {
        if( (WIMES * commonWordLength) % 16)
        {
            UWPMi += 1;
        }
    }
    else
    {
#if 0
        if(SyncLength < 16)
            SyncLength = 16;
        else
            SyncLength = 32;
#endif
    }

    *MFW = UWPMi;

    Temp = SyncLength/16;

    if( SyncLength % 16 )
    {
        Temp += 1;
    }
     

    UWPMi = UWPMi + Temp;

    TFW = UWPMi + 5;

    if(minorPerMajor  > 1)
    {
        TMaW = TFW * minorPerMajor;
    }


    if( (minorPerMajor > 1) && (TMaW <= 8000) && ( (bitRate/11) > MaBL) )
    {
        *MFE = TRUE;
    }
    else
    {
        *MFE = FALSE;
    }


    *TotalFrameWords = TFW;
    *TotalMajorFrameWords = TMaW;

    return TRUE;
}

////////////////////////////////////////////////////////////////
// BOOL CalcUnPackedMode_New(UINT32 bitRate,UINT32 wordsPerMinor, UINT32 commonWordLen,UINT32 minorPerMajor, 
//					BOOL *syncOnBool, UINT32 *majorsPerPacket)
// input:	bit rate, words per minor frame, Common word length, No of minor frames per major frame 
// output:	sync status: ON or OFF, No of major frames per packet,
// return:	status TRUE or FALSE 
////////////////////////////////////////////////////////////////
BOOL CalcUnPackedMode_New(UINT32 bitRate,UINT32 wordsPerMinor,  UINT32 minorPerMajor,UINT32 commonWordLength, 
					BOOL *MFE, UINT32 *TotalFrameWords,UINT32 *TotalMajorFrameWords,UINT32 *MFW,UINT32 SyncLength)
{
    UINT32 MaBL;
    UINT32 UWPMi;
    UINT32 TFW;
    UINT32 TMaW = 0;
    UINT32 WIMES;
 
   
    UINT32 Temp;
 
    float tempFloat; 

    if(commonWordLength <= 16)
    {
        commonWordLength = 16;
    }
    else
    {
        commonWordLength = 32;
    }

    if(SyncLength <= 16)
    {
        WIMES = wordsPerMinor;
    }
    else
    {
        WIMES = wordsPerMinor - 1;
    }

    MaBL = (SyncLength + (WIMES * commonWordLength) ) * minorPerMajor;

    UWPMi = (WIMES * commonWordLength)/16;

    if( (SyncLength  == 16) || (SyncLength == 32) )
    {
        if( (WIMES * commonWordLength) % 16)
        {
            UWPMi += 1;
        }
    }
    else
    {
#if 0
        if(SyncLength < 16)
            SyncLength = 16;
        else
            SyncLength = 32;
#endif
    }

    *MFW = UWPMi;

    Temp = SyncLength/16;

    if( SyncLength % 16 )
    {
        Temp += 1;
    }
     

    UWPMi = UWPMi + Temp;

    TFW = UWPMi + 5;

    if(minorPerMajor  > 1)
    {
        TMaW = TFW * minorPerMajor;
    }

    if( (minorPerMajor > 1) && (TMaW <= 8000) && ( (bitRate/11) > MaBL) )
    {
        *MFE = TRUE;
    }
    else
    {
        *MFE = FALSE;
    }


    *TotalFrameWords = TFW;
    *TotalMajorFrameWords = TMaW;

    return TRUE;
}
////////////////////////////////////////////////////////////////
// BOOL CalcUnPackedMode(UINT32 bitRate,UINT32 wordsPerMinor, UINT32 commonWordLen,UINT32 minorPerMajor, 
//					BOOL *syncOnBool, UINT32 *majorsPerPacket)
// input:	bit rate, words per minor frame, Common word length, No of minor frames per major frame 
// output:	sync status: ON or OFF, No of major frames per packet,
// return:	status TRUE or FALSE 
////////////////////////////////////////////////////////////////

BOOL CalcUnPackedMode(UINT32 bitRate,UINT32 wordsPerMinor, UINT32 commonWordLen,UINT32 minorPerMajor, 
					BOOL *syncOnBool, UINT32 *majorsPerPacket,UINT32 *NumberOfMinors)
{
	UINT32 bitsPerMinor=0;
	UINT32 minorRate=0;
	UINT32 maxMinorsPerPacByTime=0;
	UINT32 maxMinorsPerPacBySize=0;
	UINT32 maxNoOfMinor=0;
	UINT32 numberOfMinor=0;
	UINT32 maxMajors=0;

	//bitsPerMinor= wordsPerMinor*16;
	bitsPerMinor= wordsPerMinor*commonWordLen;

	if (bitsPerMinor==0)
		return FALSE;

	minorRate= bitRate/bitsPerMinor;
	maxMinorsPerPacByTime= minorRate/11;

	//common word length is 16 or less, a word take 16 bits
	//otherwise, a word take 16*2= 32 bits
	if (commonWordLen<=16)
		//maxMinorsPerPacBySize= 15*1024*8/(16*wordsPerMinor+INTRAPACKET_HEADERSIZE);
		maxMinorsPerPacBySize= (14000*8)/(16*wordsPerMinor+INTRAPACKET_HEADERSIZE);
	else
		maxMinorsPerPacBySize= (14000*8)/(16*2*wordsPerMinor+INTRAPACKET_HEADERSIZE);

	if (maxMinorsPerPacByTime>maxMinorsPerPacBySize)
		maxNoOfMinor= maxMinorsPerPacBySize;
	else
		maxNoOfMinor= maxMinorsPerPacByTime;

//printf("Unpacked Minor per Major %d, Max Minor %d\r\n",minorPerMajor,maxNoOfMinor);

	if (minorPerMajor==1)
	{
		numberOfMinor= maxNoOfMinor;
                *NumberOfMinors = maxNoOfMinor;
		*syncOnBool= TRUE;
	}
	else if (minorPerMajor>1)
	{
		maxMajors= maxNoOfMinor/minorPerMajor;
		if (maxMajors==0)
                {
		    *syncOnBool= FALSE;
                }
		else
		{
		    *syncOnBool= TRUE;
		    *majorsPerPacket= maxMajors;
		}
                *NumberOfMinors = minorPerMajor;
	}
	else if (minorPerMajor==0)
		*syncOnBool= FALSE;

	return TRUE;
}
