/***************************************************************************************
 *
 *    Copyright (C) 2004 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed
 *    for any purpose except as specifically authorized in writing.
 *
 *       File: M23_VideoInterface.c
 *       Version: 1.0
 *     Author: pcarrion
 *
 *            MONSSTR 2300/2100
 *
 *             The following will contain the functions that interface to the Video Controller (currently
 *             provided by MangoDSP). This interface will be used for both the M2300 and the
 *             M2100. In all functions, it is assumed that the audio portion of channel of each DSP will
 *             also be configured.
 *
 *    Revisions:
 ******************************************************************************************/

#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory.h>

#include "M23_Controller.h"
#include "M23_Stanag.h"
#include "M23_Status.h"
#include "M23_Utilities.h"
#include "M23_MP_Handler.h"
#include "M23_features.h"

#include "am_cs6651.h"

#define  CS6651_DEVICE              "/dev/cs6651"
int      CS6651_device;

int      CH10_TMATS;

#define  ON_CHIP_ADDRESS            0x40000
#define  CONTROLLER_FIFO_ADDRESS    0x80010000
#define  DMA_CSR                    0x80000820

#define  ETHERNET_DRAM_OFFSET       0x8000e000

//#define  START_OF_DATA              129
UINT32 PTest_Percentage;


//unsigned char          *DataBuffer;
unsigned char          DataBuffer[1024];

unsigned char          *UpgradeBuffer;
unsigned char          FourBytes[4];
static   int           PacketSize;

static   int           buffer;
static   int           SavePlayBlock;
static   int           PlayMode;

static int   UpgradeM1553;
static int   UpgradeVideo;
static int   UpgradeController;
static int   UpgradeM2300;
static int   UpgradePCM;


#define READ32(address) *(volatile unsigned long *)(address)
#define WRITE32(address,data) *(volatile unsigned long *)(address) = data

unsigned long M23_ReadControllerCSR(int offset);
void M23_WriteControllerCSR(int offset,unsigned long data);

void M23_CS6651_Normal();
void M23_CS6651_03X();
void M23_CS6651_SlowX(int speed);

void M23_ResetSystem()
{
   M23_WriteControllerCSR(CONTROLLER_CONTROL_CSR,SYSTEM_RESET);
}

void M23_StartPlayback(int start_scan)
{
    int    state;
    int    end;
    int    record;
    int    debug;
    UINT32 CSR;

    M23_RecorderState(&state);

    M23_GetDebugValue(&debug);
    PlayMode = 0; //Normal

    //*(volatile unsigned long *)(0x80008090) = 1;

   /*Remove Live Video Clock and Data*/
   CSR = M23_ReadControllerCSR(CONTROLLER_PLAYBACK_CONTROL);
   CSR &= ~CONTROLLER_UPSTREAM_DATA;
   CSR &= ~CONTROLLER_UPSTREAM_CLOCK;
   M23_WriteControllerCSR(CONTROLLER_PLAYBACK_CONTROL,CSR);

    /*Clear in Record Bit*/
    CSR = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_STATUS);
    CSR |= IEEE1394B_PLAYBACK_STATUS;

    end = M23_GetEndBlock(start_scan);

    if(state == STATUS_RECORD)
    {
        M23_SetRecorderState(STATUS_REC_N_PLAY);
        M23_SetStartPlayScan(start_scan);

        /*Set in Record Bit*/
        CSR = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_STATUS);
        CSR |= PLAY_IN_RECORD;
        M23_WriteControllerCSR(IEEE1394B_PLAYBACK_STATUS,CSR);

#if 0
        /*Determine if this block is in the current recording*/
        record = M23_GetRecordPointer();
        if(record <= start_scan) //the playback in occuring in the recorded file
        {
            /*Set in Record Bit*/
            CSR = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_STATUS);
            CSR |= PLAY_IN_RECORD;
            M23_WriteControllerCSR(IEEE1394B_PLAYBACK_STATUS,CSR);
        }
        else //Not in the recorded file
        {
            //end = M23_GetEndBlock(start_scan);
            //end = start_scan +  (end - (end % IEEE1394B_MAX_BLOCKS)); 
            /*Write the end scan*/
if(debug)
    printf("End Scan = %d\r\n",end);
            M23_WriteControllerCSR(IEEE1394B_PLAYBACK_NUM_BLOCKS,end);
        }
#endif
    }
    else
    {
if(debug)
    printf("Setting Recorder State to PLAY\r\n");
        M23_SetRecorderState(STATUS_PLAY);

        end = M23_GetEndBlock(start_scan);
        //end = start_scan +  (end - (end % IEEE1394B_MAX_BLOCKS)); 
        M23_SetEndPlayScan(start_scan,end);
        /*Write the end scan*/
if(debug)
    printf("End Scan = %d\r\n",end);
        M23_WriteControllerCSR(IEEE1394B_PLAYBACK_NUM_BLOCKS,end);

        /*Clear in Record Bit*/
        CSR = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_STATUS);
        CSR &= ~PLAY_IN_RECORD;
        M23_WriteControllerCSR(IEEE1394B_PLAYBACK_STATUS,CSR);

    }

    SavePlayBlock = start_scan;

    /*Now Start the Playback*/
    DiskPlaybackSeek(start_scan);
    CSR = M23_ReadControllerCSR(CONTROLLER_PLAYBACK_CONTROL);
    CSR &= ~CONTROLLER_PLAYBACK_GEN;
    CSR &= ~CONTROLLER_UPSTREAM_DATA;
    M23_WriteControllerCSR(CONTROLLER_PLAYBACK_CONTROL,CSR | CONTROLLER_PLAYBACK_GO);

    /*Set 1394 Playback*/
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_CONTROL,PLAYBACK_GO);



}

