//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: M2X_time.c
//    Version: 1.0
//     Author: dmccoy
//
//            MONSSTR 2100/2300V1
//
//            This module provides all time functionalities for M2100.
//
//    Revisions:   
//
//=====================================
//		Revision 1:
//		Author: bdu
//		Date  : 07/05/04
//		Work  : Modified previous code, combined M2X_time.c and realtime_clock.c,
//				extra functions are added.
//
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "M2X_time.h"
#include "M2X_FireWire_io.h"
#include "M2X_cmd_ascii.h"
#include "M23_Controller.h"


#define YRS_PER_CENTURY     100
#define DAYS_PER_YR         365
#define HRS_PER_DAY         24
#define MINS_PER_HR         60
#define SECS_PER_MIN        60
#define MS_PER_SEC          1000
#define MICS_PER_SEC        1000000

#define SYNC_TO_HWCLOCK		"hwclock -w -u"

void Time_ConvertToGSB( char const *time_str, GSBTime *p_gsb );

INT64 RefTime;

static UINT8 START_DAY = 1;

static struct timeval SystemTime;       // SystemTime is used for IRIG time emulation


#if 0 /*uncomment for 2100*/
static BT1553_TIME CondorTime;			// CondorTime is used to keep track of Condor time counter
#endif

static UINT64 RelativeTime;             // same as SystemTime, but it starts at 0
static UINT64 MicrosecondTimeBase;      // this is remembered from startup to compute the Relative Time

#define TEN_MHZ		1


static int days_in_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31};


/***************************************************************************************************************************************************
 *  Name :    M23_SetSystemTime()
 *
 *  Purpose : This function will Initialize the Time based on time from the Cartridge or 1553 MT-RT
 *
 *
 *  Returns : SUCCESS   Status Returned
 *            FAIL      Status unavailable
 *
 ***************************************************************************************************************************************************/

void M23_SetSystemTime(char *time)
{
    int     diffUsec;
    int     debug;
    UINT32  Upper;
    UINT32  CSR;

    GSBTime JamTime;

    M23_GetDebugValue(&debug);

    diffUsec = 1500000;

    Time_ConvertToGSB( time,&JamTime);

    if( (JamTime.Seconds + 1) > 59)
    {
        JamTime.Seconds = 1; //This adds 2 seconds to the Time read

        if( (JamTime.Minutes + 1) > 59)
        {
            JamTime.Minutes = 0;

            if( (JamTime.Hours + 1) > 23)
            {
                JamTime.Hours = 0;

                if( (JamTime.Days + 1) > 365)
                {
                    JamTime.Days = 1;
                }
                else
                {
                    JamTime.Days++;
                }

            }
            else
            {
                JamTime.Hours++;
            }

        }
        else
        {
            JamTime.Minutes++;
        }

    }
    else
    {
        JamTime.Seconds += 1;
        if( (JamTime.Seconds + 1) > 59)
        {
            JamTime.Seconds = 0;

            if( (JamTime.Minutes + 1) > 59)
            {
                JamTime.Minutes = 0;

                if( (JamTime.Hours + 1) > 23)
                {
                    JamTime.Hours = 0;

                    if( (JamTime.Days + 1) > 365)
                    {
                        JamTime.Days = 1;
                    }
                    else
                    {
                        JamTime.Days++;
                    }

                }
                else
                {
                    JamTime.Hours++;
                }

            }
            else
            {
                JamTime.Minutes++;
            }
        }
        else
        {
            JamTime.Seconds++;
        }
    }

    RefTime += (diffUsec * 10);
    RefTime = RefTime - 5000000L; //Per Stuarts Request
    M23_WriteControllerCSR(CONTROLLER_JAM_TIME_REL_LOW,(UINT32)RefTime);

    Upper = (UINT32)(RefTime >> 32);
    Upper |= TIME_JAM_BUSY_BIT;
    M23_WriteControllerCSR(CONTROLLER_JAM_TIME_REL_HIGH,Upper);

    if(debug)
        printf("Jam Rel Time  %lld, time %03d-%02d:%02d:%02d\r\n",RefTime,JamTime.Days,JamTime.Hours,JamTime.Minutes,JamTime.Seconds);

    SetTime(JamTime);

}



/***
 * This function will set the system starting day. The start day is 0 
 
* if specified, otherwise it's 1
 *
 */
void Time_SetStartDay( UINT8 day )
{
	START_DAY = (day)?1:0;
}


/***
 * This function will validate the GSB format
 */
BOOL Time_GSBTimeIsValid( GSBTime const *p_gsb )
{

    if ( START_DAY == 1 ) {

        if ( p_gsb->Days == 0 )
            return FALSE;
    }

    if ( ( p_gsb->Days          > DAYS_PER_YR + 1) ||  // +1 to account for leap years
         ( p_gsb->Hours         >= HRS_PER_DAY   ) ||
         ( p_gsb->Minutes       >= MINS_PER_HR   ) ||
         ( p_gsb->Seconds       >= SECS_PER_MIN  ) ||
         ( p_gsb->Microseconds  >= MICS_PER_SEC  ) )
    {
        return FALSE;
    }


    return TRUE;
}


