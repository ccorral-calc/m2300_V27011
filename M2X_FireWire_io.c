///////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 1998, CALCULEX, Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//     File: FireWire_io.c
//     Version: 1.0
//     Author: Paul / adnan abbas
//
//      Performs Send Command and Receive Commands to the processor through firewire.
//
//  Revisions:   
//
//


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
#include <time.h>
#include "M23_Controller.h"
#include "M2X_FireWire_io.h"
#include "M2X_time.h"

int firewireDevice;                           /* SCSI device/file descriptor */

#define SEND_CMD    0xa
#define RECEIVE_CMD 0x8
#define DEV_LUN     0x0 //if we set it to 0x1 CDB byte, 2 bits 5-7 hold LUN 

#define INQ_REPLY_LEN         96
#define INQ_CMD_LEN            6
#define TUR_CMD_LEN            6
#define READ_CAP_REPLY_LEN     8
#define SENSE_BUFF_LEN        32

#define SCSI_IOCTL_SEND_COMMAND 1

#define DEF_TIMEOUT 1000       /* 1,000 millisecs == 1 seconds */

#define BUFFER_RECEIVE_SIZE     512 
#define BUFFER_RECEIVE_CMD_SIZE 80 

#define DEBUF_OPT  0

int M2x_RetrieveCartridgeTime(char *Time);
int M2x_RetrieveCartridgeDate(char *Date);

int  CartState;
int  CartPercent;
int  DeclassErrors;

static int RMMJammed = 0;

#define READ32(address) *(volatile unsigned long *)(address)


int Is_RMM_Jammed()
{
    return(RMMJammed);
}

/***************************************************************************
                      M2x_ReadCapacity

 Parameters
 ----------
    None

 Return Value
 ------------
    None

 Remarks
 -------
  Creates the Read Capacity CDB and sends it across 1394B using Controller Logic call



***************************************************************************/
int M2x_ReadCapacity()
{
    int           i;
    int           num_sect = 0;
    int           sect_size = 512;
    int           cdb[3];
    int           loops;
    int           debug;

    UINT32        CSR;
    UINT32        CSR_WRITE;
    UINT32        Status;


    M23_GetDebugValue(&debug);

    cdb[0] = (0x25 << 24);
    cdb[1] = 0x0;
    cdb[2] = 0x0;

    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_0,cdb[0]);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_1,cdb[1]);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_2,cdb[2]);

    /*Write the size*/
    M23_WriteControllerCSR(IEEE1394B_HOST_ORB_SIZE,0xa);

    /*Write the address*/
//    M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,rcBuff);
    M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,HOST_ORB_ADDRESS);


    /*Set the direction for Host to Target, and clear  go*/
    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR | IEEE1394B_HOST_GO | IEEE1394B_HOST_TO_TARGET);

   usleep(10000);

    /*Now Wait for Host busy to clear*/
    loops = 0;
    while(1)
    {
        CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);

        if(CSR & IEEE1394B_HOST_BUSY)
        {
            usleep(1000);
            loops++;
            if(loops == 900)
            {
                break;
            }
            else
            {
                usleep(1000);
            }
        }
        else
        {
            Status = M23_ReadControllerCSR(IEEE1394B_HOST_STATUS);
            if(Status == 0x4100)
            {
                num_sect  = READ32(HOST_ORB_READ_ADDRESS);
                num_sect  = 1 + CONTROL_SWAP_FOUR(num_sect);
                sect_size = READ32(HOST_ORB_READ_ADDRESS + 4);
                sect_size = CONTROL_SWAP_FOUR(sect_size);
            }
            else
            {
                if(debug)
                    printf("Read Capacity - Bad Status 0x%x\r\n",Status);
            }

            break;
        }
    }

    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    CSR &= ~IEEE1394B_HOST_GO;
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR);

    return (num_sect);

}

/***************************************************************************
                      M2x_SendTestUnitReady

 Parameters
 ----------
    None

 Return Value
 ------------
    0 = PASS , 1 = FAIL

 Remarks
 -------
  Creates the Test Unit Ready CDB and sends it across Firewire



***************************************************************************/
int M2x_TestUnitReay()
{ 
    int           i;
    int           cdb[3];
    int           loops;
    int           status = 0;
    int           debug;

    UINT32        CSR;
    UINT32        CSR_WRITE;

    M23_GetDebugValue(&debug);

    cdb[0] = (0x0 << 24);
    cdb[1] = 0x0;
    cdb[2] = 0x0;

    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_0,cdb[0]);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_1,cdb[1]);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_2,cdb[2]);

    /*Write the size*/
    M23_WriteControllerCSR(IEEE1394B_HOST_ORB_SIZE,0x6);

    /*Write the address*/