void M23_StopPlayback(int mode)
{
    UINT32 CSR;
    int    state;
    int    debug;

    M23_GetDebugValue(&debug);

    M23_RecorderState(&state);

    /*Remove Playback Encoder Go*/
    CSR = M23_ReadControllerCSR(CONTROLLER_PLAYBACK_CONTROL);
    CSR &= ~CONTROLLER_PLAYBACK_GO;
    M23_WriteControllerCSR(CONTROLLER_PLAYBACK_CONTROL,CSR);

    /*Remove 1394B Playback Go*/
    CSR = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_CONTROL);
    CSR &= ~PLAYBACK_GO;
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_CONTROL,CSR);

    if(mode == 1)
    {
        /*Set the pointer back to the start of the last play*/
        DiskPlaybackSeek(SavePlayBlock);
    }

    if(state == STATUS_REC_N_PLAY)
    {
        M23_SetRecorderState(STATUS_RECORD);
    }
    else
    {
if(debug)
    printf(" StopPlayback Setting Recorder State to IDLE\r\n");
        M23_SetRecorderState(STATUS_IDLE);
    }

    M23_CS6651_Normal();


    //*(volatile unsigned long *)(0x80008090) = 0;

}

int  M23_GetMarkClk(int startvalue)
{
    int    done  = 0;
    int    count;
    int    save = 0;
    int    value;
    int    i;
    int    highvalue = 0x3fff;
    int    lowvalue = 0x1fff;
    UINT32 CSR = 0x80000000;
    UINT32 MarkClk;


    value = startvalue;
    for(i =0;i<26;i++)
    {
//printf("Setting DAC 0x%x\r\n",value);
        M23_SetDAC(0,value);
        M23_WriteControllerCSR(CONTROLLER_TIMECODE_MARK_CLOCK,CSR);

        count = 0;
        while(1) 
        {
            MarkClk = M23_ReadControllerCSR(CONTROLLER_TIMECODE_MARK_CLOCK);
            if(MarkClk & CSR) //Read is complete
            {
                if( (MarkClk & 0x1F) > 0xF)
                {
                    /*set the value higher*/
                    lowvalue = value;
                    value = value + ((highvalue - value)/2);
                }
                else if( (MarkClk & 0x1F) < 0x1)
                {
                    /*set the value lower*/
                    highvalue = value; //this is needed if MarkClk is too high
                    value = value - ((highvalue - lowvalue)/2);
                }
                else
                {
                    save = value;

                    if(startvalue == 0x3fff)
                    {
                        lowvalue = value;
                        value = value + ((highvalue - value)/2);
                    }
                    else
                    {
                        highvalue = value; //this is needed if MarkClk is too high
                        value = value - ((highvalue - lowvalue)/2);
                    }
                }
             
                break;
            }
            else
            {
                count++;
                if(count == 100)
                {
                    printf("Unable to complete Training\r\n");
                    done = 1;
                    break;
                }

                usleep(10);
            }
        }
    }

    return(save);

}

void M23_TrainTimeCode()
{
    int low;
    int high;
    int dac;
    int debug;

    high = M23_GetMarkClk(0x3fff);
    low  = M23_GetMarkClk(0x1fff);
   
//    printf("high Clk 0x%x,low clk 0x%x\r\n",high,low);

    dac = (high + low)/2;
    M23_SetDAC(0,dac);

    
    M23_GetDebugValue(&debug);
    if(debug)
        printf("Training done, DAC value = 0x%x\r\n",dac);
}

void M23_InitializeDataBuffer()
{

    UpgradeBuffer = (unsigned char*)malloc(2*(1024*1024));

    buffer = 0;
}


/***************************************************************************************************************************************************
 *  Name :    M23_StopRtest()
 *
 *  Purpose : This function will stop RTEST
 *
 *  Inputs :
 *  Functions Called:
 *
 *
 *
 ***************************************************************************************************************************************************/
int M23_StopRtest()
{
    int    loops = 0;
    UINT32 CSR;

    /*Remove Record BIT*/
    CSR = M23_ReadControllerCSR(IEEE1394B_RECORD);
    M23_WriteControllerCSR(IEEE1394B_RECORD,CSR & ~IEEE1394B_RECORD_GO);
 
    while(1)
    {
        CSR = M23_ReadControllerCSR(IEEE1394B_RECORD);

        if( (CSR & IEEE1394B_RECORD_BUSY) == 0 )
        {
            break;
        }
        else 
        {
            loops++;

            if(loops == 50)
            {
                break;
            }
            else
            {
                usleep(1000);
            }
        }
    }
    
    CSR = M23_ReadControllerCSR(IEEE1394B_RECORD_BLOCKS_RECORDED);

 
    UpdateBlocksWritten(CSR);

    sclose(0,1,0);

    return(0);
}

/***************************************************************************************************************************************************
 *  Name :    M23_PerformRtest()
 *
 *  Purpose : This function will setup the logic to perform RTEST
 *
 *  Inputs :
 *  Functions Called:
 *
 *
 *
 ***************************************************************************************************************************************************/
