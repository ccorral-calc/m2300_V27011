/***************************************************************************************
 *
 *    Copyright (C) 2004 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed
 *    for any purpose except as specifically authorized in writing.
 *
 *       File: M23_Ethernet.c
 *       Version: 1.0
 *     Author: pcarrion
 *
 *            MONSSTR 2300/2100
 *
 *             The following are Functions used to setup the M2300 Ethernet Interface
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
#include "M23_Utilities.h"

#if 0
//static const UINT8 TEMP_ARP[] =
static UINT8 TEMP_ARP[48] =
{
    0x00, 0x00, 0x00, 0x30,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x14, 0x52, 0x23, 0x12, 0x34, 0x08, 0x06, 0x00, 0x01,
    0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0x00, 0x14, 0x52, 0x23, 0x12, 0x34, 0xC0, 0xA8, 0x02, 0xFA,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8, 0x02, 0xFA ,0x00, 0x00
};
#endif

static UINT8 TEMP_ARP[42] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x14, 0x52, 0x23, 0x00, 0x00, 0x08, 0x06, 0x00, 0x01,
    0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0x00, 0x14, 0x52, 0x23, 0x00, 0x00, 0xC0, 0xA8, 0x00, 0xFA,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8, 0x00, 0xFA 
};


void M23_ReadEthStatus()
{
    UINT32 CSR;
    int    status;

    /*Set  Bank to 0*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);

    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000002);
    status = IsHostRequestClear();
    if(status == 1)
    {
        CSR = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
    }

}

void M23_UpdateIP(int high, int mid1, int mid2, int low)
{
    TEMP_ARP[32] = (UINT8)high;
    TEMP_ARP[42] = (UINT8)high;

    TEMP_ARP[33] = (UINT8)mid1;
    TEMP_ARP[43] = (UINT8)mid1;

    TEMP_ARP[34] = (UINT8)mid2;
    TEMP_ARP[44] = (UINT8)mid2;

    TEMP_ARP[35] = (UINT8)low;
    TEMP_ARP[45] = (UINT8)low;
}

int IsHostRequestClear()
{
    int    count = 0;
    int    clear = 0;
    UINT32 CSR;

    while(1)
    {
        CSR = M23_ReadControllerCSR(ETHERNET_HOST_REQUEST);

        if((CSR & 0x80000000) == 0)
        {
            clear = 1;
            break;
        }
        else
        {
            count++;
            if(count == 200)
            {
                break;
            }
            else
            {
                usleep(10);
            }
        }
    }

    return(clear);
}

int WaitForAutoNego()
{
    int nego;
    int loops = 0;
    int debug;

    M23_GetDebugValue(&debug);

    while(1)
    {
        nego = M23_smc_phy_read(1);
        if(nego & 0x20) //Auto Negotiation complete
        {
            if(debug)
            {
                printf("Auto Negotiation Complete\r\n");
            }
            return(1);
        }
        else
        {
            loops++;
            if(loops > 10)
            {
                if(debug)
                    printf("Auto Negotiation Not Complete\r\n");

                return(0);
            }
            else
            {
                usleep(1000);
            }
        }
    }
}

void M23_FindPort()
{
    int i;
    int count = 0;
    int value;
    int debug;

    UINT32 CSR;

    M23_GetDebugValue(&debug);

    for(i = 0; i < 256;i++)
    {
        value = (i << 5);
        M23_WriteControllerCSR(ETHERNET_BASE_ADDR,value);
        usleep(2);

        M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000000E);
        while(1)
        {
            CSR = M23_ReadControllerCSR(ETHERNET_HOST_REQUEST);

            if((CSR & 0x80000000) == 0)
            {
                break;
            }
            else
            {
                count++;
                if(count == 200)
                {
                    break;
                }
                else
                {
                    usleep(10);
                }
            }
        }

        /*Now Read the Bank*/
        CSR = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);


        if( (CSR & 0xFF00) == 0x3300)
        {
            if(debug)
                printf("Found Device @ 0x%x, Bank Register = 0x%x\r\n",value,CSR);

            i = 256;
        }
    }

}