//    M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,rcBuff);
    M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,HOST_ORB_ADDRESS);


    /*Set the direction for Host to Target, and clear  go*/
    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR | IEEE1394B_HOST_GO | IEEE1394B_HOST_TO_TARGET);

   usleep(10000);

    /*Now Wait for Host busy to clear*/
    loops = 0;
    while(1)
    {
        CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);

        if(CSR & IEEE1394B_HOST_BUSY)
        {
            usleep(1000);
            loops++;
            if(loops == 900)
            {
                break;
            }
            else
            {
                usleep(1000);
            }
        }
        else
        {
            break;
        }
    }


    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    CSR &= ~IEEE1394B_HOST_GO;
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR);


    return (status);

}


/***************************************************************************
                      M2x_SendFirewireCommand

 Parameters
 ----------
    buffer of data to send across, and size of buffer

 Return Value
 ------------
    0 = PASS , 1 = FAIL

 Remarks
 -------
  Creates the SEND_CMD CDB and sends it across Fire wire using ioctl() call



***************************************************************************/
int M2x_SendFirewireCommand(unsigned char *buff, int size)
{

    int    cdb[3];
    int    loops;
    int    i;
    int    debug;
    int    status = -1;

    UINT32 CSR;
    UINT32 CSR_WRITE;
    UINT32 Status;

    M23_GetDebugValue(&debug);

    usleep(10000);

    /*Write the CDB to the Logic*/
    cdb[0] = (SEND_CMD << 24);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_0,cdb[0]);

    cdb[1] = (size  << 24);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_1,cdb[1]);

    cdb[2] = 0; 
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_2,cdb[2]);

    /*Write the size*/
    M23_WriteControllerCSR(IEEE1394B_HOST_ORB_SIZE,size);

    M23_UpdateDPRBlock(buff);

    /*Get the data*/
    M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,HOST_ORB_ADDRESS);

    /*Set the direction for Host to Target, and clear  go*/
    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    CSR &= ~IEEE1394B_HOST_TO_TARGET;
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR | IEEE1394B_HOST_GO);


    /*Now Wait for Host busy to clear*/
    loops = 0;
    while(1)
    {
        CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);

        if(CSR & IEEE1394B_HOST_BUSY)
        {
            loops++;
            if(loops == 900)
            {
                break;
            }
            else
            {
                usleep(1000);
            }
        }
        else
        {
            Status = M23_ReadControllerCSR(IEEE1394B_HOST_STATUS);
            if(Status == 0x4100)
            {
                status = 0;
            }
            else
            {
                if(debug)
                    printf("Send Firewire Command - Bad Status 0x%x\r\n",Status);
            }

            break;
        }
    }

    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    CSR &= ~IEEE1394B_HOST_GO;
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR);

    return(status); 

}

/***************************************************************************
                      M2x_RetrieveResponse

 Parameters
 ----------
    buffer to store read info into and size of buffer

 Return Value
 ------------
    0 = PASS , 1 = FAIL

 Remarks
 -------
  Creates the RECIEVE_CMD CDB and sends it across Fire wire using ioctl() call



***************************************************************************/
int M2x_RetrieveResponse(unsigned char *buff, int size)
{
    int i;
    int cdb[3];
    int loops;
    int debug;
    int status = -1;

    unsigned long resp[20];

    UINT32 CSR;
    UINT32 CSR_WRITE;
    UINT32 Status;

    M23_GetDebugValue(&debug);

    usleep(10000);

    /*Write the CDB to the Logic*/
    cdb[0] = (RECEIVE_CMD << 24);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_0,cdb[0]);
    cdb[1] = (size << 24);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_1,cdb[1]);
    cdb[2] =  0; 
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_2,cdb[2]);

    /*Write the size*/
    M23_WriteControllerCSR(IEEE1394B_HOST_ORB_SIZE,size);

    /*Get the data*/
    M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,HOST_ORB_ADDRESS);

    /*Set the direction for Host to Target, and clear  go*/
    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR | IEEE1394B_HOST_GO | IEEE1394B_HOST_TO_TARGET);


    /*Now Wait for Host busy to clear*/
    loops = 0;
    while(1)
    {
        CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);

        if(CSR & IEEE1394B_HOST_BUSY)
        {
            loops++;
            if(loops == 900)
            {
                break;
            }
            else
            {
                usleep(1000);
            }
        }
        else
        {
            Status = M23_ReadControllerCSR(IEEE1394B_HOST_STATUS);
            if(Status == 0x4100)
            {
                for(i = 0; i < size/4;i++)
                {
                    resp[i] = READ32((HOST_ORB_READ_ADDRESS + (i*4)) );
                }
                status = 0;
                memcpy(buff,(unsigned char*)resp,size);
            }
            else
            {
                if(debug)
                    printf("Retrieve Response - Bad Status 0x%x\r\n",Status);
            }
            break;
        }
    }

    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    CSR &= ~IEEE1394B_HOST_GO;
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR);

    return(status);
}

