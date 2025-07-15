////////////////////////////////////////////////////////////////////////////////
//
//    Copyright � 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: M23_GPS.c
//    Version: 1.0
//     Author: pc
//
//            MONSSTR 2300 GPS Interface
//
//
//    Revisions:   
//

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include "M23_Controller.h"
#include "M2X_serial.h"
#include "M23_features.h"

// #define _POSIX_SOURCE 1 /* POSIX compliant source */


pthread_t GPSThread;
pthread_t GPSWaitThread;

void M23_ProcessGPS();

static int  GPSPort;
static UINT8 UBLOXBuffer[80];
static UINT8 NMEABuffer[MAX_COMMAND_LENGTH];
static int  CurrentIndex;
static int  GPS_Jammed = 0;
static int  broadcast;

static int       leap_valid = 0;
static int       NeverJammed = 1;

static int       GPS_Init;


typedef struct
{
    char txt[20];
}gps_token;


int GetGPSInit()
{
    return(GPS_Init);
}
void GPS_WaitForJamComplete()
{
    int       config;
    int       record_state;
    int       in_record = 0;
    int       debug;

    UINT32 CSR;

    INT64 RefTime;


    M23_GetDebugValue(&debug);

    if(debug)
        printf("Waiting for Time JAM\r\n");

    M23_RecorderState(&record_state);
    M23_GetConfig(&config);

    if(config == B52_CONFIG)
    {
        if(NeverJammed)
        {
            sleep(1);

            if(record_state == STATUS_RECORD)
            {
                in_record = 1;
                Stop(0);
            }
        }
    }


    while(1)
    {
        /*Check if time has been jammed*/
        CSR = M23_ReadControllerCSR(CONTROLLER_JAM_TIME_REL_HIGH);
        if(CSR & TIME_JAM_BUSY_BIT)
        {
            usleep(500000);
        }
        else
        {
            CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);
            CSR &= ~TIMECODE_RMM_JAM;
            CSR |= TIMECODE_M1553_JAM;
            M23_WriteControllerCSR(CONTROLLER_TIMECODE_ID_CSR,CSR);

            CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_HOST_REL_LOW);
            RefTime = CSR;
            CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_HOST_REL_HIGH);
            RefTime |= (((INT64) CSR) <<32);


            if(debug)
                printf("GPS Jammed %lld\r\n",RefTime);

            GPS_Jammed = 1;

            NeverJammed = 0;

            M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_GPS_NOT_SYNCED);

            if(in_record == 1)
            {
                 Record();
            }

            break;
        }
    }

    if(debug)
        printf("Done Waiting for Time JAM\r\n");
}

int GPSJammed()
{
    return(GPS_Jammed);
}

void StartUARTBroadcast()
{
    broadcast = 1;
}

void StopUARTBroadcast()
{
    broadcast = 0;
}

void SetDontJamGPS()
{
    GPS_Jammed = 1;
}


int UART_RecordData(int length,int chanid)
{
    int i;
    int state;
    int count = 0;
    int readycount = 0;
    int bytes_to_write;
    int debug;
  
    UINT16 *ptmp16;

    UINT32 CSR;

    M23_GetDebugValue(&debug);
    M23_RecorderState(&state);


    length += length % 2;
    if(length < 1000)
    {
        bytes_to_write = length;
    }
    else
    {
        bytes_to_write = 1000;
    }


#if 0
if(debug)
    printf("NMEA %s\r\n",NMEABuffer);
#endif

    ptmp16 = (UINT16 *)NMEABuffer;
    /*Check the Busy Flag, We are sharing this channel with the CONTROLLER UART Channel*/
    CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR);

    while(1)
    {
        CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_CSR);
        if( !(CSR & EVENT_UART_CHANNEL_BUSY) && (EventChannelInUse == 0) )
        {
            EventChannelInUse = 1;
            break;
        }
        else
        {
            usleep(100);
        }

    }

    while(1)
    {
        CSR = M23_ReadControllerCSR(CONTROLLER_CONTROL_CSR);

        /*Check the FIFO Empty Flag*/
        CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR);
        if(CSR & CONTROLLER_EVENT_CHANNEL_FIFO_EMPTY)
        {
            M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_LEN_CSR,length + 4);

            /*Write the CSDW*/
            M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_CSDW,0x0);

            /*Write The Channel ID and The Channel Type*/
            if(broadcast == 1)
            {
                CSR = chanid & ~BROADCAST_MASK; 
            }
            else
            {
                CSR = (chanid | BROADCAST_MASK); 
            }
            CSR |= (CONTROLLER_UART_TYPE << 16);
            M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_ID_CSR,CSR);


            /*Write The Length for use in the IPH*/
            M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR,(UINT16)(length));
            M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR,(UINT16)(length >> 16) );

            /*Write The  Data*/
            for(i = 0; i < bytes_to_write/2; i++)
            {
                M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_WRITE_CSR,*ptmp16);
                ptmp16++;
            }

            CSR = M23_ReadControllerCSR(CONTROLLER_EVENT_CHANNEL_ID_CSR);
            CSR |= CONTROLLER_EVENT_CHANNEL_PKT_READY;
