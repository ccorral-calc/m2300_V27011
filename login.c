/*
//#**************************************************************************
//#**************************************************************************
//#**************************************************************************
//  login.c
//
//  Copyright (C) 2006, CALCULEX, Inc. All rights reserved.
//
//    The material herein contains information that is proprietary and
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed
//    for any purpose except as specifically authorized in writing.
//
//  This program is to test login to a Calculex cartridge from the new
//  2300V1/2300V2 controller.
//
//  Revisions:  V1.1
//              Date:   09/27/2006
//              Author: bdu
//              Rev:    Initial release of this module. Support OHCI on Linux
//                              only (kernel 2.6.16)
//              NOTE:   This code is inherited from Marty's work on sending PHY
//                      packets on the 1394 bus.
//
//
//
//#**************************************************************************
//#**************************************************************************
//#**************************************************************************
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "login.h"

#ifndef _SWAP
#define _SWAP
#endif

static UINT32 Base_Address;
static int BusSpeed = 0;

void send_PHY_config(int node_id);


void dump_packet(UINT32 *pkt, int length)
{
  UINT32 *ptmp32 = pkt;
  int i;

  for(i=0; i<length; i++)
    {
      printf("  0x%08lX", *ptmp32);
      if((i+1)%4 == 0)
	printf("\n");
      ptmp32++;
    }
  
  if(length % 8)
    printf("\n");
}



// to get OHCI base address
UINT32 get_ohci_base_address()
{
  UINT32 base = 0;
  int fd;
  UINT32 va[7];
  IoControlStruct ics;
  int ret;


  //open the device
  fd = open("/dev/miniohci", O_RDONLY);
  if(fd == -1)
    {
      printf("cannot open OHCI board\n");
      return 0;
    }

  // get base address
  ics.lpOutBuffer = va;
  ret = ioctl(fd, CX_IOCTL_BASE_ADDRESSES_GET, &ics);
  if(ret != 0)
    {
      printf("cannot get base addresses, ret = %d\n", ret);
      close(fd);
      return 0;
    }

  memcpy(va, ics.lpOutBuffer, sizeof(va));
  base = va[0];
  Base_Address = base;

  close(fd);

  return base;
}


/****************************************************
 * Function to initialize the OHCI chip
 *
 * return: 0 = success, otherwise = fail
 *
 ****************************************************/
int initialize_ohci(struct ohci_link_regs_t *ohci_reg_ptr)
{
  UINT32 temp;
  UINT32 reg_val;

#if 1
  // Enable LPS at link
  WRITE32(Base_Address + OHCI1394_HCControlSet, 0x00080000);
  
  // Initialize the Bus Options register
  temp = ohci_reg_ptr->Bus_Options;
  temp |= 0x60000000;	//enable CMC and ISC
  temp |= 0x80000000;	//enable IRCM
  temp &= ~0x00ff0000;//set cyc_clk_acc to zero
  WRITE32(Base_Address + OHCI1394_BusOptions, temp);
  
  // Set the bus number in the Node ID register
  WRITE32(Base_Address + OHCI1394_NodeID, 0x0000FFC0);
  
  // Enable posted writes in the HC Control Set regisrer
  WRITE32(Base_Address + OHCI1394_HCControlSet, 0x00040000);
  
  // Clear the Link Control register
  WRITE32(Base_Address + OHCI1394_LinkControlSet, 0xFFFFFFFF);
  
  // Enble cycle timer and cycle master in the Link Control retister
  WRITE32(Base_Address + OHCI1394_LinkControlSet, 0x00300000);

  // Set link-active status control and and IRM contender in the PHY
  reg_val = (unsigned int) read_phy_reg( ohci_reg_ptr, 4 ) &0xFF;
  reg_val |= 0xc0;
  write_phy_reg( ohci_reg_ptr, 4, reg_val);
  
  // Set up self-id dma buffer
  WRITE32(Base_Address + OHCI1394_SelfIDBuffer, DPR_BASE_ADDRESS + DPR_SELFID_BUF);
  
  // Enable Self-ID and PHY packet reception
  WRITE32(Base_Address + OHCI1394_LinkControlSet, 0x00000600);
  
  // Set the Config ROM mapping register
  WRITE32(Base_Address + OHCI1394_ConfigROMmap, DPR_BASE_ADDRESS + DPR_CONFIG_ROM_BUF);
  
  // Block PHY packets into AR request context
  WRITE32(Base_Address + OHCI1394_LinkControlClear, 0x00000400);
  
  // Clear the ISO receive interrupt mask and status
  WRITE32(Base_Address + OHCI1394_IsoRecvIntMaskClear, 0xffffffff);
  WRITE32(Base_Address + OHCI1394_IsoRecvIntEventClear, 0xffffffff);
  
  // Clear the ISO xmit interrupt mask and status
  WRITE32(Base_Address + OHCI1394_IsoXmitIntMaskClear, 0xffffffff);
  WRITE32(Base_Address + OHCI1394_IsoXmitIntEventClear, 0xffffffff);
  
  //init AR response
  init_AR_resp_ctx( ohci_reg_ptr );

  //Accept AR requests from all nodes
  WRITE32(Base_Address + OHCI1394_AsReqFilterHiSet, 0xffffffff);
  WRITE32(Base_Address + OHCI1394_AsReqFilterLoSet, 0xffffffff);
  
  //set upper bound
  WRITE32(Base_Address + OHCI1394_PhyReqFilterHiSet, 0xffffffff);
  WRITE32(Base_Address + OHCI1394_PhyReqFilterLoSet, 0xffffffff);
  WRITE32(Base_Address + OHCI1394_PhyUpperBound, 0xffff0000);
  
  //Specify AT retries
  WRITE32(Base_Address + OHCI1394_ATRetries, 0x00000822);
  
  //Disable hardware byte swapping
  WRITE32(Base_Address + OHCI1394_HCControlClear, 0x40000000);
  
  //Enable link
  WRITE32(Base_Address + OHCI1394_HCControlSet, 0x00020000);
  
  //assume we are root now
  //WRITE32(Base_Address + OHCI1394_NodeID, 0x0000003f);
#else

   // Enable LPS at link
  WRITE32(Base_Address + OHCI1394_HCControlSet, 0x00080000);

  // Enable posted writes in the HC Control Set regisrer
  WRITE32(Base_Address + OHCI1394_HCControlSet, 0x00040000);
  
  // Clear the Link Control register
  WRITE32(Base_Address + OHCI1394_LinkControlSet, 0xFFFFFFFF);
  
  // Enble cycle timer and cycle master in the Link Control retister
  WRITE32(Base_Address + OHCI1394_LinkControlSet, 0x00300000);

  // Enable Self-ID and PHY packet reception
  WRITE32(Base_Address + OHCI1394_LinkControlSet, 0x00000600);
  
  // Block PHY packets into AR request context
  WRITE32(Base_Address + OHCI1394_LinkControlClear, 0x00000400);

  //Accept AR requests from all nodes
  WRITE32(Base_Address + OHCI1394_AsReqFilterHiSet, 0xffffffff);
  WRITE32(Base_Address + OHCI1394_AsReqFilterLoSet, 0xffffffff);
  
  //set upper bound
  WRITE32(Base_Address + OHCI1394_PhyReqFilterHiSet, 0xffffffff);
  WRITE32(Base_Address + OHCI1394_PhyReqFilterLoSet, 0xffffffff);
  WRITE32(Base_Address + OHCI1394_PhyUpperBound, 0xffff0000);
  
  //Specify AT retries
  WRITE32(Base_Address + OHCI1394_ATRetries, 0x00000822);

  //Enable link
  WRITE32(Base_Address + OHCI1394_HCControlSet, 0x00020000);
#endif
  return 0;
}