/***************************************************************************
                      M2x_GetCartridgeTime

 Parameters
 ----------
    char pointer to return the read time in 

 Return Value
 ------------
    0 = PASS, 1 = FAIL

 Remarks
 -------
  calls M2x_RetrieveCartridgeDate and M2x_RetrieveCartridgeTime command 
  and use them both to calculate complete time string with Day of yesr in it

***************************************************************************/
int M2x_GetCartridgeTime(char *Time)
{
    UINT32 CSR;
    char   time_part[48];

    int    status;
    int    debug;


    M23_GetDebugValue(&debug);

    memset(time_part,0,48);

    status = M2x_RetrieveCartridgeTime(&time_part[0]);
    
    if(status == 0)
    {

        Time[0] =  time_part[5];	//3
        Time[1] =  time_part[6];	//6
        Time[2] =  time_part[7];	//6

        Time[3] =  ' ';			// 
        Time[4] =  time_part[9];		//2
        Time[5] =  time_part[10];		//3
        Time[6] =  time_part[11];		//:
        Time[7] =  time_part[12];		//5
        Time[8] =  time_part[13];		//9
        Time[9] =  time_part[14];		//:
        Time[10] = time_part[15];		//5
        Time[11] = time_part[16];		//9
        Time[12] = time_part[17];		//.
        Time[13] = time_part[18];		//9
        Time[14] = time_part[19];		//9
        Time[15] = time_part[20];		//9
        Time[16] = time_part[21];		//?
        Time[17] = time_part[22];		//?
        Time[18] = time_part[23];

        CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_HOST_REL_LOW);
        RefTime = CSR;
        CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_HOST_REL_HIGH);
        RefTime |= (((INT64) CSR) <<32);

        if(debug)
            printf("Time Received from Cartridge %s\r\n",Time);


        RMMJammed = 1;
    }

    return(status);
}