void M23_PerformRtest(int scans,int test,int mode)
{
    static int num_scans;
    UINT32     CSR;

    if(mode == 0)
    {
        if(scans > 0)
        {
            num_scans = scans;
        }
        else
        {
            num_scans = -1;
        }


        switch(test)
        {
            case 1:
                M23_WriteControllerCSR(IEEE1394B_RECORD_FIFO_MUX,RECORD_MUX_SELECT_0XBYTE);
                break;
            case 2:
                M23_WriteControllerCSR(IEEE1394B_RECORD_FIFO_MUX,RECORD_MUX_SELECT_0X32);
                break;
            case 3:
                M23_WriteControllerCSR(IEEE1394B_RECORD_FIFO_MUX,RECORD_MUX_SELECT_PSEUDO);
                break;
            case 4:
                M23_WriteControllerCSR(IEEE1394B_RECORD_FIFO_MUX,RECORD_MUX_SELECT_ZERO);
                break;
            case 5:
                M23_WriteControllerCSR(IEEE1394B_RECORD_FIFO_MUX,RECORD_MUX_SELECT_FIVES);
                break;
            case 6:
                M23_WriteControllerCSR(IEEE1394B_RECORD_FIFO_MUX,RECORD_MUX_SELECT_AS);
                break;
            default:
                M23_WriteControllerCSR(IEEE1394B_RECORD_FIFO_MUX,RECORD_MUX_SELECT_FS);
                break;
        }

        sopen("RTEST","W");

        CSR = M23_ReadControllerCSR(IEEE1394B_RECORD);
        M23_WriteControllerCSR(IEEE1394B_RECORD,CSR | IEEE1394B_RECORD_GO);
    }
    else if(mode == 1) //Continue with RTEST
    {
        if(num_scans != -1)
        {
            CSR = M23_ReadControllerCSR(IEEE1394B_RECORD_BLOCKS_RECORDED);
 
            num_scans -= CSR;

            UpdateBlocksWritten(CSR);
            if(num_scans < 1)
            {
                //Stop RTEST
                M23_StopRtest();
            }
        }
    }
    else
    {
        //Stop RTEST
        M23_StopRtest();
    }
}

void M23_WriteSecureFile()
{
    UINT32     CSR;

    M23_WriteControllerCSR(IEEE1394B_RECORD_FIFO_MUX,RECORD_MUX_SELECT_0XBYTE);
    sopen("Secure Erase","W");

    CSR = M23_ReadControllerCSR(IEEE1394B_RECORD);
    M23_WriteControllerCSR(IEEE1394B_RECORD,CSR | IEEE1394B_RECORD_GO);
}


/***************************************************************************************************
************************************************
 *  Name :    M23_ReadControllerMemorySpace()
 *
 *  Purpose : This function will print the memory space at the specified offset
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************
************************************************/

void M23_ReadControllerMemorySpace(int offset,int num_bytes)
{
    int           i;
    int           size = 0;
    int           count = 0;
    int           lines = 0;
    unsigned long tmpbuf[512];

    memset( tmpbuf, 0, sizeof(tmpbuf) );

    M23_WriteControllerCSR(0x80,(1<<31));
    //Do Read Here
    memcpy(tmpbuf,(unsigned long *)(CONTROLLER_CSR_OFFSET + offset),num_bytes);

    size = num_bytes/4;
    if( num_bytes %4)
    {
        size++;
    }

    printf("%03x - ",(lines *4) + offset);
    lines += 8;
    for(i=0;i<size;i++)
    {
        printf("%08x ", *(unsigned long*)&tmpbuf[i]);
        if( count == 7)
        {
            printf("\r\n%03x - ",(lines*4) + offset);
            lines += 8;
            count = 0;
        }
        else
            count++;
    }

    if(count < 7)
        printf("\r\n");

}
void M23_ReadMemorySpace(int offset,int num_bytes)
{
    int           i;
    int           size = 0;
    int           count = 0;
    int           lines = 0;
    unsigned long tmpbuf[512];

    memset( tmpbuf, 0, sizeof(tmpbuf) );

    //Do Read Here
    memcpy(tmpbuf,(unsigned long *)(offset | 0x80000000),num_bytes);

    size = num_bytes/4;
    if( num_bytes %4)
    {
        size++;
    }

    printf("%03x - ",(lines *4) + offset);
    lines += 8;
    for(i=0;i<size;i++)
    {
        printf("%08x ", *(unsigned long*)&tmpbuf[i]);
        if( count == 7)
        {
            printf("\r\n%03x - ",(lines*4) + offset);
            lines += 8;
            count = 0;
        }
        else
            count++;
    }

    if(count < 7)
        printf("\r\n");

}


/***************************************************************************************************************************************************
 *  Name :    M23_UpdateDPRBlock(char *buffer)
 *
 *  Purpose : This function will write a Block of Data to DPR
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/
 
void M23_UpdateDPRBlock(unsigned char *buffer)
{
    unsigned long *ptmp;
    unsigned long *dest_address = (unsigned long*)HOST_ORB_READ_ADDRESS;
    int           i;

    ptmp = (unsigned long *)buffer;
    for(i = 0; i < 128;i++)
    {
        WRITE32(dest_address,*ptmp);
        dest_address++;
        ptmp++;
    }


    
}
void M23_UpdateDPRBlockSwapped(unsigned char *buffer)
{
    unsigned long *ptmp;
    unsigned long *dest_address = (unsigned long*)HOST_ORB_READ_ADDRESS;
    int           i;

    ptmp = (unsigned long *)buffer;
    for(i = 0; i < 128;i++)
    {
        WRITE32(dest_address,SWAP_FOUR(*ptmp));
        dest_address++;
        ptmp++;
        usleep(1000);
    }

   M23_ReadMemorySpace(HOST_ORB_ADDRESS,512);

    
}

/***************************************************************************************************************************************************
 *  Name :    M23_ReadControllerCSR()
 *
 *  Purpose : This function will Read from a 16-bit CSR
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/
 
unsigned long M23_ReadControllerCSR(int offset)
{
    unsigned long CSRValue = 0xdeadbeef;

    /*Change to Use New Controller*/     
      //ControllerHandle.read_memory(&ControllerHandle,0,&CSRValue,offset,2);

    CSRValue = READ32(CONTROLLER_CSR_OFFSET + offset);

    //SWAP_FOUR(CSRValue);

    return (CSRValue);
}