/****************************************************
 * Function to build topology map of local bus
 *
 * return: total nodes on the bus
 *
 ****************************************************/
int get_node_map(PHY_NODE *node)
{
  int node_count = 0;
  int gen;
  int size;
  UINT32 temp;
  UINT32 ptr;
  int i;

  //get generation and size
  gen = get_selfID_generation();
  size = (get_selfID_size() - 1) / 2;

#ifdef _DEBUG
  printf("generation=%d, size=%d\n", gen, size);
#endif

  //now get the pointer to self ID buffer
  ptr = READ32(Base_Address + OHCI1394_SelfIDBuffer) & 0xFFFFF800;

  // start reading self ID from ptr, skip the first quadlet
  ptr += 4;
  for(i=0; i<size; i++)
    {
      temp = READ32(ptr);
      ptr += 8;
   
      //decode the PHY
      node[i].node_id = (temp & 0x3F000000) >> 24;
      node[i].speed = (temp & 0xC000) >> 14;
      node[i].contender = (temp & 800) >> 11;
      node[i].link_active = (temp & 0x00400000) >> 22;
      node[i].p0 = (temp & 0xC0) >> 6;
      node[i].p1 = (temp & 0x30) >> 4;
      node[i].p2 = (temp & 0xC) >> 2;

      //decide node type. Only controller and cartridge will be active
      if(node[i].link_active == 1)
	{
	  if(node[i].p0 == 2 && node[i].p1 == 1 && node[i].p2 == 1) //p0 has parent, p1, p2 N/C, cartridge
	    node[i].type = Calculex_Cartridge;
	  else if(node[i].p0 == 1 && node[i].p1 == 1 && node[i].p2 == 3) //p0, p1 N/C, p2 has child, controller
	    node[i].type = Calculex_Controller;
	  else if(node[i].p0 == 3 && node[i].p1 == 1 && node[i].p2 == 1) //p1 has child, p1,p2 N/C, remote cartridge
	    node[i].type = Remote_Cartridge;
	}

      node_count++;
      printf("node %d, self ID = 0x%08lx, p0=%d, p1=%d, p2=%d, link=%d\n", i, temp, node[i].p0, node[i].p1, node[i].p2, node[i].link_active);
    }

  return node_count;
}


//function to initialize the first 3 quadlets of SCSI ORB
int initialize_scsi_orb(int node_id)
{
  struct dual_port_RAM_t *dpr_ptr;
  UINT32 *ptmp32;

  dpr_ptr = (struct dual_port_RAM_t*)(0x80000000 | DPR_BASE_ADDRESS);

  ptmp32 = (UINT32*)&dpr_ptr->AT_SCSI_ORB;

  //set first 2 scsi quadlets to NULL
  *ptmp32 = 0xFFFFFFFF;
  ptmp32++;
  *ptmp32 = 0xFFFFFFFF;
  ptmp32++;

  *ptmp32 = 0xFFC00000 | (node_id << 16);
  *ptmp32 = SWAP_FOUR(*ptmp32);

  //make both contexts point to SCSI ORB
  WRITE32(DRP_RESERVED_SPACE_A, SWAP_FOUR(0xFFC00000 | (node_id << 16)));
  WRITE32(DRP_RESERVED_SPACE_A+4, SWAP_FOUR(DPR_BASE_ADDRESS + DPR_AT_SCSI_ORB));
  WRITE32(DRP_RESERVED_SPACE_B, SWAP_FOUR(0xFFC00000 | (node_id << 16)));
  WRITE32(DRP_RESERVED_SPACE_B+4, SWAP_FOUR(DPR_BASE_ADDRESS + DPR_AT_SCSI_ORB));

  //change management agent to command agent in both contexts
  dpr_ptr = (struct dual_port_RAM_t*)(0x80000000 | DPR_BASE_ADDRESS);
  dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[2]	= 0xF0100008;	//Dest_addr=FFFFF0100008
  dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[2]	= 0xF0100008;	//Dest_addr=FFFFF0100008

  return 0;
}