void M23_SMC_MII_OUT(unsigned int value,int bits)
{
    int          status;

    unsigned int mii_reg;
    unsigned int mask;
    unsigned int reg = 0;

    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000008);
    status = IsHostRequestClear();
    //usleep(3);
    if(status == 1)
    {
        reg = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
    }

    mii_reg = reg & ~(0x0004 | 0x0008 | 0x0001);
    mii_reg |= 0x0008;

    for(mask = 1 << (bits - 1); mask; mask >>= 1)
    {
        if(value & mask)
        {
            mii_reg |= 0x0001;
        }
        else
        {
            mii_reg &= ~0x0001;
        }

        M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,mii_reg);
        M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);
        usleep(2);
        M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,mii_reg | 0x0004);
        M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);
        usleep(2);
    }
}

int M23_SMC_MII_IN(int bits)
{
    int          status;
    unsigned int mii_reg;
    unsigned int mask;
    unsigned int reg = 0;
    unsigned int val;

    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000008);
    //usleep(3);
    status = IsHostRequestClear();
    if(status == 1)
    {
        reg = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
    }

    mii_reg = reg & ~(0x0004 | 0x0008 | 0x0001);
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,mii_reg);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);
    usleep(2);

    for(mask = 1 << (bits - 1),val = 0; mask; mask >>= 1)
    {
        M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000008);
        //usleep(3);
        status = IsHostRequestClear();
        if(status == 1)
        {
            reg = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
        }

        if(reg & 0x0002)
        {
            val |= mask;
        }

        M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,mii_reg);
        M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);
        usleep(2);
        M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,mii_reg | 0x0004);
        M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);
        usleep(2);

    }

    return(val);
}

