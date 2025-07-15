/***********************************************************************
 *
 *     Copyright (c) CALCULEX, Inc., 2004
 *
 ***********************************************************************
 *
 *   Filename :   tmatsparse.c
 *   Author   :   bdu
 *   Revision :   1.00
 *   Date     :   May 27, 2003
 *   Product  :   Test TMATS parsing
 *
 *   Revision :
 *
 *   Description:   This will parse a TMATS in any given order of channels.
 *
 ***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "M23_Controller.h"
#include "M23_Typedefs.h"
#include "M2X_channel.h"
#include "M2X_Tmats.h"
#include "M2X_Const.h"
#include "M23_Utilities.h"
#include "M23_MP_Handler.h"
#include "M23_features.h"
#include "M23_EventProcessor.h"
#include "version.h"



static char *TmatsGetBuffer;
static char *TmatsRecordBuffer;
char        *TmatsBuffer;

static BOOL Tmats_Loaded_To_Read = FALSE;

M1553_BUS *EventDef;
PCM_DEF   *PCMEventDef;
DATA_CONVERSION *dataConversion;


INDEX_CHANNEL indexChannel;

int  get_new_line(char *fileContents, char *line);
int split_line(char *line, ctoken *token, int *numTokens);
long int Binary2UL(char *str);



static int uart;
static int pulse;

int    TmatsVersion;
UINT16 TmatsChksum;
UINT16 TmatsChksumExtended;

int VoiceInfo[2];
int M1553Info[32];
int VideoInfo[32];
int PCMInfo[32];
int TimeInfo;
int EthernetInfo;
int VideoIds[32];

static const char *Months[] =
{
    "EMPTY",
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

void M23_AllocateTmats()
{
    TmatsGetBuffer       = (char *)malloc(64*1024);
    TmatsBuffer          = (char *)malloc(64*1024);
    TmatsRecordBuffer    = (char *)malloc(64*1024);

}

int ProcessTmats(int setup,M23_CHANNEL *m23_chan)
{
    FILE              *fp;
    ctoken            tokens[10];
    int               numTokens;
    int               i;
    int               j;
    int               ret;
    int               status;
    int               copy = 0;
    int               line_length;
    int               vindex = 0;
    int               mindex = 0;
    int               aindex = 0;
    int               pindex = 0;
    int               index = 0;
    int               bindex;
    int               eindex;
    int               debug = 0;
    int               tmp1;
    int               len = 0;
    int               tmp2;
    int               tmp3;
    int               tmp4;
    int               ContUart = 0;
    int               length;
    char              tmp[20];
    char              tmpLine[100];
    char              line[100];
    char              *curLine;
    char              tmats_filename[32];

    UINT16 *ptmp16;




    M23_GetDebugValue(&debug);

    AutoRecord = 0;
    Video_Output_Format = NTSC;
    Discrete_Disable = 0;

    VoiceInfo[0] = 0xFF;
    VoiceInfo[1] = 0xFF;

    //M23_SetRemoteValue(0);
    //IEEE1394_Bus_Speed = BUS_SPEED_S800;

    TmatsVersion = 0x0;

    m23_chan->NumBuses = 0;

    for(i = 0;i < 32;i++)
    {
        M1553Info[i] = 0xFF;
        VideoInfo[i] = 0xFF;
        PCMInfo[i] = 0xFF;
    }
    TimeInfo = 0xFF;

    EthernetInfo = 0xFF;

    memset( TmatsBuffer, 0, 64*1024);
    memset( TmatsGetBuffer, 0,64*1024);

    if(LoadedTmatsFromCartridge == 0)
    {
        sprintf( tmats_filename, "./Setup%d.tmt", setup );
        fp = fopen( tmats_filename, "rb" );
        if ( !fp)
        {
            if(debug)
                printf( "SetupChannels: TMATS file %s does not exist\r\n", tmats_filename );
    
            return M23_ERROR;
        }

        status = fread( TmatsBuffer, 1, 64*1024,fp );
    }
    else
    {
        strcpy(TmatsBuffer , TmatsRecordBuffer);
    }


    /*First We Need to Find All CDT*/
    memset(line,0x0,100);
    curLine = TmatsBuffer;
    while((line_length = get_new_line(curLine, line)) > 1)
    {
        if( (line[0] == 0xD) || (line[0] == 0xA) )
        {
        }
        else
        {
            line[line_length-2] = '\0';  /* chop off ";" */
            ret = split_line(line, tokens, &numTokens);
            if(ret == 0)
            {
                if(numTokens < 1)
                {
                    printf("Error in TMATS file - 1 CDT\r\n");
                    return(1);
                }
            }
            else
            {
                printf("Error in TMATS file - 2 CDT\r\n");
                return(1);
            }
            if(tokens[0].txt[0] ==  'R')
            {
                if(strncmp(tokens[1].txt, "CDT",3) == 0)
                {
                    if(strncmp(tokens[2].txt, "VIDIN", 5) == 0)
                    {
                        VideoInfo[vindex++] = atoi(&tokens[1].txt[4]);
                    }
                    else if( (strncmp(tokens[2].txt, "1553IN", 6) == 0) || (strncmp(tokens[2].txt,"DISIN",5) == 0) )
                    {
                        M1553Info[mindex++] = atoi(&tokens[1].txt[4]);
                    }
                    else if( (strncmp(tokens[2].txt, "PCMIN", 5) == 0) || (strncmp(tokens[2].txt,"UARTIN",6) == 0) || (strncmp(tokens[2].txt,"PULSEIN",7) == 0) )
                    {
                        PCMInfo[pindex++] = atoi(&tokens[1].txt[4]);
                    }
                    else if(strncmp(tokens[2].txt, "ANAIN", 5) == 0)
                    {
                        VoiceInfo[aindex++] = atoi(&tokens[1].txt[4]);
                    }
                    else if(strncmp(tokens[2].txt, "TIMEIN", 6) == 0)
                    {
                        TimeInfo  = atoi(&tokens[1].txt[4]);
                    }
                    else if(strncmp(tokens[2].txt, "ETHIN", 3) == 0)
                    {
                        EthernetInfo  = atoi(&tokens[1].txt[4]);
                    }

                }
            }
        }

        curLine += line_length;
    }


    /*First Pass is to Get all Non Channel Specific attributes*/
    memset(line,0x0,100);
    curLine = TmatsBuffer;
    while((line_length = get_new_line(curLine, line)) > 1)
    {
        copy = 0;
        memset(tmpLine,0x0,100); 
        strncpy(tmpLine,line,line_length);
        if( (line[0] == 0xD) || (line[0] == 0xA) )
        {
        }
        else
        {
            line[line_length-2] = '\0';  /* chop off ";" */
            ret = split_line(line, tokens, &numTokens);
            if(ret == 0)
            {
                if(numTokens < 1)
                {
                    printf("Error in TMATS file - 1 NON\r\n");
                    return(1);
                }
            }
            else
            {
                printf("Error in TMATS file - 2 NON\r\n");
                return(1);
            }

            /* decode the generic header part */
            if(tokens[0].txt[0] == 'G')
            {
                if(strncmp(tokens[1].txt, "PN",2) == 0)        /* G\PN -- Program Name */
                {
                    copy = 1;
                }
                else if(strncmp(tokens[1].txt, "106",3) == 0)       /* G\106 -- IRIG 106 rev level */
                {
                    copy = 1;
                }
                if(strncmp(tokens[1].txt, "RN",2) == 0)        /* G\RN -- Revision Number*/
                {
                    copy = 1;
                }
            }
            else if(tokens[0].txt[0] == 'V' )
            {
                if(strncmp(tokens[1].txt, "RLS",3) == 0)     /* V-x\RLS\RMM; Remote Receiver */
                {
                    copy = 1;
                }
                else if(strncmp(tokens[1].txt, "RTS",3) == 0)  /* V-x\RTS\ETO; External Time Only*/
                {
                    copy = 1;
                }
                else if(strncmp(tokens[1].txt, "SDR",3) == 0)  /* V-x\SDR\BS; 1394 Bus Speed*/
                {
                    //copy = 1;
                    copy = 0;
                }
                else if(strncmp(tokens[1].txt, "VOF",3) == 0)  /* V-x\VOF\OST; Video Output Format*/
                {
                    copy = 1;
                }
                else if(strncmp(tokens[1].txt, "DCD",3) == 0)  /* V-x\DCD\D; Discrete Disable*/
                {
                    copy = 1;
                }
                else if(strncmp(tokens[1].txt, "RIE",3) == 0)  /* V-x\RIE\D; Internal Events Disable*/
                {
                    copy = 1;
                }
                else if( (strncmp(tokens[1].txt, "MRC", 4) == 0) ) /*V-x\MRC; M1553 Pause Resume*/
                {
                    copy = 1;
                }
                else if(strncmp(tokens[2].txt, "RRE",3) == 0)     /* V-x\CLX\RRE; Remote Receiver */
                {
                    copy = 1;
                }
                else if( (strncmp(tokens[2].txt, "RID", 3) == 0) ) /*V-x\CLX\RID;  M1553 RT Control*/
                {
                    copy = 1;
                }
                else if( (strncmp(tokens[2].txt, "DATE", 4) == 0) ) /*V-x\CLX\DATE; Set Recorder Date*/
                {
                    copy = 1;
                }
                else if( (strncmp(tokens[2].txt, "SO1", 3) == 0) ) /*V-x\CLX\SO1; Serial 1 Data out*/
                {
                    copy = 1;
                }
                else if( (strncmp(tokens[2].txt, "SO2", 3) == 0) ) /*V-x\CLX\SO2; Serial 1 Data out*/
                {
                    copy = 1;
                }
            }
            else if(tokens[0].txt[0] == 'C' )
                copy = 1;

        }

        curLine += line_length;

        if(copy == 1)
        {
            strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
            len += line_length;
            memset(line,0x0,100);
        }
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);

    }


