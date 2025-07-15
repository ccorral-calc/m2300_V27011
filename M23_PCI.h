
/****************************************************************************************
 *
 *    Copyright (C) 2004 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and 
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
 *    for any purpose except as specifically authorized in writing.
 *
 *    File: M23_Controller.c
 *
 *    Version: 1.0
 *    Author: pcarrion
 *
 *    MONSSTR 2300
 *
 *    Defines the parameters that will be used in communicating with the M2300 Controller Board
 *       
 *
 *    Revisions:  
 ******************************************************************************************/




/* configuration offsets */
#define VENDOR_ID_OFFSET	    0x00
#define DEVICE_ID_OFFSET	    0x02
#define COMMAND_OFFSET		    0x04
#define STATUS_OFFSET		    0x06
#define REVISION_OFFSET		    0x08
#define PROG_IF_OFFSET		    0x09
#define SUB_CLASS_OFFSET	    0x0a
#define BASE_CLASS_OFFSET	    0x0b
#define CACHE_LINE_OFFSET	    0x0c
#define LATENCY_TIMER_OFFSET	0x0d
#define HEADER_TYPE_OFFSET	    0x0e
#define BIST_OFFSET		        0x0f
#define REGION0_BASE_OFFSET	    0x10
#define REGION1_BASE_OFFSET	    0x14
#define REGION2_BASE_OFFSET	    0x18
#define REGION3_BASE_OFFSET	    0x1c
#define REGION4_BASE_OFFSET	    0x20
#define REGION5_BASE_OFFSET	    0x24
#define CARDBUS_CISPTR_OFFSET   0x28
#define SUB_VENDOR_ID_OFFSET    0x2c
#define SUB_DEVICE_ID_OFFSET    0x2e
#define EXP_ROM_OFFSET		    0x30
#define INT_LINE_OFFSET		    0x3c
#define INT_PIN_OFFSET		    0x3d
#define MIN_GNT_OFFSET		    0x3e
#define MAX_LAT_OFFSET		    0x3f


/*******************************************************************************
*
* Type 0 PCI Configuration Space Header
*
*/

typedef struct {
    unsigned short  vendor_id;
    unsigned short  device_id;
    unsigned short  command;
    unsigned short  status;
    unsigned char   revision_id;
    unsigned char   prog_if;
    unsigned char   sub_class;
    unsigned char   base_class;
    unsigned char   cache_line_size;
    unsigned char   latency_timer;
    unsigned char   header_type;
    unsigned char   bist;
    unsigned long   pcibase_addr0;
    unsigned long   pcibase_addr1;
    unsigned long   pcibase_addr2;
    unsigned long   pcibase_addr3;
    unsigned long   pcibase_addr4;
    unsigned long   pcibase_addr5;
    unsigned long   cardbus_cis_ptr;
    unsigned short  sub_vendor_id;
    unsigned short  sub_device_id;
    unsigned long   pcibase_exp_rom;
    unsigned long   reserved2[2];
    unsigned char   int_line;
    unsigned char   int_pin;
    unsigned char   min_gnt;
    unsigned char   max_lat;
 } PCI_CONFIG_SPACE_0;

