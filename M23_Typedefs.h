/****************************************************************************************
 *
 *    Copyright (C) 2004 CALCULEX Inc.
 *
 *    The material herein contains information that is proprietary and 
 *    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
 *    for any purpose except as specifically authorized in writing.
 *
 *    File: M23_Typedefs.h
 *
 *    Version: 1.0
 *    Author: dmccoy
 *
 *    MONSSTR 2300
 *
 *    Defines common types for M2300 modules. This module was taken from the DMIC/FMIC systems
 *       
 *
 *    Revisions:  
 ******************************************************************************************/
#ifndef _TYPEDEFS_H_
#define _TYPEDEFS_H_




#define __IC960  1

/*suetemp
#define IP_DEBUG 1
suetemp*/


/**#define DEBUG_VERBOSE 1**/


#define WIDTH16
/*
#define WIDTH8
*/

/*
#define LOOP_TESTS_ENABLED
*/

#ifndef M2300

typedef unsigned char   UINT8;
typedef unsigned short  UINT16;
typedef unsigned long   UINT32;

typedef char    INT8;
typedef short   INT16;
typedef long    INT32;


#ifdef _WIN32
typedef unsigned __int64    UINT64;
typedef __int64             INT64;
#else
typedef unsigned long long int   UINT64;
typedef long long int          INT64;

#endif


#define ONE_MEG  (1024*1024)


#define  FALSE  0
#define  TRUE   1
typedef unsigned long BOOL;

#endif /*#ifndef M2300*/


#if 0
typedef enum { 
    FALSE, 
    TRUE 
} BOOL;
#endif


#define ReadMem64( reg )            *(volatile UINT64 *)( reg )
#define WriteMem64( reg, data )     *(volatile UINT64 *)( reg ) = ( (UINT64) data )

#define ReadMem32( reg )            *(volatile UINT32 *)( reg )
#define WriteMem32( reg, data )     *(volatile UINT32 *)( reg ) = ( (UINT32) data )

#define ReadMem16( reg )            *(volatile UINT16 *)( reg )
#define WriteMem16( reg, data )     *(volatile UINT16 *)( reg ) = ( (UINT16) data )

#define ReadMem8( reg )             *(volatile UINT8 *)( reg )
#define WriteMem8( reg, data )      *(volatile UINT8 *)( reg ) = ( (UINT8) data )

#define SWAP_ALL(x) \
      x = (UINT16) ( ((x & 0x8000) >> 15) | ((x & 0x4000) >> 13) | ((x & 0x2000) >> 11) | ((x & 0x1000) >> 9) | \
                     ((x & 0x0800) >> 7)  | ((x & 0x0400) >> 5)  | ((x & 0x0200) >> 3)  | ((x & 0x0100) >> 1) | \
                     ((x & 0x0001) << 15) | ((x & 0x0002) << 13) | ((x & 0x0004) << 11) | ((x & 0x0008) << 9) | \
                    ((x & 0x0010) << 7)  | ((x & 0x0020) << 5)  | ((x & 0x0040) << 3)  | ((x & 0x0080) << 1) )


#define SWAP_FOUR(x)  \
	 x = (((x & 0x000000ff) << 24) | ((x & 0x0000ff00 ) <<  8) |    \
     ((x & 0x00ff0000) >>  8) | ((x & 0xff000000 ) >> 24))
     
#define SWAP_TWO(x)  \
	  x = (UINT16) (((x & 0x000000ff) << 8) | ((x & 0x0000ff00 ) >>  8)) 

#define SWAP_EIGHT(x)  \
        x =(UINT64) \
            ( ((x & 0x00000000000000ffLL) << 56) | ((x & 0x000000000000ff00LL) << 40) | \
              ((x & 0x0000000000ff0000LL) << 24) | ((x & 0x00000000ff000000LL) <<  8) | \
              ((x & 0x000000ff00000000LL) >>  8) | ((x & 0x0000ff0000000000LL) >> 24) | \
              ((x & 0x00ff000000000000LL) >> 40) | ((x & 0xff00000000000000LL) >> 56) )

#define SWP_EIGHT(x)  ( ((x & 0x00000000000000ffLL) << 56) | ((x & 0x000000000000ff00LL) << 40) | \
                     ((x & 0x0000000000ff0000LL) << 24) | ((x & 0x00000000ff000000LL) <<  8) | \
                     ((x & 0x000000ff00000000LL) >>  8) | ((x & 0x0000ff0000000000LL) >> 24) | \
                     ((x & 0x00ff000000000000LL) >> 40) | ((x & 0xff00000000000000LL) >> 56) )

#ifndef NULL
#define NULL    (0)
#endif


#define YRS_PER_CENTURY     100
#define DAYS_PER_YR         365
#define HRS_PER_DAY         24
#define MINS_PER_HR         60
#define SECS_PER_MIN        60
#define EIGHTHSECS_PER_SEC  8
#define MS_PER_SEC          1000
#define MS_PER_EIGHTSEC     125

//#define ONE_GIG             1048576000
#define ONE_GIG             1000000000


/*Define some ASCII Character codes to make code more readable*/

#define BackSpace      0x8
#define LineFeed       0xA
#define CarriageReturn 0xD


#endif  /* _TYPEDEFS_H_ */