/***
 * This function will convert 48-bit time tag to GSB
 */
void Time_TimeTagToGSB( MicroSecTimeTag const tag, GSBTime *p_gsb )
{
    UINT32 d, h, m, s, us;

    if ( !p_gsb )
        return;
    

    d = (UINT32)( (((UINT64)tag.Upper << 32) + tag.Lower) 
        / MICS_PER_SEC / SECS_PER_MIN / MINS_PER_HR / HRS_PER_DAY );

    s = (UINT32)( (((UINT64)tag.Upper << 32) + tag.Lower) / MICS_PER_SEC 
        - d * SECS_PER_MIN * MINS_PER_HR * HRS_PER_DAY );

    us = (UINT32)( (((UINT64)tag.Upper << 32) + tag.Lower) 
        - ((((UINT64)tag.Upper << 32) + tag.Lower) / MICS_PER_SEC) * MICS_PER_SEC );


    h = s / SECS_PER_MIN / MINS_PER_HR;

    s -= h * SECS_PER_MIN * MINS_PER_HR;


    m = s / SECS_PER_MIN;

    s -= m * SECS_PER_MIN;
 
    if(d != 0)
      d++; //bdu070604

    p_gsb->Days = d + START_DAY;
 

    p_gsb->Hours = h;
    p_gsb->Minutes = m;
    p_gsb->Seconds = s;
    p_gsb->Microseconds = us;

	return;
}
    

/***
 * This function will convert GSB to 48-bit time tag
 */
void Time_GSBToTimeTag( GSBTime const *p_gsb, MicroSecTimeTag *p_tag )
{
    UINT32 days, s, mics;

    if ( !p_gsb || !p_tag)
        return;


    // subtract the start day
    //
    days = p_gsb->Days;
    if ( days >= START_DAY )    // avoid underflow
        days -= START_DAY;



    s = ((( days ) * HRS_PER_DAY 
                 + p_gsb->Hours ) * MINS_PER_HR 
                 + p_gsb->Minutes ) * SECS_PER_MIN
                 + p_gsb->Seconds;
    mics = p_gsb->Microseconds;
    

    p_tag->Upper = (UINT32)(((UINT64) s * MICS_PER_SEC) >> 32);
    p_tag->Lower = s * MICS_PER_SEC + mics;

	return;
}


/***
 * This function will convert an ASCII string that contains 
 * ddd hh:mm:ss.us to 48-bit time tag.
 */
void Time_ASCIIToTimeTag( char const *time_str, MicroSecTimeTag *p_tag )
{
    GSBTime gsbt = { 0, 0, 0, 0, 0,0 };

    if ( !time_str || !p_tag )
    {
	//printf("Time_ASCIIToTimeTag: invalid time string or null pointer to time tag.\r\n");
        return;
    }

    Time_ASCIIToGSB( time_str, &gsbt );
    Time_GSBToTimeTag( &gsbt, p_tag );

    return;
}


/***
 * This function will convert 48-bit time tag to a readable string
 */
void Time_TimeTagToASCII( MicroSecTimeTag const tag, char *time_str )
{
    GSBTime gsbt = { 0, 0, 0, 0, 0,0 };

    if ( !time_str )
        return;

    Time_TimeTagToGSB( tag, &gsbt );
    Time_GSBToASCII( &gsbt, time_str );

	return;
}

/***
 * This function will interpret the GSB value and convert it
 * to ASCII string
 */
void Time_GSBToASCII( GSBTime const *p_gsb, char *time_str )
{
    if ( !time_str || !p_gsb )
        return;

    sprintf( time_str, "%03lu %02lu:%02lu:%02lu.%06lu", 
                            p_gsb->Days,
                            p_gsb->Hours,
                            p_gsb->Minutes,
                            p_gsb->Seconds,
                            p_gsb->Microseconds
                        );

	return;
}



/***
 * This function will convert ASCII string formatted as ddd hh:mm:ss.us
 * to GSB format.
 *
 * e.g.:   "033 14:00:00.000123" is February 2nd, 2:00 pm. +123 microseconds
 *
 * trailing values (after 'ddd') may be omitted - they will be zeroed.
 */
void Time_ASCIIToGSB( char const *time_str, GSBTime *p_gsb )
{
    UINT32 d = 0, h = 0, m = 0, s = 0, us = 0;
    int arg_count = -1;

    if ( !time_str || !p_gsb )
	{
		//printf("Time_ASCIIToGSB: invalid time string or null GSB pointer.\r\n");
        return;
	}


    // format is DDD HH:MM:SS.SSSSSS
    //
    //arg_count = sscanf( time_str, "%lu %c %lu:%lu:%lu.%lu", &d, &dash,&h, &m, &s, &us );
    arg_count = sscanf( time_str, "%lu %lu:%lu:%lu.%lu", &d, &h, &m, &s, &us );

    if ( arg_count <= 0 )
	{
		//printf("Time_ASCIIToGSB: invalid time string.\n");
        return;        // error, not even one argument!
	}

    // add trailing zeroes that are implied by the decimal point.
    //  (i.e.   .45 seconds is 450000 
    //
    while ( (us * 10) < 1000000 ) {
        us *= 10;
        if ( us == 0 )
            break;
    }

    p_gsb->Days = d;
    p_gsb->Hours = h;
    p_gsb->Minutes = m;
    p_gsb->Seconds = s;
    p_gsb->Microseconds = us;

    // check the time
    //
    if ( !Time_GSBTimeIsValid( p_gsb ) )
	{
		//printf("Time_ASCIIToGSB: cannot convert time string to a valid GSB time.\n\n");
        return;
	}


    return;
}