int  M23_smc_phy_read(int phyreg)
{
    int phyaddr = 0; //No External Phy's , will always be 0
    int phydata;
    int csr = 0;
    int mii_reg;
    int status;

    /*Select Bank 3*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x3);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
    usleep(2);

    /*Idle - 32 ones*/
    M23_SMC_MII_OUT(0xFFFFFFFF,32);

    /*start code (01) + read(10) + phyaddr + phyreg*/
    M23_SMC_MII_OUT((6 << 10) | (phyaddr << 5) | phyreg,14);

    /*Turnaround (2bit) + phydata */
    phydata = M23_SMC_MII_IN(18);


    /*Return To Idle State*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000008);
    //usleep(1);
    status = IsHostRequestClear();
    if(status == 1)
    {
        csr = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
    }
    //csr = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);

    mii_reg = csr & ~(0x0004 | 0x0008 | 0x0001);

    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,mii_reg);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);
    usleep(2);

    /*Set back to BANK 2*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x2);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
 
    return(phydata);
}

void M23_smc_phy_write(int phyreg,int phydata)
{
    int phyaddr = 0; //No External Phy's , will always be 0
    int csr;
    int mii_reg;
    int status;

    /*Select Bank 3*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x3);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
    usleep(2);

    /*Idle - 32 ones*/
    M23_SMC_MII_OUT(0xFFFFFFFF,32);

    /*start code (01) + write(01) + phyaddr + phyreg + turnaround + phydata*/
    M23_SMC_MII_OUT((5 << 28) | (phyaddr << 23) | (phyreg << 18) | (2 << 16) | phydata,32);

    /*Return To Idle State*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000008);
   // usleep(3);
    status = IsHostRequestClear();
    if(status == 1)
    {
        csr = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
    }
    //csr = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
    mii_reg = csr & ~(0x0004 | 0x0008 | 0x0001);

    /*Set back to BANK 2*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x2);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
}

int M23_smc_phy_reset()
{
    int timeout;

    unsigned int bmcr;

    M23_smc_phy_write(0,0x8000);

    for(timeout = 2; timeout; timeout--)
    {
        usleep(100000);
      
        bmcr = M23_smc_phy_read(0);

        if( !(bmcr & 0x8000) )
        {
            break;
        }
    }

    return(bmcr & 0x8000);
}

void M23_SendData(unsigned char *data,int len)
{
    int          i;
    int          temp_len = 0x30;
    unsigned int packet_no = 0;
    unsigned int status;
    unsigned int csr = 0;
    unsigned int poll = 32;
    unsigned int num_pages;
    UINT16 *ptmp16 = (UINT16*)data;

    /*Enable Bank 2*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x2);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
    usleep(2);

    /*Allocate the MMU memory*/
    num_pages = ((temp_len & ~1) + (6-1)) >> 8;
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,( (1<<5) | num_pages));
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000010);
    usleep(2);
 
    do{
        M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000000C);
        //usleep(2);
        status = IsHostRequestClear();
        if(status == 1)
        {
            csr = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
        }
        //csr = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
        usleep(2);

        if(csr & 0x08)
        {
            /*Acknowledge Interrupt*/
            M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x08);
            M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001C);
            //printf("MMU Allocation Complete\r\n");
            usleep(2);
            break;
        }
        else
        {
            usleep(10);
        }
    
    }while(--poll);

    /*Get Packet Number*/
    //M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000013);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000003);
    status = IsHostRequestClear();
    if(status == 1)
    {
        packet_no = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
    }
   // usleep(2);
    //packet_no = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);

    /*Point to the beginning of the packet*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,packet_no);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000012);
    usleep(2);

    /*Get the Pointer to Auto Increment*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x4000);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000016);
    usleep(2);

    //M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,*ptmp16);
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x00);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);

    //SWAP_TWO(*ptmp16);
    //M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,*ptmp16);
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x30);
    //M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,SWAP_TWO(temp_len));
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);


    /*Now copy the data*/
    //for(i = 0; i< (len - 6)/2;i++)
    for(i = 0; i < 21;i++)
    {
        //SWAP_TWO(*ptmp16);
        //printf("Writing Data 0x%04x\r\n",*ptmp16);
        M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,*ptmp16);
        M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);
        usleep(3);
        ptmp16++;
    }

    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);
    usleep(2);

    /*Queue the packet for TX*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0xC0);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000010);
    usleep(2);

}


void M23_EthernetReset()
{
    UINT32 CSR = 0;
    int count = 0;
    int status = 0;
    int debug;

    M23_GetDebugValue(&debug);

    /*Enable Bank 2*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x2);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
    usleep(2);

    /*Set Interrrupt Enable*/
    //M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0100);
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001C);
    usleep(2);

    /*Set  Bank to 0*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
    usleep(2);

    /*Soft Reset*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x8000);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000014);
    usleep(2);

    /*Set  Bank to 1*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x1);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
    usleep(2);

    /*Write 0x9000 to Register 0*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x9000);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000010);
    usleep(2);

    /* Disable transmit and receive functionality */
    /*Set  Bank to 0*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
    usleep(2);

    /*Write 0x0 to Register 4, RCR Clear*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000014);
    usleep(2);

    /*Write 0x0 to Register 0, TCR Clear*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000010);
    usleep(2);

    /*Set  Bank to 1*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x1);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
    usleep(2);

    /*read Control Register*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000000C);
    status = IsHostRequestClear();
    if(status == 1)
    {
        CSR = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
    }
 //   usleep(2);
 //   CSR = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
    CSR |= 0x0880;

    /*Write Control Register*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,CSR);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001C);
    usleep(2);

    /*Reset the MMU*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x2);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
    usleep(2);

    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,(2<<5));
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000010);
    
    usleep(2);

    while(1)
    {
        /*Read The Busy BIT*/
        M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000000);
        //usleep(2);
        status = IsHostRequestClear();
        if(status == 1)
        {
            CSR = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
        }
        //CSR = M23_ReadControllerCSR(ETHERNET_HOST_READ_DATA);
        if(CSR & 0x1)
        {
            count++;
            if(count == 500)
            {
                printf("Still Busy after MMU Reset\r\n");
            }
        }
        else
        {
            //printf("Busy cleared after MMU Reset\r\n");
            break;
        } 
    }

}