/***************************************************************************************************************************************************
 *  Name :    M23_WriteControllerCSR()
 *
 *  Purpose : This function will write a 16-bit CSR
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/
 
void M23_WriteControllerCSR(int offset,unsigned long data)
{

    //SWAP_FOUR(data);
    WRITE32((CONTROLLER_CSR_OFFSET + offset),data);

}



/* -----------------------------------------------------------------------------------
   The following three functions are put here so that we can use the same buffer used
   in the recording of data
   ----------------------------------------------------------------------------------*/

/***************************************************************************************************************************************************
 *  Name :    M23_PerformPtest()
 *
 *  Purpose : This function will read and validate a known pattern to the cartridge
 *            --Adding this function here so that we can use a previously allocated buffer.
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/
int M23_PerformPtest(int start_scan,int test)
{
    int end;

    end = M23_GetEndBlock(start_scan);
    M23_SetEndPlayScan(start_scan,end);

    /*Set the Start Scan*/
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_POINTER_IN,start_scan);

    /*Set The type of Test*/
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_TEST,(test << 16));

   /*Set the Play GO bit*/
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_CONTROL,PLAYBACK_GO);

    return(0);
}
/***************************************************************************************************************************************************
 *  Name :    M23_StopPtest()
 *
 *  Purpose : This function will read and validate a known pattern to the cartridge
 *            --Adding this function here so that we can use a previously allocated buffer.
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/
int M23_StopPtest()
{
    UINT32 CSR;
    UINT32 ErrorLow;
    UINT32 ErrorHigh;

   /*Clear the Play GO bit*/
    CSR = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_CONTROL);
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_CONTROL,CSR & ~PLAYBACK_GO);

    /*Set The type of Test to CASCADE*/
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_TEST,0x0);

    /*Printout the Number of blocks for tested*/
    CSR = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_BLOCKS_OUT);

    /*Get The Error Bits*/
    ErrorLow = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_TEST_ERROR_BITS_LSW);
    ErrorHigh = M23_ReadControllerCSR(IEEE1394B_PLAYBACK_TEST_ERROR_BITS_MSW);

    printf("PTEST Complete - Blocks = %d, Errors 0x%x%08x\r\n",CSR,ErrorHigh,ErrorLow);


    return(0);
}
/***************************************************************************************************************************************************
 *  Name :    M23_ValidateTmats()
 *
 *  Purpose : This function will Validate the TMATS file from the cartridge 
 *
 *
 *  Returns : NONE
 *
 ***************************************************************************************************************************************************/
int ValidateTmats(int pktsize,int datasize,unsigned char *tmats)
{
    int    i;
    int    debug;
    int    size;
    UINT16 chksum;
    UINT16 calc = 0;
    UINT16 *ptmp16;
 
    M23_GetDebugValue(&debug);

    size = pktsize - 26;
    ptmp16 = (UINT16*)tmats;

    for(i = 0; i < size/2;i++)
    {
        calc += *ptmp16;
        ptmp16++;
    }

    chksum = *ptmp16;
    if(debug)
        printf("calc checksum = 0x%x,checksum read 0x%x\r\n",calc,chksum);
   
    if(chksum == calc)
    {
        return(0);
    }
    else
    {
        return(-1);
    }

}

/***************************************************************************************************************************************************
 *  Name :    M23_ReWriteTmatsFile()
 *
 *  Purpose : This function will write the TMATS file to block 129, Add a Time, Node and Root Packet
 *
 *
 *  Returns : NONE
 *
 ***************************************************************************************************************************************************/
int M23_ReWriteTmatsFile()
{
    int i;
    int size;
    int block = 129;

    size = PacketSize;
    if(PacketSize % M23_BLOCK_SIZE)
    {
        PacketSize += (M23_BLOCK_SIZE - (PacketSize%M23_BLOCK_SIZE) );
    }

    DiskWriteSeek(129);

    /*We Need to do 512 byte writes*/
    for(i = 0; i < (PacketSize/512);i++)
    {
        swrite( (UpgradeBuffer + (i*512)),512,1,0);
        block++;
        DiskWriteSeek(block);
    }

    M23_AddNewStanag(size,PacketSize/512);

    return 0;
}

/***************************************************************************************************************************************************
 *  Name :    M23_LoadTmatsFromRMM()
 *
 *  Purpose : This function will read the TMATS file from the cartridge.
 *
 *
 *  Returns : NONE
 *
 ***************************************************************************************************************************************************/