//    strncpy(&TmatsGetBuffer[len],"G\\COM:===\r\n",11);
 //   len += 11;


    /*Second Pass is to Get Time channel Specific attributes*/
    /*First Get TK attrubure*/
    memset(line,0x0,100);
    curLine = TmatsBuffer;
    while((line_length = get_new_line(curLine, line)) > 1)
    {
        copy = 0;

        memset(tmpLine,0x0,100); 
        strncpy(tmpLine,line,line_length);

        if( (line[0] == 0xD) || (line[0] == 0xA) )
        {
        }
        else
        {

            line[line_length-2] = '\0';  /* chop off ";" */
            ret = split_line(line, tokens, &numTokens);
            if(ret == 0)
            {
                if(numTokens < 1)
                {
                    printf("Error in TMATS file - 1 Time\r\n");
                    return(1);
                }
            }
            else
            {
                printf("Error in TMATS file - 2 Time\r\n");
                return(1);
            }

            if(tokens[0].txt[0] == 'R')                         /* R-group */
            {
                if(strncmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
                {
                    if(TimeInfo == atoi(&tokens[1].txt[4]) )
                    {
                        copy = 1;
                    }
                }
            }

        }

        curLine += line_length;

        if(copy == 1)
        {
            strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
            len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
            memset(line,0x0,100);
        }
    }

    memset(line,0x0,100);
    curLine = TmatsBuffer;
    while((line_length = get_new_line(curLine, line)) > 1)
    {
        copy = 0;
        memset(tmpLine,0x0,100); 
        strncpy(tmpLine,line,line_length);

        if( (line[0] == 0xD) || (line[0] == 0xA) )
        {
        }
        else
        {

            line[line_length-2] = '\0';  /* chop off ";" */
            ret = split_line(line, tokens, &numTokens);
            if(ret == 0)
            {
                if(numTokens < 1)
                {
                    printf("Error in TMATS file - 1 Time\r\n");
                    return(1);
                }
            }
            else
            {
                printf("Error in TMATS file - 2 Time\r\n");
                return(1);
            }

            if(tokens[0].txt[0] == 'R')                         /* R-group */
            {
                if(strncmp(tokens[1].txt, "CHE-", 4) == 0)         /* R-x\CHE-n channel enable */
                {
                    if(TimeInfo == atoi(&tokens[1].txt[4]) )
                    {
                        copy = 1;
                    }
                }
                else if(strncmp(tokens[1].txt, "DSI-", 4) == 0)         /* R-x\DSI-n channel name */
                {
                    if(TimeInfo == atoi(&tokens[1].txt[4]) )
                    {
                        copy = 1;
                    }
                }
                else if(strncmp(tokens[1].txt, "CDT-", 4) == 0)         /* R-x\CDT-n channel name */
                {
                    if(TimeInfo == atoi(&tokens[1].txt[4]) )
                    {
                        copy = 1;
                    }
                }
	        else if(strncmp(tokens[1].txt,"TFMT",4) == 0)
	        {
                    copy = 1;
	        }
	        else if(strncmp(tokens[1].txt,"TSRC",4) == 0)
	        {
                    copy = 1;
	        }
                else if(strncmp(tokens[1].txt, "TK4-", 4) == 0)           /* R-x\TK4-n channel number of Type */
                {
                    if(TimeInfo == atoi(&tokens[1].txt[4]) )
                    {
                        copy = 1;
                    }
                }
            }
            else if(strncmp(tokens[0].txt, "V-", 2) == 0)               /* V-group */
            {
                if(strncmp(tokens[1].txt, "TMS",3) == 0)    /*M1553 TIMING*/     
                {
                    copy = 1;
                }
                else if(strncmp(tokens[2].txt, "CN-", 3) == 0)           /* V-x\CLX\CN-n channel number */
                {
                    if(TimeInfo == atoi(&tokens[2].txt[3]) )
                    {
                        copy = 1;
                    }
                }
	        else if(strncmp(tokens[3].txt,"1PPS",4) == 0)
	        {
                    copy = 1;
	        }

            }
        }

        curLine += line_length;

        if(copy == 1)
        {
            strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
            len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
            memset(line,0x0,100);
        }
    }

    //strncpy(&TmatsGetBuffer[len],"G\\COM:===\r\n",11);
    //len +=11;


    /*Second Pass is to Get Voice channel Specific attributes*/
    for(i = 0; i< aindex;i++)
    {
        memset(line,0x0,100);
        curLine = TmatsBuffer;
        while((line_length = get_new_line(curLine, line)) > 1)
        {
            copy = 0;
            memset(tmpLine,0x0,100); 
            strncpy(tmpLine,line,line_length);

            if( (line[0] == 0xD) || (line[0] == 0xA) )
            {
            }
            else
            {
                line[line_length-2] = '\0';  /* chop off ";" */
                ret = split_line(line, tokens, &numTokens);
                if(ret == 0)
                {
                    if(numTokens < 1)
                    {
                        printf("Error in TMATS file - 1 Voice\r\n");
                        return(1);
                    }
                }
                else
                {
                    printf("Error in TMATS file Voice\r\n");
                    return(1);
                }

                if(strncmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group */
                {
                    if(strncmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
                    {
                        if(VoiceInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                }
            }

            curLine += line_length;
            if(copy == 1)
            {
                strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
                len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
                memset(line,0x0,100);
            }
        }

        memset(line,0x0,100);
        curLine = TmatsBuffer;
        while((line_length = get_new_line(curLine, line)) > 1)
        {
            copy = 0;
            memset(tmpLine,0x0,100); 
            strncpy(tmpLine,line,line_length);

            if( (line[0] == 0xD) || (line[0] == 0xA) )
            {
            }
            else
            {

                line[line_length-2] = '\0';  /* chop off ";" */
                ret = split_line(line, tokens, &numTokens);
                if(ret == 0)
                {
                    if(numTokens < 1)
                    {
                        printf("Error in TMATS file - 1 Voice\r\n");
                        return(1);
                    }
                }
                else
                {
                    printf("Error in TMATS file Voice\r\n");
                    return(1);
                }

                if(strncmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group */
                {
                    if(strncmp(tokens[1].txt, "CHE-", 4) == 0)    /* R-x\CHE-n channel enable */
                    {
                        if(VoiceInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "DSI-", 4) == 0)     /* R-x\DSI-n channel name */
                    {
                        if(VoiceInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "CDT-", 4) == 0)     /* R-x\DSI-n channel name */
                    {
                        if(VoiceInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "TK4-", 4) == 0)          /* R-x\TK4-n channel of type*/
                    {
                        if(VoiceInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "AGI-", 4) == 0)          /* R-x\AGI-n Gain*/
                    {
                        if(VoiceInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                }
                else if(strncmp(tokens[0].txt, "V-", 2) == 0)               /* V-group */
                {
     
                     if(strncmp(tokens[2].txt, "CN-", 3) == 0)           /* V-x\CLX\CN-n channel number */
                     {
                        if(VoiceInfo[i] == atoi(&tokens[2].txt[3]) )
                        {
                            copy = 1;
                        }
                     }
                     if(strncmp(tokens[3].txt, "PGA-", 4) == 0)           /* V-x\CLX\A\PGA-n Audio gain */
                     {
                        if(VoiceInfo[i] == atoi(&tokens[3].txt[4]) )
                        {
                            copy = 1;
                        }
                     }
                }
            }

            curLine += line_length;
            if(copy == 1)
            {
                strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
                len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
                memset(line,0x0,100);
            }
        }

        //strncpy(&TmatsGetBuffer[len],"G\\COM:===\r\n",11);
        //len += 11;
    }

    /*Third Pass is to Get M1553 channel Specific attributes*/
    for(i = 0; i< mindex;i++)
    {
        memset(line,0x0,100);
        curLine = TmatsBuffer;
        while((line_length = get_new_line(curLine, line)) > 1)
        {
            copy = 0;
            memset(tmpLine,0x0,100); 
            strncpy(tmpLine,line,line_length);

            if( (line[0] == 0xD) || (line[0] == 0xA) )
            {
            }
            else
            {

                line[line_length-2] = '\0';  /* chop off ";" */
                ret = split_line(line, tokens, &numTokens);
                if(ret == 0)
                {
                    if(numTokens < 1)
                    {
                        printf("Error in TMATS file - 1 M1553\r\n");
                        return(1);
                    }
                }
                else
                {
                    printf("Error in TMATS file - 2 M1553\r\n");
                    return(1);
                }
                if(strncmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group     */
                {
                    if(strncmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
                    {
                        if(M1553Info[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                }
            }
            curLine += line_length;

            if(copy == 1)
            {
                strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
                len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
                memset(line,0x0,100);
            }
        }

        memset(line,0x0,100);
        curLine = TmatsBuffer;
        while((line_length = get_new_line(curLine, line)) > 1)
        {
            copy = 0;
            memset(tmpLine,0x0,100); 
            strncpy(tmpLine,line,line_length);

            if( (line[0] == 0xD) || (line[0] == 0xA) )
            {
            }
            else
            {
                line[line_length-2] = '\0';  /* chop off ";" */
                ret = split_line(line, tokens, &numTokens);
                if(ret == 0)
                {
                    if(numTokens < 1)
                    {
                        printf("Error in TMATS file - 1 M1553\r\n");
                        return(1);
                    }
                }
                else
                {
                    printf("Error in TMATS file - 2 M1553\r\n");
                    return(1);
                }
                if(strncmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group     */
                {
                    if(strncmp(tokens[1].txt, "CHE-", 4) == 0)         /* R-x\CHE-n channel enable */
                    {
                        if(M1553Info[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "DSI-", 4) == 0)         /* R-x\DSI-n channel name */
                    {
                        if(M1553Info[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "DLN-", 4) == 0)         /* R-x\DSI-n channel name */
                    {
                        if(M1553Info[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "CDT-", 4) == 0)   /* R-x\CDT-n */
                    {
                        if(M1553Info[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt,"BDLN-",5) == 0)   /*B-x\BDLN-n ; Bus Data Link*/
                    {
                        if(M1553Info[i] == atoi(&tokens[1].txt[5]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt,"CDLN-",5) == 0)   /*B-x\CDLN-n ; Channel Data Link*/
                    {
                        if(M1553Info[i] == atoi(&tokens[1].txt[5]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "TK4-", 4) == 0)          /* R-x\TK4-n channel of type*/
                    {
                        if(M1553Info[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "DTF-", 4) == 0)          /* R-x\DTF-n Discrete Channel Type*/
                    {
                        if(M1553Info[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "DMSK-", 5) == 0)          /* R-x\DTF-n Discrete Channel Type*/
                    {
                        if(M1553Info[i] == atoi(&tokens[1].txt[5]) )
                        {
                            copy = 1;
                        }
                    }

                }
                else if(strncmp(tokens[0].txt, "V-", 2) == 0)               /* V-group */
                {
                    if(strncmp(tokens[1].txt, "RDF",3) == 0)     /*V-x\RDF   M1553 Filtering*/
                    {
                      
                        if(strncmp(tokens[3].txt, "RTA",3) == 0)    /*V-x\RDF\CWM\RTA RT*/
                        {
                            sscanf(&tokens[3].txt[4],"%d-%d",&tmp1,&tmp2);
                            if(M1553Info[i] == tmp1)
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "TR",2) == 0)    /*V-x\RDF\CWM\TR Transmit-Receive*/
                        {
                            sscanf(&tokens[3].txt[3],"%d-%d",&tmp1,&tmp2);
                            if(M1553Info[i] == tmp1)
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "SA",2) == 0)    /*V-x\RDF\CWM\Sa SubAddress*/
                        {
                            sscanf(&tokens[3].txt[3],"%d-%d",&tmp1,&tmp2);
                            if(M1553Info[i] == tmp1)
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "FDT",3) == 0)    /*V-x\RDF\FDT Filtering Type*/
                        {
                            sscanf(&tokens[2].txt[4],"%d-%d",&tmp1,&tmp2);
                            if(M1553Info[i] == tmp1)
                            {
                                copy = 1;
                            }
                        }
                        else if(tokens[2].txt[0] == 'N') /*V-x\RDF\N Num Filtered Messages*/
                        {
                            if(M1553Info[i] == atoi(&tokens[2].txt[2]) )
                            {
                                copy = 1;
                            }
                        }
                    }
                    else if(strcmp(tokens[2].txt, "B") == 0)          /* V-x\CLX\B\... */
                    {
                        if(strncmp(tokens[3].txt, "WWP-", 4) == 0)     /* V-x\CLX\B\WWP-n watch word pattern */
                        {
                            if(M1553Info[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "WWM-", 4) == 0)     /* V-x\CLX\B\WWM-n watch word mask */
                        {
                            if(M1553Info[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "WWLI-", 5) == 0)    /* V-x\CLX\B\WWLI-n watch word lock internal */
                        {
                            if(M1553Info[i] == atoi(&tokens[3].txt[5]) )
                            {
                                copy = 1;
                            }
                        }
                    }
                    else
                    {
                        if(strncmp(tokens[2].txt, "CN-", 3) == 0)           /* V-x\CLX\CN-n channel number */
                        {
                            if(M1553Info[i] == atoi(&tokens[2].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "CSM-", 4) == 0)     /* V-x\CLX\CSM-n packet method */
                        {
                            if(M1553Info[i] == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt,"IIM-",4) == 0)   /*V-x\CLX\IIM-n ; Included in mask*/
                        {
                            if(M1553Info[i] == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                    }
                }
            }

            curLine += line_length;

            if(copy == 1)
            {
                strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
                len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
                memset(line,0x0,100);
            }
        }

        //strncpy(&TmatsGetBuffer[len],"G\\COM:===\r\n",11);
        //len += 11;
    }

    /*Fourth  Pass is to Get Video channel Specific attributes*/
    for(i = 0; i< vindex;i++)
    {
        memset(line,0x0,100);
        curLine = TmatsBuffer;
        while((line_length = get_new_line(curLine, line)) > 1)
        {
            copy = 0;
            memset(tmpLine,0x0,100); 
            strncpy(tmpLine,line,line_length);

            if( (line[0] == 0xD) || (line[0] == 0xA) )
            {
            }
            else
            {
                line[line_length-2] = '\0';  /* chop off ";" */
                ret = split_line(line, tokens, &numTokens);
                if(ret == 0)
                {
                    if(numTokens < 1)
                    {
                        printf("Error in TMATS file - 1 Video \r\n");
                        return(1);
                    }
                }
                else
                {
                    printf("Error in TMATS file - 2 Video \r\n");
                    return(1);
                }

                if(strncmp(tokens[0].txt, "R-", 2) == 0)                /* R-group */
                {
                    if(strncmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
                    {
                        if(VideoInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                }
            }
            curLine += line_length;

            if(copy == 1)
            {
                strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
                len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
                memset(line,0x0,100);
            }
        }

        memset(line,0x0,100);
        curLine = TmatsBuffer;
        while((line_length = get_new_line(curLine, line)) > 1)
        {
            copy = 0;
            memset(tmpLine,0x0,100); 
            strncpy(tmpLine,line,line_length);

            if( (line[0] == 0xD) || (line[0] == 0xA) )
            {
            }
            else
            {

                line[line_length-2] = '\0';  /* chop off ";" */
                ret = split_line(line, tokens, &numTokens);
                if(ret == 0)
                {
                    if(numTokens < 1)
                    {
                        printf("Error in TMATS file - 1 Video \r\n");
                        return(1);
                    }
                }
                else
                {
                    printf("Error in TMATS file - 2 Video \r\n");
                    return(1);
                }

                if(strncmp(tokens[0].txt, "R-", 2) == 0)              /* R-group */
                {
                    if(strncmp(tokens[1].txt, "CHE-", 4) == 0)        /* R-x\CHE-n channel enable */
                    {
                        if(VideoInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "DSI-", 4) == 0)   /* R-x\DSI-n channel name */
                    {
                        if(VideoInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "VED-", 4) == 0)    /* R-x\VED-n Video Encoder Delay*/
                    {
                        if(VideoInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "VXF-", 4) == 0)    /* R-x\VXF-n Video Format*/
                    {
                        if(VideoInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "VST-", 4) == 0)    /* R-x\VST-n Video Signal Type*/
                    {
                        if(VideoInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "VSF-", 4) == 0)    /* R-x\VSF-n Video Signal Format Type*/
                    {
                        if(VideoInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "CBR-", 4) == 0)    /* R-x\CBR-n Video Constant Bit Rate*/
                    {
                        if(VideoInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "CDT-", 4) == 0) /* R-x\CDT-n */
                    {
                        if(VideoInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncmp(tokens[1].txt, "TK4-", 4) == 0)      /* R-x\TK4-n channel of type*/
                    {
                        if(VideoInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                }

                if(strncmp(tokens[0].txt, "V-", 2) == 0)                /* V-group */
                {
                    if( strncmp(tokens[1].txt,"VCO",3) == 0 )      /* Video Overlay*/
                    {
                        if(strncmp(tokens[2].txt, "OET-", 4) == 0)      /* V-x\VCO\OET-n Overlay Event Toggle*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "VOE-", 4) == 0)     /* V-x\VCO\VOE-n Overlay Enable */
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "OE-", 3) == 0)     /* V-x\VCO\OE-n Overlay Enable */
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "OBG-", 4) == 0)     /* V-x\VCO\OBG-n Overlay Format */
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "VOC-", 4) == 0)     /* V-x\VCO\VOC-n Overlay Time Coding */
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "OLF-", 4) == 0)     /* V-x\VCO\OLF-n Overlay Time Coding */
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "X-", 2) == 0)      /* V-x\VOX-n Overlay X offset*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[2]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "Y-", 2) == 0)      /* V-x\VOY-n Overlay Y offset*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[2]) )
                            {
                                copy = 1;
                            }
                        }
    
                    }
                    else if( strncmp(tokens[1].txt, "VO", 2) == 0 )      /* Video Overlay*/
                    {
                        if(strncmp(tokens[2].txt, "OET-", 4) == 0)      /* V-x\VCO\OET-n Overlay Event Toggle*/
                        {
                            if( (i+1) == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "X-", 2) == 0)      /* V-x\VOX-n Overlay X offset*/
                        {
                            if( (i+1) == atoi(&tokens[2].txt[2]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "Y-", 2) == 0)      /* V-x\VOY-n Overlay Y offset*/
                        {
                            if( (i+1) == atoi(&tokens[2].txt[2]) )
                            {
                                copy = 1;
                            }
                        }

                    }
                    if ((strncmp(tokens[1].txt,"ASI",3) == 0) )      /* Video Overlay*/
                    {
                        if(strncmp(tokens[2].txt, "ASL-", 4) == 0)   /* V-x\ASI\ASL-n Audio Source Left*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt, "ASR-", 4) == 0)  /*V-x\ASI\ASR-n Audio Source Right*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                    }
                    else if(strcmp(tokens[2].txt, "V") == 0)                 /* V-x\CLX\V\... */
                    {
                        if(strncmp(tokens[3].txt, "CR-", 3) == 0)      /* V-x\CLX\V\CR-n internal clock rate */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "VM-", 3) == 0)      /* V-x\CLX\V\VM-n video mode */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "VC-", 3) == 0)      /* V-x\CLX\V\VC-n video compression */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "VR-", 3) == 0)      /* V-x\CLX\V\VR-n video resolution */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "VF-", 3) == 0)      /* V-x\CLX\V\VF-n video format */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "VI-", 3) == 0)      /* V-x\CLX\V\VI-n video input */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "AM-", 3) == 0)      /* V-x\CLX\V\AM-n audio mode */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "AR-", 3) == 0)      /* V-x\CLX\V\AR-n audio rate */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "AG-", 3) == 0)      /* V-x\CLX\V\AG-n audio gain */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "ASL-", 3) == 0)  /* V-x\CLX\V\ASL-n audio source Left */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "ASR-", 3) == 0)  /* V-x\CLX\V\ASR-n audio source Right */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "VOS-", 4) == 0)  /* V-x\CLX\V\VOS-n Overlay Font Size  */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "VOC-", 4) == 0)  /* V-x\CLX\V\VOY-n Overlay Time Coding*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "VOF-", 4) == 0)      /* V-x\CLX\V\VOF-n Overlay Format*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "VOP-", 4) == 0)      /* V-x\CLX\V\VOP-n Overlay Generate*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "VOE-", 4) == 0)      /* V-x\CLX\V\VOE-n Overlay Enable*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt, "LVO-", 4) == 0)      /* V-x\CLX\V\LVO-n Video Out*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt,"HLE-",4) == 0)  /*V-x\CLX\HLE-n ; Hard Loop Enable */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[3].txt,"RED-",4) == 0)  /*V-x\CLX\V\RED-n ; VIDEO Record Enable */
                        {
                            if(VideoInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                    }
                    else
                    {
                        if(strncmp(tokens[2].txt, "CN-", 3) == 0)       /* V-x\CLX\CN-n channel number */
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[3]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncmp(tokens[2].txt,"IIM-",4) == 0)   /*V-x\CLX\IIM-n ; Included in mask*/
                        {
                            if(VideoInfo[i] == atoi(&tokens[2].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                    }
                }
            }

            curLine += line_length;

            if(copy == 1)
            {
                strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
                len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
                memset(line,0x0,100);
            }
        }

       // strncpy(&TmatsGetBuffer[len],"G\\COM:===\r\n",11);
       // len += 11;
    }


    /*Fifth Pass is to Get Ethernet channel Specific attributes*/
    memset(line,0x0,100);
    curLine = TmatsBuffer;
    while((line_length = get_new_line(curLine, line)) > 1)
    {
        copy = 0;
        memset(tmpLine,0x0,100); 
        strncpy(tmpLine,line,line_length);
        if( (line[0] == 0xD) || (line[0] == 0xA) )
        {
        }
        else
        {

            line[line_length-2] = '\0';  /* chop off ";" */
            ret = split_line(line, tokens, &numTokens);
            if(ret == 0)
            {
                if(numTokens < 1)
                {
                    printf("Error in TMATS file - 1 Time\r\n");
                    return(1);
                }
            }
            else
            {
                printf("Error in TMATS file - 2 Time\r\n");
                return(1);
            }

            if(tokens[0].txt[0] == 'R')                         /* R-group */
            {
                if(strncmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
                {
                    if(EthernetInfo == atoi(&tokens[1].txt[4]) )
                    {
                        copy = 1;
                    }
                }
            }
        }
        curLine += line_length;

        if(copy == 1)
        {
            strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
            len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
            memset(line,0x0,100);
        }
    }


    memset(line,0x0,100);
    curLine = TmatsBuffer;
    while((line_length = get_new_line(curLine, line)) > 1)
    {
        copy = 0;
        memset(tmpLine,0x0,100); 
        strncpy(tmpLine,line,line_length);

        if( (line[0] == 0xD) || (line[0] == 0xA) )
        {
        }
        else
        {

            line[line_length-2] = '\0';  /* chop off ";" */
            ret = split_line(line, tokens, &numTokens);
            if(ret == 0)
            {
                if(numTokens < 1)
                {
                    printf("Error in TMATS file - 1 Time\r\n");
                    return(1);
                }
            }
            else
            {
                printf("Error in TMATS file - 2 Time\r\n");
                return(1);
            }

            if(tokens[0].txt[0] == 'R')                         /* R-group */
            {
                if(strncmp(tokens[1].txt, "CHE-", 4) == 0)         /* R-x\CHE-n channel enable */
                {
                    if(EthernetInfo == atoi(&tokens[1].txt[4]) )
                    {
                        copy = 1;
                    }
                }
                else if(strncmp(tokens[1].txt, "DSI-", 4) == 0)         /* R-x\DSI-n channel name */
                {
                    if(EthernetInfo == atoi(&tokens[1].txt[4]) )
                    {
                        copy = 1;
                    }
                }
                else if(strncmp(tokens[1].txt, "CDT-", 4) == 0)         /* R-x\CDT-n channel name */
                {
                    if(EthernetInfo == atoi(&tokens[1].txt[4]) )
                    {
                        copy = 1;
                    }
                }
                else if(strncmp(tokens[1].txt, "TK4-", 4) == 0)          /* R-x\TK4-n channel of type*/
                {
                    if(EthernetInfo == atoi(&tokens[1].txt[4]) )
                    {
                        copy = 1;
                    }
                }
            }
            else if(strncmp(tokens[0].txt, "V-", 2) == 0)               /* V-group */
            {
                if(strncmp(tokens[2].txt, "EFM",3) == 0)    /*Only for Airbus V-x\CLX\EFM*/     
                {
                    copy = 1;
                }
                else if(strncmp(tokens[2].txt, "CN-", 3) == 0)           /* V-x\CLX\CN-n channel number */
                {
                    if(EthernetInfo == atoi(&tokens[2].txt[3]) )
                    {
                        copy = 1;
                    }
                }
	        else if(strncmp(tokens[2].txt,"EIP",3) == 0)    /*V-x\CLX\EIP*/
	        {
                    copy = 1;
	        }
                else if(strncmp(tokens[2].txt,"RIP",3) == 0)    /*V-x\CLX\RIP - Radar E address*/
                {
                    copy = 1;
                }
                else if(strncmp(tokens[2].txt,"RPN",3) == 0)    /*V-x\CLX\RPN - Radar PORT*/
                {
                    copy = 1;
                }
	        else if(strncmp(tokens[2].txt,"EMA",3) == 0)    /*V-x\CLX\EMA*/
	        {
                    copy = 1;
                }
	        else if(strncmp(tokens[2].txt,"ECP",3) == 0)    /*V-x\CLX\ECP*/
	        {
                    copy = 1;
                }
            }
        }

        curLine += line_length;

        if(copy == 1)
        {
            strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
            len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
            memset(line,0x0,100);
        }
    }

    strncpy(&TmatsGetBuffer[len],"G\\COM:===\r\n",11);
    len +=11;

    /*Sixth Pass is to Get PCM channel Specific attributes*/
    for(i = 0; i< pindex;i++)
    {
        memset(line,0x0,100);
        curLine = TmatsBuffer;
        while((line_length = get_new_line(curLine, line)) > 1)
        {
            copy = 0;
            memset(tmpLine,0x0,100);
            strncpy(tmpLine,line,line_length);
            if( (line[0] == 0xD) || (line[0] == 0xA) )
            {
            }
            else
            {

                line[line_length-2] = '\0';  /* chop off ";" */
                ret = split_line(line, tokens, &numTokens);
                if(ret == 0)
                {
                    if(numTokens < 1)
                    {
                        printf("Error in TMATS file - 1 PCM\r\n");
                        return(1);
                    }
                }
                else
                {
                    printf("Error in TMATS file - 2 PCM\r\n");
                    return(1);
                }
                if(strncasecmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group     */
                {
                    if(strncasecmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                }
            }
            curLine += line_length;

            if(copy == 1)
            {
                strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
                len += line_length;
                memset(line,0x0,100);
            }
        }

        memset(line,0x0,100);
        curLine = TmatsBuffer;
        while((line_length = get_new_line(curLine, line)) > 1)
        {
            copy = 0;
            memset(tmpLine,0x0,100);
            strncpy(tmpLine,line,line_length);

            if( (line[0] == 0xD) || (line[0] == 0xA) )
            {
            }
            else
            {

                line[line_length-2] = '\0';  /* chop off ";" */
                ret = split_line(line, tokens, &numTokens);
                if(ret == 0)
                {
                    if(numTokens < 1)
                    {
                        printf("Error in TMATS file - 1 PCM\r\n");
                        return(1);
                    }
                }
                else
                {
                    printf("Error in TMATS file - 2 PCM\r\n");
                    return(1);
                }

                if(strncasecmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group     */
                {
                    if(strncasecmp(tokens[1].txt, "CHE-", 4) == 0)         /* R-x\CHE-n channel enable */
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "DSI-", 4) == 0)         /* R-x\DSI-n channel name */
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "DLN-", 4) == 0)         /* R-x\DSI-n channel name */
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "CDT-", 4) == 0)   /* R-x\CDT-n */
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "PDP-", 4) == 0)         /* R-x\DP-n Data Packing Option */
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "PWWP-", 5) == 0)         /* R-x\PWWP-n Watch Word Pattern*/
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[5]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "PWWM-", 5) == 0)         /* R-x\PWWP-n Watch Word Mask*/
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[5]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "PWWB-", 5) == 0)        /* R-x\PWWP-n Watch Word Bit Position*/
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[5]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "TK4-", 4) == 0)          /* R-x\TK4-n channel of type*/
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "IST-", 4) == 0)          /* R-x\IST-n Input source*/
                    {
                        if(PCMInfo[i] == atoi(&tokens[1].txt[4]) )
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "UCR-", 4) == 0)          /* R-x\UCR-n UART Baud Rate*/
                    {
                        sscanf(&tokens[1].txt[4],"%d-%d",&tmp1,&tmp2);
                        if(PCMInfo[i] == tmp1)
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "UCB-", 4) == 0)          /* R-x\UCB-n UART Bits per Word*/
                    {
                        sscanf(&tokens[1].txt[4],"%d-%d",&tmp1,&tmp2);
                        if(PCMInfo[i] == tmp1)
                        {
                            copy = 1;
                        }
                    }
                    else if(strncasecmp(tokens[1].txt, "UCP-", 4) == 0)          /* R-x\UCP-n UART Parity*/
                    {
                        sscanf(&tokens[1].txt[4],"%d-%d",&tmp1,&tmp2);
                        if(PCMInfo[i] == tmp1)
                        {
                            copy = 1;
                        }
                    }

                }
                else if(strncasecmp(tokens[0].txt, "V-", 2) == 0)               /* V-group */
                {
                    if(strcmp(tokens[2].txt, "P") == 0)          /* V-x\CLX\P\... */
                    {
                        if(strncasecmp(tokens[3].txt, "D10-", 4) == 0)          /* R-x\TK4-n channel of type*/
                        {
                            if(PCMInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncasecmp(tokens[3].txt, "CP4-", 4) == 0)  /* R-x\TK4-n channel of type*/
                        {
                            if(PCMInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncasecmp(tokens[3].txt, "CP7-", 4) == 0)  /* R-x\TK4-n channel of type*/
                        {
                            if(PCMInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                        }
                        else if(strncasecmp(tokens[3].txt, "MF6-", 4) == 0)  /* R-x\TK4-n channel of type*/
                        {
                            if(PCMInfo[i] == atoi(&tokens[3].txt[4]) )
                            {
                                copy = 1;
                            }
                       }
                       else if(strncasecmp(tokens[3].txt, "IDCM-", 5) == 0)  /* R-x\TK4-n channel of type*/
                       {
                           if(PCMInfo[i] == atoi(&tokens[3].txt[5]) )
                           {
                               copy = 1;
                           }
                       }
                       else if(strncasecmp(tokens[3].txt, "D9-", 3) == 0)  /* R-x\TK4-n channel of type*/
                       {
                           if(PCMInfo[i] == atoi(&tokens[3].txt[3]) )
                           {
                               copy = 1;
                           }
                       }
                   }
                   else
                   {
                       if(strncasecmp(tokens[2].txt, "IIM-", 4) == 0)  /* R-x\TK4-n channel of type*/
                       {
                           if(PCMInfo[i] == atoi(&tokens[2].txt[4]) )
                           {
                               copy = 1;
                           }
                       }
                       else if(strncmp(tokens[2].txt, "CUCE-", 5) == 0)  /* V-x\CLX\CUCE-n channel of type*/
                       {
                           if(PCMInfo[i] == atoi(&tokens[2].txt[5]) )
                           {
                               copy = 1;
                           }

                           if(tokens[3].txt[0] == 'T')
                           {
                               ContUart = 1;
                           }
                       }


                   }
               }
               else if(tokens[0].txt[0] == 'P')
               {
                    /*Get the PCM index*/
                    index = atoi(&tokens[0].txt[2]);
                    if(ContUart) //this means we have a controller UART which is index 0 and num chans is 9
                    {
                        if(index == i)
                        {
                            copy = 1;
                        }
                    }
                    else
                    {
                        if(index == (i+1))
                        {
                            copy = 1;
                        }
                    }

                }
            }

            curLine += line_length;

            if(copy == 1)
            {
                strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
                len += line_length;
                memset(line,0x0,100);
            }
        }

        strncpy(&TmatsGetBuffer[len],"G\\COM:===\r\n",11);
        len += 11;
    }


    /* Seventh  Pass is to Get the rest of the Attributes*/
    memset(line,0x0,100);
    curLine = TmatsBuffer;
    while((line_length = get_new_line(curLine, line)) > 1)
    {
        copy = 0;
        memset(tmpLine,0x0,100); 
        strncpy(tmpLine,line,line_length);

        if( (line[0] == 0xD) || (line[0] == 0xA) )
        {
        }
        else
        {
            line[line_length-2] = '\0';  /* chop off ";" */
            ret = split_line(line, tokens, &numTokens);
            if(ret == 0)
            {
                if(numTokens < 1)
                {
                    printf("Error in TMATS file - 1 Rest\r\n");
                    return(1);
                }
            }
            else
            {
                printf("Error in TMATS file - 2 Rest %s\r\n",tmpLine);
                return(1);
            }

            if(strncmp(tokens[0].txt, "R-1",3) == 0 )
            {
                if( strncmp(tokens[1].txt, "EV", 2) == 0 )      /* R-1\EV - Event */
                {
                    copy = 1;
                }
                else if(strncmp(tokens[1].txt, "IDX",3) == 0) 
                {
                    copy = 1;
                }
                else if(strncmp(tokens[1].txt, "RI5",3) == 0) 
                {
                    sscanf(tokens[2].txt,"%d-%d-%d",&m23_chan->month,&m23_chan->day,&m23_chan->year);
                    M23_SetYear(m23_chan->year);
                }
            }
            else if(strncmp(tokens[0].txt, "V-", 2) == 0)               /* V-group */
            {
                if(strncmp(tokens[1].txt, "VO",2) == 0)     /*V-x\VO   Video Overlay*/
                {
                    copy = 1;
                }
                else if(strncmp(tokens[1].txt, "ARE",3) == 0)     /*V-x\ARE  Auto Record*/
                {
                    if(tokens[3].txt[0] == 'T')
                    {
                        AutoRecord = 1;
                    }     
                }
            }
        }

        curLine += line_length;

        if(copy == 1)
        {
            strncpy(&TmatsGetBuffer[len],tmpLine,line_length);
            len += line_length;
//printf("line len %d,Overall Len %d -> %s\r\n",line_length,len,tmpLine);
            memset(line,0x0,100);
        }
    }

    /*Eight Pass is to Get the Bus Attributes ->B Group used for Events*/
    memset(line,0x0,100);
    curLine = TmatsBuffer;
    while((line_length = get_new_line(curLine, line)) > 1)
    {
        memset(tmpLine,0x0,100); 
        strncpy(tmpLine,line,line_length);
        if( (line[0] == 0xD) || (line[0] == 0xA) )
        {
        }
        else
        {

            line[line_length-2] = '\0';  /* chop off ";" */
            ret = split_line(line, tokens, &numTokens);
            if(ret == 0)
            {
                if(numTokens < 1)
                {
                    printf("Error in TMATS file - 1 Time\r\n");
                    return(1);
                }
            }
            else
            {
                printf("Error in TMATS file - 2 Time\r\n");
                return(1);
            }

            if(tokens[0].txt[0] == 'B')                         /* B-group */
            {
                /*Get the Bus index*/
                index = atoi(&tokens[0].txt[2]);

                if(strncasecmp(tokens[1].txt, "BNA-", 4) == 0)  /* B-x\BNA Bus Name*/
                {
                    strcpy((EventDef + index)->BusName,tokens[2].txt);
                    m23_chan->NumBuses++;
                }
                else if(strncasecmp(tokens[1].txt, "NMS", 3) == 0)  /* B-x\NMS Number of Messages*/
                {
                    (EventDef+index)->NumMsgs = atoi(tokens[3].txt);
                }
                else if(strncasecmp(tokens[1].txt, "TRM-", 4) == 0)  /* B-x\TRM Transmit-Receive*/
                {
                    sscanf(&tokens[1].txt[4],"%d-%d",&tmp1,&tmp2);
                    (EventDef+index)->Msg[tmp2].TransRcv = atoi(tokens[2].txt);
                }
                else if(strncasecmp(tokens[1].txt, "STA-", 4) == 0)  /* B-x\STA Sub-Address */
                {
                    sscanf(&tokens[1].txt[4],"%d-%d",&tmp1,&tmp2);
                    (EventDef+index)->Msg[tmp2].SubAddr =  Binary2UL(tokens[2].txt);
                }
                else if(strncasecmp(tokens[1].txt, "TRA-", 4) == 0)  /* B-x\TRA Remote Terminal*/
                {
                    sscanf(&tokens[1].txt[4],"%d-%d",&tmp1,&tmp2);
                    (EventDef+index)->Msg[tmp2].RT =  Binary2UL(tokens[2].txt);
                }
                else if(strncasecmp(tokens[1].txt, "DWC-", 4) == 0)  /* B-x\DWC Message Word Count */
                {
                    sscanf(&tokens[1].txt[4],"%d-%d",&tmp1,&tmp2);
                    for(j = 0; j < 5;j++)
                    {
                        if( (tokens[2].txt[j] == 'X') || (tokens[2].txt[j] == 'x') )
                        {
                            tokens[2].txt[j] = '0';
                        }
                    }
                    (EventDef+index)->Msg[tmp2].WordCount =  Binary2UL(tokens[2].txt);
                }
                else if(strncasecmp(tokens[1].txt, "MN", 2) == 0)  /* B-x\MN Number Of Measurands or Measurand Name */
                {
                    if(strncasecmp(tokens[2].txt, "N-", 2) == 0)
                    {
                        sscanf(&tokens[2].txt[2],"%d-%d",&tmp1,&tmp2);
                        (EventDef+index)->Msg[tmp2].NumMeas = atoi(tokens[3].txt);
                    }
                    else if(strncasecmp(tokens[1].txt, "MN-", 3) == 0)  /* B-x\MN-i-i-i  Measurand Name */
                    {
                        sscanf(&tokens[1].txt[3],"%d-%d-%d",&tmp1,&tmp2,&bindex);
                        memset((EventDef+index)->Msg[tmp2].Event[bindex].MeasName,0x0,32);
                        strcpy((EventDef+index)->Msg[tmp2].Event[bindex].MeasName,tokens[2].txt);
                    }
                }
                else if(strncasecmp(tokens[1].txt, "MT-", 3) == 0)  /* B-x\MT Measurement type*/
                {
                    sscanf(&tokens[1].txt[3],"%d-%d-%d",&tmp1,&tmp2,&bindex);
                    (EventDef+index)->Msg[tmp2].Event[bindex].CW_Only = 0;

                    if( (tokens[2].txt[0] == 'C') || (tokens[2].txt[0] == 'c') )
                    {
                        (EventDef+index)->Msg[tmp2].Event[bindex].CW_Only = 1;
                        (EventDef+index)->Msg[tmp2].Event[bindex].NumWords = 1;
                    }
                }
                else if(strncasecmp(tokens[1].txt, "MTO-", 4) == 0)  /* B-x\MTO Transmit Order*/
                {
                    sscanf(&tokens[1].txt[4],"%d-%d-%d-%d",&tmp1,&tmp2,&bindex,&eindex);
                    EventDef[index].Msg[tmp2].Event[bindex].TransOrder[eindex] = 0;
                    if(strncasecmp(tokens[2].txt, "MSB", 3) == 0)
                    {
                        (EventDef+index)->Msg[tmp2].Event[bindex].TransOrder[eindex] = 1;
                    }
                }
                else if(strncasecmp(tokens[1].txt, "MBM-", 4) == 0)  /* B-x\MBM Mask*/
                {
                    sscanf(&tokens[1].txt[4],"%d-%d-%d-%d",&tmp1,&tmp2,&bindex,&eindex);
                    if(tokens[2].txt[0] == 'F')
                    {
                        (EventDef+index)->Msg[tmp2].Event[bindex].Mask[eindex] =  0xFFFF;
                    }
                    else
                    {
                        (EventDef+index)->Msg[tmp2].Event[bindex].Mask[eindex] =  Binary2UL(tokens[2].txt);
                    }
                }
                else if(strncasecmp(tokens[1].txt, "MWN-", 4) == 0)  /* B-x\MWN Word Fragments*/
                {
                    sscanf(&tokens[1].txt[4],"%d-%d-%d-%d",&tmp1,&tmp2,&bindex,&eindex);
                    (EventDef+index)->Msg[tmp2].Event[bindex].WordNumber[eindex] =  atoi(tokens[2].txt);
                }
                else if(strncasecmp(tokens[1].txt, "NML", 3) == 0)  /* B-x\NML\N Number of Measurements*/
                {
                    sscanf(&tokens[2].txt[2],"%d-%d-%d",&tmp1,&tmp2,&bindex);
                    (EventDef+index)->Msg[tmp2].Event[bindex].NumWords =  atoi(tokens[3].txt);
                }
                else if(strncasecmp(tokens[1].txt, "MFP-", 4) == 0)  /* B-x\MFP  Fragment Position*/
                {
                    sscanf(&tokens[1].txt[4],"%d-%d-%d-%d",&tmp1,&tmp2,&bindex,&eindex);
                    (EventDef+index)->Msg[tmp2].Event[bindex].Position[eindex] =  atoi(tokens[2].txt);
                }
            }
        }
        curLine += line_length;
        memset(line,0x0,100);
    }

    //PutResponse( 0, "****************************end********\r\n" );
    //PutResponse( 0, TmatsGetBuffer );

    if(LoadedTmatsFromCartridge == 0)
    {
        fclose(fp);
    }


    status = ParseTMATSFile(TmatsGetBuffer, m23_chan);

   

    return(status);
}