void M23_EnableEthernetPHY()
{
    int phy_cap;
    int ad_cap;
    int status;
    int debug;
 
    M23_GetDebugValue(&debug);

    /*Reset the PHY*/
    if(M23_smc_phy_reset())
    {
        printf("Phy Reset timed out\r\n");
        return;
    }
    else
    {
        //printf("Phy Reset Complete\r\n");
    }

    /*Only enable bits have changed,and Link fail*/
    M23_smc_phy_write(0x13,0x2000 | 0x1000 | 0x0800 | 0x0400 | 0x0200 | 0x0100 | 0x0080 | 0x0040);

    /*Enable Bank 0*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);
    usleep(2);

    /*Enable 100 and Full Duplex and Auto Negotiate*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x3800);
   // M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x3000);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001a);
    usleep(2);

    phy_cap = M23_smc_phy_read(0x1);

    if( !(phy_cap & 0x0008) )
    {
        if(debug)
            printf("Auto Negotiation Not Supported\r\n");
    }

    ad_cap = 0x0001;

    if( phy_cap & 0x8000) //100Base-T4 capable
    {
        ad_cap |= 0x0200;
    }

    if( phy_cap & 0x4000) //100Base Full duplex
    {
        ad_cap |= 0x0100;
    }

    if( phy_cap & 0x2000) //100Base half duplex
    {
        ad_cap |= 0x0080;
    }

    if( phy_cap & 0x1000) //10Base Full duplex
    {
        ad_cap |= 0x0040;
    }

    if( phy_cap & 0x0800) //10Base Half duplex
    {
        ad_cap |= 0x0020;
    }

    /*Update the Auto-Neg Advertisement Register*/
    M23_smc_phy_write(0x04,ad_cap);

    status = M23_smc_phy_read(0x4);

    /* Restart auto-negotiation process in order to advertise my caps */
    M23_smc_phy_write( 0x0, 0x1000 | 0x0200);
    //M23_smc_phy_write( 0x0, 0x0200);

    /*Right now we are not checking Media*/

    status = M23_smc_phy_read(0x0);

    /*Set  Bank to 2*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x2);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);

    printf("Enable Ethernet PHY Complete\r\n");
    return;
}


void M23_StartEthernet(int High,int Mid,int Low,int CH10)
{
    UINT32 CSR;
    int    status;
    int    debug;

    M23_GetDebugValue(&debug);

    /*First Reset the Ethernet Chip*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,ETHERNET_RESET_BIT);
    usleep(1);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x0);
    usleep(300000);

    //M23_FindPort();

    /*Set the Address to 0x300*/
    M23_WriteControllerCSR(ETHERNET_BASE_ADDR,0x300);

    M23_EthernetReset();

    /*Set  Bank to 0*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0);
    usleep(2);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);

    /*Enable Transmit*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x81);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000010);

    /*Enable Receiver*/

    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0);

    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000014);
    usleep(2);


    /*Set  Bank to 1, to Set the MAC Address*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x1);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);

#if 1
    /*Write The High MAC*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,(UINT16)High);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000014);
    usleep(2);
    /*Write The Mid MAC*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,(UINT16)Mid);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000016);
    usleep(2);
    /*Write The Low  MAC*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,(UINT16)Low);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x80000018);
    usleep(2);
#endif

    /*Set  Bank to 2*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x2);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);

    /*Set Interrrupt Enable*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x0100);
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001C);

    M23_EnableEthernetPHY();

    status = WaitForAutoNego();

#if 0
    if(status == 1)
    {
        //printf("Auto-Negotiation completed\r\n");
        //M23_SendData((unsigned char*)TEMP_ARP,48); 
        //M23_SendData((unsigned char*)TEMP_ARP,42); 
    }
    else
    {
        if(debug)
            printf("Auto-Negotiation Did Not Complete\r\n");
    }
#endif



    /*Set  Bank to 2*/
    M23_WriteControllerCSR(ETHERNET_HOST_WRITE_DATA,0x2);
    /*Write This value to Register 0xE*/
    M23_WriteControllerCSR(ETHERNET_HOST_REQUEST,0x8000001E);

}