int M23_LoadTmatsFromRMM(int setup,UINT32 Address,char *name)
{
    int  return_status;
    int  readsize;
    int  bytes_read;
    int  i;
    int  size ;
    int  data ;

    int  debug;

    PACKETHEADER header;

    M23_GetDebugValue(&debug);

    /*Read the Header first to get the size of the TMATS data*/
    //DiskReadSeek(START_OF_DATA);
    DiskReadSeek(Address);


    /*We Need to Read The Header in Order to get the Size of the TMATS File*/
    bytes_read = DiskRead(UpgradeBuffer,512 );
    if(bytes_read == 0)
    {
        M23_SetHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
        //M23_SetFaultLED(0);
        return(0);
    }

    memcpy(&header,UpgradeBuffer,28);
    size = header.PacketLength;
    PacketSize = size;
    data = header.DataLength;

    if(debug)
        printf("Packet Size %d\r\n",PacketSize);

    DiskReadSeek(Address);

    if(size % 512)
    {
        readsize = size + (512 - (size%512) );
    }
    else
    {
        readsize = size;
    }

    memset(UpgradeBuffer,0x0,readsize);
 
    for(i = 0; i < (readsize/512);i++)
    {
        bytes_read = DiskRead((UpgradeBuffer + (i*512)),512 );
        if(bytes_read == 0)
        {
            M23_SetHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
            //M23_SetFaultLED(0);
            return(0);
        }
    }

    return_status = ValidateTmats(size,data,&UpgradeBuffer[24]);
    if(debug)
        printf("Validating Tmats File\r\n");

    if(return_status == 0)
    {
        memset(FourBytes,0x0,4);
        memcpy(FourBytes,&UpgradeBuffer[size-4],4);
        memset(&UpgradeBuffer[size-4],0x0,(2*(1024*1024)) - (size -4) );

        LoadedTmatsFromCartridge = 1;

        if(debug)
            printf("Loading Tmats File\r\n");

        LoadTmats(&UpgradeBuffer[28]);

        if(debug)
            printf("Setting Up Configuration %d\r\n",setup);

        return_status = SetupSet(setup,0);
        TmatsLoaded = 1;

#if 0
        if(debug)
            printf("Saving Configuration %d\r\n",setup);

        SaveSetup(setup);
#endif

        memcpy(&UpgradeBuffer[size-4],FourBytes,4);


#if 0
        if( return_status == NO_ERROR )
        {
            Setup_Save();
            M23_ClearHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
        }
        else
        {
            M23_SetHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
        }
#endif

        if(CH10_TMATS)
        {
            TmatsWrite(&UpgradeBuffer[28]);
            TmatsSave(setup);
        }

    }
    else
    {
        M23_SetHealthBit(RECORDER_FEATURE, M23_RECORDER_CONFIGURATION_FAIL);
        //M23_SetFaultLED(0);

    }

    if(debug)
        printf("Done Loading %d ..\r\n",setup);

    return(0);
}

/***************************************************************************************************************************************************
 *  Name :    M23_ValidateVersions(char *entries)
 *
 *  Purpose : This function will determine which boards will be upgraded.
 *
 *
 *  Returns : NONE
 *
 ***************************************************************************************************************************************************/
void M23_ValidateVersions(char *entries)
{
    int    i;
    int    NumBoards;
    int    month;
    int    day;
    int    year;

    UINT32 CSR;
    UINT32 CSR1;

    char   tmp[64];

    FILE   *fp;

    M23_NumberMPBoards(&NumBoards);

    if(entries[6] == 'M')
    {
        /*Versions we want to program*/
        strncpy(SystemVersion,&entries[1],5);
        strncpy(M2300Version,&entries[6],12);
        strncpy(ControllerVersion,&entries[18],17);
        strncpy(VideoVersion,&entries[35],12);
        strncpy(M1553Version,&entries[47],12);
        strncpy(PCMVersion,&entries[59],10);
    }
    else
    {
        /*Versions we want to program*/
        strncpy(SystemVersion,&entries[1],5);
        strncpy(M2300Version,&entries[7],12);
        strncpy(ControllerVersion,&entries[19],17);
        strncpy(VideoVersion,&entries[36],12);
        strncpy(M1553Version,&entries[48],12);
        strncpy(PCMVersion,&entries[60],10);
    }

#if 0
    memset(tmp1,0x0,32);
    strncpy(tmp1,&entries[6],12);
    if(strncmp(M2300Version,&entries[6],12) == 0)
    {
        UpgradeM2300 = 0;
    }
#endif


    /*Get the version that are currently Programmed*/
    memset(tmp,0x0,64);

    CSR = M23_ReadControllerCSR(CONTROLLER_COMPILE_TIME);

    year  = (CSR >> 8) & 0x000000FF;
    month = (CSR >> 24) & 0x000000FF;
    day   = (CSR >> 16) & 0x000000FF;

    sprintf(tmp,"Controller %02x%02x%02x",month,day,year);

printf("Current Version %s, New Version %s\r\n",tmp,ControllerVersion);
    if(strncmp(tmp,ControllerVersion,17) == 0)
    {
        UpgradeController = 0;
    }


    for(i = 0; i < NumBoards;i++)
    {
        memset(tmp,0x0,64);
        CSR = M23_mp_read_csr(i,BAR1,MP_COMPILE_TIME_OFFSET);

        CSR1 = M23_mp_read_csr(i,BAR1,MP_CSR_OFFSET);

        if(M23_MP_IS_PCM(i))
        {
            year  = (CSR >> 8) & 0x000000FF;
            month = (CSR >> 24) & 0x000000FF;
            day   = (CSR >> 16) & 0x000000FF;
            sprintf(tmp,"PCM %02x%02x%02x",month,day,year);
            printf("Current Version %s, New Version %s\r\n",tmp,PCMVersion);
            if(strncmp(tmp,PCMVersion,10) == 0)
            {
                UpgradePCM = 0;
            }
        }
        //else if(M23_MP_IS_M1553(i))
        if( (M23_MP_IS_M1553(i)) || ((CSR >>24) == 0x40) || (M23_MP_IS_DM(i)) )
        {
            year  = (CSR >> 8) & 0x000000FF;
            month = (CSR >> 24) & 0x000000FF;
            day   = (CSR >> 16) & 0x000000FF;

            sprintf(tmp,"M1553 %02x%02x%02x",month,day,year);
            printf("Current Version %s, New Version %s\r\n",tmp,M1553Version);
            if(strncmp(tmp,M1553Version,12) == 0)
            {
                UpgradeM1553 = 0;
            }
        }
        //else
        else if( (M23_MP_IS_Video(i)) || ((CSR1 >>24) == 0x30) )
        {
            year  = (CSR >> 8) & 0x000000FF;
            month = (CSR >> 24) & 0x000000FF;
            day   = (CSR >> 16) & 0x000000FF;
            sprintf(tmp,"Video %02x%02x%02x",month,day,year);

            printf("Current Version %s, New Version %s\r\n",tmp,VideoVersion);
            if(strncmp(tmp,VideoVersion,12) == 0)
            {
                UpgradeVideo = 0;
            }
        }

    }

    fp = fopen( "/tmp/SystemVersions", "wb+" );
    fwrite(entries, 1, 72, fp );

    if(fp)
        fclose(fp);

    system("sync");
    system("cp /tmp/SystemVersions /usr/calculex/SystemVersions");
    system("sync");
}

