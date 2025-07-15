/*
//#**************************************************************************
//#**************************************************************************
//#**************************************************************************
//  typedefs.h
//
//  Copyright (C) 2005, CALCULEX, Inc.
//
//    The material herein contains information that is proprietary and
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed
//    for any purpose except as specifically authorized in writing.
//
//
//  Revisions:  V1.0
//              Date:   9/07/2006
//              Author: bdu
//              Rev:    Initial release of this module. Support OHCI on Linux
//                              only (kernel 2.6.16)
//
//
//
//#**************************************************************************
//#**************************************************************************
//#**************************************************************************
*/

#ifndef _TYPEDEFS_H_

/*
 * --- Includes--------------------------------------------------------------
 */


typedef unsigned char  UINT8;
typedef unsigned short UINT16;
typedef unsigned long  UINT32;

typedef char    INT8;
typedef short   INT16;
typedef int    INT32;
typedef unsigned long long    UINT64;


typedef int BOOL;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE  1
#endif


#endif // _TYPEDEFS_H_


