
#include "M23_Typedefs.h"

#define INTRAPACKET_HEADERSIZE 80

BOOL CalcThroughPutMode(UINT32 bitRate,UINT32 *noOfDataWordsPerPacket,UINT32 *noOfMinors);
BOOL CalcPackedMode(UINT32 bitRate,UINT32 wordsPerMinor, UINT32 minorPerMajor, UINT32 commonWordLen,
					BOOL *MFE, UINT32 *TotalFrameWords,UINT32 *TotalMajorFrameWords,UINT32 *MFW,UINT32 SyncLength);
BOOL CalcUnPackedMode(UINT32 bitRate,UINT32 wordsPerMinor, UINT32 commonWordLen,UINT32 minorPerMajor, 
					BOOL *syncOnBool, UINT32 *majorsPerPacket,UINT32 *NumberOfMinors);