//force the controller to be root on bus
int force_root(struct ohci_link_regs_t *link_ptr)
{
  UINT32 my_id;
  UINT32 temp;

  my_id = READ32(Base_Address + OHCI1394_NodeID) & 0x0000003F;
  printf("my_id=%d\n", my_id);
  send_PHY_config(my_id);

#if 0
  //issue short bus reset
  temp = read_phy_reg(link_ptr, 5) & 0xFF;
  temp |= 0x40;
  write_phy_reg(link_ptr, 5, temp);
#endif

  //issue bus reset
  temp = read_phy_reg(link_ptr, 1) & 0xFF;
  temp |= 0xC0;
  write_phy_reg(link_ptr, 1, temp);

  return 0;
}


/****************************************************
 * Function to set 1394 bus speed
 *
 * return: 0=success, 1=error
 *
 ****************************************************/
int set_bus_speed(BUS_SPEED speed)
{
  switch(speed)
    {
    case Speed_100:
      BusSpeed = 0;
      break;

    case Speed_200:
      BusSpeed = 1;
      break;

    case Speed_400:
      BusSpeed = 2;
      break;

    case Speed_800:
      BusSpeed = 3;
      break;

    default:
      printf("un-supported speed\n");
      BusSpeed = 0;
      break;
    }

  return 0;
}

/************************** Send Login ORB Packet ****************************
 * send_phy_config above used its own private non-SBP2 OHCI context descriptor.
 * we are going to use the AT_REQ_A context to send the SBP2 login ORB.  We will
 * use the AT_REQ_B context for the next SBP2 ORB and toggle between _A and _B
 * for all remaining SBP2 ORBS.  Initialize both _A and _B context programs.
 *****************************************************************************/