#if 0
if(debug)
    printf("UART - Writing PKY RDY = 0x%x to 0x%x,count = %d\r\n",CSR,CONTROLLER_EVENT_CHANNEL_ID_CSR,readycount);
#endif
            M23_WriteControllerCSR(CONTROLLER_EVENT_CHANNEL_ID_CSR,CSR);

            count += (i*2);

            if( count >= length) /*all the data has been written*/
            {
                break;
            }
            else
            {
                if( (length - count) < 1000)
                {
                    bytes_to_write = length - count;
                }
                else
                {
                    bytes_to_write = 1000;
                }
            }

        }
        else
        {
            readycount++;

            if(readycount > 100)
            {
                if(debug)
                    printf("Event FIFO Never Ready\r\n");
                break;
            }
            else
            {
                usleep(100);
            }
        }
    }

    EventChannelInUse = 0;
    //}


    return(0);
}


int GPS_split_line(char *line, gps_token *token, int *numTokens)
{
    int  i;
    char **bp = &line;
    char *tok;

    /* empty the token structure */
    for(i=0; i<15; i++)
        strcpy(token[i].txt, "");

    *numTokens = 0;
    i = 0;

   while ( (tok = strsep(bp, ",") ) )
   {
           strcpy(token[i].txt, tok);
           i++;
   }

    *numTokens = i+1;

    return(0);
}

void ProcessUBLOXCommand()
{
    int i;
    int config;
    int debug;
    int gps_debug;
    int leap;
    
    static int previous = 0xFF;

    UINT32  tacc = 0;

    M23_GetDebugValue(&debug);
    M23_GetGPSDebugValue(&gps_debug);
    M23_GetConfig(&config);


    if( GPS_Jammed == 0 )
    {
        if( (UBLOXBuffer[0] == 0xB5) && (UBLOXBuffer[1] == 0x62) )
        {
            if( (UBLOXBuffer[2] == 0x1) && (UBLOXBuffer[3] == 0x20) )
            {


                if(UBLOXBuffer[17] & 0xF8)
                {
                    //if(debug)
                    //   printf("Invalid 0x%x\r\n",UBLOXBuffer[17]);
                    return;
                }
               
                leap = UBLOXBuffer[16];
                if(gps_debug)
                    printf("flags 0x%x,leap seconds %d\r\n",UBLOXBuffer[17],leap);

                //tacc =  (UINT32)UBLOXBuffer[21];
                //tacc += ((UINT32)(UBLOXBuffer[20]) << 8);
                //tacc += ((UINT32)(UBLOXBuffer[19]) << 16);
                //tacc += ((UINT32)(UBLOXBuffer[18]) << 24);

                memcpy(&tacc,&UBLOXBuffer[18],4);
                tacc = tacc/1000000;


                if( (UBLOXBuffer[17] & 0x4) && (tacc <= 50) )
                {
                    if(gps_debug)
                        printf("Valid 0x%x,Time Accuracy = %d\r\n",UBLOXBuffer[17],tacc);

                    if(leap == previous)
                        leap_valid = 1;
                    else
                        previous = leap;
                }
                else 
                {
                    if(gps_debug)
                        printf("Not Valid 0x%x 0x%x\r\n",UBLOXBuffer[17],UBLOXBuffer[16]);

                    leap_valid = 0;
                    previous = leap;
                }

            }
        }
    }
}