/***************************************************************************
                      M2x_SetCartridgeTime

 Parameters
 ----------
    char string that has the time value to be set

 Return Value
 ------------
    None

 Remarks
 -------
  calls M2x_SendFirewireCommand and M2x_RetrieveResponse command with .TIME XXX command



***************************************************************************/
int M2x_SetCartridgeTime(char *Time)
{
    /* .Time Command*/
    int           status;
    unsigned char timeBuffer [BUFFER_RECEIVE_SIZE];

    memset(timeBuffer,0x0,BUFFER_RECEIVE_SIZE);
  
    sprintf(&timeBuffer[0],".TIME %s\r\n",Time);


    status  = M2x_SendFirewireCommand(timeBuffer,strlen(timeBuffer));

    if(status == 0)
    {
        memset(timeBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(timeBuffer,BUFFER_RECEIVE_CMD_SIZE -1);

    }

    return(status);
}

/***************************************************************************
                      M2x_RetrieveCartridgeTime

 Parameters
 ----------
    None

 Return Value
 ------------
    None

 Remarks
 -------
  calls M2x_SendFirewireCommand and M2x_RetrieveResponse command with .TIME command



***************************************************************************/
int M2x_RetrieveCartridgeTime(char *Time)
{
    int status;

    /* .Time Command*/
    unsigned char timeBuffer [BUFFER_RECEIVE_SIZE];

    memset(timeBuffer,0x0,BUFFER_RECEIVE_SIZE);
  
    strncpy(&timeBuffer[0],".TIME\r\n",7);

    status = M2x_SendFirewireCommand(timeBuffer,7);

    if(status == 0)
    {
        memset(timeBuffer,0x0,BUFFER_RECEIVE_SIZE);

        M23_WriteControllerCSR(0x124,0x1);

        status = M2x_RetrieveResponse(timeBuffer,BUFFER_RECEIVE_CMD_SIZE -1);

        if(status == 0)
        {
            strcpy(Time,timeBuffer);
        }
    }

    return(status);
}

/***************************************************************************
                      M2x_EraseCartridge

 Parameters
 ----------
    None

 Return Value
 ------------
    None

 Remarks
 -------
  calls M2x_SendFirewireCommand and M2x_RetrieveResponse command with .TIME command



***************************************************************************/
int M2x_EraseCartridge(int mode)
{
    int status;

    /* .ERASE Command*/
    unsigned char eraseBuffer [BUFFER_RECEIVE_SIZE];

    memset(eraseBuffer,0x0,BUFFER_RECEIVE_SIZE);
  
    if(mode)
    {
        strncpy(eraseBuffer,".ERASE 2\r\n",10);
    }
    else
    {
        strncpy(eraseBuffer,".ERASE 0\r\n",10);
    }

    status = M2x_SendFirewireCommand(eraseBuffer,10);

    if(status == 0)
    {
        memset(eraseBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(eraseBuffer,BUFFER_RECEIVE_CMD_SIZE - 1);

    }

    return(status);

}
/***************************************************************************
                      M2x_FlushCartridge

 Parameters
 ----------
    None

 Return Value
 ------------
    None

 Remarks
 -------
  calls M2x_SendFirewireCommand and M2x_RetrieveResponse command with .TIME command



***************************************************************************/
int M2x_FlushCartridge()
{
    int status;
    int debug;

    /* .ERASE Command*/
    unsigned char flushBuffer [BUFFER_RECEIVE_SIZE];

    memset(flushBuffer,0x0,BUFFER_RECEIVE_SIZE);
  
    strncpy(flushBuffer,".FLUSH\r\n",8);
    
    status = M2x_SendFirewireCommand(flushBuffer,8);

    if(status == 0)
    {
        memset(flushBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(flushBuffer,BUFFER_RECEIVE_CMD_SIZE - 1);
    }
    else
    {
        M23_GetDebugValue(&debug);
        if(debug)
            printf("Flush Failed\r\n");
    }

    return(status);
}


/***************************************************************************
                      M23_Declassify

 Parameters
 ----------
    None

 Return Value
 ------------
    None

 Remarks
 -------
  calls M2x_SendFirewireCommand and M2x_RetrieveResponse command with .Declassify command

***************************************************************************/
int M23_Declassify()
{
    int status;

    /* .Declass Command*/
    unsigned char declassBuffer [BUFFER_RECEIVE_SIZE];

    memset(declassBuffer,0x0,BUFFER_RECEIVE_SIZE);

    strncpy(declassBuffer,".DECLASSIFY\r\n",13);

    status = M2x_SendFirewireCommand(declassBuffer,13);

    if(status == 0)
    {
        memset(declassBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(declassBuffer,BUFFER_RECEIVE_CMD_SIZE -1);
    }

    return(status);
}



/***************************************************************************
                      M2x_SendBITCommand

 Parameters
 ----------
    None

 Return Value
 ------------
    None

 Remarks
 -------
  calls M2x_SendFirewireCommand and M2x_RetrieveResponse command with .BIT command



***************************************************************************/
int M2x_SendBITCommand()
{
    /* .BIT Command*/
    int           status;
    unsigned char BIT_Buffer [BUFFER_RECEIVE_SIZE];

    memset(BIT_Buffer,0x0,BUFFER_RECEIVE_SIZE);
    strncpy(&BIT_Buffer[0],".BIT\r\n",6);

    
    status = M2x_SendFirewireCommand(BIT_Buffer,strlen(BIT_Buffer));

    if(status == 0)
    {
        memset(BIT_Buffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(BIT_Buffer,BUFFER_RECEIVE_CMD_SIZE -1);
    }

    return(status);
}


/***************************************************************************
                      M2x_SendHealthCommand

 Parameters
 ----------
    None

 Return Value
 ------------
    None

 Remarks
 -------
  calls M2x_SendFirewireCommand and M2x_RetrieveResponse command with .HEALTH command



***************************************************************************/
int M2x_SendHealthCommand()
{
    /* .Health Command*/
    unsigned char healthBuffer [BUFFER_RECEIVE_SIZE];
    int status;

    memset(healthBuffer,0x0,BUFFER_RECEIVE_SIZE);
  
    strncpy(&healthBuffer[0],".HEALTH\r\n",9);

    status = M2x_SendFirewireCommand(healthBuffer,strlen(healthBuffer));
  
    if(status == 0)
    {
        memset(healthBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(healthBuffer,BUFFER_RECEIVE_CMD_SIZE -1);
    }

    return(status);

}

/***************************************************************************
                      M2x_SendCriticalCommand

 Parameters
 ----------
    None

 Return Value
 ------------
    None

 Remarks
 -------
  calls M2x_SendFirewireCommand and M2x_RetrieveResponse command with .CRITICAL command



***************************************************************************/
int M2x_SendCriticalCommand()
{
    /* .Critical Command*/
    unsigned char criticalBuffer [BUFFER_RECEIVE_SIZE];
    int status;

    memset(criticalBuffer,0x0,BUFFER_RECEIVE_SIZE);
  
    strncpy(&criticalBuffer[0],".CRITICAL\r\n",11);

    status = M2x_SendFirewireCommand(criticalBuffer,strlen(criticalBuffer));
  
    if(status == 0)
    {
        memset(criticalBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(criticalBuffer,BUFFER_RECEIVE_CMD_SIZE -1);
    }

    return(status);
}

/***************************************************************************
                      M2x_SendStatusCommand

 Parameters
 ----------
    None

 Return Value
 ------------
    None

 Remarks
 -------
  calls M2x_SendFirewireCommand and M2x_RetrieveResponse command with .STATUS command



***************************************************************************/

int M2x_SendStatusCommand()
{
    int status;
    /* .Status Command*/
    unsigned char statusBuffer [BUFFER_RECEIVE_SIZE];
    char          StatResponse[80];
    char          pct[3];

    memset(statusBuffer,0x0,BUFFER_RECEIVE_SIZE);

    strncpy(&statusBuffer[0],".STATUS\r\n",9);


    status = M2x_SendFirewireCommand(statusBuffer,9);

    if(status == 0)
    {
        memset(statusBuffer,0x0,BUFFER_RECEIVE_SIZE);

        status = M2x_RetrieveResponse(statusBuffer,BUFFER_RECEIVE_CMD_SIZE -1);
        if(status == 0)
        {
            memset(StatResponse,0x0,80);
            strncpy(StatResponse,statusBuffer,20);

            memset(pct,0x0,3);
            sprintf(pct,"%c%c",statusBuffer[2],statusBuffer[3]);
            CartState = atoi(pct);
            if(CartState != 1)
            {                  
                memset(pct,0x0,3);
                sprintf(pct,"%c%c",statusBuffer[11],statusBuffer[12]);
                CartPercent = atoi(pct);
                //PC Remove , not working at this time CartPercent = CartPercent/2;
            }

            memset(pct,0x0,3);
            sprintf(pct,"%c%c",statusBuffer[8],statusBuffer[9]);
            DeclassErrors = atoi(pct);
        }
    }

    return(status);

}


/***************************************************************************
                      M2x_SetCartridgeDate

 Parameters
 ----------
    Pointer to char string that has Date

 Return Value
 ------------
    None

 Remarks
 -------
  calls M2x_SendFirewireCommand and M2x_RetrieveResponse command with .DATE command



***************************************************************************/
int M2x_SetCartridgeDate(char *Date)
{
    /* .Date Command*/
    int           status;
    unsigned char dateBuffer [BUFFER_RECEIVE_SIZE];

    memset(dateBuffer,0x0,BUFFER_RECEIVE_SIZE);
  
    sprintf(&dateBuffer[0],".Date %s\r\n",Date);

    status = M2x_SendFirewireCommand(dateBuffer,strlen(dateBuffer));

    if(status == 0)
    {
        memset(dateBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(dateBuffer,BUFFER_RECEIVE_CMD_SIZE -1);
    }

    return(status);
}

/***************************************************************************
                       M2x_RetrieveCartridgeDate

 Parameters
 ----------
    Pointer to char string to load Date to

 Return Value
 ------------
    None

 Remarks
 -------
  Sends .DATE command to processor over firewire
  Recieve's response



***************************************************************************/
int M2x_RetrieveCartridgeDate(char *Date)
{
    /*Date Command*/
    int           status;
    unsigned char dateBuffer [BUFFER_RECEIVE_SIZE];

    memset(dateBuffer,0x0,BUFFER_RECEIVE_SIZE);
    strncpy(&dateBuffer[0],".DATE\r\n",7);

    status = M2x_SendFirewireCommand(dateBuffer,strlen(dateBuffer));

    if(status == 0)
    {
        memset(dateBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(dateBuffer,79);

        if(status == 0)
        {
            strcpy( Date , dateBuffer);
        }
    }

    return(status);
}


/***************************************************************************
                       M23_SetBBListCommand(int command)

 Parameters
 ----------
    command - 0 - SECURE , 1-NONSECURE 

 Return Value
 ------------
    None

 Remarks
 -------
  Sends .BBLIST command to processor over firewire
  Recieve's response



***************************************************************************/
int M23_SendBBListCommand(int command)
{
    int           status;
    unsigned char ListBuffer [BUFFER_RECEIVE_SIZE];

    memset(ListBuffer,0x0,BUFFER_RECEIVE_SIZE);
    if(command == 1)
    {
        strncpy(ListBuffer,".BBLIST UNSECURED\r\n",19);
    }
    else
    {
        strncpy(ListBuffer,".BBLIST SECURED\r\n",17);
    }

    status = M2x_SendFirewireCommand(ListBuffer,strlen(ListBuffer));
 
    if(status == 0)
    {

        memset(ListBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(ListBuffer,BUFFER_RECEIVE_SIZE);

        if(status == 0)
        {
            PutResponse(0,ListBuffer);
        }
    }

    return(status);
}

/***************************************************************************
                       M23_SetBBReadCommand(int block)

 Parameters
 ----------
    block number 

 Return Value
 ------------
    None

 Remarks
 -------
  Sends .BBLIST command to processor over firewire
  Recieve's response



***************************************************************************/
int M23_SendBBReadCommand(int block)
{
    int           status;
    unsigned char ReadBuffer [BUFFER_RECEIVE_SIZE];

    memset(ReadBuffer,0x0,BUFFER_RECEIVE_SIZE);
    sprintf(ReadBuffer,".BBREAD %d\r\n",block);

    status = M2x_SendFirewireCommand(ReadBuffer,strlen(ReadBuffer));

    if(status == 0)
    {
        memset(ReadBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(ReadBuffer,BUFFER_RECEIVE_SIZE);

        if(status == 0)
        {
            PutResponse(0,ReadBuffer);
        }
    }

    return(status);
}


/***************************************************************************
                       M23_SendBBSecureCommand()

 Parameters
 ----------
    block number 

 Return Value
 ------------
    None

 Remarks
 -------
  Sends .BBLIST command to processor over firewire
  Recieve's response



***************************************************************************/
int M23_SendBBSecureCommand(int block)
{
    unsigned char SecureBuffer [BUFFER_RECEIVE_SIZE];

    memset(SecureBuffer,0x0,BUFFER_RECEIVE_SIZE);
    sprintf(SecureBuffer,".BBSECURE %d\r\n",block);

printf("Sending %s\r\n",SecureBuffer);
//    M2x_SendFirewireCommand(SecureBuffer,strlen(SecureBuffer));

    return(0);
}

/***************************************************************************
                       M23_RetrieveCartridgeVersion

 Parameters
 ----------
    None

 Return Value
 ------------
    None

 Remarks
 -------
  Sends .Version command to processor over firewire

***************************************************************************/
int M23_RetrieveCartridgeVersion(char *Version)
{
    /*Date Command*/
    int           status;
    int           size;
    unsigned char VersionBuffer [BUFFER_RECEIVE_SIZE];


    memset(VersionBuffer,0x0,BUFFER_RECEIVE_SIZE);
    strncpy(VersionBuffer,".VERSION\r\n",10);


    status = M2x_SendFirewireCommand(VersionBuffer,strlen(VersionBuffer));

    if(status == 0)
    {
        memset(VersionBuffer,0x0,BUFFER_RECEIVE_SIZE);
        status = M2x_RetrieveResponse(VersionBuffer,79);

        if(status == 0)
        {

            for(size = 0; size < strlen(VersionBuffer);size++)
            {
                if(VersionBuffer[size] == ' ')
                {
                    break;
                }
            }

            if(strlen(VersionBuffer) > 0)
            {
                strncpy( Version , VersionBuffer,strlen(VersionBuffer) - 1);
            }
            status = size;
        }

    }

    return(status);
}