void send_login_orb(struct ohci_link_regs_t *link_ptr, int phy_id, int target_id)
{
  struct dual_port_RAM_t *dpr_ptr;
  UINT32 temp;
  int i;
  UINT32 *ptmp32;
  

#ifdef _DEBUG
  printf("send_login_ORB: phy_id = %01d  target_id = %01d\n", phy_id, target_id);
#endif
  

  // setup the AT Request Context Program A descriptors.  The first is an output_more_immediate
  // descriptor that transports the header (first four quadlets) of an IEEE-1394 write_block_data
  // packet (without the CRC which is added by the link IC), followed by an output_last
  // descriptor that transports the immediate data which is an 8-byte address of the login ORB. The
  // destination for this write operation is the cartridge's Management Agent register which is at
  // CSR Base + 30000 = FFFFF0030000.

  dpr_ptr = (struct dual_port_RAM_t*)(0x80000000 | DPR_BASE_ADDRESS);


  // now initialize the login ORB in dual port memory
  dpr_ptr->AT_LOGIN_ORB.password_ptr[0]	= 0;
  dpr_ptr->AT_LOGIN_ORB.password_ptr[1]	= 0;

  temp = 0xFFC00000 | (phy_id << 16);
  dpr_ptr->AT_LOGIN_ORB.login_resp_ptr[0] = temp;	//Login response buffer pntr MSW

  dpr_ptr->AT_LOGIN_ORB.login_resp_ptr[1] =  DPR_BASE_ADDRESS + DPR_LOGIN_RESP_BUF; //0x00041240; 
  //dpr_ptr->AT_LOGIN_ORB.lun = 0;			//Login to LUN-0
  dpr_ptr->AT_LOGIN_ORB.cmd_word = 0x90000000;		//Notify + exclusive
  dpr_ptr->AT_LOGIN_ORB.login_resp_length = 0x00000010;		//Will accept up to 16 byte reply
  //dpr_ptr->AT_LOGIN_ORB.password_length	= 0;		//No password
  temp = 0xFFC00000 | (phy_id << 16);
  dpr_ptr->AT_LOGIN_ORB.status_fifo_ptr[0] = temp;	//Status FIFO pntr MSW

  dpr_ptr->AT_LOGIN_ORB.status_fifo_ptr[1] = DPR_BASE_ADDRESS + DPR_STATUS_FIFO; //0x00041250;


#ifdef _SWAP  //need to swap the byte order. bdu090806
  ptmp32 = (UINT32*)&dpr_ptr->AT_LOGIN_ORB;
  for(i=0; i<sizeof(dpr_ptr->AT_LOGIN_ORB)/4; i++)
    {
      *ptmp32 = SWAP_FOUR(*ptmp32);
      ptmp32++;
    }
#endif

  //set command descriptor
  dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.desc_cmd_word	= 0x02000010;	//Output_more_immediate cmd/key
  dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.reserved[0]	= 0;
  dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.reserved[1]	= 0;
  dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.reserved[2]	= 0;

  dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[0] = 0x00800510 | (BusSpeed << 16);  //see Fig.7-12 of OHCI 1.1/ bdu091106
  temp = 0xFFC0FFFF | (target_id << 16);
  dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[1]	= temp;	        //Src_ID=FFC2 + (see next)
  
  dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[2]	= 0xF0030000;	//Dest_addr=FFFFF003000
  dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[3]	= 0x00080000;	//IEEE-1394 Data_Length=8

  //output last
  dpr_ptr->AT_REQ_CTX_A.ohci_last.desc_cmd_word	= 0x100C0008;	//Output_last cmd/key

  //prepare the data address
  WRITE32(DRP_RESERVED_SPACE_A, SWAP_FOUR(0xFFC00000 | (phy_id << 16)));
  WRITE32(DRP_RESERVED_SPACE_A + 4, SWAP_FOUR((DPR_BASE_ADDRESS + DPR_AT_LOGIN_ORB)));

#ifdef _DEBUG
  printf("login orb pointer:\n");
  dump_packet((UINT32*)DRP_RESERVED_SPACE_A, 2);
#endif
  
  dpr_ptr->AT_REQ_CTX_A.ohci_last.data_address = (UINT32)(DRP_RESERVED_SPACE_A & 0x7FFFFFFF);
  dpr_ptr->AT_REQ_CTX_A.ohci_last.branch_address = 0;
  dpr_ptr->AT_REQ_CTX_A.ohci_last.timeStamp = 0;

#ifdef _DEBUG 
  printf("output last:\n");
  dump_packet((UINT32*)&dpr_ptr->AT_REQ_CTX_A, 12);
#endif  

  /** Marty wanted to setup context B the same as context A for login. bdu100506 **/
  //set command descriptor
  dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.desc_cmd_word	= 0x02000010;	//Output_more_immediate cmd/key
  dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.reserved[0]	= 0;
  dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.reserved[1]	= 0;
  dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.reserved[2]	= 0;

  dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[0] = 0x00800510 | (BusSpeed << 16);  //see Fig.7-12 of OHCI 1.1/ bdu091106
  temp = 0xFFC0FFFF | (target_id << 16);
  dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[1]	= temp;	        //Src_ID=FFC2 + (see next)
  
  dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[2]	= 0xF0030000;	//Dest_addr=FFFFF003000
  dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[3]	= 0x00080000;	//IEEE-1394 Data_Length=8

  //output last
  dpr_ptr->AT_REQ_CTX_B.ohci_last.desc_cmd_word	= 0x100C0008;	//Output_last cmd/key

  //prepare the data address
  WRITE32(DRP_RESERVED_SPACE_B, SWAP_FOUR(0xFFC00000 | (phy_id << 16)));
  WRITE32(DRP_RESERVED_SPACE_B + 4, SWAP_FOUR((DPR_BASE_ADDRESS + DPR_AT_LOGIN_ORB)));

  dpr_ptr->AT_REQ_CTX_B.ohci_last.data_address = (UINT32)(DRP_RESERVED_SPACE_B & 0x7FFFFFFF);
  dpr_ptr->AT_REQ_CTX_B.ohci_last.branch_address = 0;
  dpr_ptr->AT_REQ_CTX_B.ohci_last.timeStamp = 0;

  //****** END OF CONTEXT B ADDING ********

  usleep(3000);

  // to send the ORB, put the login context program address with a Z value of 4 (four 16-byte chunks) in the
  // context pointer register
  WRITE32(Base_Address + OHCI1394_AsReqTrCommandPtr, DPR_BASE_ADDRESS + DPR_AT_REQ_CTX_A + 3); //0x00041003, We load the context pointer register

#ifdef _DEBUG
  printf("send_login_ORB: Command_Ptr loaded with 0x%08x\n", link_ptr->AT_Req_Command_Ptr);
#endif

  usleep(3000);

  //set the RUN bit
  WRITE32(Base_Address + OHCI1394_AsReqTrContextControlSet, 0x8000);

  return;
}