int ParseTMATSFile (char *fileContents, M23_CHANNEL *m23_chan)
{
    char              line[100];
    char              tmpLine[100];
    int               line_length;
    char              *curLine;
    char              *searchPtr;
    ctoken            tokens[10];
    int               numTokens;
    int               chanType;
    int               tmp_length;
    int               vchan=0;
    int               m1553chan=0;
    int               pcmchan = 0;
    int               timechan = 0;
    int               voicechan = 0;
    int               ethchan = 0;
    int               uartchan = 0;
    int               dmchan = 0;
    int               last_tk = 0xFFFF;
    char              *chanBuf;
    char              tmp[4];
    int               event;
    VIDEO_CHANNEL     *m23_video;
    M1553_CHANNEL     *m23_m1553;
    PCM_CHANNEL       *m23_pcm;
    TIMECODE_CHANNEL  *m23_time;
    VOICE_CHANNEL     *m23_voice;
    ETHERNET_CHANNEL  *m23_eth;
    DISCRETE_CHANNEL  *m23_dis;
    int               result;
    int               ret;
    int               i;
    int               vid;
    int               map;
    int               status;
    int               debug;
    int               index = 0;
    int               offset = 0;

    M23_GetDebugValue(&debug);

    chanBuf = (char*)malloc(64*1024);

//printf("--\r\n%s",fileContents);

    /*Clear Out Event Structure*/
    for(i = 0; i < MAX_NUM_EVENTS;i++)
    {
        memset(EventTable[i].EventName,0x0,33);
        memset(EventTable[i].MeasurementSource,0x0,33);
        memset(EventTable[i].EventDesc,0x0,257);

        EventTable[i].EventCount     = 0;
        EventTable[i].CalculexEvent  = 0;
        EventTable[i].EventEnabled   = 0;
        EventTable[i].EventLimit     = 0;
    }

    CurrentNodeIndex = 0;

    uart  = 0;
    pulse = 0;

    CH10_TMATS = 1;

    curLine = fileContents;

    PauseResume.Enabled = 0;

    m23_chan->NumConfigured1553  = 0;
    m23_chan->NumConfiguredPCM   = 0;
    m23_chan->NumConfiguredVideo = 0;
    m23_chan->NumConfiguredDiscrete = 0;
    m23_chan->NumberConversions = 0;

    m23_chan->EventsAreEnabled = 0;
    m23_chan->IndexIsEnabled = 0;
    TimeSet_Command = 1;

    while((line_length = get_new_line(curLine, line)) > 1)
    {
        line[line_length-2] = '\0';  /* chop off ";" */
        ret = split_line(line, tokens, &numTokens);
        if(ret == 0)
        {
            if(numTokens < 1)
            {
                printf("Error in TMATS file - 1\r\n");
                return(1);
            }
        }
        else
        {
            printf("Error in TMATS file - 2\r\n");
            return(1);
        }

        /* decode the generic header part */
        if(strcmp(tokens[0].txt, "G") == 0)
        {
            if(strncmp(tokens[1].txt, "PN",2) == 0)        /* G\PN -- Program Name */
            {
                strcpy(m23_chan->ProgramName, tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "106",3) == 0)       /* G\106 -- IRIG 106 rev level */
            {
                m23_chan->VersionNumber = atoi(tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "RN",2) == 0)        /* G\RN -- Revision Number*/
            {
                TmatsVersion = atoi(tokens[2].txt);
            }

        }
        else if(strncmp(tokens[0].txt, "V-", 2) == 0)             /* V-group */
        {
            if(strncmp(tokens[1].txt, "RLS",3) == 0) 
            {
                if(strncmp(tokens[2].txt, "RCS",3) == 0)  /*Deal with TMATS as ch10 or Eglin Security*/
                {
                    if(tokens[3].txt[0] == 'E')
                    {
                        CH10_TMATS = 0;
                    }
                }
                if(strncmp(tokens[2].txt, "RMM",3) == 0) /* V-x\RLS\RMM; Remote Receiver */
                {
#if 0
                    if(tokens[3].txt[0] == 'E')
                    {
                        M23_SetRemoteValue(1);
                        IEEE1394_Bus_Speed = BUS_SPEED_S400;
                    }
                    else
                    {
                        M23_SetRemoteValue(0);
                        IEEE1394_Bus_Speed = BUS_SPEED_S800;
                    }
#endif
                }
            }
            else if(strncmp(tokens[1].txt, "RTS",3) == 0)  /* V-x\RTS\ETO; External Time Only*/
            {
                if(strncmp(tokens[2].txt, "ETO",3) == 0) 
                {
                    if(tokens[3].txt[0] == 'T')
                    {
                        TimeSet_Command = 0;
                    }
                    else
                    {
                        TimeSet_Command = 1;
                    }
                }
          
            }
            else if(strncmp(tokens[1].txt, "SDR",3) == 0)  /* V-x\SDR\BS; 1394 Bus Speed*/
            {
#if 0
                if(strncmp(tokens[3].txt, "S400",3) == 0) 
                {
                    IEEE1394_Bus_Speed = BUS_SPEED_S400;
                }
#endif
            }
            else if(strncmp(tokens[1].txt, "VOF",3) == 0)  /* V-x\VOF\OST; Video Output Format*/
            {
                if(tokens[3].txt[0] == '1')
                {
                    Video_Output_Format = RGB;
                    M23_I2c_SetRegisters(0xA,0x38,0x7,0x5);
                    //M23_I2c_SetRegisters(0xA,0x38,0x47,0x0);
                }
                else
                {
                    Video_Output_Format = NTSC;
                    //M23_I2c_SetRegisters(0x7,0x38,0x0);
                    M23_I2c_SetRegisters(0xB,0x7,0x38,0x0);
                }
            }
            else if(strncmp(tokens[1].txt, "DCD",3) == 0)  /* V-x\DCD\D; Discrete Disable*/
            {
                if(tokens[3].txt[0] == 'T')
                {
                    Discrete_Disable = 1;
                }
            }
            else if(strncmp(tokens[1].txt, "RIE",3) == 0)  /* V-x\RIE\D; Internal Events Disable*/
            {
            }
            else if(strncmp(tokens[1].txt, "MRC",3) == 0)  /* V-x\MRC\; M1553 Pause Resume*/
            {
                if(tokens[2].txt[0] == 'E')
                {
                    if(tokens[3].txt[0] == 'T')
                    {
                        PauseResume.Enabled = 1;
                    }
 
                }
                else if(strncmp(tokens[2].txt, "ID",2) == 0)   /* V-x\MRC\ID; Pause-Resume ID */
                {
                    PauseResume.ChanID = atoi(tokens[3].txt);    
                } 
                else if(strncmp(tokens[2].txt, "RCT",3) == 0)   /* V-x\MRC\ID; Control Type*/
                {
                    if(tokens[3].txt[0] == '1')
                    {
                        PauseResume.Type = PAUSE_RESUME; 
                    }
                    else
                    {
                        PauseResume.Type = STOP_START; 
                    }
                } 
                else if(strncmp(tokens[2].txt, "SPM",3) == 0)   /* V-x\MRC\SPM Stop-Pause CW*/
                {
                    sscanf(tokens[3].txt,"%x",&PauseResume.PauseCW);
                }
                else if(strncmp(tokens[2].txt, "SRM",3) == 0)   /* V-x\MRC\SPM Stop-Pause CW*/
                {
                    sscanf(tokens[3].txt,"%x",&PauseResume.ResumeCW);
                }
            }
            else if(strncmp(tokens[2].txt, "RRE",3) == 0)     /* V-x\CLX\RRE; Remote Receiver */
            {
#if 0
                if(tokens[3].txt[0] == 'T')
                {
                    M23_SetRemoteValue(1);
                    IEEE1394_Bus_Speed = BUS_SPEED_S400;
                }
                else
                {
                    M23_SetRemoteValue(0);
                    IEEE1394_Bus_Speed = BUS_SPEED_S800;
                }
#endif
            }
            else if( (strncmp(tokens[2].txt, "RID", 3) == 0) ) /*V-x\CLX\RID;  M1553 RT Control*/
            {
                if( strncmp(tokens[3].txt, "HC", 2) == 0 ) 
                {
                    m23_chan->M1553_RT_Control = M1553_RT_HOST_CONTROL;
                }
                else if( strncmp(tokens[3].txt, "NO", 2) == 0 ) 
                {
                    m23_chan->M1553_RT_Control = M1553_RT_NONE;
                }
                else
                {
                    m23_chan->M1553_RT_Control = atoi(tokens[3].txt);
                }
            }
            else if( (strncmp(tokens[2].txt, "DATE", 4) == 0) ) /*V-x\CLX\DATE;  Recorder Date*/
            {
                sscanf(tokens[3].txt,"%d-%d-%d",&m23_chan->month,&m23_chan->day,&m23_chan->year);
                M23_SetYear(m23_chan->year);
            }
            else if( (strncmp(tokens[2].txt, "SO1", 3) == 0) ) /*V-x\CLX\SO1; Serial 1 Data Out*/
            {
                result = atoi(tokens[3].txt); 
                M23_SetSerial(1,result);
            }
            else if( (strncmp(tokens[2].txt, "SO2", 3) == 0) ) /*V-x\CLX\SO2; Serial 2 Data Out*/
            {
                result = atoi(tokens[3].txt); 
                M23_SetSerial(2,result);
            }
        }
        else if(strncasecmp(tokens[0].txt, "C-", 2) == 0)             /* C-group */
        {
            index = atoi(&tokens[0].txt[2]);
            m23_chan->NumberConversions = index;

            if(strncasecmp(tokens[1].txt, "DCN",3) == 0)     /* C-x\DCN; Measurement Name */
            {
                strcpy((dataConversion + index)->DerivedName,tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "DCT",3) == 0)     /* C-x\DCT;  Conversion Type */
            {
                if(strncasecmp(tokens[2].txt, "DER",3) == 0)
                {
                    (dataConversion+index)->ConvType = 1;
                }
                else
                {
                    (dataConversion+index)->ConvType = 0;
                }
            }
            else if(strncasecmp(tokens[1].txt, "DPA",3) == 0)     /* C-x\DPA; Algorithm */
            {
                if(strncasecmp(tokens[2].txt, "==",2) == 0)
                {
                    (dataConversion + index)->operator = EQUAL;
                }
                else if(strncasecmp(tokens[2].txt, "!=",2) == 0)
                {
                    (dataConversion + index)->operator = NOT_EQUAL;
                }
                else if(strncasecmp(tokens[2].txt, "<=",2) == 0)
                {
                    (dataConversion + index)->operator = LESS_EQUAL;
                }
                else if(strncasecmp(tokens[2].txt, ">=",2) == 0)
                {
                    (dataConversion + index)->operator = GREATER_EQUAL;
                }
                else if(tokens[2].txt[0] == '<')
                {
                    (dataConversion + index)->operator = LESS;
                }
                else if(tokens[2].txt[0] == '>')
                {
                    (dataConversion + index)->operator = GREATER;
                }
                else if(strncasecmp(tokens[2].txt, "||",2) == 0)
                {
                    (dataConversion + index)->operator = LOR;
                }
                else if(strncasecmp(tokens[2].txt, "&&",2) == 0)
                {
                    (dataConversion + index)->operator = LAND;
                }
                else if(tokens[2].txt[0] == '|')
                {
                    (dataConversion + index)->operator = BOR;
                }
                else if(tokens[2].txt[0] == '&')
                {
                    (dataConversion + index)->operator = BAND;
                }
                else if(tokens[2].txt[0] == '^')
                {
                    (dataConversion + index)->operator = BXOR;
                }
                else if(tokens[2].txt[0] == '+')
                {
                    (dataConversion + index)->operator = ADD;
                }
                else if(tokens[2].txt[0] == '-')
                {
                    (dataConversion + index)->operator = SUBTRACT;
                }
                else if(tokens[2].txt[0] == '*')
                {
                    (dataConversion + index)->operator = MULTIPLY;
                }
                else if(tokens[2].txt[0] == '/')
                {
                    (dataConversion + index)->operator = DIVIDE;
                }
                else if(tokens[2].txt[0] == '%')
                {
                    (dataConversion + index)->operator = MOD;
                }
            }
            else if(strncasecmp(tokens[1].txt, "DPTM",4) == 0)     /* C-x\DPTM; Trigger Measureand */
            {
                strcpy((dataConversion + index)->TriggerName,tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "DPNO",4) == 0)     /* C-x\DPNO; Number of Occurences */
            {
                (dataConversion + index)->NumBeforeValid = atoi(tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "DP-",3) == 0)     /* C-x\DP-n; Measureand*/
            {
                offset = atoi(&tokens[1].txt[3]);
                if(offset == 1)
                {
                    memset((dataConversion + index)->MeasName_1,0x0,32);
                    strcpy((dataConversion + index)->MeasName_1,tokens[2].txt);
                }
                else
                {
                    memset((dataConversion + index)->MeasName_2,0x0,32);
                    strcpy((dataConversion + index)->MeasName_2,tokens[2].txt);
                }
            }
            else if(strncasecmp(tokens[1].txt, "DP",2) == 0)     /* C-x\DP\N; Number of Input Measureands*/
            {
                (dataConversion + index)->NumOperands = atoi(tokens[3].txt);
            }
            else if(strncasecmp(tokens[1].txt, "MOT1",4) == 0)     /* C-x\MOT1 High Measurement Value*/
            {
                (dataConversion + index)->HighLimit = atoi(tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "MOT2",4) == 0)     /* C-x\MOT2 Low Measurement Value*/
            {
                (dataConversion + index)->LowLimit = atoi(tokens[2].txt);
            }
        }


        /* need to retrieve DIR and # of channels later. bdu052804 */

        /* decide channel type and copy the whole channel section */
        if(numTokens == 3 &&
           strcmp(tokens[0].txt, "R-1") == 0 &&
           strncmp(tokens[1].txt, "TK1-", 4) == 0 )      /* R-1\TK-1  -- track number, channel ID */
        {
            strcpy(chanBuf, "");
            searchPtr = curLine + line_length;
            while((tmp_length = get_new_line(searchPtr, tmpLine)) > 1)
            {
                memset(tokens[2].txt,0x0,64);
                tmpLine[tmp_length-2] = '\0';
                ret = split_line(tmpLine, tokens, &numTokens);
                if(ret != 0)
                {
                    printf("Error in TMATS file - 3\r\n");
                    return(1);
                }
                if(numTokens == 3)
                {
                    if(strncmp(tokens[2].txt, "VIDIN", 5) == 0)
                    {
                        chanType = INPUT_VIDEO;
                        break;
                    }
                    else if( (strncmp(tokens[2].txt, "1553IN", 6) == 0) || (strncmp(tokens[2].txt,"DISIN",5) == 0) )
                    {
                        chanType = INPUT_M1553;
                        break;
                    }
                    else if( (strncmp(tokens[2].txt, "PCMIN", 5) == 0) || (strncmp(tokens[2].txt,"UARTIN",6) == 0) || (strncmp(tokens[2].txt,"PULSEIN",7) == 0))
                    {
                        if( strncmp(tokens[2].txt,"UARTIN",6) == 0 )
                        {
                            uart = 1;
                        }
                        else if( strncmp(tokens[2].txt,"PULSEIN",7) == 0 )
                        {
                            pulse = 1;
                        }

                        //printf("PCM tokens = %s\r\n",tokens[2].txt);
                        chanType = INPUT_PCM;
                        break;
                    }
                    else if(strncmp(tokens[2].txt, "TIMEIN", 5) == 0)
                    {
                        //printf("PCM tokens = %s\r\n",tokens[2].txt);
                        chanType = INPUT_TIMECODE;
                        break;
                    }
                    else if(strncmp(tokens[2].txt, "ANAIN", 5) == 0)
                    {
                        //printf("PCM tokens = %s\r\n",tokens[2].txt);
                        chanType = INPUT_VOICE;
                        break;
                    }
                    else if(strncmp(tokens[2].txt, "ETHIN", 5) == 0)
                    {
                        chanType = INPUT_ETHERNET;
                        break;
                    }


                }
                searchPtr += tmp_length;
            }


            /* get the first line, i.e. R-1\TK1-n:ID */
            line_length = get_new_line(curLine, line);
            line[line_length] = '\0';
            strcat(chanBuf, line);
            curLine += line_length;
            while(1)   /* do till hit the next R-1\Tk1-n or Comment Line */
            {
                line_length = get_new_line(curLine, line);
                if(line_length <= 1 )
                    break;

                line[line_length] = '\0';
                strcpy(tmpLine, line);
                line[line_length-2] = '\0';
                ret = split_line(line, tokens, &numTokens);
                if(ret != 0)
                {
                    printf("Error in TMATS file - 4, %s\r\n",line);
                    return(1);
                }

                if(numTokens == 3 &&
                   strncmp(tokens[0].txt, "R-1",3) == 0 &&
                   strncmp(tokens[1].txt, "TK1-", 4) == 0)
                {
                   break;
                }
                else if(strncmp(tokens[0].txt, "R-1",3) == 0 &&
                   strncmp(tokens[1].txt, "EV", 2) == 0)
                {
                    break;
                }
                else if(strncmp(tokens[0].txt, "R-1",3) == 0 &&
                   strncmp(tokens[1].txt, "IDX", 3) == 0)
                {
                    break;
                }
                else if( (tokens[0].txt[0] == 'G') && 
                   strncmp(tokens[1].txt, "COM", 3) == 0)
                {
                    break;
                }
                else
                {
                    strcat(chanBuf, tmpLine);  /* append lines of this channel to the channel buffer for later decoding */
                    curLine += line_length;
                }
            }
            curLine -= line_length;     /* go back one line */
            chanBuf[strlen(chanBuf)-1] = '\0';

            /* decoding channel TMATS info */
            switch(chanType)
            {
            case INPUT_VIDEO:       /* decode video section */
                m23_video = &(m23_chan->video_chan[vchan]);
                m23_video->included = FALSE;
                M23_SetDefaultOverlayParameters(vchan);
                ret = Decode_VideoChannel(chanBuf, m23_video, &result,vchan);
                if(ret != 0)
                {
                    printf("Error in TMATS file - Decode Video\r\n");
                    return(1);
                }
                status = result;
                HealthArrayTypes[m23_video->chanID] = VIDEO_FEATURE;
                vchan++;
                break;

            case INPUT_M1553:       /* decode 1553 section */
                m23_m1553 = &(m23_chan->m1553_chan[m1553chan]);
                m23_dis   = &(m23_chan->dm_chan);
                m23_m1553->included = FALSE;
                //printf("Decoding %d M1553 channel\r\n",m1553chan+1);
                ret = Decode_M1553Channel(chanBuf, m23_m1553,m23_dis, &result);
                if(ret != 0)
                {
                    printf("Error in TMATS file - Decode M1553\r\n");
                    return(1);
                }
                status = result;
                if(m23_m1553->isDiscrete)
                {
                    HealthArrayTypes[m23_dis->chanID] = DISCRETE_FEATURE;
                    dmchan++;
                }
                else
                {
                    HealthArrayTypes[m23_m1553->chanID] = M1553_FEATURE;
                    m1553chan++;
                }

                break;
            case INPUT_PCM:       /* decode PCM section */
                //printf("PCM chanType = %d\r\n",chanType);
                m23_pcm = &(m23_chan->pcm_chan[pcmchan]);
                m23_pcm->included = FALSE;
                ret = Decode_PCMChannel(chanBuf, m23_pcm, &result);
                if(ret != 0)
                {
                    printf("Error in TMATS file - Decode PCM\r\n");
                    return(1);
                }

                if(uart == 1)
                {
                    if(m23_pcm->OnController == 1)
                    {
                         m23_chan->uart_chan.isEnabled = m23_pcm->isEnabled;
                         m23_chan->uart_chan.m_dataType = INPUT_UART;
                         strcpy(m23_chan->uart_chan.chanName,m23_pcm->chanName);
                         m23_chan->uart_chan.chanNumber = m23_pcm->chanNumber;
                         m23_chan->uart_chan.chanID = m23_pcm->chanID;
                         HealthArrayTypes[m23_chan->uart_chan.chanID] = UART_FEATURE;
                         uartchan++;
                    }
                    else
                    {
                        m23_pcm->pcmCode = PCM_UART;
                        HealthArrayTypes[m23_pcm->chanID] = PCM_FEATURE;
                        pcmchan++;
                    }
                }
                else
                {
                    HealthArrayTypes[m23_pcm->chanID] = PCM_FEATURE;
                    pcmchan++;
                }

                uart = 0;
                pulse = 0;
                status = result;
                break;

	    case INPUT_TIMECODE:
	        m23_time = &(m23_chan->timecode_chan);
		ret = Decode_TimeChannel(chanBuf, m23_chan,m23_time, &result);
                if(ret != 0)
                {
                    printf("Error in TMATS file - Decode Time \r\n");
                    return(1);
                }
		status = result;
                HealthArrayTypes[m23_time->chanID] = TIMECODE_FEATURE;
		timechan++;
		break;
	    case INPUT_VOICE:
		m23_voice = &(m23_chan->voice_chan[voicechan]);
		ret = Decode_VoiceChannel(chanBuf, m23_voice, &result);
                if(ret != 0)
                {
                    printf("Error in TMATS file - Decode Voice \r\n");
                    return(1);
                }
		status = result;
                HealthArrayTypes[m23_voice->chanID] = VOICE_FEATURE;
		voicechan++;
		break;
	    case INPUT_ETHERNET:
	        m23_eth = &(m23_chan->eth_chan);
		ret = Decode_Ethernet(chanBuf, m23_chan,m23_eth, &result);
                if(ret != 0)
                {
                    printf("Error in TMATS file - Decode Ethernet \r\n");
                    return(1);
                }
		status = result;
                HealthArrayTypes[m23_eth->chanID] = ETHERNET_FEATURE;
		ethchan++;
		break;

            default:
                printf("Unknown channel type.\r\n");
                break;
            }

        } /* if (numTokens == 3 && ... ) */
        else if(strcmp(tokens[0].txt, "R-1") == 0 &&
           strncmp(tokens[1].txt, "EV", 2) == 0 )      /* R-1\EV - Event */
        {
            if(numTokens > 2)
            {
                memset(tmp,0x0,4);
                if(strncasecmp(tokens[2].txt,"ID-",3) == 0) /*This is the Event Name*/
                {
                    event = atoi(&tokens[2].txt[3]);


                    //strncpy(EventTable[event].EventName,tokens[3].txt,strlen(tokens[3].txt) - 1);
                    strncpy(EventTable[event].EventName,tokens[3].txt,strlen(tokens[3].txt));

                    EventTable[event].DualEvent = 0;
                    EventTable[event].NoResponseEvent = 0;
                    if(strncasecmp(EventTable[event].EventName,"BUS PAUSE",9) == 0) /*Pause Event*/
                    {
                        M23_SetPauseEvent(event);
                    }
                    else if(strncasecmp(EventTable[event].EventName,"BUS RESUME",10) == 0) /*Resume Event*/
                    {
                        M23_SetResumeEvent(event);
                    }
                    else if(strncasecmp(EventTable[event].EventName,"EGI SYNC",8) == 0) /*EGI Sync Event*/
                    {
                        M23_SetTimeSyncEvent(event);
                    }

                }
                else if(strncasecmp(tokens[2].txt,"D-",2) == 0) /*This is the Event Description*/
                {
                    event = atoi(&tokens[2].txt[2]);
                    strncpy(EventTable[event].EventDesc,tokens[3].txt,strlen(tokens[3].txt) - 1);
                    //strncpy(EventTable[event].EventDesc,tokens[3].txt,strlen(tokens[3].txt));
                }
                else if(tokens[2].txt[0] == 'E') /*This is the Event Enable*/
                {
                    if(tokens[3].txt[0] == 'T')
                    {
                        m23_chan->EventsAreEnabled = 1;
                    }
                    else
                    {
                        m23_chan->EventsAreEnabled = 0;
                    }
                }
                else if(tokens[2].txt[0] == 'T') /*This is the Event Type*/
                {
                    event = atoi(&tokens[2].txt[2]);

                    EventTable[event].EventEnabled = 1;

                    if(tokens[3].txt[0] == 'E')
                    {
                        EventTable[event].EventType = EVENT_TYPE_EXTERNAL;
                    }
                    else if(tokens[3].txt[0] == 'D')
                    {
                        EventTable[event].EventType = EVENT_TYPE_MEAS_DISCRETE;
                    }
                    else if(tokens[3].txt[0] == 'L')
                    {
                        EventTable[event].EventType = EVENT_TYPE_MEAS_LIMIT;
                    }
                    else if(tokens[3].txt[0] == 'R')
                    {
                        EventTable[event].EventType = EVENT_TYPE_RECORDER;
                    }
                    else
                    {
                        EventTable[event].EventType = EVENT_TYPE_OTHER;
                    }
                }
                else if(tokens[2].txt[0] == 'P') /*This is the Event Priority*/
                {
                    event = atoi(&tokens[2].txt[2]);

                    if(tokens[3].txt[0] == '1')
                    {
                        EventTable[event].EventPriority = EVENT_PRIORITY_1;
                    }
                    else if(tokens[3].txt[0] == '2')
                    {
                        EventTable[event].EventPriority = EVENT_PRIORITY_2;
                    }
                    else if(tokens[3].txt[0] == '3')
                    {
                        EventTable[event].EventPriority = EVENT_PRIORITY_3;
                    }
                    else if(tokens[3].txt[0] == '4')
                    {
                        EventTable[event].EventPriority = EVENT_PRIORITY_4;
                    }
                    else if(tokens[3].txt[0] == '5')
                    {
                        EventTable[event].EventPriority = EVENT_PRIORITY_5;
                    }
                    else //Default to Priority 1
                    {
                       EventTable[event].EventPriority = EVENT_PRIORITY_1;
                    }

                    //printf("Event %d Priority %d\r\n",event,EventTable[event].EventPriority);
                }
                else if(strncasecmp(tokens[2].txt,"LC-",3) == 0) /*This is the Event Limit Count*/
                {
                    event = atoi(&tokens[2].txt[3]);

                    EventTable[event].EventLimit = atoi(tokens[3].txt);

                }
                else if(strncasecmp(tokens[2].txt,"MS-",3) == 0) /*This is the Event Measurement Source*/
                {
                    event = atoi(&tokens[2].txt[3]);
                    strncpy(EventTable[event].MeasurementSource,tokens[3].txt,strlen(tokens[3].txt));
                }
                else if(strncasecmp(tokens[2].txt,"MN-",3) == 0) /*This is the Event Measurement Link*/
                {
                    event = atoi(&tokens[2].txt[3]);
                    strncpy(EventTable[event].MeasurementLink,tokens[3].txt,strlen(tokens[3].txt));
                }
                else if(strncasecmp(tokens[2].txt,"DLN-",4) == 0) /*This is the Event Data Link*/
                {
                    event = atoi(&tokens[2].txt[4]);
                    strncpy(EventTable[event].DataLink,tokens[3].txt,strlen(tokens[3].txt));
                }
                else if(strncasecmp(tokens[2].txt,"CM-",3) == 0) /*This is the Event */
                {
                    event = atoi(&tokens[2].txt[3]);
                    EventTable[event].EventMode = atoi(tokens[3].txt);
                }
                else if(strncasecmp(tokens[2].txt,"IC-",3) == 0) /*This is the Event */
                {
                    event = atoi(&tokens[2].txt[3]);
                    if(tokens[3].txt[0] == 'T')
                    {
                        EventTable[event].EventIC = 1;
                    }
                    else
                    {
                        EventTable[event].EventIC = 0;
                    }
                }
                else if(tokens[2].txt[0] == 'N') /*This is the Number of Events*/
                {
                    m23_chan->NumEvents = atoi(tokens[3].txt);
                }
            }

        }
        else if(strcmp(tokens[0].txt, "R-1") == 0 &&
           strncmp(tokens[1].txt, "IDX", 3) == 0 )      /* R-1\IDX - Index */
        {
            if(tokens[2].txt[0] == 'E') /*This is the Event Enable*/
            {
                if(tokens[3].txt[0] == 'T')
                {
                    m23_chan->IndexIsEnabled = 1;
                }
                else
                {
                    m23_chan->IndexIsEnabled = 0;
                }
            }
            else if(strncmp(tokens[2].txt, "ICV", 3) == 0 )      /* R-1\IDX\ICV - Index Count*/
            {
                indexChannel.IndexCount = atoi(tokens[3].txt);
            }
            else if(strncmp(tokens[2].txt, "ITV", 3) == 0 )      /* R-1\IDX\ITC - Index Time*/
            {
                indexChannel.IndexTime = atoi(tokens[3].txt);
            }
            else if(strncmp(tokens[2].txt, "IT", 2) == 0 )      /* R-1\IDX\IT - Index Type*/
            {
                if(tokens[3].txt[0] == 'T')
                {
                    indexChannel.IndexType = INDEX_CHANNEL_TIME;
                }
                else
                {
                    indexChannel.IndexType = INDEX_CHANNEL_COUNT;
                }
                
            }
        }

        curLine += line_length;

    }

    m23_chan->NumConfigured1553      = m1553chan;
    m23_chan->NumConfiguredPCM       = pcmchan;
    m23_chan->NumConfiguredVideo     = vchan;
    m23_chan->NumConfiguredVoice     = voicechan;
    m23_chan->NumConfiguredEthernet  = ethchan;
    m23_chan->NumConfiguredUART      = uartchan;
    m23_chan->NumConfiguredDiscrete  = dmchan;


    m23_chan->numChannels = pcmchan + m1553chan + vchan + timechan + voicechan + ethchan + uartchan + dmchan;

    return(status);
}

