/*
//#**************************************************************************
//#**************************************************************************
//#**************************************************************************
//  login.h
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

#include "link_driver.h"
#include "typedefs.h"


typedef enum{
  Calculex_Controller = 0,
  Calculex_Cartridge,
  Remote_Cartridge,
  Calculex_Mid_Link,
  FW_Analyzer,
  Unknown_Device  //last one
}NODE_TYPE;

typedef struct
{
  NODE_TYPE type;
  int node_id;
  int speed;
  int p0;
  int p1;
  int p2;
  int link_active;
  int contender;
}PHY_NODE;
  

typedef struct
{
  UINT32*         lpInBuffer;             // pointer to buffer to supply input data
  UINT32          nInBufferSize;          // size of input buffer
  UINT32*         lpOutBuffer;            // pointer to buffer to receive output data
  UINT32          nOutBufferSize;         // size of output buffer
  UINT32*         BytesReturned;          // pointer to variable to receive output byte count
  UINT32          direction;              // only used during DMA
}IoControlStruct;

typedef enum{
  Speed_100 = 0,
  Speed_200,
  Speed_400,
  Speed_800
}BUS_SPEED;

//
//--- ioctl() function codes ------------------------------------------------
//
#define OHCI_FUNC(x) ((('F' << 24) | ('C' << 16)) | x)

typedef enum {
  CX_IOCTL_RETURN_BOARD_INFORMATION     = OHCI_FUNC(0),
  CX_IOCTL_BASE_ADDRESSES_GET           = OHCI_FUNC(10),
  CX_IOCTL_DMA_ONE_BUFFER               = OHCI_FUNC(11),
  CX_IOCTL_PCI_READ_WRITE               = OHCI_FUNC(12),
} OHCI_COMMAND_CODES;

//
//--------------- PROTOTYPES ----------------------
//
UINT32 get_ohci_base_address();
int initialize_ohci(struct ohci_link_regs_t *link_ptr);
int get_node_map(PHY_NODE *node);
int set_bus_speed(BUS_SPEED speed);
int initialize_scsi_orb(int node_id);

int force_root(struct ohci_link_regs_t *link_ptr);


UINT8 read_phy_reg( struct ohci_link_regs_t *link_ptr, int reg );
void write_phy_reg( struct ohci_link_regs_t *link_ptr, int reg, int data );
void link_soft_reset( struct ohci_link_regs_t *link_ptr );
void init_AR_resp_ctx( struct ohci_link_regs_t *link_ptr );

void send_login_orb(struct ohci_link_regs_t *link_ptr, int root_id, int target_id );
int  read_blocks(struct ohci_link_regs_t *link_ptr, int phy_id, CONTEXT context, UINT32 block, int num_blocks );
int  write_blocks(struct ohci_link_regs_t *link_ptr, int phy_id, CONTEXT context, UINT32 block, int num_blocks );


int get_selfID_generation();
int get_selfID_size();

#define SWAP_FOUR(x)    (   ( ((x) & 0x000000ff) << 24 ) | \
                                ( ((x) & 0x0000ff00) <<  8 ) | \
                                ( ((x) & 0x00ff0000) >>  8 ) | \
                                ( ((x) & 0xff000000) >> 24 )    )


//macros to access oxford directly. bdu091906
#define READ32(address) *(volatile unsigned long *)(address)
#define WRITE32(address, data) *(volatile unsigned long *)(address)=data