// function to read blocks from cartridge
// return value: 0 = OK, 1 = error
int  read_blocks(struct ohci_link_regs_t *link_ptr, int phy_id, CONTEXT context, UINT32 block, int num_blocks )
{
  struct dual_port_RAM_t *dpr_ptr;
  int target_id = 0;
  UINT32 temp;
  UINT32 *ptmp32;
  int i;

  dpr_ptr = (struct dual_port_RAM_t*)(0x80000000 | DPR_BASE_ADDRESS);

  //setup SCSI CDB
  dpr_ptr->AT_SCSI_ORB.next_orb_mslw = 0xFFFFFFFF;  //NULL
  dpr_ptr->AT_SCSI_ORB.next_orb_lslw = 0xFFFFFFFF;  //NULL
  dpr_ptr->AT_SCSI_ORB.data_desc_mslw = 0xFFC00000 | (phy_id << 16); //hard-coded, with analyzer, it's 0xFFC3
  dpr_ptr->AT_SCSI_ORB.data_desc_lslw = DPR_BASE_ADDRESS + DPR_HOST_BUFFER; //0x00041500; //address in controller space
  dpr_ptr->AT_SCSI_ORB.dir_size = 0x8C000000 | (num_blocks * 512);       //read 512x64 bytes 
  dpr_ptr->AT_SCSI_ORB.cdb.cdb_blk[0] = (0x28 << 24) | ((block >> 16) & 0xFFFF);        // op code|LUN0|block hi
  dpr_ptr->AT_SCSI_ORB.cdb.cdb_blk[1] = (block & 0xFFFF) << 16;
  dpr_ptr->AT_SCSI_ORB.cdb.cdb_blk[2] = num_blocks << 24;   //num of blocks


#ifdef _SWAP  //need to swap the byte order. bdu090806
  ptmp32 = (UINT32*)&dpr_ptr->AT_SCSI_ORB;
  for(i=0; i<sizeof(dpr_ptr->AT_SCSI_ORB)/4; i++)
    {
      *ptmp32 = SWAP_FOUR(*ptmp32);
      ptmp32++;
    }
#endif

  // printf("AT SCSI ORB addr=0x%08lX\n", (UINT32)&dpr_ptr->AT_SCSI_ORB);

  // make sure the STATUS_FIFO is empty so we can see when ORB processing by the cartridge is complete
  dpr_ptr->STATUS_FIFO[0] = 0;
  dpr_ptr->STATUS_FIFO[1] = 0;

  //set command descriptor
  switch(context)
    {
    case context_a:
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.desc_cmd_word = 0x02000010;	//Output_more_immediate cmd/key
      //dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.desc_reqCount = 16;		//OHCI Immediate data length
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.reserved[0]	= 0;
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.reserved[1]	= 0;
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.reserved[2]	= 0;
  
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[0] = 0x00802110 | (BusSpeed << 16);  //see Fig.7-12 of OHCI 1.1/ bdu091106
      temp = 0xFFC0FFFF | (target_id << 16);
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[1]	= temp;	        
      
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[2]	= 0xF0100008;	//Dest_addr=FFFFF0100008
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[3]	= 0x00080000;	//IEEE-1394 Data_Length=8
      
      //output last
      dpr_ptr->AT_REQ_CTX_A.ohci_last.desc_cmd_word	= 0x100C0008;	//Output_last cmd/key
      //dpr_ptr->AT_REQ_CTX_A.ohci_last.desc_reqCount	= 8;		//OHCI Immediate data length
        
      //prepare the data address
      WRITE32(DRP_RESERVED_SPACE2, SWAP_FOUR(0xFFC00000 | (phy_id << 16)));
      WRITE32(DRP_RESERVED_SPACE2 + 4, SWAP_FOUR((DPR_BASE_ADDRESS + DPR_AT_SCSI_ORB)));  //0x41200
      
      dpr_ptr->AT_REQ_CTX_A.ohci_last.data_address = (UINT32)(DRP_RESERVED_SPACE2 & 0x7FFFFFFF);
      dpr_ptr->AT_REQ_CTX_A.ohci_last.branch_address = 0;
      //dpr_ptr->AT_REQ_CTX_A.ohci_last.xferStatus = 0;
      dpr_ptr->AT_REQ_CTX_A.ohci_last.timeStamp = 0;

      dpr_ptr->AT_REQ_CTX_B.ohci_last.branch_address = ((UINT32)&dpr_ptr->AT_REQ_CTX_A & 0x7FFFFFF0) | 0x3;
      dpr_ptr->AT_REQ_CTX_A.ohci_last.branch_address = 0; //no link to another context
      break;

    case context_b:
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.desc_cmd_word = 0x02000010;	//Output_more_immediate cmd/key
      //dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.desc_reqCount = 16;		//OHCI Immediate data length
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.reserved[0]	= 0;
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.reserved[1]	= 0;
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.reserved[2]	= 0;
  
      //temp = 0xFFC01110 | (phy_id << 16);
      //dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[0]	= temp;		//Dest_ID=FFC0,Label=4,Tcode=1
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[0] = 0x00802110 | (BusSpeed << 16);  //see Fig.7-12 of OHCI 1.1/ bdu091106
      temp = 0xFFC0FFFF | (target_id << 16);
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[1]	= temp;	        
      
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[2]	= 0xF0100008;	//Dest_addr=FFFFF0100008
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[3]	= 0x00080000;	//IEEE-1394 Data_Length=8
      
      //output last
      dpr_ptr->AT_REQ_CTX_B.ohci_last.desc_cmd_word	= 0x100C0008;	//Output_last cmd/key
      //dpr_ptr->AT_REQ_CTX_B.ohci_last.desc_reqCount	= 8;		//OHCI Immediate data length
     
      
      //prepare the data address
      WRITE32(DRP_RESERVED_SPACE2, SWAP_FOUR(0xFFC00000 | (phy_id << 16)));
      WRITE32(DRP_RESERVED_SPACE2 + 4, SWAP_FOUR((DPR_BASE_ADDRESS + DPR_AT_SCSI_ORB)));  //0x41200
      
      dpr_ptr->AT_REQ_CTX_B.ohci_last.data_address = (UINT32)(DRP_RESERVED_SPACE2 & 0x7FFFFFFF);
      dpr_ptr->AT_REQ_CTX_B.ohci_last.branch_address = 0;
      //dpr_ptr->AT_REQ_CTX_B.ohci_last.xferStatus = 0;
      dpr_ptr->AT_REQ_CTX_B.ohci_last.timeStamp = 0;

      dpr_ptr->AT_REQ_CTX_A.ohci_last.branch_address = ((UINT32)&dpr_ptr->AT_REQ_CTX_B & 0x7FFFFFF0) | 0x3;
      dpr_ptr->AT_REQ_CTX_B.ohci_last.branch_address = 0; //no link to another context
      break;

    default:
      printf("wrong context space\n");
      return 1;
    }
      
  //according to OHCI, I should not be able to change this value if the RUN and ACTIVE bits are set. bdu091206
  //link_ptr->AT_Req_Command_Ptr = ((UINT32)&dpr_ptr->AT_REQ_CTX_B & 0xFFFFFFF0) | 0x3;	// We load the context pointer register

  //!!!!!! reset the branch address and Z of the descriptor that was the end of list being processed by HC !!!!
  //dpr_ptr->AT_REQ_CTX_A.ohci_last.branch_address = ((UINT32)&dpr_ptr->AT_REQ_CTX_B & 0x7FFFFFF0) | 0x3;

  //set the wake bit
  WRITE32(Base_Address + OHCI1394_AsReqTrContextControlSet, 0x1000);


  return 0;
}