/***************************************************************************************************************************************************
 *  Name :    M23_UpgradeFromRMM()
 *
 *  Purpose : This function will read files from the Cartidge and use them to update the recorder 
 *            system. It will then Erase the Cartridge
 *
 *
 *  Returns : NONE
 *
 ***************************************************************************************************************************************************/
void M23_UpgradeFromRMM(int StartAddress)
{

    int              i;
    int              j;
    int              device;
    int              num_boards;
    int              num_blocks;
    int              offset = 0; 
    float            real_blocks;
    int              bytes_left;
    int              bytes_read;
    int              slots[6] = {0,0,0,0,0,0};
    UINT32           CSR;
    RECORDER_UPGRADE header;
    FILE             *fp;

    char             File[100];

    char             Proc_File[512];
    char             *Token;
    char             DevName[20];
    int              flashSize;
    int              EraseSize;
    char             Name[20];


    /*Turn on RECORD and FAULT LEDS Discretes*/
    M23_SetRecordLED();
    M23_SetFaultLED(0);

    UpgradeM1553 = 1;
    UpgradeVideo = 1;
    UpgradeController = 1;
    UpgradeM2300 = 1;
    UpgradePCM = 1;


    /*Read the Header first to get the size of the TMATS data*/
    bytes_read = DiskReadSeek(StartAddress);

    DiskRead(DataBuffer,512,1,0);

    if( (DataBuffer[0] == 'V') || (DataBuffer[0] == 'v') )
    {
        memset(File,0x0,81);
        memcpy(File,DataBuffer,81);
        M23_ValidateVersions(File);

        memcpy((unsigned char*)&header,&DataBuffer[81],sizeof(RECORDER_UPGRADE) );


        bytes_left = 512 - sizeof(RECORDER_UPGRADE) - 81;
        offset = 81;
    }
    else
    {
        memcpy((unsigned char*)&header,DataBuffer,sizeof(RECORDER_UPGRADE) );
        bytes_left = 512 - sizeof(RECORDER_UPGRADE);
    }


    printf("Number of Entries = %d\r\n",header.NumEntries);
    for(i =0;i< header.NumEntries;i++)
    {
        printf("Entry = %d -> type %d,Length %d\r\n",i,header.Files[i].Type,header.Files[i].Length);
    }

    /*find which boards occupy which slots*/
    M23_NumberMPBoards(&num_boards);
    for(i = 0;i<num_boards;i++)
    {
        CSR = M23_mp_read_csr(i,BAR1,MP_CSR_OFFSET);

        if( (M23_MP_IS_M1553(i)) || ((CSR >>24) == 0x40) || (M23_MP_IS_DM(i)) )
        {
            if(UpgradeM1553)
            {
                slots[i] = MP_M1553_DEVICE;
            }
        }
        else if( (M23_MP_IS_Video(i)) || ((CSR >>24) == 0x30) )
        {
            if(UpgradeVideo)
            {
                slots[i] = MP_VIDEO_DEVICE;
            }
        }
        else if(M23_MP_IS_PCM(i))
        {
            if(UpgradePCM)
            { 
                slots[i] = MP_PCM_DEVICE;
            }
        }
    }

    
    memset(UpgradeBuffer,0x0,2*(1024*0124));
    /*Copy the Remnant of the current Read to the upgrade buffer*/
    memcpy(UpgradeBuffer,(DataBuffer + sizeof(RECORDER_UPGRADE) + offset),bytes_left);

    for(i = 0; i < header.NumEntries;i++)
    {
        num_blocks = (header.Files[i].Length - bytes_left)/512;

        real_blocks = (float)(header.Files[i].Length - bytes_left)/512;
        if(real_blocks != 0)
        {
            num_blocks++;
        }

        for(j = 0; j < num_blocks;j++)
        {
           bytes_read = DiskRead(((UpgradeBuffer + bytes_left) + (j * 512) ), 512);
        }

        bytes_left = (num_blocks * 512) - (header.Files[i].Length - bytes_left);

        /*Now Copy the remnant for later*/
        //memset(DataBuffer,0x0,1024*1024);
        memset(DataBuffer,0x0,1024);
        memcpy(DataBuffer,(UpgradeBuffer + header.Files[i].Length),bytes_left);

        switch(header.Files[i].Type)
        {
            case M2300_UPGRADE:
                if(UpgradeM2300)
                {
                    printf("\r\nUpgrading M2300\r\n");
                    fp = fopen( "/tmp/m2300v2", "wb+" );
                    fwrite( UpgradeBuffer, 1, header.Files[i].Length, fp );

                    if(fp)
                        fclose(fp);

                    system("sync");
                    system("cp /tmp/m2300v2 /usr/calculex/m2300");
                    system("rm -f /tmp/m2300v2");
                    system("sync");
                    printf("\r\nDone Upgrading M2300\r\n");
                }
                break;
            case CONTROLLER_SLUSHWARE:
                if(UpgradeController)
                {
                    printf("\r\nUpgrading Controller\r\n");
                    memset(Proc_File,0x0,512);

                    fp = fopen( "/proc/mtd", "r" );
                    fread(Proc_File,1,512,fp);
                    fclose(fp);

                    Token = strtok(Proc_File,"\n");
                    while(Token != NULL)
                    {
                        /*We don't need to look at the first entry as it is a header*/
                        Token = strtok(NULL,"\n");
                        memset(DevName,0x0,20);
                        memset(Name,0x0,20);
                        sscanf(Token,"%s %x %x %s",DevName,&flashSize,&EraseSize,Name);
                        if( (strncmp(DevName,"mtd0",4) == 0) || (strncmp(DevName,"MTD0",4) == 0) )
                            break;
                    }


                    fp = fopen( "/tmp/control.rbf", "wb+" );
                    fwrite( UpgradeBuffer, 1, header.Files[i].Length, fp );
                    fclose(fp);
                    system("sync");

                    if(EraseSize == 0x20000)
                    {
                        system("flash_erase /dev/mtd0 0 12");
                    }
                    else
                    {
                        system("flash_erase /dev/mtd0 0 24");
                    }

                    system("cp /tmp/control.rbf /dev/mtd0");
                    system("sync");
                    printf("\r\nDone Upgrading Controller\r\n");
                }

                break;
            case VIDEO_SLUSHWARE:

                for(device = 0;device<num_boards;device++)
                {
                    if(slots[device] == MP_VIDEO_DEVICE)
                    {
                        printf("\r\nUpgrading Video\r\n");
                        M23_MP_ProgramFlash(device, START_PROGRAM_SECTOR_ADDRESS,header.Files[i].Length,UpgradeBuffer);
                        printf("\r\nDone Upgrading Video\r\n");
                    }
                }

                break;

            case M1553_SLUSHWARE:

                for(device = 0;device<num_boards;device++)
                {
                    if(slots[device] == MP_M1553_DEVICE)
                    {
                        printf("\r\nUpgrading M1553\r\n");
                        M23_MP_ProgramFlash(device, START_PROGRAM_SECTOR_ADDRESS,header.Files[i].Length,UpgradeBuffer);
                        printf("\r\nDone Upgrading M1553\r\n");
                    }
                }
                break;

            case PCM_SLUSHWARE:

                for(device = 0;device<num_boards;device++)
                {
                    if(slots[device] == MP_PCM_DEVICE)
                    {
                         M23_MP_ProgramFlash(device, START_PROGRAM_SECTOR_ADDRESS,header.Files[i].Length,UpgradeBuffer);
                    }
                }
                break;

            case ETHERNET_KERNEL:
                fp = fopen( "/tmp/kernel.bin", "wb+" );
                if(fp){
                    fwrite( UpgradeBuffer, 1, header.Files[i].Length, fp );

                    fclose(fp);

                    system("flash_erase /dev/mtd3 0 100");
                    system("cp /tmp/kernel.bin /dev/mtd3");
                    system("rm -f /tmp/kernel.bin");
                    system("sync");
                    system("sync");
                    printf("\r\nDone Upgrading Ethernet Driver\r\n");
                }else
                    printf("Cannot Open Ethernet Kernel File\r\n");
                break;
            default:
                break;
 
        }

        /*Copy the Remnant of the current Read to the upgrade buffer*/
        memset(UpgradeBuffer,0x0,2*(1024*0124));
        memcpy(UpgradeBuffer,DataBuffer,bytes_left);
    }

    /*Turn off ALL Discretes*/
    M23_ClearFaultLED(0);
    M23_ClearRecordLED();
    //M23_ResetSystem();

}