void ProcessGPSCommand()
{
    int       i;
    int       numTokens;
    int       hour;
    int       minutes;
    int       seconds;
    int       ms;
    int       day;
    int       month;
    int       year;
    int       debug;
    int       return_status;


    int       days_in_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31}; 

    static int count = 0;
    static int JamPending = 0;

    gps_token NMEA[15];

    char Valid;
    char Command[8];
    char UTC_Time[11];
    char Date[7];

    char JamTime[20];


    UINT32 CSR;


    M23_GetDebugValue(&debug);

    if(NMEABuffer[0] == '$')
    {
        if( strncmp(NMEABuffer,"$GPRMC",6) == 0 )
        {
            if(leap_valid == 1)
            {
                GPS_split_line(NMEABuffer,NMEA,&numTokens);

                strcpy(Command,NMEA[0].txt);
                strcpy(UTC_Time,NMEA[1].txt);
                Valid = NMEA[2].txt[0];
                strcpy(Date,NMEA[9].txt);

                if(Valid == 'A')
                {
                    if( (GPS_Jammed == 0) && (JamPending == 0) )
                    {
                        CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ID_CSR);

                        if( CSR & CONTROLLER_TIMECODE_IN_SYNC)
                        {
                            M23_ClearHealthBit(TIMECODE_FEATURE,M23_TIMECODE_SYNC_ERROR);
                            /*Get Months*/
                            sscanf(Date,"%02d%02d%02d",&day,&month,&year);

                            /*Set the System Year*/
                            year = 2000 + year;
                            M23_SetYear(year);

                            if( (year %4) == 0)
                            {
                                days_in_month[1] = 29;
                            }
                            else
                            {
                                days_in_month[1] = 28;
                            }

                            days_in_month[0] = 31;

                            for(i = 0; i < (month - 1);i++)
                            {
                                day += days_in_month[i];
                            }

                            sscanf(UTC_Time,"%02d%02d%02d.%d",&hour,&minutes,&seconds,&ms);

                            memset(JamTime,0x0,20);
                            sprintf(JamTime,"%03d %02d:%02d:%02d.%03d",day,hour,minutes,seconds,ms);

                            if(debug)
                                printf("Time From GPS %s\r\n",JamTime);

                            M23_SetSystemTime(JamTime);
                            JamPending = 1;

                            //GPS_Jammed = 1;
            
                            return_status = pthread_create(&GPSWaitThread, NULL,(void *)GPS_WaitForJamComplete, NULL);

                       
                        }
                        else
                        {

                            if(debug)
                                printf("1PPS not in sync 0x%x\r\n",CSR);
                        }
                    }
                }
                else
                {

                    if(debug)
                        printf("NOT VALID %s,%s\r\n",NMEA[0].txt,NMEA[2].txt);
                }
            }
        }
        else
        {
            if(debug)
                printf("NOT GPRMC %s\r\n",NMEABuffer);
        }
    }
    else
    {
            if(debug)
                printf("NOT $ %s\r\n",NMEABuffer);
    }
}

int GPSPortInitialize( )
{
    struct termios newtio;
    UART_CONFIG config;

    CurrentIndex = 0;
    config.Baud = BAUD_96;
    config.Parity = NO_PARITY;
    config.Databits = EIGHT_DATA_BITS;
    config.Stopbits = ONE_STOP_BIT;
    config.Flowcontrol = NO_FLOW_CONTROL;


    GPSPort = open( "/dev/ttyS2", O_RDWR | O_NOCTTY ); 
    if ( GPSPort < 0 ) 
    {
        perror("GPS Port"); 
        return -2;
    }

    // set baud rate
    //
    bzero( &newtio, sizeof(newtio) );
    tcgetattr( GPSPort,  &newtio );
    cfmakeraw(&newtio);
    newtio.c_cflag |= (CLOCAL | CREAD | B9600);    


    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;
 
    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* n-blocking read (return when 0 or more chars received) */

    tcflush( GPSPort, TCIFLUSH);
    tcsetattr( GPSPort, TCSANOW, &newtio );


    /*
    printf( "cflag %08x\n", newtio.c_cflag );
    printf( "iflag %08x\n", newtio.c_iflag );
    printf( "oflag %08x\n", newtio.c_oflag );
    printf( "lflag %08x\n", newtio.c_lflag );

    */

    //printf( "lflag %08x\n", newtio.c_lflag );


    return 0;
}


int GPS_SerialGetChar(char *byte)
{
    int nchars = 0;

    nchars = read( GPSPort, byte, 1);
   if(nchars < 0 )
    perror("read");

    return nchars;
}