// function to write blocks from cartridge
// return value: 0 = OK, 1 = error
int  write_blocks(struct ohci_link_regs_t *link_ptr, int phy_id, CONTEXT context, UINT32 block, int num_blocks )
{
  struct dual_port_RAM_t *dpr_ptr;
  int target_id = 0;
  UINT32 temp;
  UINT32 *ptmp32;
  int i;

  dpr_ptr = (struct dual_port_RAM_t*)(0x80000000 | DPR_BASE_ADDRESS);

  //setup SCSI CDB
  dpr_ptr->AT_SCSI_ORB.next_orb_mslw = 0xFFFFFFFF;  //NULL
  dpr_ptr->AT_SCSI_ORB.next_orb_lslw = 0xFFFFFFFF;  //NULL
  dpr_ptr->AT_SCSI_ORB.data_desc_mslw = 0xFFC00000 | (phy_id << 16); //hard-coded, with analyzer, it's 0xFFC3
  dpr_ptr->AT_SCSI_ORB.data_desc_lslw = 0x00041500; //address in controller space
  dpr_ptr->AT_SCSI_ORB.dir_size = 0x84000000 | (num_blocks * 512);       //read 512x64 bytes 
  dpr_ptr->AT_SCSI_ORB.cdb.cdb_blk[0] = (0x2A << 24) | ((block >> 16) & 0xFFFF);        // op code|LUN0|block hi
  dpr_ptr->AT_SCSI_ORB.cdb.cdb_blk[1] = (block & 0xFFFF) << 16;
  dpr_ptr->AT_SCSI_ORB.cdb.cdb_blk[2] = num_blocks << 24;   //num of blocks


#ifdef _SWAP  //need to swap the byte order. bdu090806
  ptmp32 = (UINT32*)&dpr_ptr->AT_SCSI_ORB;
  for(i=0; i<sizeof(dpr_ptr->AT_SCSI_ORB)/4; i++)
    {
      *ptmp32 = SWAP_FOUR(*ptmp32);
      ptmp32++;
    }
#endif


  // make sure the STATUS_FIFO is empty so we can see when ORB processing by the cartridge is complete
  dpr_ptr->STATUS_FIFO[0] = 0;
  dpr_ptr->STATUS_FIFO[1] = 0;

  //set command descriptor
  switch(context)
    {
    case context_a:
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.desc_cmd_word = 0x02000010;	//Output_more_immediate cmd/key
      //dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.desc_reqCount = 16;		//OHCI Immediate data length
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.reserved[0]	= 0;
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.reserved[1]	= 0;
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.reserved[2]	= 0;
  
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[0] = 0x00802110 | (BusSpeed << 16);  //see Fig.7-12 of OHCI 1.1/ bdu091106
      temp = 0xFFC0FFFF | (target_id << 16);
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[1]	= temp;	        
      
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[2]	= 0xF0100008;	//Dest_addr=FFFFF0100008
      dpr_ptr->AT_REQ_CTX_A.ohci_more_immed.payload[3]	= 0x00080000;	//IEEE-1394 Data_Length=8
      
      //output last
      dpr_ptr->AT_REQ_CTX_A.ohci_last.desc_cmd_word	= 0x100C0008;	//Output_last cmd/key
      //dpr_ptr->AT_REQ_CTX_A.ohci_last.desc_reqCount	= 8;		//OHCI Immediate data length
        
      //prepare the data address
      WRITE32(DRP_RESERVED_SPACE2, SWAP_FOUR(0xFFC00000 | (phy_id << 16)));
      WRITE32(DRP_RESERVED_SPACE2 + 4, SWAP_FOUR((DPR_BASE_ADDRESS + DPR_AT_SCSI_ORB)));  //0x41200
      
      dpr_ptr->AT_REQ_CTX_A.ohci_last.data_address = (UINT32)(DRP_RESERVED_SPACE2 & 0x7FFFFFFF);
      dpr_ptr->AT_REQ_CTX_A.ohci_last.branch_address = 0;
      //dpr_ptr->AT_REQ_CTX_A.ohci_last.xferStatus = 0;
      dpr_ptr->AT_REQ_CTX_A.ohci_last.timeStamp = 0;

      dpr_ptr->AT_REQ_CTX_B.ohci_last.branch_address = ((UINT32)&dpr_ptr->AT_REQ_CTX_A & 0x7FFFFFF0) | 0x3;
      dpr_ptr->AT_REQ_CTX_A.ohci_last.branch_address = 0; //no link to another context
      break;

    case context_b:
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.desc_cmd_word = 0x02000010;	//Output_more_immediate cmd/key
      //dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.desc_reqCount = 16;		//OHCI Immediate data length
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.reserved[0]	= 0;
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.reserved[1]	= 0;
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.reserved[2]	= 0;
  
      //temp = 0xFFC01110 | (phy_id << 16);
      //dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[0]	= temp;		//Dest_ID=FFC0,Label=4,Tcode=1
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[0] = 0x00802110 | (BusSpeed << 16);  //see Fig.7-12 of OHCI 1.1/ bdu091106
      temp = 0xFFC0FFFF | (target_id << 16);
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[1]	= temp;	        
      
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[2]	= 0xF0100008;	//Dest_addr=FFFFF0100008
      dpr_ptr->AT_REQ_CTX_B.ohci_more_immed.payload[3]	= 0x00080000;	//IEEE-1394 Data_Length=8
      
      //output last
      dpr_ptr->AT_REQ_CTX_B.ohci_last.desc_cmd_word	= 0x100C0008;	//Output_last cmd/key
      //dpr_ptr->AT_REQ_CTX_B.ohci_last.desc_reqCount	= 8;		//OHCI Immediate data length
      
      
      //prepare the data address
      WRITE32(DRP_RESERVED_SPACE2, SWAP_FOUR(0xFFC00000 | (phy_id << 16)));
      WRITE32(DRP_RESERVED_SPACE2 + 4, SWAP_FOUR((DPR_BASE_ADDRESS + DPR_AT_SCSI_ORB)));  //0x41200
      
      dpr_ptr->AT_REQ_CTX_B.ohci_last.data_address = (UINT32)(DRP_RESERVED_SPACE2 & 0x7FFFFFFF);
      dpr_ptr->AT_REQ_CTX_B.ohci_last.branch_address = 0;
      //dpr_ptr->AT_REQ_CTX_B.ohci_last.xferStatus = 0;
      dpr_ptr->AT_REQ_CTX_B.ohci_last.timeStamp = 0;

      dpr_ptr->AT_REQ_CTX_A.ohci_last.branch_address = ((UINT32)&dpr_ptr->AT_REQ_CTX_B & 0x7FFFFFF0) | 0x3;
      dpr_ptr->AT_REQ_CTX_B.ohci_last.branch_address = 0; //no link to another context
      break;

    default:
      printf("wrong context space\n");
      return 1;
    }
      
  //set the wake bit
  WRITE32(Base_Address + OHCI1394_AsReqTrContextControlSet, 0x1000);

  return 0;
}