/************************************************************************************************
 *     This section are the TRD specific commands, either throught the COM or M1553
 ***********************************************************************************************/



/*******************************************************************************************************
*  Name :    M23_PerformAssign()
*
*  Purpose : This function will Perform the Assign command
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
void M23_PerformAssign(int OutputChannel,int InputChannel)
{
    M23_WriteControllerCSR(CONTROLLER_PLAYBACK_ID,InputChannel);
}

void M23_WriteC6651(int offset,int value)
{
    int *CSR;

    CSR = (int*)(CS6651_BASE + (offset*4));

    //printf("Writing value 0x%x to address 0x%x,0x%x\r\n",value, CS6651_BASE + offset,CSR);
    *CSR = value;

}

int M23_ReadC6651(int offset)
{
    int *CSR;

    CSR = (int*)(CS6651_BASE + (offset*4));

    return(*CSR);
}


/*******************************************************************************************************
*  Name :    M23_InitializeCS6651()
*
*  Purpose : This function will Perform the Assign command
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
void M23_InitializeCS6651()
{
    /*Open the character device so that we can set the modes*/
    CS6651_device = open(CS6651_DEVICE,O_RDWR);

    /*Initialize the 1us clock divisor*/
    M23_WriteC6651(CS6651_DIVISOR,CS6651_1US_DIVISOR);

    M23_WriteC6651(CS6651_DISPLAY_SUBS_CONFIG,0x0);

    /*Initialize the Display size fo 720x480*/
    M23_WriteC6651(CS6651_DISPLAY_ROW_REG,CS6651_DISPLAY_ROW);
    M23_WriteC6651(CS6651_DISPLAY_COL_REG,CS6651_DISPLAY_COLUMN);

}

