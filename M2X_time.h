//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: M2X_time.h
//    Version: 1.0
//     Author: dmccoy
//
//            MONSSTR 2100
//
//            This module provides all time functionalities for M2100.
//
//    Revisions:   
//
//=====================================
//		Revision 1:
//		Author: bdu
//		Date  : 07/05/04
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _M2X_TIME_H_
#define _M2X_TIME_H_


#include <sys/time.h>
#include "M23_Typedefs.h"
#include "M2X_channel.h"
#include "M2X_Const.h"


// Definitions:
//
//      GSB     Grouped Straight Binary Time.  This is normal 
//              Day/Hour/Min/Second/Fractional Second format.
//
//      MicroSecTimeTag   48-bit timestamp used in record headers.
//
//      ASCII   The string version of GSB Time
//

typedef struct {
    UINT32  Years;
    UINT32  Days;
    UINT32  Hours;
    UINT32  Minutes;
    UINT32  Seconds;
    UINT32  Microseconds;
} GSBTime;


typedef struct {
    UINT32  Upper;
    UINT32  Lower;
} MicroSecTimeTag;


typedef struct {
	UINT32 Lower;
	UINT16 Upper;
} CH10TIMETAG;


extern INT64 RefTime;

//conversions
void Time_TimeTagToGSB( MicroSecTimeTag const tag, GSBTime *p_gsb );

void Time_GSBToTimeTag( GSBTime const *p_gsb, MicroSecTimeTag *p_tag );


void Time_TimeTagToASCII( MicroSecTimeTag const tag, char *time_str );

void Time_ASCIIToTimeTag( char const *time_str, MicroSecTimeTag *p_tag );


void Time_GSBToASCII( GSBTime const *p_gsb, char *time_str );

void Time_ASCIIToGSB( char const *time_str, GSBTime *p_gsb );



// Conversion method setting (applies to all functions above):
//
//      001 00:00:00.000 is the traditional converson of a timestamp of 0 milliseconds.
//  Older DCRsi's (and MONSSTR, through version 2.4) do this conversion using day 0 as
//  a base instead of day 1.  The following function allows this setting to be reverted 
//  to the older (incorrect) method.
//
void Time_SetStartDay( UINT8 day );
//
//  day         0 if specified, 1 otherwise. bdu070504  
//
//  return      No need to return any value. bdu070504
//


// Check the time for out-of-range values
//
BOOL Time_GSBTimeIsValid( GSBTime const *p_gsb );




// Normalize time (used after adding two times )
//
void Time_Normalize( GSBTime * p_time );
// 
//  p_time      pointer to GSB time struct (d/h/m/s/ms)
//



// new functions added, some are from realtime_clock module, some are new. bdu070504
int M2X_rtc_Initialize();

/**** real time ****/
int M2X_Get_System_Time(MicroSecTimeTag *tm);
int M2X_Set_System_Time(MicroSecTimeTag *tm);

/**** IRIG time ****/
int M2X_Get_IRIGTime_From_System(GSBTime *p_gsb_time, MicroSecTimeTag *p_relative_time);
int M2X_Set_IRIGTime_To_System(GSBTime *p_gsb_time);

/* function to get 32-bit-lower and 16-bit-upper time tag */
void get_48bit_timetag(struct timeval tm, CH10TIMETAG *tt);

/* function to set time from cartridge */
int M2X_Set_System_Time_from_Cartridge();


#endif  // _M2X_TIME_H_