/***
 * This function will convert ASCII string formatted as ddd hh:mm:ss.ms
 * to GSB format.
 *
 * e.g.:   "033 14:00:00.123" is February 2nd, 2:00 pm. +123 milliseconds
 *
 */
void Time_ConvertToGSB( char const *time_str, GSBTime *p_gsb )
{
    UINT32 d = 0, h = 0, m = 0, s = 0, ms = 0;
    int arg_count = -1;

    if ( !time_str || !p_gsb )
	{
		//printf("Time_ASCIIToGSB: invalid time string or null GSB pointer.\r\n");
        return;
	}


    // format is DDD HH:MM:SS.SSS
    //
    arg_count = sscanf( time_str, "%lu %lu:%lu:%lu.%lu", &d, &h, &m, &s, &ms );

    p_gsb->Days = d;
    p_gsb->Hours = h;
    p_gsb->Minutes = m;
    p_gsb->Seconds = s;
    p_gsb->Microseconds = ms * 1000;

    // check the time
    //
    if ( !Time_GSBTimeIsValid( p_gsb ) )
    {
        //printf("Time_ASCIIToGSB: cannot convert time string to a valid GSB time.\n\n");
        return;
    }


    return;
}



/***
 * Normalize the time, i.e. max 60 secs per min, 24 hrs per day, etc.
 */
void Time_Normalize( GSBTime * p_time )
{
    if ( p_time->Microseconds >= MICS_PER_SEC ) {
        p_time->Microseconds -= MICS_PER_SEC;
        p_time->Seconds++;
    }

    if ( p_time->Seconds >= SECS_PER_MIN ) {
        p_time->Seconds -= SECS_PER_MIN;
        p_time->Minutes++;
    }


    if ( p_time->Minutes >= MINS_PER_HR ) {
        p_time->Minutes -= MINS_PER_HR;
        p_time->Hours++;
    }


    if ( p_time->Hours >= HRS_PER_DAY ) {
        p_time->Hours -= HRS_PER_DAY;
        p_time->Days++;
    }
}


/***
 * Recorder BCD to GSB
 */
void RecordBCD_To_GSB( UINT32 Upper, UINT32 Lower,GSBTime * p_time )
{
    /*Read the Upper Time*/
    p_time->Days = 100 * ((Upper>>8) & 0x000F);
    p_time->Days += 10*  ((Upper >> 4) & 0x000F);
    p_time->Days +=  (Upper & 0x000F);

    p_time->Microseconds = 0;


    p_time->Hours = 10 * ((Lower >> 28) & 0x000F);
    p_time->Hours +=  ((Lower>>24)  & 0x000F);

    p_time->Minutes =  10 * ( (Lower >> 20) & 0x000F);
    p_time->Minutes +=  ((Lower >> 16) & 0x000F);

    p_time->Seconds =  10 * ((Lower >> 12) & 0x000F);
    p_time->Seconds +=  ((Lower >> 8)& 0x000F);

}


//##################################################################################//
// new functions added below, some are from realtime_clock module, some are new.    //
// bdu070504                                                                        //
//##################################################################################//


/*
//#**************************************************************************
//
//                             M2X_rtc_Initialize
//
// Parameters
// ----------
//   None
//  
//
// Return Value
// ------------
//   This function returns an error code. A value of zero indicates success.
//
// Remarks
// -------
//   This purpose of this function is to initialize the time module in M2100.
//	It will reset the starting time and get the system time.
//
//
//#**************************************************************************
*/
int M2X_rtc_Initialize()
{
	/* get the system time */
	gettimeofday(&SystemTime, NULL);
	MicrosecondTimeBase = ((UINT64) SystemTime.tv_sec * 1000000) + ((UINT64) SystemTime.tv_usec);

#if 0 /*uncomment for 2100*/
	/* reset Condor time */
	CondorTime.microseconds = 0;
	CondorTime.topuseconds = 0;
#endif

	/* reset relative time */
	RelativeTime = 0;

	return M23_SUCCESS;
}



void Time_GetDayMonthYear(int julian,int *day,int *month,int *year)
{
    int Month = 1;
    int days = julian;
    int i = 0;

    M23_GetYear(year);

    if( (*year % 4) == 0 )
    {
        days_in_month[1] = 29;
    }
    else
    {
        days_in_month[1] = 28;
    }

    while(1)
    {
        days -= days_in_month[i];
        if(days <= 0)
        {
            days += days_in_month[i];
            break;
        } 
        else
        {
            Month++;
            i++;
        }
    }

    *month = Month;
    *day   = days;
}