void GPS_SerialGetLine(char *p_buffer,int *size)
{
    int  nchars = 0;
    char byte;
    int  total = 0;

    while(1)
    {
        nchars = read( GPSPort, &byte, 1 );
        if(nchars < 0 )
        {
            break;
        }
        else if(nchars == 1)
        {
            if( byte == '\n')
            {
                break;
            }
            if( byte != '\r')
            {
                p_buffer[total] = byte;
                total++;
            }

        }
    }

    *size = total;

}

int GPS_SerialGetChars( char *p_buffer, int buffer_size )
{
    int nchars;
    int total = 0;

    UINT8 byte;

    while(1)
    {
        nchars = read( GPSPort, &byte, 1 );
        *p_buffer++ = byte;
        total += nchars;
        if(total >= buffer_size)
            break;
    }
    return total;
}


int GPS_SerialPutChars( char *p_buffer )
{
    int length;

    if ( p_buffer == NULL )
        return -3;

    length = strlen( p_buffer );

    if ( length )
    {
        write( GPSPort, p_buffer, length );
        /*write( PortDescriptor[1], p_buffer, length );*/
    }

    return 0;
}


int GPS_SerialPutChar( char byte )
{

    write( GPSPort, &byte, 1 );

    return 0;
}

void GPS_PutString( char *response )
{
    int length;

    if ( response == NULL )
        return;

    length = strlen( response );

    GPS_SerialPutChars(response);
}

void GPS_GetCommand(int record,int chan_id)
{
    int chars_read;
    int debug;
    int gps_debug;
    int total;
    int i;

    UINT8 calc;
    UINT8 rcvd;
    UINT8 tmp[3];

    UINT8 byte;
    UINT8 ChkA = 0;
    UINT8 ChkB = 0;

    UINT16  bytes_to_read;

    static int bytes_left = 100;


    M23_GetDebugValue(&debug);
    M23_GetGPSDebugValue(&gps_debug);

    chars_read = GPS_SerialGetChar( &byte);

    if(chars_read > 0)
    {
        if(byte == 0xB5) //This is an UBLOX command
        {
            memset(UBLOXBuffer,0x0,80);
            UBLOXBuffer[0] = byte;
            total = 1;
            chars_read = GPS_SerialGetChars( &UBLOXBuffer[1],5);
            total += chars_read;

            bytes_to_read =  (UINT16)UBLOXBuffer[4];
            bytes_to_read += ((UINT16)(UBLOXBuffer[5]) << 8);

            /*Now Read the payload*/
            chars_read = GPS_SerialGetChars( &UBLOXBuffer[6],bytes_to_read + 2); //include the checksum
            total += chars_read;

            for(i = 2;i < 22;i++)
            {
                ChkA = ChkA + UBLOXBuffer[i];
                ChkB = ChkB + ChkA;
            }

#if 0
            if(debug)
            {
                if( (ChkA != UBLOXBuffer[22]) && (ChkB != UBLOXBuffer[23]) )
                {
                    printf("Bytes Read = %d\r\n",chars_read);
                    for(i = 0; i < total;i++)
                    {
                        printf("[%d]0x%x ",i,UBLOXBuffer[i]);
                    }
                    printf("\r\n");


                    printf("0x%x 0x%x == 0x%x 0x%x\r\n",ChkA,ChkB,UBLOXBuffer[22],UBLOXBuffer[23]);
                }
                   
           }
#endif
            if( (ChkA == UBLOXBuffer[22]) && (ChkB == UBLOXBuffer[23]) )
               ProcessUBLOXCommand();
#if 0
            else {
               if(debug) {
                    for(i = 0; i < total;i++)
                    {
                        printf("[%d]0x%x ",i,UBLOXBuffer[i]);
                    }
                    printf("\r\n");
                }
            }
#endif
       
        }
        else if(byte == '$') //This is a NEMA command
        {
            memset(NMEABuffer,0x0,MAX_COMMAND_LENGTH);

            NMEABuffer[0] = byte;

            GPS_SerialGetLine(&NMEABuffer[1],&total);

            calc = 0;
            i = 1;
            while(1) {
                if(NMEABuffer[i] != '*')
                    calc = calc ^ NMEABuffer[i];
                else
                    break;
                i++;

                if(i > 200)
                    break;
            }
            memset(tmp,0x0,3);
            strncpy(tmp,&NMEABuffer[i+1],2);
            sscanf(tmp,"%x",&rcvd);
#if 0
            if(debug)
            {
                if(calc != rcvd)
                    printf("0x%x NMEA 0x%x - %s\r\n",calc,rcvd,tmp);
            }
#endif

            if(calc == rcvd) {
                if(record != 0)
                    UART_RecordData(total+1,chan_id);

                if(gps_debug)
                    printf("NMEA %s\r\n",NMEABuffer);

                if( strncmp(NMEABuffer,"$GPRMC",6) == 0)
                {
                    ProcessGPSCommand();
                }
            }
        }
        else
        {
            if(byte == 'G') //we dropped a byte, but we can still process the NMEA message
            {
                //if(debug)
                //    printf("dropped a byte, from previous %c,current %c\r\n",UBLOXBuffer[23],byte);
                memset(NMEABuffer,0x0,MAX_COMMAND_LENGTH);

                NMEABuffer[0] = '$';
                NMEABuffer[1] = byte;

                GPS_SerialGetLine(&NMEABuffer[2],&total);

                calc = 0;
                i = 1;
                while(1) {
                    if(NMEABuffer[i] != '*')
                        calc = calc ^ NMEABuffer[i];
                    else
                        break;
                    i++;

                    if(i > 200)
                    break;
                }
                memset(tmp,0x0,3);
                strncpy(tmp,&NMEABuffer[i+1],2);
                sscanf(tmp,"%x",&rcvd);
#if 0
                if(debug)
                {
                    if(calc != rcvd)
                        printf("Dropped 0x%x NMEA 0x%x - %s\r\n",calc,rcvd,tmp);
                }
#endif
                if(calc == rcvd) {
                    if(record != 0)
                        UART_RecordData(total+2,chan_id);

                    if( strncmp(NMEABuffer,"$GPRMC",6) == 0)
                    {
                        ProcessGPSCommand();
                    }
                }
            }
        }
    }
}