/* parsing the video channel TMATS section */
int Decode_VideoChannel(char *videoTmats, VIDEO_CHANNEL *m23_video, int *status,int vchan)
{
    char    line[100];
    ctoken tokens[10];
    int    ret;
    int    numTokens;
    int    line_length;
    int    bitrate;

    *status = TMATS_PARSE_SUCCESS;

    m23_video->NumMaps = 0;

    m23_video->m_dataType = INPUT_VIDEO;

    m23_video->videoMode = 1;
    m23_video->audioMode = 1;

    m23_video->VideoOut = 0;


    m23_video->videoCompression = VIDEO_IP_15_VIDCOMP;
    m23_video->internalClkRate = VIDEO_4_0M_CLOCKRATE;
    m23_video->videoFormat = VIDEO_NTSC_VIDFORMAT;
    m23_video->OverlaySize = LARGE_FONT;

    while((line_length = get_new_line(videoTmats, line)) > 1)
    {
        line[line_length-2] = '\0';  /* chop off ";" */
        ret = split_line(line, tokens, &numTokens);
        if(ret == 0)
        {
            if(numTokens < 1)
            {
                printf("Error in TMATS file\r\n");
                return(1);
            }
        }
        else
        {
            return(1);
        }

        if(tokens[0].txt[0] == 'G')                /* G */
        {
            return(0);
        }

        if(strncmp(tokens[0].txt, "R-", 2) == 0)                /* R-group */
        {
            if(strncmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
            {
                m23_video->chanID = atoi(tokens[2].txt);
		VideoIds[vchan] = m23_video->chanID;
            }
            else if(strncmp(tokens[1].txt, "CHE-", 4) == 0)         /* R-x\CHE-n channel enable */
            {
                if(tokens[2].txt[0] == 'T')
                {
                    //printf("VIDEO ENABLED\r\n");
                    m23_video->isEnabled = 1;
                }
                else
                {
                    //printf("VIDEO NOT ENABLED\r\n");
                    m23_video->isEnabled = 0;
                }
            }
            else if(strncmp(tokens[1].txt, "DSI-", 4) == 0)         /* R-x\DSI-n channel name */
            {
                strcpy(m23_video->chanName, tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "VED-", 4) == 0)         /* R-x\VED-n Video Encoder Delay*/
            {
                m23_video->VideoDelay = atoi(tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "TK4-", 4) == 0)          /* R-x\TK4-n channel of type*/
            {
                m23_video->chanNumber = atoi(tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "VXF-", 4) == 0)    /* R-x\VXF-n Video Format*/
            {
            }
            else if(strncmp(tokens[1].txt, "VST-", 4) == 0)    /* R-x\VST-n Video Signal Type*/
            {
                if(tokens[2].txt[0] == '3') 
                {
                    m23_video->videoInput = VIDEO_SVID_VIDINPUT;
                }
                else
                {
                    m23_video->videoInput = VIDEO_COMP_VIDINPUT;
                }
            }
            else if(strncmp(tokens[1].txt, "VSF-", 4) == 0)    /* R-x\VSF-n Video Signal Format Type*/
            {
                if (tokens[2].txt[0] == '2')
                {
                    m23_video->videoFormat = VIDEO_PAL_VIDFORMAT;
                }
                else 
                {
                    m23_video->videoFormat = VIDEO_NTSC_VIDFORMAT;
                }
            }
            else if(strncmp(tokens[1].txt, "CBR-", 4) == 0)    /* R-x\CBR-n Video Constant Bit Rate*/
            {
                bitrate = atoi(tokens[2].txt);
 
                if(bitrate < 3000000)
                {
                    m23_video->internalClkRate = VIDEO_2_0M_CLOCKRATE;
                }
                else if( bitrate < 4000000)
                {
                    m23_video->internalClkRate = VIDEO_3_0M_CLOCKRATE;
                }
                else if(bitrate < 5000000)
                {
                    m23_video->internalClkRate = VIDEO_4_0M_CLOCKRATE;
                }
                else if(bitrate < 8000000)
                {
                    m23_video->internalClkRate = VIDEO_6_0M_CLOCKRATE;
                }
                else if(bitrate < 10000000)
                {
                    m23_video->internalClkRate = VIDEO_8_0M_CLOCKRATE;
                }
                else if(bitrate < 12000000)
                {
                    m23_video->internalClkRate = VIDEO_10_0M_CLOCKRATE;
                }
                else if(bitrate < 15000000)
                {
                    m23_video->internalClkRate = VIDEO_12_0M_CLOCKRATE;
                }
                else
                {
                    m23_video->internalClkRate = VIDEO_15_0M_CLOCKRATE;
                }
            }

        }
        if(strncmp(tokens[0].txt, "V-", 2) == 0)                /* V-group */
        {
            if( strncmp(tokens[1].txt, "VO", 2) == 0 )      /* Video Overlay*/
            {
                if(strncmp(tokens[2].txt, "OET-", 4) == 0)      /* V-x\OET-n Overlay Event Toggle*/
                {
                    if(tokens[3].txt[0] == 'T')
                    {
                        m23_video->Overlay_Event_Toggle = EVENT_TOGGLE_ON;
                    }
                    else
                    {
                        m23_video->Overlay_Event_Toggle = EVENT_TOGGLE_OFF;
                    }
                }
                else if(strncmp(tokens[2].txt, "X-", 2) == 0)      /* V-x\VOX-n Overlay X offset*/
                {
                    m23_video->Overlay_X = atoi(tokens[3].txt) * 2;
                }
                else if(strncmp(tokens[2].txt, "Y-", 2) == 0)      /* V-x\VOY-n Overlay Y offset*/
                {
                    m23_video->Overlay_Y = atoi(tokens[3].txt) / 2;
                }
             }
             else if( strncmp(tokens[1].txt, "VCO", 3) == 0 )      /* Video Overlay*/
             {
                if(strncmp(tokens[2].txt, "OET-", 4) == 0)      /* V-x\VCO\OET-n Overlay Event Toggle*/
                {
                    if(tokens[3].txt[0] == 'T')
                    {
                        m23_video->Overlay_Event_Toggle = EVENT_TOGGLE_ON;
                    }
                    else
                    {
                        m23_video->Overlay_Event_Toggle = EVENT_TOGGLE_OFF;
                    }
                }
                else if(strncmp(tokens[2].txt, "X-", 2) == 0)      /* V-x\VCO\VOX-n Overlay X offset*/
                {
                    m23_video->Overlay_X = atoi(tokens[3].txt) * 2;
                }
                else if(strncmp(tokens[2].txt, "Y-", 2) == 0)      /* V-x\VCO\VOY-n Overlay Y offset*/
                {
                    m23_video->Overlay_Y = atoi(tokens[3].txt) / 2;
                }
                else if(strncmp(tokens[2].txt, "OE-", 3) == 0)      /* V-x\VCO\OE-n Overlay Enable */
                {
                    if(tokens[3].txt[0] == 'T')
                    {
                        m23_video->OverlayEnable = OVERLAY_ON;
                    }
                    else
                    {
                        m23_video->OverlayEnable = OVERLAY_OFF;
                    }
                }
                else if(strncmp(tokens[2].txt, "OBG-", 4) == 0)      /* V-x\VCO\OBG-n Overlay Format*/
                {
             
                    if(strncmp(tokens[3].txt,"BOW",3) == 0)
                    {
                        m23_video->Overlay_Format = BLACK_WHITE;
                    }
                    else if(strncmp(tokens[3].txt,"WOB",3) == 0)
                    {
                        m23_video->Overlay_Format = WHITE_BLACK;
                    }
                    else if(strncmp(tokens[3].txt,"BOT",3) == 0)
                    {
                        m23_video->Overlay_Format = BLACK_TRANS;
                    }
                    else if(strncmp(tokens[3].txt,"WOT",3) == 0)
                    {
                        m23_video->Overlay_Format = WHITE_TRANS;
                    }
 
                }
                else if(strncmp(tokens[2].txt, "VOC-", 4) == 0)    /* V-x\VCO\VOC-n Overlay Time Coding*/
                {
                    if(strncmp(tokens[3].txt,"TO",2) == 0)
                    {
                        m23_video->Overlay_Time = TIME_ONLY;     /*No day or milliseconds*/
                    }
                    else if(strncmp(tokens[3].txt,"DTM",3) == 0)  
                    {
                        m23_video->Overlay_Time = TIME_DAY_MS;   /*time day and milliseconds*/
                    }
                    else if(strncmp(tokens[3].txt,"DT",2) == 0)  
                    {
                        m23_video->Overlay_Time = TIME_DAY;      /*time,day no millisecondes*/
                    }
                    else if(strncmp(tokens[3].txt,"TM",2) == 0)  
                    {
                        m23_video->Overlay_Time = TIME_MS;       /*no day but with milliseconds*/
                    }
                    else
                    {
                        m23_video->Overlay_Time = NO_TIME_DAY;
                    }
                }
                else if(strncmp(tokens[2].txt, "OLF-", 4) == 0)    /* V-x\VCO\VOC-n Overlay Time Coding*/
                {
                    if(strncmp(tokens[3].txt,"TO",2) == 0)
                    {
                        m23_video->Overlay_Time = TIME_ONLY;     /*No day or milliseconds*/
                    }
                    else if(strncmp(tokens[3].txt,"DTM",3) == 0)  
                    {
                        m23_video->Overlay_Time = TIME_DAY_MS;   /*time day and milliseconds*/
                    }
                    else if(strncmp(tokens[3].txt,"DT",2) == 0)  
                    {
                        m23_video->Overlay_Time = TIME_DAY;      /*time,day no millisecondes*/
                    }
                    else if(strncmp(tokens[3].txt,"TM",2) == 0)  
                    {
                        m23_video->Overlay_Time = TIME_MS;       /*no day but with milliseconds*/
                    }
                    else
                    {
                        m23_video->Overlay_Time = NO_TIME_DAY;
                    }
                }
 
             }
             if( strncmp(tokens[1].txt, "ASI", 2) == 0) /* Audio Selection*/
             {
                 if(strncmp(tokens[2].txt, "ASL-", 4) == 0)  /* V-x\ASL-n audio source Left */
                 {
                     if( (strncmp(tokens[3].txt,"LC",2) == 0) )
                     {
                         m23_video->audioSourceL = AUDIO_LOCAL;
                     }
                     else if( (strncmp(tokens[3].txt,"ET",2) == 0) )
                     {
                         m23_video->audioSourceL = AUDIO_EVENT_TONE;
                     }
                     else if( (strncmp(tokens[3].txt,"NO",2) == 0) )
                     {
                         m23_video->audioSourceL = AUDIO_NONE;
                     }
                     else //We Have an Audio Channel
                     {
                         m23_video->audioSourceL = atoi(tokens[3].txt); 
                     }
                 }
                 else if(strncmp(tokens[2].txt, "ASR-", 4) == 0)  /* V-x\ASR-n audio source Right */
                 {
                     if( (strncmp(tokens[3].txt,"LC",2) == 0) )
                     {
                         m23_video->audioSourceR = AUDIO_LOCAL;
                     }
                     else if( (strncmp(tokens[3].txt,"ET",2) == 0) )
                     {
                         m23_video->audioSourceR = AUDIO_EVENT_TONE;
                     }
                     else if( (strncmp(tokens[3].txt,"NO",2) == 0) )
                     {
                         m23_video->audioSourceR = AUDIO_NONE;
                     }
                     else //We Have an Audio Channel
                     {
                         m23_video->audioSourceR = atoi(tokens[3].txt); 
                     }
                 }
             }
             else if(strcmp(tokens[2].txt, "V") == 0)                 /* V-x\CLX\V\... */
             {
                 if(strncmp(tokens[3].txt, "CR-", 3) == 0)      /* V-x\CLX\V\CR-n internal clock rate */
                 {
                     bitrate = atoi(tokens[4].txt);
 
                     if(bitrate < 3000000)
                     {
                         m23_video->internalClkRate = VIDEO_2_0M_CLOCKRATE;
                     }
                     else if( bitrate < 4000000)
                     {
                         m23_video->internalClkRate = VIDEO_3_0M_CLOCKRATE;
                     }
                     else if(bitrate < 5000000)
                     {
                         m23_video->internalClkRate = VIDEO_4_0M_CLOCKRATE;
                     }
                     else if(bitrate < 8000000)
                     {
                         m23_video->internalClkRate = VIDEO_6_0M_CLOCKRATE;
                     }
                     else if(bitrate < 10000000)
                     {
                         m23_video->internalClkRate = VIDEO_8_0M_CLOCKRATE;
                     }
                     else if(bitrate < 12000000)
                     {
                         m23_video->internalClkRate = VIDEO_10_0M_CLOCKRATE;
                     }
                     else if(bitrate < 15000000)
                     {
                         m23_video->internalClkRate = VIDEO_12_0M_CLOCKRATE;
                     }
                     else
                     {
                         m23_video->internalClkRate = VIDEO_15_0M_CLOCKRATE;
                     }
                 }
                 else if(strncmp(tokens[3].txt, "VC-", 3) == 0)      /* V-x\CLX\V\VC-n video compression */
                 {
                     if (strncmp(tokens[4].txt,"IP-5",4) == 0)
                     {
                         m23_video->videoCompression = VIDEO_IP_5_VIDCOMP;
                     }
                     else if (strncmp(tokens[4].txt,"IP-15",5) == 0)
                     {
                         m23_video->videoCompression = VIDEO_IP_15_VIDCOMP;
                     }
                     else if (strncmp(tokens[4].txt,"IP-45",5) == 0)
                     {
                         m23_video->videoCompression = VIDEO_IP_45_VIDCOMP ;
                     }
                     else if (strncmp(tokens[4].txt,"IPB-6",5) == 0)
                     {
                         m23_video->videoCompression = VIDEO_IP_5_VIDCOMP;
                     }
                     else if (strncmp(tokens[4].txt,"IPB-16",6) == 0)
                     {
                         m23_video->videoCompression = VIDEO_IPB_15_VIDCOMP;
                     }
                     else if (strncmp(tokens[4].txt,"IPB-45",6) == 0)
                     {
                         m23_video->videoCompression = VIDEO_IPB_45_VIDCOMP;
                     }
                     else if (strncmp(tokens[4].txt,"I",1) == 0)
                     {
                         m23_video->videoCompression = VIDEO_I_VIDCOMP;
                     }
                     else
                     {
                         m23_video->videoCompression = VIDEO_IP_15_VIDCOMP;
                     }
                }
                else if(strncmp(tokens[3].txt, "VR-", 3) == 0)      /* V-x\CLX\V\VR-n video resolution */
                {
                    if (strncmp(tokens[4].txt,"FULL",4) == 0)
                    {
                        m23_video->videoResolution = VIDEO_FULL_VIDRES;
                    }
                    else if (strncmp(tokens[4].txt,"HALF",4) == 0)
                    {
                        m23_video->videoResolution = VIDEO_HALF_VIDRES;
                    }
                    else if (strncmp(tokens[4].txt,"SIFHALF",7) == 0)
                    {
                        m23_video->videoResolution = VIDEO_SIFHALF_VIDRES;
                    }
                }
                else if(strncmp(tokens[3].txt, "VF-", 3) == 0)      /* V-x\CLX\V\VF-n video format */
                {
                    if (strncmp(tokens[4].txt,"NTSC",4) == 0)
                    {
                        m23_video->videoFormat = VIDEO_NTSC_VIDFORMAT;
                    }
                    else if (strncmp(tokens[4].txt,"PAL",3) == 0)
                    {
                        m23_video->videoFormat = VIDEO_PAL_VIDFORMAT;
                    }
                }
                else if(strncmp(tokens[3].txt, "VI-", 3) == 0)      /* V-x\CLX\V\VI-n video input */
                {
                    if (strncmp(tokens[4].txt,"COMP",4) == 0)
                    {
                        m23_video->videoInput = VIDEO_COMP_VIDINPUT;
                    }
                    else if (strncmp(tokens[4].txt,"SVID",4) == 0)
                    {
                        m23_video->videoInput = VIDEO_SVID_VIDINPUT;
                    }
                }
                else if(strncmp(tokens[3].txt, "AR-", 3) == 0)      /* V-x\CLX\V\AR-n audio rate */
                {
                    if (strcmp(tokens[4].txt,"64") == 0)
                        m23_video->audioRate = VIDEO_64_AUDIORATE;
                    else if (strcmp(tokens[4].txt,"96") == 0)
                        m23_video->audioRate = VIDEO_96_AUDIORATE;
                    else if (strcmp(tokens[4].txt,"192") == 0)
                        m23_video->audioRate = VIDEO_192_AUDIORATE;
                }

                else if(strncmp(tokens[3].txt, "AG-", 3) == 0)      /* V-x\CLX\V\AG-n audio gain */
                {
                    if (strcmp(tokens[4].txt,"0") == 0)
                        m23_video->audioGain = VIDEO_0_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"1.5") == 0)
                        m23_video->audioGain = VIDEO_1_5_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"3.0") == 0)
                        m23_video->audioGain = VIDEO_3_0_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"4.5") == 0)
                        m23_video->audioGain = VIDEO_4_5_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"6.0") == 0)
                        m23_video->audioGain = VIDEO_6_0_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"7.5") == 0)
                        m23_video->audioGain = VIDEO_7_5_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"9.0") == 0)
                        m23_video->audioGain = VIDEO_9_0_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"10.5") == 0)
                        m23_video->audioGain = VIDEO_10_5_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"12") == 0)
                        m23_video->audioGain = VIDEO_12_0_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"13.5") == 0)
                        m23_video->audioGain = VIDEO_13_5_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"15") == 0)
                        m23_video->audioGain = VIDEO_15_0_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"16.5") == 0)
                        m23_video->audioGain = VIDEO_16_5_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"18") == 0)
                        m23_video->audioGain = VIDEO_18_0_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"19.5") == 0)
                        m23_video->audioGain = VIDEO_19_5_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"21") == 0)
                        m23_video->audioGain = VIDEO_21_0_AUDIOGAIN;
                    else if (strcmp(tokens[4].txt,"22.5") == 0)
                        m23_video->audioGain = VIDEO_22_5_AUDIOGAIN;
                }
                else if(strncmp(tokens[3].txt, "ASL-", 4) == 0)  /* V-x\CLX\V\ASL-n audio source Left */
                {
                    if( (strncmp(tokens[4].txt,"LC",2) == 0) )
                    {
                        m23_video->audioSourceL = AUDIO_LOCAL;
                    }
                    else if( (strncmp(tokens[4].txt,"ET",2) == 0) )
                    {
                        m23_video->audioSourceL = AUDIO_EVENT_TONE;
                    }
                    else if( (strncmp(tokens[4].txt,"NO",2) == 0) )
                    {
                        m23_video->audioSourceL = AUDIO_NONE;
                    }
                    else //We Have an Audio Channel
                    {
                        m23_video->audioSourceL = atoi(tokens[4].txt); 
                    }
                }
                else if(strncmp(tokens[3].txt, "ASR-", 4) == 0)  /* V-x\CLX\V\ASR-n audio source Right */
                {
                    if( (strncmp(tokens[4].txt,"LC",2) == 0) )
                    {
                        m23_video->audioSourceR = AUDIO_LOCAL;
                    }
                    else if( (strncmp(tokens[4].txt,"ET",2) == 0) )
                    {
                        m23_video->audioSourceR = AUDIO_EVENT_TONE;
                    }
                    else if( (strncmp(tokens[4].txt,"NO",2) == 0) )
                    {
                        m23_video->audioSourceR = AUDIO_NONE;
                    }
                    else //We Have an Audio Channel
                    {
                        m23_video->audioSourceR = atoi(tokens[4].txt); 
                    }
                }
                else if(strncmp(tokens[3].txt, "VOE-", 4) == 0)      /* V-x\CLX\V\VOE-n Overlay Enable */
                {
                    if(tokens[4].txt[0] == 'T')
                    {
                        m23_video->OverlayEnable = OVERLAY_ON;
                    }
                    else
                    {
                        m23_video->OverlayEnable = OVERLAY_OFF;
                    }
                }
                else if(strncmp(tokens[3].txt, "VOS-", 4) == 0)      /* V-x\CLX\V\VOS-n Overlay Font Size  */
                {
                    if(tokens[4].txt[0] == 'L')
                    {
                        m23_video->OverlaySize = LARGE_FONT;
                    }
                    else
                    {
                        m23_video->OverlaySize = SMALL_FONT;
                    }
                }
                else if(strncmp(tokens[3].txt, "VOC-", 4) == 0)      /* V-x\CLX\V\VOY-n Overlay Time Coding*/
                {
                    if(strncmp(tokens[4].txt,"TO",2) == 0)
                    {
                        m23_video->Overlay_Time = TIME_ONLY;     /*No day or milliseconds*/
                    }
                    else if(strncmp(tokens[4].txt,"DTM",3) == 0)  
                    {
                        m23_video->Overlay_Time = TIME_DAY_MS;   /*time day and milliseconds*/
                    }
                    else if(strncmp(tokens[4].txt,"DT",2) == 0)  
                    {
                        m23_video->Overlay_Time = TIME_DAY;      /*time,day no millisecondes*/
                    }
                    else if(strncmp(tokens[4].txt,"TM",2) == 0)  
                    {
                        m23_video->Overlay_Time = TIME_MS;       /*no day but with milliseconds*/
                    }
                    else
                    {
                        m23_video->Overlay_Time = NO_TIME_DAY;
                    }
                }
                else if(strncmp(tokens[3].txt, "VOF-", 4) == 0)      /* V-x\CLX\V\VOF-n Overlay Format*/
                {
             
                    if(strncmp(tokens[4].txt,"BOW",3) == 0)
                    {
                        m23_video->Overlay_Format = BLACK_WHITE;
                    }
                    else if(strncmp(tokens[4].txt,"WOB",3) == 0)
                    {
                        m23_video->Overlay_Format = WHITE_BLACK;
                    }
                    else if(strncmp(tokens[4].txt,"BOT",3) == 0)
                    {
                        m23_video->Overlay_Format = BLACK_TRANS;
                    }
                    else
                    {
                        m23_video->Overlay_Format = WHITE_TRANS;
                    }
 
                }
                else if(strncmp(tokens[3].txt, "VOP-", 4) == 0)      /* V-x\CLX\V\VOP-n Overlay Generate*/
                {
                    if(strncmp(tokens[4].txt,"OFF",3) == 0)
                    {
                        m23_video->Overlay_Generate = GENERATE_OFF;
                    }
                    else
                    {
                        m23_video->Overlay_Generate = GENERATE_BW;
                    }
                }
                else if(strncmp(tokens[3].txt, "LVO-", 4) == 0)      /* V-x\CLX\V\LVO-n Video Out*/
                {
                    if(tokens[4].txt[0] == 'T')
                    {
                        m23_video->VideoOut = 1;
                    }
                }
            }
            else
            {
                if(strncmp(tokens[2].txt, "CN-", 3) == 0)       /* V-x\CLX\CN-n channel number */
                {
                    m23_video->chanNumber = atoi(tokens[3].txt);
//printf("Read Channel Number %d\r\n",m23_video->chanNumber);
                }
                else if(strncmp(tokens[2].txt,"IIM-",4) == 0)   /*V-x\CLX\IIM-n ; Included in mask*/
                {
                    if(tokens[3].txt[0] == 'T')
                    {
                        m23_video->included = TRUE;
                    }
                }
            }
        }

        videoTmats += line_length;
    }

    return(0);
}