// reset the link
void link_soft_reset( struct ohci_link_regs_t *link_ptr) 
{
  unsigned int i;

  WRITE32(Base_Address + OHCI1394_HCControlSet, 0x00010000);

  for (i =0; i < LINK_LOOP_COUNT; i++) {

    if (!(READ32(Base_Address + OHCI1394_HCControlSet) & 0x00010000)) 
      return;

#ifdef _DEBUG
    printf("link_soft_reset: sleeping %d\n", i);
#endif

    usleep(10000);
  }

#ifdef _DEBUG
  printf("link_soft_reset: timed out %d\n", i);
#endif

  return;
}



/* Function to read/write PHY registers
 * 
 * According to OHCI 1.1, p.54, the PHY control register:
 *
 *   31  | 30-28 | 27-24 | 23-16 | 15 | 14 | 13-12 | 11-8   | 7-0 
 * ----------------------------------------------------------------
 * rdDone  resv    rdAddr rdData   R     W   resv   regAddr  wrData
 *
 */
UINT8 read_phy_reg( struct ohci_link_regs_t *link_ptr, int reg)
{
  UINT32 i, temp;

  //  	printf("PHY_Control offset is 0x%08x\n", &link_ptr->PHY_Control);
  WRITE32(Base_Address + OHCI1394_PhyControl, (reg << 8) | 0x00008000);

  for (i = 0; i < MAX_PHY_LOOP; i++) {
    if ( READ32(Base_Address + OHCI1394_PhyControl) & 0x80000000 )
      break;
    usleep(1000);
  }

  temp = READ32(Base_Address + OHCI1394_PhyControl);

#ifdef _DEBUG
  if (i >= MAX_PHY_LOOP)
    printf("read_phy_reg timed out: reg#=0x%02x PhyCtrlReg=0x%08lx\n", reg, temp);
#endif

  return (temp & 0x00ff0000) >> 16;

}


// Write PHY Reg, see previous comments
void write_phy_reg( struct ohci_link_regs_t *link_ptr, int reg, int data )
{

  UINT32 i, temp;

  WRITE32(Base_Address + OHCI1394_PhyControl, (reg << 8) | data | 0x00004000);

  for (i = 0; i < MAX_PHY_LOOP; i++) {
    temp = READ32(Base_Address + OHCI1394_PhyControl);
    if (!(temp & 0x00004000))
      break;

    usleep(1000);
  }

#ifdef _DEBUG
  if (i >= MAX_PHY_LOOP)
    printf("write_phy_reg timed out: reg#=0x%02x PhyCtrlReg=0x%08lx\n", reg, temp);
#endif

  return;

}



