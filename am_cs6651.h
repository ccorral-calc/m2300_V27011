/*
//#**************************************************************************
//#**************************************************************************
//#**************************************************************************
//  cs6651.h
//
//  Copyright (C) 2006, CALCULEX, Inc. All rights reserved.
//
//    The material herein contains information that is proprietary and
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed
//    for any purpose except as specifically authorized in writing.
//
//  Definitions for the Amphion CS6651 char module
//
//  Revisions:  V1.0
//              Date:   09/21/06
//              Author: pcarrion
//              Rev:    Initial release of this module. Support Amphion CS6651 on Linux
//                              only (kernel 2.6.9)
//
//
//
//#**************************************************************************
//#**************************************************************************
//#**************************************************************************
*/


#ifndef _CS6651_H_
#define _CS6651_H_

#define CS6651_BASE            0x80001000  //Temp

/*Amphion CS6651 Register Address Map*/
#define CS6651_RESET_CORE              0x0
#define CS6651_INTERRUPT_MASK          0x4
#define CS6651_INTERRUPT_STATUS        0x5
#define CS6651_DIVISOR                 0x11
#define CS6651_DISPLAY_SUBS_CONFIG     0x18
#define CS6651_DISPLAY_SUBS_CONTRL     0x1A
#define CS6651_DISPLAY_ROW_REG         0x1B
#define CS6651_DISPLAY_COL_REG         0x1C
#define CS6651_STALL_MASK              0x20
#define CS6651_STALL_CONTROL           0x22
#define CS6651_STREAM_INFO             0x34

/*Register 0x4 and 0x5*/
#define CS6651_REMOVE_STALL             0x0
#define CS6651_FRAME_DONE              (1<<6)
#define CS6651_PARSER_STALL            (1<<6)
#define CS6651_DISPLAY_DONE            (1<<1)

/*Register 0x11*/
#define CS6651_1US_DIVISOR              0x1B

/*Register 0x18*/
#define CS6651_DISPLAY_MODE             0x04
#define CS6651_EXIT_HOST_DISPLAY        0x00

/*Register 0x1A*/
#define CS6551_MODE_TOP_BOTTOM          0x102000
#define CS6551_MODE_RESUME_TOP_BOTTOM   0x1020E0

/*Register 0x1B*/
#define CS6651_DISPLAY_ROW              0x1E0000
//#define CS6651_DISPLAY_ROW              0x1DE000

/*Register 0x1C*/
#define CS6651_DISPLAY_COLUMN           0x2D0000
//#define CS6651_DISPLAY_COLUMN           0x2CE000

/*Register 0x20*/
#define CS6651_CLEAR_STALL_EOP          0x0
#define CS6651_STALL_BOP                0x4
#define CS6651_STALL_EOP                0x8
#define CS6651_SKIP_BOP                 0x10

/*Register 0x22*/
#define CS6651_STALL_CONTINUE           0x1
#define CS6651_SKIP_TO_NEXT             0x2

/*Register 0x34*/
#define CS6651_I_FRAME                  0x1
#define CS6651_P_FRAME                  0x2

//
//--- ioctl() function codes ------------------------------------------------
//
#define CS6651_FUNC(x) ((('F' << 24) | ('C' << 16)) | x)

typedef enum {
  CS6651_NORMAL                         = CS6651_FUNC(0),
  CS6651_PAUSE                          = CS6651_FUNC(10),
  CS6651_2X                             = CS6651_FUNC(11),
  CS6651_05X                            = CS6651_FUNC(12),
  CS6651_130X                           = CS6651_FUNC(13),
  CS6651_3X                             = CS6651_FUNC(14),
  CS6651_SINGLE_STEP                    = CS6651_FUNC(15),
}OHCI_COMMAND_CODES;



#endif  //_CS6651_H_