/* parsing the 1553 channel TMATS section */
int Decode_M1553Channel(char *m1553Tmats, M1553_CHANNEL *m23_m1553, DISCRETE_CHANNEL *m23_dis, int *status)
{
    char    line[100];
    ctoken tokens[10];
    int    ret;
    int    index;
    int    debug;
    int    cw;
    int    numTokens;
    int    line_length;

    *status = TMATS_PARSE_SUCCESS;

    m23_m1553->m_dataType = INPUT_M1553;

    m23_m1553->m_WatchWordPattern = 0x0;
    m23_m1553->m_WatchWordMask = 0x0;
    m23_m1553->m_WatchWordLockIntervalInMilliseconds = 10000;
    m23_m1553->WatchWordEnabled = 0;
    m23_dis->DiscreteMask = 0;
    m23_m1553->isDiscrete = 0;
    m23_m1553->NumFiltered = 0;


    M23_GetDebugValue(&debug);

    while((line_length = get_new_line(m1553Tmats, line)) > 1)
    {
        line[line_length-2] = '\0';     /* chop off ";" */
        ret = split_line(line, tokens, &numTokens);
        if(ret == 0)
        {
            if(numTokens < 1)
            {
                printf("Error in TMATS file, quit...\n");
                return(1);
            }
        }
        else
        {
            return(1);
        }

        if(tokens[0].txt[0] == 'G')                /* G */
        {
            return(0);
        }

        if(strncmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group */
        {
            if(strncmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
            {
                m23_m1553->chanID = atoi(tokens[2].txt);
                m23_dis->chanID = atoi(tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "CHE-", 4) == 0)         /* R-x\CHE-n channel enable */
            {
                if(tokens[2].txt[0] == 'T')
                {
                    //printf("M1553 ENABLED\r\n");
                    m23_m1553->isEnabled = 1;
                    m23_dis->isEnabled   = 1;

                }
                else
                {
                    //printf("M1553 NOT ENABLED\r\n");
                    m23_m1553->isEnabled = 0;
                    m23_dis->isEnabled   = 0;
                }
            }
            else if(strncmp(tokens[1].txt, "DSI-", 4) == 0)         /* R-x\DSI-n channel name */
            {
                strcpy(m23_m1553->chanName, tokens[2].txt);
                strcpy(m23_dis->chanName, tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt,"BDLN-",5) == 0)   /*B-x\BDLN-n ; Bus Data Link*/
            {
                strcpy(m23_m1553->BusLink,tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt,"CDLN-",5) == 0)   /*B-x\CDLN-n ; Channel Data Link*/
            {
                strcpy(m23_m1553->BusLink,tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "TK4-", 4) == 0)          /* R-x\TK4-n channel of type*/
            {
                m23_m1553->chanNumber = atoi(tokens[2].txt);
                m23_dis->chanNumber = atoi(tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt,"DTF-",4) == 0)           /*R-x\DTF  Discrete Type Format*/
            {
                m23_m1553->isDiscrete = 1;
            }
            else if(strncmp(tokens[1].txt,"DMSK-",5) == 0)           /*R-x\DMSK  Discrete Channel Mask*/
            {
                m23_dis->DiscreteMask |= Binary2UL(tokens[2].txt);
                m23_m1553->isDiscrete = 1;
            }

        }
        else if(strncmp(tokens[0].txt, "V-", 2) == 0)               /* V-group */
        {
            if(strncmp(tokens[1].txt, "RDF",3) == 0)     /*V-x\RDF   M1553 Filtering*/
            {
                if(tokens[2].txt[0] == 'N')  /*V-x\RDF\N: M1553 Num Filtered Messages*/
                {
                    m23_m1553->NumFiltered = atoi(tokens[3].txt);
                   
                    if(debug)
                    {
                        printf("Number Filtered = %d\r\n",m23_m1553->NumFiltered);
                    }
                }
                else if(strncmp(tokens[3].txt, "RTA",3) == 0)  /*V-x\RDF\CWM\RTA: M1553 Filtered RT*/
                {
                    sscanf(&tokens[3].txt[4],"%d-%d",&cw,&index);
                    if( (tokens[4].txt[0] == 'X') || (tokens[4].txt[0] == 'x') )
                    {
                        m23_m1553->FilteredRT[index-1] = 0xFF;    
                    }
                    else
                    {
                        sscanf(tokens[4].txt,"%d",&cw);
                        m23_m1553->FilteredRT[index-1] = cw;    
                    }

                }
                else if(strncmp(tokens[3].txt, "SA",2) == 0)  /*V-x\RDF\CWM\SA: M1553 Filtered SubAddress*/
                {
                    sscanf(&tokens[3].txt[3],"%d-%d",&cw,&index);
                    if( (tokens[4].txt[0] == 'X') || (tokens[4].txt[0] == 'x') )
                    {
                        m23_m1553->FilteredSA[index-1] = 0xFF;    
                    }
                    else
                    {
                        sscanf(tokens[4].txt,"%d",&cw);
                        m23_m1553->FilteredSA[index-1] = cw;    
                    }

                }
                else if(strncmp(tokens[3].txt, "TR",2) == 0)  /*V-x\RDF\CWM\TR: M1553 Filtered Tranmit,Receive*/
                {
                    sscanf(&tokens[3].txt[3],"%d-%d",&cw,&index);
                    if( (tokens[4].txt[0] == 'X') || (tokens[4].txt[0] == 'x') )
                    {
                        m23_m1553->FilteredTR[index-1] = 0xFF;    
                    }
                    else
                    {
                        sscanf(tokens[4].txt,"%d",&cw);
                        m23_m1553->FilteredTR[index-1] = cw;    
                    }

                }
                else if(strncmp(tokens[2].txt, "FDT",3) == 0)  /*V-x\RDF\FDT: M1553 Filter Type*/
                {
                    if(strncmp(tokens[3].txt, "EX",2) == 0)  /*Filter Exclude*/
                    {
                        m23_m1553->FilterType = EXCLUDE;    
                        if(debug)
                        {
                            printf("Filter Type Exclude\r\n");
                        }
                    }
                    else
                    {
                        m23_m1553->FilterType = INCLUDE;    
                        if(debug)
                        {
                            printf("Filter Type Include\r\n");
                        }
                    }
         
                }
            }
            else if(strcmp(tokens[2].txt, "B") == 0)                     /* V-x\CLX\B\... */
            {

                if(strncmp(tokens[3].txt, "WWP-", 4) == 0)     /* V-x\CLX\B\WWP-n watch word pattern */
                {
                    m23_m1553->m_WatchWordPattern = Binary2UL(tokens[4].txt);
                }

                else if(strncmp(tokens[3].txt, "WWM-", 4) == 0)     /* V-x\CLX\B\WWM-n watch word mask */
                {
                    m23_m1553->m_WatchWordMask = Binary2UL(tokens[4].txt);
                }

                else if(strncmp(tokens[3].txt, "WWLI-", 5) == 0)    /* V-x\CLX\B\WWLI-n watch word lock internal */
                {
                    m23_m1553->m_WatchWordLockIntervalInMilliseconds = atoi(tokens[4].txt);
                    if( m23_m1553->m_WatchWordLockIntervalInMilliseconds == 0)
                    {
                        m23_m1553->WatchWordEnabled = 0;
                    }
                    else
                    {
                        m23_m1553->WatchWordEnabled = 1;
                    }
                }
            }
            else
            {
                if(strncmp(tokens[2].txt, "CN-", 3) == 0)           /* V-x\CLX\CN-n channel number */
                {
                    m23_m1553->chanNumber = atoi(tokens[3].txt);
                }
                else if(strncmp(tokens[2].txt, "CSM-", 4) == 0)     /* V-x\CLX\CSM-n packet method */
                {
                    if(strncmp(tokens[3].txt,"AUTO",4) == 0 )
                    {
                        m23_m1553->pktMethod = M1553_PACKET_METHOD_AUTO;
                    }
                    else
                    {
                        m23_m1553->pktMethod = M1553_PACKET_METHOD_SINGLE;
                    }
                }
                else if(strncmp(tokens[2].txt,"IIM-",4) == 0)   /*V-x\CLX\IIM-n ; Included in mask*/
                {
                    if(tokens[3].txt[0] == 'T')
                    {
                        m23_m1553->included = TRUE;
                    }
                }
            }
        }
        else if(strncmp(tokens[0].txt, "B-", 2) == 0)               /* B-group */
        {
        }
        else if(strncmp(tokens[0].txt, "M-", 2) == 0)               /* M-group */
        {
        }

        m1553Tmats += line_length;
    }

    return(0);
}


/* parsing the PCM channel TMATS section */
int Decode_PCMChannel(char *pcmTmats, PCM_CHANNEL *m23_pcm, int *status)
{
    char   line[200];
    ctoken tokens[10];
    char   pcmSync[48];
    int    numTokens;
    int    line_length;
    int    ret;
    int    link;
    int    index;
    int    wordnum;

    *status = TMATS_PARSE_SUCCESS;

    m23_pcm->m_dataType = INPUT_PCM;

    m23_pcm->pcm_WatchWordPattern = 0;
    m23_pcm->pcm_WatchWordMask = 0;
    m23_pcm->pcm_WatchWordOffset = 0;
     m23_pcm->OnController = 0;

    m23_pcm->pcm_WatchWordMinorSFID = 0;
    m23_pcm->pcmSFIDBitOffset = 0xFFFFFFFF;

    m23_pcm->pcmDirection = PCM_NORMAL;
    m23_pcm->pcmClockPolarity = PCM_NORMAL;

    m23_pcm->PCM_FilteringEnabled = FILTERING_NONE;
    m23_pcm->PCM_CRCEnable = NO_CRC;
    m23_pcm->PCM_RecordFilteredSelect = FALSE;
    m23_pcm->PCM_TransmitFilteredSelect = FALSE;

    m23_pcm->PCM_RecordEnable = TRUE;

    for(index = 0; index < 1024;index++)
    {
        m23_pcm->PCM_WordNumber[index] = 0;
    }

    while((line_length = get_new_line(pcmTmats, line)) > 1)
    {
        //line[line_length-3] = '\0';     /* chop off ";" */
                line[line_length-2] = '\0';
        ret = split_line(line, tokens, &numTokens);
        if(ret == 0)
        {
            if(numTokens < 1)
            {
                printf("Error in TMATS file, quit...\n");
                return(1);
            }
        }
        else
        {
            return(1);
        }

        if(tokens[0].txt[0] == 'G')                /* G */
        {
            return(0);
        }


        if(strncasecmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group */
        {
            if(strncasecmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
            {
                m23_pcm->chanID = atoi(tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "CHE-", 4) == 0)         /* R-x\CHE-n channel enable */
            {
                if(tokens[2].txt[0] == 'T')
                {
                    m23_pcm->isEnabled = 1;
                }
                else
                {
                    m23_pcm->isEnabled = 0;
                }
            }
            else if(strncasecmp(tokens[1].txt, "DSI-", 4) == 0)         /* R-x\DSI-n channel name */
            {
                strcpy(m23_pcm->chanName, tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "PDP-", 4) == 0)         /* R-x\DP-n Data Packing Option */
            {
                if(strncasecmp(tokens[2].txt,"UN",2) == 0 )
                {
                    m23_pcm->pcmDataPackingOption = PCM_UNPACKED;
                }
                else if(strncasecmp(tokens[2].txt,"TM",2) == 0 )
                {
                    m23_pcm->pcmDataPackingOption = PCM_THROUGHPUT;
                }
                else
                {
                    m23_pcm->pcmDataPackingOption = PCM_PACKED;
                }

                //strcpy(m23_pcm->pcmDataPackingOption, tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "TK4-", 4) == 0)          /* R-x\TK4-n channel of type*/
            {
                m23_pcm->chanNumber = atoi(tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "IST-", 4) == 0)          /* R-x\IST-n channel of type*/
            {
                if(strncasecmp(tokens[2].txt,"RS422",5) == 0)
                {
                    m23_pcm->pcmInputSource = PCM_422;
                }
                else
                {
                    m23_pcm->pcmInputSource = PCM_TTL;
                }
            }
            else if(strncasecmp(tokens[1].txt, "UCB-",4) == 0)                   /* R-x\UCB- ,Data Bits*/
            {
                m23_pcm->UARTDataBits = atoi(tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "UCR-",4) == 0)                   /*R-x\UCR- ,Baud Rate*/
            {
                m23_pcm->UARTBaudRate = atoi(tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "UCP-",4) == 0)                   /* R-x\UCP- ,Parity*/
            {
                if(tokens[2].txt[0] == 'E' )
                {
                    m23_pcm->UARTParity = PARITY_EVEN;
                }
                else if(tokens[2].txt[0]== 'O' )
                {
                    m23_pcm->UARTParity = PARITY_ODD;
                }
                else
                {
                    m23_pcm->UARTParity = PARITY_NONE;
                }
            }

        }

        else if(strncasecmp(tokens[0].txt, "V-", 2) == 0)               /* V-group */
        {
            if(strncmp(tokens[2].txt, "CN-",3) == 0)                   /* V-x\CLX\CN-n:m\... */
            {
                m23_pcm->chanNumber = atoi(tokens[3].txt);
            }
            else if(strcmp(tokens[2].txt, "P") == 0)                   /* V-x\CLX\P\... */
            {
                if(strncasecmp(tokens[3].txt, "CP7-", 4) == 0)      /* V-x\CLX\P\CP7-n:i Bit Sync */
                {
                    if(tokens[4].txt[0] == 'F')
                    {
                        m23_pcm->pcmBitSync = FALSE;
                    }
                    else
                    {
                        m23_pcm->pcmBitSync = TRUE;
                    }
                }
                else if(strncmp(tokens[3].txt, "D9-", 3) == 0)          /* V-x\CLX\P\D9-n:i Clock Polarity */
                {
                    if(tokens[4].txt[0] == 'N')
                    {
                        m23_pcm->pcmClockPolarity = PCM_NORMAL;
                    }
                    else
                    {
                        m23_pcm->pcmClockPolarity = PCM_INVERTED;
                    }
                }

                else if(strncasecmp(tokens[3].txt, "MF6-", 4) == 0)          /* V-x\CLX\P\MF6-n:i Sync Mask */
                {
                    memset(pcmSync,0x0,48);
                    strncpy(pcmSync,tokens[4].txt,strlen(tokens[4].txt));
                    m23_pcm->pcmSyncMask = ( Binary2UL(pcmSync));

                }
                else if(strncasecmp(tokens[3].txt, "IDCM-", 5) == 0)          /* V-x\CLX\P\IDCM-n-j:i ID counter Mask */
                {
                    memset(pcmSync,0x0,48);
                    strncpy(pcmSync,tokens[4].txt,strlen(tokens[4].txt));

                    //PC 1/16/06 m23_pcm->pcmIdCounterMask = ~(Binary2UL(pcmSync));
                    m23_pcm->pcmIdCounterMask = (Binary2UL(pcmSync));
                    //printf("ID Mask = %s 0x%x\r\n",tokens[4].txt,m23_pcm->pcmIdCounterMask);
                }
                else if(strncasecmp(tokens[3].txt, "IDCB-", 5) == 0)          /* V-x\CLX\P\IDCB-n-j:i SFID Bit offset in Minor*/
                {
                    m23_pcm->pcmSFIDBitOffset = atoi(tokens[4].txt);
                }
                else if(strncasecmp(tokens[3].txt, "WWP-", 4) == 0)         /* V-x\CLX\P\WWP-n Watch Word Pattern*/
                {
                    m23_pcm->pcm_WatchWordPattern = Binary2UL(tokens[4].txt);
                }
                else if(strncasecmp(tokens[3].txt, "WWM-", 4) == 0)         /*V-x\CLX\P\WWP-n Watch Word Mask*/
                {
                    m23_pcm->pcm_WatchWordMask = Binary2UL(tokens[4].txt);
                }
                else if(strncasecmp(tokens[3].txt, "WWB-", 4) == 0)        /* V-x\CLX\P\WWB-n Watch Word Bit Position*/
                {
                    m23_pcm->pcm_WatchWordOffset = atoi(tokens[4].txt);
                }
                else if(strncasecmp(tokens[3].txt, "WWF-", 4) == 0)        /* V-x\CLX\P\WWF-n Watch Word Bit Position*/
                {
                    m23_pcm->pcm_WatchWordMinorSFID = atoi(tokens[4].txt);
                }

            }
            else if(strncmp(tokens[2].txt, "CUCE-",5) == 0)                   /* V-x\CLX\CUCE- ,ON Controller*/
            {
                if(tokens[3].txt[0] == 'T')
                {
                    m23_pcm->OnController = 1;
                }
            }
            else if(strncasecmp(tokens[2].txt,"IIM-",4) == 0)   /*V-x\CLX\IIM-n ; Included in mask*/
            {
                if(tokens[3].txt[0] == 'T')
                {
                    m23_pcm->included = TRUE;
                }
            }
            else if(strncasecmp(tokens[2].txt,"D1D-",4) == 0)   /*V-x\CLX\D1D-n ; RNRZL direction*/
            {
                if(tokens[3].txt[0] == 'R')
                {
                    m23_pcm->pcmDirection = PCM_INVERTED;
                }
            }
            else if(strncasecmp(tokens[2].txt,"FME-",4) == 0)   /*V-x\CLX\FME-n ; Filtering Enabled*/
            {
                if(tokens[3].txt[0] == 'I')
                {
                    m23_pcm->PCM_FilteringEnabled = INCLUDE;
                }
                else if(tokens[3].txt[0] == 'E')
                {
                    m23_pcm->PCM_FilteringEnabled = EXCLUDE;
                }

            }
            else if(strncasecmp(tokens[2].txt,"FEC-",4) == 0)   /*V-x\CLX\FEC-n ;  Number Of Filter Entries*/
            {
                m23_pcm->PCM_NumFilterd = atoi(tokens[3].txt);
            }
            else if(strncasecmp(tokens[2].txt,"MFW3-",5) == 0)   /*V-x\CLX\MFW3-n ;  Filter Word Number*/
            {
                //sscanf(&tokens[2].txt[5],"%d-%d",&link,&index);
                wordnum = atoi(tokens[3].txt);
                m23_pcm->PCM_WordNumber[wordnum -1] = wordnum;
            }
            else if(strncasecmp(tokens[2].txt,"CRCE-",5) == 0)   /*V-x\CLX\CRCE-n ;  CRC Enable*/
            {
                if(strncasecmp(tokens[3].txt,"MA",2) == 0 )
                {
                    m23_pcm->PCM_CRCEnable = MAJOR_CRC;
                }
                else if(strncasecmp(tokens[3].txt,"MI",2) == 0 )
                {
                    m23_pcm->PCM_CRCEnable = MINOR_CRC;
                }
            }
            else if(strncasecmp(tokens[2].txt,"RFDS-",5) == 0)   /*V-x\CLX\RFDS-n ; Record Filtered Data Select*/
            {
                if(tokens[3].txt[0] == 'T')
                {
                    m23_pcm->PCM_RecordFilteredSelect = TRUE;
                }
            }
            else if(strncasecmp(tokens[2].txt,"TFDS-",5) == 0)   /*V-x\CLX\TFDS-n ; Transmit Filtered Data Select*/
            {
                if(tokens[3].txt[0] == 'T')
                {
                    m23_pcm->PCM_TransmitFilteredSelect = TRUE;
                }
            }
        }
        else if(strncasecmp(tokens[0].txt, "P-", 2) == 0)               /* P-group */
        {
             //printf("P_%s\r\n",tokens[1].txt);

            if(strcmp(tokens[1].txt, "D1") == 0)                   /* P-x\D1-n:m\... PCM Code NRZ-L */
            {
                if(strncasecmp(tokens[2].txt,"NRZ-L",5) == 0 )  /*NRZL*/
                {
                    m23_pcm->pcmCode = PCM_NRZL;
                }
                else if(strncasecmp(tokens[2].txt,"RNRZ-L",6) == 0 )  /*NRZL*/
                {
                    m23_pcm->pcmCode = PCM_RNRZL;
                }
                else if(strncasecmp(tokens[2].txt,"BIO-L",5) == 0 )  /*NRZL*/
                {
                    m23_pcm->pcmCode = PCM_BIOL;
                }
                else if(strncasecmp(tokens[2].txt,"UART",4) == 0 )  /*NRZL*/
                {
                    m23_pcm->pcmCode = PCM_UART;
                }
                //strcpy(m23_pcm->pcmCode,tokens[2].txt);
            }
            else if(strcmp(tokens[1].txt, "D2") == 0)              /* P-x\D2-n:m\... */
            {
                m23_pcm->pcmBitRate = atoi(tokens[2].txt);
            }
            else if(strcmp(tokens[1].txt, "D4") == 0)              /* P-x\D4-n:m\... */
            {
                if(tokens[2].txt[0] == 'N')
                {
                    m23_pcm->pcmDataPolarity = PCM_NORMAL;
                }
                else
                {
                    m23_pcm->pcmDataPolarity = PCM_INVERTED;
                }
                //strcpy(m23_pcm->pcmDataPolarity, tokens[2].txt);
            }
            else if(strcmp(tokens[1].txt, "D6") == 0)              /* P-x\D6-n:m\... */
            {
                if(tokens[2].txt[0] == 'N')
                {
                    m23_pcm->pcmDataDirection = PCM_NORMAL;
                }
                else
                {
                    m23_pcm->pcmDataDirection = PCM_INVERTED;
                }
            }
            else if(strcmp(tokens[1].txt, "F2") == 0)              /* P-x\F2-n:m\... */
            {
                if(tokens[2].txt[0] == 'M')
                {
                    m23_pcm->pcmWordTransferOrder = MSB_FIRST;
                }
                else
                {
                    m23_pcm->pcmWordTransferOrder = LSB_FIRST;
                }
            }
            else if(strcmp(tokens[1].txt, "SYNC1") == 0)              /* P-x\SYNC1-n:m\... */
            {
                m23_pcm->pcmInSyncCriteria = atoi(tokens[2].txt);
            }
            else if(strcmp(tokens[1].txt, "SYNC2") == 0)              /* P-x\SYNC2-n:m\... */
            {
                m23_pcm->pcmSyncPatternCriteria = atoi(tokens[2].txt);
            }
            else if(strcmp(tokens[1].txt, "SYNC3") == 0)              /* P-x\SYNC3-n:m\... */
            {
                m23_pcm->pcmNumOfDisagrees = atoi(tokens[2].txt);
            }
            else if(strcmp(tokens[1].txt, "MF4") == 0)              /* P-x\MF4-n:m\... */
            {
                m23_pcm->pcmSyncLength = atoi(tokens[2].txt);
            }
            else if(strcmp(tokens[1].txt, "MF5") == 0)              /* P-x\MF5-n:m\... */
            {
                memset(pcmSync,0x0,48);

                strncpy(pcmSync,tokens[2].txt,strlen(tokens[2].txt));

                m23_pcm->pcmSyncPattern = Binary2UL(pcmSync);
                //printf("Sync Pattern -> %s,0x%x\r\n",pcmSync,m23_pcm->pcmSyncPattern);
            }
            else if(strcmp(tokens[1].txt, "F1") == 0)              /* P-x\F1-n:m\... */
            {
                m23_pcm->pcmCommonWordLength = atoi(tokens[2].txt);
            }
            else if(strcmp(tokens[1].txt, "MF1") == 0)              /* P-x\MF1-n:m\... */
            {
                m23_pcm->pcmWordsInMinorFrame = atoi(tokens[2].txt);
            }
            else if(strcmp(tokens[1].txt, "MF2") == 0)              /* P-x\MF2-n:m\... */
            {
                m23_pcm->pcmBitsInMinorFrame = atoi(tokens[2].txt);
            }
            else if(strcmp(tokens[1].txt, "MF") == 0)              /* P-x\MF\N:m\... */
            {
                m23_pcm->pcmMinorFramesInMajorFrame = atoi(tokens[3].txt);//Skip N -> tokens[2]
            }
            else if(strncasecmp(tokens[1].txt, "IDC1-",5) == 0)              /* P-x\IDC1-n:m\... */
            {
                m23_pcm->pcmSFIDCounterLocation = atoi(tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "IDC4-",5) == 0)              /* P-x\IDC4-n:m\... */
            {
                m23_pcm->pcmSFIDLength = atoi(tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "IDC6-",5) == 0)              /* P-x\IDC6-n:m\... */
            {
                m23_pcm->pcmIdCounterInitialValue = atoi(tokens[2].txt);
            }
            else if(strncasecmp(tokens[1].txt, "DLN",3) == 0)              /* P-x\DLN:m  Data Link Name*/
            {
                strcpy(m23_pcm->LinkName, tokens[2].txt);
            }

        }

        pcmTmats += line_length;

    }


    return(0);

}

/* parsing the Ethernet channel TMATS section */
int Decode_Ethernet(char *EthTmats, M23_CHANNEL *m23_chan,ETHERNET_CHANNEL *m23_eth, int *status)
{
    char   line[100];
    char   tmp[10];
    ctoken tokens[10];
    int    ret;
    int    numTokens;
    int    other = 0;
    int    line_length;

    *status = TMATS_PARSE_SUCCESS;

    m23_eth->CH10 = 1;

    m23_eth->MacLow  = 0x0000;
    m23_eth->MacMid  = 0x5223;
    m23_eth->MacHigh = 0x0014;

    m23_eth->IPHigh = 0;
    m23_eth->IPMid1 = 0;
    m23_eth->IPMid2 = 0;
    m23_eth->IPLow  = 0;

    m23_eth->Mode = 1; //Promiscous Mode

    while((line_length = get_new_line(EthTmats, line)) > 1)
    {
        line[line_length-2] = '\0';     /* chop off ";" */
        ret = split_line(line, tokens, &numTokens);
        if(ret == 0)
        {
            if(numTokens < 1)
            {
                printf("Error in TMATS file, quit...\n");
                return(1);
            }
        }
        else
        {
            return(1);
        }

        if(tokens[0].txt[0] == 'G')                /* G */
        {
            return(0);
        }

        if(strncmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group */
        {
            if(strncmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
            {
                m23_eth->chanID = atoi(tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "CHE-", 4) == 0)         /* R-x\CHE-n channel enable */
            {
                if(tokens[2].txt[0] == 'T')
                {
                    m23_eth->isEnabled = 1;
                }
                else
                {
                    m23_eth->isEnabled = 0;
                }
            }
            else if(strncmp(tokens[1].txt, "DSI-", 4) == 0)         /* R-x\DSI-n channel name */
            {
                strcpy(m23_eth->chanName, tokens[2].txt);
            }
        }
        else if(strncmp(tokens[0].txt, "V-", 2) == 0)           /* V-group */
        {
            if(strncmp(tokens[2].txt, "EFM-", 4) == 0)          /*EFM is only set for AirBus */
            {
                if(strncmp(tokens[3].txt, "AB", 2) == 0)        /*This is AirBus*/
                {
                    m23_eth->CH10 = 0;
                }
            }
            if(strncmp(tokens[2].txt, "EIP-", 4) == 0)          /*EIP is the IP Address*/
            {
                sscanf(tokens[3].txt,"%d.%d.%d.%d",&m23_eth->IPHigh,&m23_eth->IPMid1,&m23_eth->IPMid2,&m23_eth->IPLow);
                /*Setup the New ARP Message with this IP*/
                M23_UpdateIP(m23_eth->IPHigh,m23_eth->IPMid1,m23_eth->IPMid2,m23_eth->IPLow);
                
            }
            else if(strncmp(tokens[2].txt, "RIP-", 4) == 0)          /*RIP is the Radar IP Address*/
            {
                sscanf(tokens[3].txt,"%d.%d.%d.%d",&m23_eth->Radar_IPHigh,&m23_eth->Radar_IPMid1,&m23_eth->Radar_IPMid2,&m23_eth->Radar_IPLow);
            }
            else if(strncmp(tokens[2].txt, "RPN-", 4) == 0)          /* port is the Radar PORT*/
            {
                m23_eth->port = atoi(tokens[3].txt);
            }
            else if(strncmp(tokens[2].txt, "EMA-", 4) == 0)          /*EMA is the MAC Address*/
            {
                memset(tmp,0x0,10);
                strncpy(tmp,tokens[3].txt,4);
                sscanf(tmp,"%x",&m23_eth->MacHigh);

                memset(tmp,0x0,10);
                strncpy(tmp,&tokens[3].txt[4],4);
                sscanf(tmp,"%x",&m23_eth->MacMid);

                memset(tmp,0x0,10);
                strncpy(tmp,&tokens[3].txt[8],4);
                sscanf(tmp,"%x",&m23_eth->MacLow);
            }
            else if(strncmp(tokens[2].txt, "ECP-", 4) == 0)          /* ECP is Ethernet capture type*/
            {
                if(tokens[3].txt[0] == 'T')
                    m23_eth->Mode = 0;
                else if(tokens[3].txt[0] == 'M')
                    m23_eth->Mode = 2;
            }

          
        }

        EthTmats += line_length;
    }


    return(0);
}

/* parsing the Time channel TMATS section */
int Decode_TimeChannel(char *TimeTmats, M23_CHANNEL *m23_chan,TIMECODE_CHANNEL *m23_time, int *status)
{
    char   line[100];
    ctoken tokens[10];
    int    ret;
    int    debug;
    int    numTokens;
    int    other = 0;
    int    line_length;

    *status = TMATS_PARSE_SUCCESS;

    M23_GetDebugValue(&debug);
    m23_time->m_dataType = INPUT_TIMECODE;

    m23_time->Source = TIMESOURCE_IS_INTERNAL;

    while((line_length = get_new_line(TimeTmats, line)) > 1)
    {
        line[line_length-2] = '\0';     /* chop off ";" */
        ret = split_line(line, tokens, &numTokens);
        if(ret == 0)
        {
            if(numTokens < 1)
            {
                printf("Error in TMATS file, quit...\n");
                return(1);
            }
        }
        else
        {
            return(1);
        }

        if(tokens[0].txt[0] == 'G')                /* G */
        {
            return(0);
        }

        if(strncmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group */
        {
            if(strncmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
            {
                m23_time->chanID = atoi(tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "CHE-", 4) == 0)         /* R-x\CHE-n channel enable */
            {
                if(tokens[2].txt[0] == 'T')
                {
                    //printf("TIME ENABLED\r\n");
                    m23_time->isEnabled = 1;
                }
                else
                {
                    //printf("TIME NOT ENABLED\r\n");
                    m23_time->isEnabled = 0;
                }
            }
            else if(strncmp(tokens[1].txt, "DSI-", 4) == 0)         /* R-x\DSI-n channel name */
            {
                strcpy(m23_time->chanName, tokens[2].txt);
            }
	    else if(strncmp(tokens[1].txt,"TFMT",4) == 0)
	    {
	        m23_time->Format = tokens[2].txt[0];
	    }
	    else if(strncmp(tokens[1].txt,"TSRC",4) == 0)
	    {
                if(tokens[2].txt[0] == 'E')
                {
                    if(m23_time->Source != TIMESOURCE_IS_M1553)
                    {
	                m23_time->Source = TIMESOURCE_IS_IRIG;
                    }
                }
	        else if(tokens[2].txt[0] == 'R')
                {
	            m23_time->Source = TIMESOURCE_IS_RMM;
                }
	    }
            else if(strncmp(tokens[1].txt, "TK4-", 4) == 0)          /* R-x\TK4-n channel of type */
            {
                m23_time->chanNumber = atoi(tokens[2].txt);
            }
        }
        else if(strncmp(tokens[0].txt, "V-", 2) == 0)               /* V-group */
        {
            if(strncmp(tokens[1].txt, "TMS",3) == 0)    /*M1553 TIMING*/     
            {
                if(tokens[2].txt[0] == 'E') /*V-x\TMS\E  Timing Enable*/
                {
                    if(tokens[3].txt[0] == 'T') /*Enabled*/
                    {
                        m23_time->m1553_timing.IsEnabled = 1;
	                m23_time->Source = TIMESOURCE_IS_M1553;
                    }
                    else
                    {
                        m23_time->m1553_timing.IsEnabled = 0;
                    }
                }
                else if(strncmp(tokens[2].txt, "ID",2) == 0) /*V-x\TMS\ID Channel ID*/
                {
                    if(strncmp(tokens[3].txt, "HC",2) == 0) /*V-x\TMS\ID Channel ID*/
                    {
                        m23_time->m1553_timing.ChannelId = 0xFF;
                    }
                    else
                    {
                        m23_time->m1553_timing.ChannelId = atoi(tokens[3].txt);
                    }
                }
                else if(strncmp(tokens[2].txt, "FMT",2) == 0) /*V-x\TMS\FMT M1553 Format*/
                {
                    m23_time->m1553_timing.Format = atoi(tokens[3].txt);
                }
                else if(strncmp(tokens[2].txt, "TCW",3) == 0) /*V-x\TMS\TCW Command Word*/
                {
                    sscanf(tokens[3].txt,"%x",&m23_time->m1553_timing.CommandWord);
                    //m23_time->m1553_timing.CommandWord = atoi(tokens[3].txt);
                }
                else if(strncmp(tokens[2].txt, "WLN",3) == 0) /*V-x\TMS\WLN Time Word Location in Message*/
                {
                    m23_time->m1553_timing.WordLocation = atoi(tokens[3].txt);
                }
                else if(strncmp(tokens[2].txt, "TWO",3) == 0) /*V-x\TMS\TWO Time Word Order*/
                {
                    if(tokens[3].txt[0] == 'L')
                    {
                        m23_time->m1553_timing.WordOrder = TIME_LSB_FIRST;
                    }
                    else
                    {
                        m23_time->m1553_timing.WordOrder = TIME_MSB_FIRST;
                    }
                }
                else if(tokens[2].txt[0] == 'N')        /*V-x\TMS\N: M1553 Number Of Time Words*/
                { 
                    m23_time->m1553_timing.NumberOfWords = atoi(tokens[3].txt);
                }
                else if(tokens[2].txt[0] == 'E')        /*V-x\TMS\E: M1553 Timing Enable*/
                { 
                    if(tokens[3].txt[0] == 'T')
                    {
                        m23_time->m1553_timing.IsEnabled = 1;
                    }
                    else
                    {
                        m23_time->m1553_timing.IsEnabled = 0;
                    }
                }
            }
            else if(strncmp(tokens[2].txt, "CN-", 3) == 0)           /* V-x\CLX\CN-n channel number */
            {
                m23_time->chanNumber = atoi(tokens[3].txt);
            }
        }


        TimeTmats += line_length;
    }

    return(0);

}


/* parsing the 1553 channel TMATS section */
int  Decode_VoiceChannel(char *VoiceTmats, VOICE_CHANNEL *m23_voice, int *status)
{
    char   line[100];
    ctoken tokens[10];
    int    numTokens;
    int    ret;
    int    line_length;

    *status = TMATS_PARSE_SUCCESS;

    m23_voice->m_dataType = INPUT_VOICE;
    m23_voice->Gain = 0x4;

    while((line_length = get_new_line(VoiceTmats, line)) > 1)
    {
        line[line_length-2] = '\0';     /* chop off ";" */
        ret = split_line(line, tokens, &numTokens);
        if(ret == 0)
        {
            if(numTokens < 1)
            {
                printf("Error in TMATS file, quit...\n");
                return(1);
            }
        }
        else
        {
            return(1);
        }

        if(tokens[0].txt[0] == 'G')                /* G */
        {
            return(0);
        }

        if(strncmp(tokens[0].txt, "R-", 2) == 0)                    /* R-group */
        {
            if(strncmp(tokens[1].txt, "TK1-", 4) == 0)          /* R-x\TK1-n channel ID */
            {
                m23_voice->chanID = atoi(tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "CHE-", 4) == 0)         /* R-x\CHE-n channel enable */
            {
                if(tokens[2].txt[0] == 'T')
                {
                    m23_voice->isEnabled = 1;
                }
                else
                {
                    m23_voice->isEnabled = 0;
                }
            }
            else if(strncmp(tokens[1].txt, "DSI-", 4) == 0)         /* R-x\DSI-n channel name */
            {
                strcpy(m23_voice->chanName, tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "TK4-", 4) == 0)          /* R-x\TK4-n channel of type*/
            {
                m23_voice->chanNumber = atoi(tokens[2].txt);
            }
            else if(strncmp(tokens[1].txt, "AGI-", 4) == 0)          /* R-x\AGI-n Gain*/
            {
                 if(strncmp(tokens[2].txt, "200", 3) == 0)   /* 0dB*/
                 {
                     m23_voice->Gain = 0x0;
                 }
                 else if(strncmp(tokens[2].txt, "283", 3) == 0) /* 3dB*/
                 {
                     m23_voice->Gain = 0x1;
                 }
                 else if(strncmp(tokens[2].txt, "400", 3) == 0) /*6dB*/
                 {
                     m23_voice->Gain = 0x2;
                 }
                 else if(strncmp(tokens[2].txt, "566", 3) == 0) /*9dB*/
                 {
                     m23_voice->Gain = 0x3;
                 }
                 else if(strncmp(tokens[2].txt, "800", 3) == 0) /*12dB*/
                 {
                     m23_voice->Gain = 0x4;
                 }
                 else if(strncmp(tokens[2].txt, "1131", 3) == 0) /*15dB*/
                 {
                     m23_voice->Gain = 0x5;
                 }
                 else if(strncmp(tokens[2].txt, "1600", 4) == 0) /*18dB*/
                 {
                     m23_voice->Gain = 0x6;
                 }
                 else if(strncmp(tokens[2].txt, "2263", 4) == 0) /*21dB*/
                 {
                     m23_voice->Gain = 0x7;
                 }
                 else if(strncmp(tokens[2].txt, "3200", 4) == 0) /*24dB*/
                 {
                     m23_voice->Gain = 0x8;
                 }
            }
            
        }
        else if(strncmp(tokens[0].txt, "V-", 2) == 0)               /* V-group */
        {
 
             if(strncmp(tokens[2].txt, "CN-", 3) == 0)           /* V-x\CLX\CN-n channel number */
             {
                m23_voice->chanNumber = atoi(tokens[3].txt);
             }
             if(strncmp(tokens[3].txt, "SC-", 3) == 0)           /* V-x\CLX\A\SC-N Audio Source */
             {
                 if(tokens[4].txt[0] == 'L')
                 {
                     m23_voice->Source = AUDIO_GLOBAL_LEFT;
                 }
                 else
                 {
                     m23_voice->Source = AUDIO_GLOBAL_RIGHT;
                 }
             }
             if(strncmp(tokens[3].txt, "PGA-", 4) == 0)           /* V-x\CLX\A\PGA-n Audio gain */
             {
                 if(tokens[4].txt[0] == '0')    //0dB
                 {
                     m23_voice->Gain = 0x0;
                 }
                 else if(tokens[4].txt[0] == '3')    //3dB
                 {
                     m23_voice->Gain = 0x1;
                 }
                 else if(tokens[4].txt[0] == '6')    //6dB
                 {
                     m23_voice->Gain = 0x2;
                 }
                 else if(tokens[4].txt[0] == '9')    //9dB
                 {
                     m23_voice->Gain = 0x3;
                 }
                 else if(tokens[4].txt[0] == '1')    
                 {
                     if(tokens[4].txt[1] == '2') //12db
                     {
                         m23_voice->Gain = 0x4;
                     }
                     else if(tokens[4].txt[1] == '5') //15db
                     {
                         m23_voice->Gain = 0x5;
                     }
                     else if(tokens[4].txt[1] == '8') //18db
                     {
                         m23_voice->Gain = 0x6;
                     }
                 }
                 else if(tokens[4].txt[0] == '2')    
                 {
                     if(tokens[4].txt[1] == '1') //21db
                     {
                         m23_voice->Gain = 0x7;
                     }
                     else if(tokens[4].txt[1] == '4') //24db
                     {
                         m23_voice->Gain = 0x8;
                     }
                 }
             }
        }


        VoiceTmats += line_length;
    }

    return(0);

}
int get_new_line_orig(char *fileContents, char *line)
{
    int i = 0;

    strcpy(line, "");

    
    while(fileContents[i] != '\0'
        && fileContents[i] != '\n'
        && fileContents[i] > 0)
    {
        line[i] = fileContents[i];
        i++;
    }

    line[i] = '\n';

    return i+1;
}

int get_new_line(char *fileContents, char *line)
{
    int i = 0;

    //strcpy(line, "");
    memset(line,0x0,100);


    while(fileContents[i] != '\0'
        && fileContents[i] != '\n'
        && fileContents[i] > 0)
    {
        line[i] = fileContents[i];
        i++;

        if(i >= 98)
            break;
    }


    line[i++] = '\n';

    if(line[0] == 0x20) { //this is a space
        line[0] = '\n';
        i = 1;
    }

    return i;
}



int split_line(char *line, ctoken *token, int *numTokens)
{
    int i;
    char *ptr;
    char cp[MAX_STRING];

    /* empty the token structure */
    for(i=0; i<10; i++)
        strcpy(token[i].txt, "");

    *numTokens = 0;
    i = 0;

    ptr = strtok(line, "\\");
    if(ptr == NULL)
    {
        PutResponse(0,"not a valid TMATS line.\r\n");
        return(1);
    }

    while(ptr != NULL)
    {
        strcpy(token[i].txt, ptr);
        strcpy(cp, ptr);    /* make a copy so that can do more spliting */
        i++;
        ptr = strtok(NULL, "\\");
    }

    ptr = strtok(cp, ":");
    if(ptr == NULL)
    {
        PutResponse(0,"missing value in the TMATS line.\r\n");
        return(1);
    }

    strcpy(token[i-1].txt, ptr);
    ptr = strtok(NULL, ":");
     strcpy(token[i].txt, ptr);
    *numTokens = i+1;

    return(0);
}

// Is a long int really an unsigned long int???
long int Binary2UL(char *str)
{
    int len, i;
    long int value=0;

    len = strlen(str);
    for(i=0; i<len; i++)
    {
        if ((str[i] == '0') || (str[i] == '1'))
            value = (value << 1) + (str[i] - '0');
    }

    return value;
}

int LoadTmats(char *TmatsData )
{
    memset( TmatsRecordBuffer, 0, 64*1024);
    strcpy(TmatsRecordBuffer , TmatsData);
    Tmats_Loaded_To_Read = TRUE;
}

int TmatsWrite(char *TmatsData )
{
    strcpy(TmatsGetBuffer , TmatsData);

    Tmats_Loaded_To_Read = TRUE;

    return NO_ERROR;
}

M23_PrepareTmatsBuffer()
{
    memset( TmatsGetBuffer, 0, 64*1024);
}


int TmatsSaveWrite( )
{
    int  end = 0;
    int  index;
    int  chars_read;
    char byte;
    char tmats_char[255];
    char tmats_line_holder[255];


    //Initialize the buffer to 0's
    memset(tmats_char,0,sizeof(tmats_char));
    memset(tmats_line_holder,0,sizeof(tmats_line_holder));
    memset(TmatsGetBuffer,0x0,64*1024);

    //Change System State to reading Tmats from user


    //Poll for END command
    while(1)
    {
        memset(tmats_line_holder,0,sizeof(tmats_line_holder));
        memset(tmats_char,0,sizeof(tmats_char));

        //Poll on the compoart for TMATS chars
        while(memcmp(tmats_char,"\n",1) != 0)
        {
            memset(tmats_char,0,sizeof(tmats_char));
            if( SerialGetChars( 0, &tmats_char[0], 1 ) )
            {
                //store chars the TMATS line
                strcat(tmats_line_holder,tmats_char);
            }
        }

            if( (strncasecmp( tmats_line_holder, "\nEND", 4 ) == 0) ||
                (strncasecmp( tmats_line_holder , "END" , 3 ) == 0) )
                break;

            //store everything to the TMATS buffer
            strcat(TmatsGetBuffer,tmats_line_holder);

    }

    Tmats_Loaded_To_Read = TRUE;


    return NO_ERROR;
}



//int TmatsRead( char *TmatsData )
int TmatsRead()
{
    CH10_STATUS RecorderStatus = GetRecorderStatus();

    //TMATS Read function checks if there is any tmats information read to the
    //volatile memory by TMATSGet command, if yes it prints the contents of
    //volatile buffer to the compoart
    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) )
    {
        printf( "Error: Invalid state for Tmats command\n" );
        return ERROR_INVALID_MODE;
    }

    if( Tmats_Loaded_To_Read != TRUE)
    {
        printf( "Error: Perform TMATS GET <x> command before reading\n" );
        return ERROR_COMMAND_FAILED;
    }

     //strcpy(TmatsData, TmatsGetBuffer);
     PutResponse( 0, TmatsGetBuffer );

    //Tmats_Loaded_To_Read = FALSE; 
    //It will Allow the TMATSread to return tmats from the first time TMATSget is
    //issued till TMATSsave or TMATSwrite over writes the TMATS buffer 

    return NO_ERROR;
}

int TmatsDelete( int setup )
{
    char deleteCommand[32];

    sprintf( deleteCommand, "rm -f /usr/calculex/Setup%d.tmt", setup );

    system(deleteCommand);

    return 0;
}

int TmatsDeleteAll()
{

    system("rm -f /usr/calculex/*.tmt");

    return 0;
}

int TmatsSave( int setup )
{
    CH10_STATUS RecorderStatus = GetRecorderStatus();

    //Tmats Save function takes a parameter of the setup file number
    //and save the contents of Volatile buffer to the system under system/ 
    //directory  
    char tmats_filename[32];
    FILE *p_file;


    if ( ( RecorderStatus != STATUS_IDLE ) && ( RecorderStatus != STATUS_ERROR ) )
    {
        printf( "Error: Invalid state for Tmats command\n" );
        return ERROR_INVALID_MODE;
    }

    if(Tmats_Loaded_To_Read != TRUE)
    {
    
        printf( "Error: No Tmats in the volatile buffer\n" );
        return ERROR_COMMAND_FAILED;
    
    }


    // open TMATS file with the number passes as parameter and write 
    // the contents of volatile buffer to it
    //sprintf( tmats_filename, "/usr/Calculex/Setup%d.tmt", setup );
    sprintf( tmats_filename, "./Setup%d.tmt", setup );
    p_file = fopen( tmats_filename, "wb" );

    if ( !p_file ) 
    {
        printf( "TmatsGet: TMATS file %s can not be created\n", tmats_filename );
        Tmats_Loaded_To_Read = FALSE;
        return ERROR_COMMAND_FAILED;
    }
    fwrite( TmatsGetBuffer, 1, strlen( TmatsGetBuffer ), p_file );

    if(p_file)
    fclose(p_file);

    //PC 4_21_05 Tmats_Loaded_To_Read = FALSE;

    return NO_ERROR;
}

int AddVersionToTmats(int length)
{

    int    i;
    int    numMP;
    int    month;
    int    day;
    int    year;
    int    hour;
    int    return_status;
    char   lyear[3];
    char   lmonth[3];
    char   lday[3];
    char   link[80];
    char   value[23];
    char   Unit[33];


    UINT32 LongCSR;
    char   tmp[256];

    char   version[8];

    int len;

    M23_NumberMPBoards(&numMP);

    len = length;

    memset(tmp,0x0,256);
    M23_GetUnitName(Unit);
    sprintf(tmp,"G\\COM: Unit Name                %s;      \r\n",Unit);
    strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
    len += strlen(tmp); 

    /*Add Versions to Host*/


    memset(tmp,0x0,256);
    sprintf(tmp,"G\\COM: System Versions          %s;      \r\n",SystemVersion);
    strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
    len += strlen(tmp); 


    memset(tmp,0x0,256);

    sprintf( tmp,"G\\COM: Firmware Version       - %s %s;\r\n",VERSION_DATE,VERSION_TIME);
    strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
    len += strlen(tmp); 


    memset(tmp,0x0,256);

    /*Print out the Controller version*/
    memset(tmp,0x0,256);


    LongCSR = M23_ReadControllerCSR(CONTROLLER_COMPILE_TIME);

    year  = (LongCSR >> 8) & 0x000000FF;
    month = (LongCSR >> 24) & 0x000000FF;

    if(month > 9)
    {
        month -= 6;
    }

    if(year > 9)
    {
        year -= 6;
    }

    day   = (LongCSR >> 16) & 0x000000FF;

    hour = LongCSR & 0x000000FF;

    sprintf(tmp,"G\\COM: Controller Board       - %s %02x %d %02x:00:00;\r\n",Months[month],day,year+2000,hour);
    strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
    len += strlen(tmp); 

    memset(tmp,0x0,256);

    for(i = 0; i < numMP;i++)
    {
        memset(tmp,0x0,256);
        LongCSR = M23_mp_read_csr(i,BAR1,MP_COMPILE_TIME_OFFSET);
        hour  = LongCSR & 0x000000FF;


        if(M23_MP_IS_PCM(i))
        {
            year  = (LongCSR >> 8) & 0x000000FF;
            month = (LongCSR >> 24) & 0x000000FF;

            if(month > 9)
            {
                month -= 6;
            }

            if(year > 9)
            {
                year -= 6;
            }

            day   = (LongCSR >> 16) & 0x000000FF;
            if(M23_MP_PCM_4_Channel(i))
            {
                sprintf(tmp,"G\\COM: PCM %d (4 Channel)      - %s %02x %d %02x:00:00;\r\n",i+1,Months[month],day,year+2000,hour);
                strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
                len += strlen(tmp); 
            }
            else
            {
                sprintf(tmp,"G\\COM: PCM %d (8 Channel)    - %s %02x %d %02x:00:00;\r\n",i+1,Months[month],day,year+2000,hour);
                strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
                len += strlen(tmp); 
            }
        }
        else if(M23_MP_IS_M1553(i))
        {
            year  = (LongCSR >> 8) & 0x000000FF;
            month = (LongCSR >> 24) & 0x000000FF;

            if(month > 9)
            {
                month -= 6;
            }
            if(year > 9)
            {
                year -= 6;
            }
            day   = (LongCSR >> 8) & 0x000000FF;
            /*Month Day year(yymmddhh) are different than PCM(mmddyyhh) */
            if(M23_MP_M1553_4_Channel(i))
            {
                sprintf(tmp,"G\\COM: M1553 %d (4 Channel)    - %s %02x %d %02x:00:00;\r\n",i+1,Months[month],day,year+2000,hour);
                strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
                len += strlen(tmp); 
            }
            else
            {
                sprintf(tmp,"G\\COM: M1553 %d (8 Channel)    - %s %02x %d %02x:00:00;\r\n",i+1,Months[month],day,year+2000,hour);
                strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
                len += strlen(tmp); 
            }
        }
        else if(M23_MP_IS_DM(i))
        {
            year  = (LongCSR >> 24) & 0x000000FF;
            month = (LongCSR >> 16) & 0x000000FF;
            if(month > 9)
            {
                month -= 6;
            }
            if(year > 9)
            {
                year -= 6;
            }
            day   = (LongCSR >> 8) & 0x000000FF;
            /*Month Day year(yymmddhh) are different than PCM(mmddyyhh) */
            sprintf(tmp,"G\\COM: DMP %d (4-1553 1-Dis)   - %s %02x %d %02x:00:00;\r\n",i+1,Months[month],day,year+2000,hour);
            strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
            len += strlen(tmp);
        }
        else
        {
            year  = (LongCSR >> 8) & 0x000000FF;
            month = (LongCSR >> 24) & 0x000000FF;

            if(month > 9)
            {
                month -= 6;
            }
            if(year > 9)
            {
                year -= 6;
            }

            day   = (LongCSR >> 16) & 0x000000FF;
            sprintf(tmp,"G\\COM: Video %d (4 Channel)    - %s %02x %d %02x:00:00;\r\n",i+1,Months[month],day,year+2000,hour);
            strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
            len += strlen(tmp); 
        }

    }

    return_status = DiskIsConnected();

    if(return_status)
    {
        memset(tmp,0x0,256);
        memset(version,0x0,8);
        memset(value,0x0,23);
        strncpy(version,RMM_Version,RMM_VersionSize);
        strncpy(value,&RMM_Version[RMM_VersionSize+1],20); 
        sprintf( tmp,"G\\COM: RMM Version %8s   - %s;\r\n",version,value);
        strncpy(&TmatsGetBuffer[len],tmp,strlen(tmp));
        len += strlen(tmp); 
    }


    return(len);


}

int TmatsGet( int setup )
{
    char tmats_filename[32];
    char TailName[40];
    int  taillen;
    int  config;
    int  len = 0;
    int  debug;
    FILE *p_file;

    CH10_STATUS RecorderStatus = GetRecorderStatus();


    if( LoadedTmatsFromCartridge == 0)
    {
        // open existing TMATS file and read it to the volatile memory
        //
        sprintf( tmats_filename, "./Setup%d.tmt", setup );
        //sprintf( tmats_filename, "/usr/Calculex/Setup%d.tmt", setup );
        p_file = fopen( tmats_filename, "rb" );

        if ( !p_file ) 
        {
            M23_GetDebugValue(&debug);
            if(debug)
                printf( "TmatsGet: TMATS file %s does not exist\r\n", tmats_filename );

            Tmats_Loaded_To_Read = FALSE;
            return ERROR_COMMAND_FAILED;
        }
    }

    M23_GetConfig(&config);
    memset( TmatsGetBuffer, 0, 64*1024);

    if( (config == 1) || (config = 2) ) //Only Eglin Units
    {
        memset(TailName,0x0,40);
        strncpy(TailName,"G\\TA:",5);
        M23_GetTailNumber(&TailName[5]);
        taillen = strlen(TailName);
        TailName[taillen++] = ';';
        TailName[taillen++] = '\r';
        TailName[taillen++] = '\n';

        strncpy(TmatsGetBuffer,TailName,taillen);
  
        if(RecordTmats == 0)
        {
            len = AddVersionToTmats(taillen);
        }

        //fread( &TmatsGetBuffer[len], 1, sizeof( TmatsGetBuffer ), p_file );
        if( LoadedTmatsFromCartridge == 0)
        {
            fread( &TmatsGetBuffer[len], 1, 64*1024, p_file );
        }
        else
        {
            strncpy( &TmatsGetBuffer[len], TmatsRecordBuffer,(64*1024) - len );
        }
    }
    else //All other units
    {
        if(RecordTmats == 0)
        {
            len = AddVersionToTmats(0);
        }
        if( LoadedTmatsFromCartridge == 0)
        {
            fread( &TmatsGetBuffer[len], 1, 64*1024, p_file );
        }
        else
        {
            strncpy( &TmatsGetBuffer[len], TmatsRecordBuffer,(64*1024) - len );
        }
    }


    Tmats_Loaded_To_Read = TRUE;


    if( LoadedTmatsFromCartridge == 0)
    {
        if(p_file)
        fclose(p_file);
    }

    return NO_ERROR;
}


int M23_TmatsWriteToHost()
{
    int    i;
    int    count = 0;
    int    loops = 0;
    int    length;
    int    bytes_to_write;
    int    setup;
    int    return_status;
    UINT32 CSR;
    UINT32 RootOffset;
    UINT16 *ptmp16;

    M23_CHANNEL  const *config;


    SetupGet(&config);


    /*Get the Current Setup*/
    SetupViewCurrent(&setup);

    /*Get the Current TMATS and load into buffer*/
    return_status = TmatsGet(setup);

    if(return_status != NO_ERROR)
    {
        return(return_status);
    }

    length = strlen(TmatsGetBuffer);

    ptmp16 = (UINT16*)TmatsGetBuffer;
    /*since we have to write words, this will include fill if any*/
    length += length % 2;

    if(config->IndexIsEnabled )
    {
        /*Start The Index Channel Stuff
        CSR = M23_ReadControllerCSR( CONTROLLER_INDEX_CHANNEL_GO);
        M23_WriteControllerCSR( CONTROLLER_INDEX_CHANNEL_GO,CSR | INDEX_CHANNEL_RECORD);*/

        if(length % 4)
        {
            RootOffset = (length + 24 + 4 + 2 ) + 36 + 52 + 8;
        }
        else
        {
            RootOffset = (length + 24 + 4 + 2 + 2) + 36 + 52 + 8;
        }

        M23_WriteControllerCSR( CONTROLLER_INITIAL_ROOT_OFFSET,RootOffset);
    }
    else
    {
        /*clear The Index Channel Stuff*/
        CSR = M23_ReadControllerCSR( CONTROLLER_INDEX_CHANNEL_GO);
        M23_WriteControllerCSR( CONTROLLER_INDEX_CHANNEL_GO,CSR & ~INDEX_CHANNEL_RECORD);
    }


    /*Write the length to Host Channel CSR*/
    M23_WriteControllerCSR(CONTROLLER_HOST_CHANNEL_DATA_LEN_CSR,length);



    if(length < 1000)
    {
        bytes_to_write = length;
    }
    else
    {
        bytes_to_write = 1000;
    }
    /*Set the Packet type to Set Up Record*/
    CSR = M23_ReadControllerCSR(CONTROLLER_HOST_CHANNEL_ID_CSR);
    CSR |= 0x100000;
    M23_WriteControllerCSR(CONTROLLER_HOST_CHANNEL_ID_CSR,CSR);

    /*enable the Host Channel Record*/
    M23_SetHostChannelRecord();

    while(1)
    {
        /*Check the FIFO Empty Flag*/
        CSR = M23_ReadControllerCSR(CONTROLLER_HOST_FIFO_CSR);
        if(CSR & CONTROLLER_HOST_CHANNEL_FIFO_EMPTY)
        {


            /*Write The  Data*/
            for(i = 0; i < bytes_to_write/2; i++)
            {
                //PC - Remove SWAp 5/31/06
                M23_WriteControllerCSR(CONTROLLER_HOST_FIFO_CSR,*ptmp16);
                ptmp16++;
            }

            if(count == 0)
            {
              /*On the first data, set the packet Ready Bit*/
                CSR = M23_ReadControllerCSR(CONTROLLER_HOST_CHANNEL_ID_CSR);
                M23_WriteControllerCSR(CONTROLLER_HOST_CHANNEL_ID_CSR,CSR | CONTROLLER_HOST_CHANNEL_PKT_READY);
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
            loops++;

            if(loops == 100)
            {
                printf("Host FIFO Never Empty -> %d\r\n",count);

#if 0
                if(CSR & CONTROLLER_WAITING_FOR_BUF_TO_CLEAR)
                {
                    PutResponse(0,"Waiting For Buf End Ack to Clear\r\n");
                }
                else if(CSR & CONTROLLER_WAITING_FOR_END_ACK)
                {
                    PutResponse(0,"Waiting For Buf End Ack\r\n");
                }
                else if(CSR & CONTROLLER_WAITING_FOR_START)
                {
                    PutResponse(0,"Waiting For Buf Start\r\n");
                }
#endif

                break;
            }
        }
        usleep(2);

    }
}