//******************************* Initialize AR Response Context *******************************
void init_AR_resp_ctx( struct ohci_link_regs_t *link_ptr ) 
{
  struct dual_port_RAM_t *dpr_ptr;
  UINT32 temp;
  int i=0;
  
  // first make sure the context is inactive
  WRITE32(Base_Address + OHCI1394_AsRspRcvContextControlClear, 0x00008000);	// clear RUN bit
  while (READ32(Base_Address + OHCI1394_AsRspRcvContextControlClear) & 0x400) 
    {
      i++;
      if (i > 5000) 
	{
#ifdef _DEBUG
	  printf("init_AR_resp_ctx: Context never went inactive\n");
#endif
	  return;
	}
      usleep(1000);
    }

  temp = READ32(Base_Address + OHCI1394_AsRspRcvContextControlClear);

#ifdef _DEBUG
  printf("init_AR_resp_ctx: Context Ctrl Reg = 0x%08lx\n", temp);
#endif

  // setup the INPUT_MORE context descriptors
  dpr_ptr = (struct dual_port_RAM_t*) DPR_BASE_ADDRESS;

  dpr_ptr->AR_RSP_CTX_A.desc_cmd_word = 0x280C0040;
  dpr_ptr->AR_RSP_CTX_A.dataAddress = (unsigned int) dpr_ptr->reserved2;
  dpr_ptr->AR_RSP_CTX_A.branchAddress = 0;
  dpr_ptr->AR_RSP_CTX_A.resCount = 0; //was 2048;
  
  dpr_ptr->AR_RSP_CTX_B.desc_cmd_word = 0x280C0040;
  dpr_ptr->AR_RSP_CTX_B.dataAddress = (unsigned int) dpr_ptr->reserved2;
  dpr_ptr->AR_RSP_CTX_B.branchAddress = 0;
  dpr_ptr->AR_RSP_CTX_B.resCount = 0; //was 2048;
  
  temp = (unsigned int) Base_Address + OHCI1394_AsRspRcvCommandPtr;
#ifdef _DEBUG
    printf("init_AR_resp_ctx: Command_Ptr reg addr is 0x%08lx\n", temp);
#endif
  
  temp = (unsigned int) &(dpr_ptr->AR_RSP_CTX_A) + 1;
  WRITE32(Base_Address + OHCI1394_AsRspRcvCommandPtr, temp);
#ifdef _DEBUG
    printf("init_AR_resp_ctx: Command_Ptr loaded with 0x%08lx\n", temp);
#endif
  
 
  WRITE32(Base_Address + OHCI1394_AsRspRcvContextControlSet, 0x00008000);	// set RUN bit
  
  usleep(1000);
  temp = READ32(Base_Address + OHCI1394_AsRspRcvContextControlSet);
#ifdef _DEBUG
    printf("init_AR_resp_ctx: Context Ctrl Reg after run = 0x%08lx\n", temp);
#endif
  
  for (i =0; i < LINK_LOOP_COUNT; i++)
    {
      if (READ32(Base_Address + OHCI1394_AsRspRcvContextControlSet) & 0x400) 
	{
#ifdef _DEBUG
	    printf("init_AR_resp_ctx: Context active\n");
#endif	    
	  return;
	}

#ifdef _DEBUG
	printf("init_AR_resp_ctx: sleeping %d\n", i);
#endif

      usleep(10000);
    }

#ifdef _DEBUG
  printf("init_AR_resp_ctx: Context never went active\n");
#endif
  
  return;
}


// Get PHY self ID generation. Every time bus reset, the selfID generation 
// is increased by 1.
//
// return: self ID generation 
int get_selfID_generation()
{
  UINT32 temp;

  temp = READ32(Base_Address + OHCI1394_SelfIDCount);
  
  return (int)((temp & 0x00FF0000) >> 16);
}

// get PHY self ID size
int get_selfID_size()
{
  UINT32 temp;

  temp = READ32(Base_Address + OHCI1394_SelfIDCount);

  return (int)((temp & 0x7FC) >> 2);
}



void send_PHY_config(int phy_id) 
{
  struct dual_port_RAM_t *dpr_ptr;
  int i = 0;
  UINT32 temp;
  UINT32 config_lword;

  // first make sure the context is inactive
  WRITE32(Base_Address + OHCI1394_AsReqTrContextControlClear, 0x00008000);

  // format the phy config packet lword
  config_lword = 0x00800000 | (phy_id << 24);

  // setup the OUTPUT_LAST_IMMEDIATE context descriptor
  dpr_ptr = (struct dual_port_RAM_t*)(0x80000000 | DPR_BASE_ADDRESS);

  dpr_ptr->AT_PHY_CFG.desc_cmd_word	= 0x120C0008;
  dpr_ptr->AT_PHY_CFG.reserved		= 0;
  dpr_ptr->AT_PHY_CFG.branch_address	= 0;
  dpr_ptr->AT_PHY_CFG.timeStamp		= 0;
  dpr_ptr->AT_PHY_CFG.payload[0]	= config_lword;
  dpr_ptr->AT_PHY_CFG.payload[1]	= ~config_lword;
  dpr_ptr->AT_PHY_CFG.payload[2]	= 0;
  dpr_ptr->AT_PHY_CFG.payload[3]	= 0;


  temp = (unsigned int) &(dpr_ptr->AT_PHY_CFG) + 2;
  WRITE32(Base_Address + OHCI1394_AsReqTrCommandPtr, temp);

  WRITE32(Base_Address + OHCI1394_AsReqTrContextControlSet, 0x00008000);   //set RUN bit

  for (i =0; i < 5; i++) {
    if (READ32(Base_Address + OHCI1394_AsReqTrContextControlSet) & 0x400) 
      {
#ifdef _DEBUG
	printf("send_PHY_config: AT Request Context active\n");
#endif
	return;
      }

    usleep(1000000);
  }

  return;
}