void M23_ProcessGPS()
{
    M23_CHANNEL *config;



    broadcast = 0;
    GPS_Init = 1;

    SetupGet(&config);

    while(1)
    {

        if( (config->timecode_chan.Format == 'N') || (config->timecode_chan.Format == 'U') ||(config->uart_chan.isEnabled) )
        { 
            if(config->uart_chan.isEnabled)
            {
                GPS_GetCommand(1,config->uart_chan.chanID);
            }
            else
            {
                GPS_GetCommand(0,0);
            }
        }
        else
        {
            if(config->uart_chan.isEnabled)
            {
                GPS_GetCommand(1,config->uart_chan.chanID);
            }
            else
            {
                sleep(1);
            }
        }
    }
}

void M23_StartGPS()
{
    int status = 0;
    int debug;
    int config;


    M23_GetConfig(&config);
    M23_GetDebugValue(&debug);


    GPS_Init = 0;

    system("stty -F /dev/ttyS2 9600");
    system("echo \\$PUBX,41,1,0007,0003,9600,0,0*20 > /dev/ttyS2");
    system("stty -F /dev/ttyS2 9600");
    system("echo \\$PUBX,40,TXT,0,0,0,0*43 > /dev/ttyS2");
    system("echo \\$PUBX,40,VTG,0,0,0,0*5E > /dev/ttyS2");
    system("echo \\$PUBX,40,ZDA,0,0,0,0*44 > /dev/ttyS2");

    if(config == B52_CONFIG)
    {
        system("echo \\$PUBX,40,GST,1,1,1,0*5A > /dev/ttyS2");
    }
    else
    {
        system("echo \\$PUBX,40,GSV,0,0,0,0*59 > /dev/ttyS2");
        system("echo \\$PUBX,40,GLL,0,0,0,0*5C > /dev/ttyS2");
        system("echo \\$PUBX,40,GSA,0,0,0,0*4E > /dev/ttyS2");
    }

    system("echo -ne '\\xB5\\x62\\x06\\x01\\x03\\x00\\x01\\x20\\x01\\x2c\\x83' > /dev/ttyS2");

    /*First Initialize the device*/
    GPSPortInitialize( );

    status = pthread_create(&GPSThread, NULL,(void *) M23_ProcessGPS, NULL);

    if(debug){
        if(status == 0)
            PutResponse(0,"GPS Thread Created Successfully\r\n");
        else
            PutResponse(0,"GPS Create Thread Failed\r\n");
    }

}