/*******************************************************************************************************
*  Name :    M23_CS6651_Pause()
*
*  Purpose : This function will Perform the Assign command
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
void M23_CS6651_Pause(int mode)
{
    int rv;

    if(mode == 0)  //Start Pause
    {
        //M23_InitializeCS6651();
        rv = ioctl( CS6651_device, CS6651_PAUSE, 0 );
    }
    else
    {

        if(PlayMode == 1)
        {
            M23_CS6651_SlowX(2);
        }
        else if(PlayMode == 2)
        {
            M23_CS6651_SlowX(30);
        }
        else if(PlayMode == 3)
        {
            // PC Remove 060507 M23_CS6651_Normal();
            M23_CS6651_03X();
        }
        else
        {
            M23_CS6651_Normal();
        }
        //usleep(32000);
  
        //clear and exit
        //*(volatile unsigned long *)(0x80008090) = 0;
        //*(volatile unsigned long *)(0x80008090) = 1;

    }
}
/*******************************************************************************************************
*  Name :    M23_CS6651_Single()
*
*  Purpose : This function will Playback Data at 3x speed
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
void M23_CS6651_Single()
{
    int rv;

    //M23_InitializeCS6651();
    rv = ioctl( CS6651_device, CS6651_SINGLE_STEP, 0 );
}

/*******************************************************************************************************
*  Name :    M23_CS6651_03X()
*
*  Purpose : This function will Playback Data at 3x speed
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
void M23_CS6651_03X()
{
    int rv;

    //M23_InitializeCS6651();
    //rv = ioctl( CS6651_device, CS6651_3X, 0 );
    rv = ioctl( CS6651_device, CS6651_2X, 0 );
    PlayMode = 3;
}

/*******************************************************************************************************
*  Name :    M23_CS6651_02X()
*
*  Purpose : This function will Playback Data at 2x speed
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
void M23_CS6651_02X()
{
    int rv;

    //M23_InitializeCS6651();
    rv = ioctl( CS6651_device, CS6651_2X, 0 );
    PlayMode = 3;
}

/*******************************************************************************************************
*  Name :    M23_CS6651_Normal()
*
*  Purpose : This function will Playback Data at 3x speed
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
void M23_CS6651_Normal()
{
    int rv;

    //M23_InitializeCS6651();
    rv = ioctl( CS6651_device, CS6651_NORMAL, 0 );

    usleep(32000);

    //clear and exit
    *(volatile unsigned long *)(0x80008090) = 0;
    *(volatile unsigned long *)(0x80008090) = 1;

}

/*******************************************************************************************************
*  Name :    M23_CS6651_SlowX()
*
*  Purpose : This function will Playback Data at 1/2 or 1/30 speed
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
void M23_CS6651_SlowX(int speed)
{
    int rv;
    int debug;

    M23_GetDebugValue(&debug);

    if(debug)
        printf("Changing Speed to %d\r\n",speed);

    //M23_InitializeCS6651();
    if(speed == 2)
    {
        rv = ioctl( CS6651_device, CS6651_05X, 0 );
        PlayMode = 1;
    }
    else
    {
        rv = ioctl( CS6651_device, CS6651_130X, 0 );
        PlayMode = 2;
    }
}

void GetMACAddress(UINT16 *High,UINT16 *Mid,UINT16 *Low)
{
    FILE *fd;
    int high1,high2;
    int mid1,mid2;
    int low1,low2;
    int debug;
    char macaddr[20];
    char newmac[20];


    *High = 0x1400;
    *Mid  = 0x2352;
    *Low  = 0x0000;


    M23_GetDebugValue(&debug);

    fd = fopen( "macaddress.txt", "r" );
    if( fd == NULL) 
    {
        return;
    }
    else
    {
        if(debug)
            printf("Found macaddress.txt\r\n");
    }

    

    fread(macaddr,17,1,fd);

    sscanf(macaddr,"%x:%x:%x:%x:%x:%x",&high1,&high2,&mid1,&mid2,&low1,&low2);
    memset(newmac,0x0,20);
    sprintf(newmac,"%02x%02x-%02x%02x-%02x%02x",high2,high1,mid2,mid1,low2,low1);

    sscanf(newmac,"%04x-%04x-%04x",&high1,&mid1,&low1);


    *High = (UINT16)high1;
    *Mid = (UINT16)mid1;
    *Low = (UINT16)low1;

}


/*******************************************************************************************************
*  Name :    M23_ConfigureRadarENET()
*
*  Purpose : This function will Setup the Radar Ethernet interface
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
void M23_ConfigureRadarENET(int ourIP,int RadarIP, int port,char mode)
{
    WRITE32((ETHERNET_DRAM_OFFSET + 0x4),ourIP);
    WRITE32((ETHERNET_DRAM_OFFSET + 0x8),RadarIP);
    WRITE32((ETHERNET_DRAM_OFFSET + 0xC),port);
    WriteMem8((ETHERNET_DRAM_OFFSET + 0x18),mode);
    WRITE32((ETHERNET_DRAM_OFFSET + 0x0),0xDEADBEEF);
}

/*******************************************************************************************************
*  Name :    M23_RadarENETStatus()
*
*  Purpose : This function will Setup the Radar Ethernet interface
*
*
*  Returns : SUCCESS   Status Returned
*            FAIL      Status unavailable
*
 ******************************************************************************************************/
void M23_RadarENETStatus(int *status,int *data )
{
    int stat;
    int data_received;

    stat = READ32(ETHERNET_DRAM_OFFSET + 0x10);
    data_received = READ32(ETHERNET_DRAM_OFFSET + 0x14);
    //data_received = READ32(ETHERNET_DRAM_OFFSET + 0x18);


   *status = stat;
   *data   = data_received;
}

