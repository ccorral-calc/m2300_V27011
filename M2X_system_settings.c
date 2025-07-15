//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: system_settings.c
//    Version: 1.0
//     Author: dm
//
//            MONSSTR 2100 System Settings
//
//            Stores settings that are saved across power-downs
//
//    Revisions:   
//

#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>


#include "M2X_system_settings.h"
#include "M23_Controller.h"
#include "M23_features.h"
#include "M23_Utilities.h"
#include "M23_I2c.h"

#include "M2X_FireWire_io.h"
#include "M2X_serial.h"
#include "M2X_cmd_ascii.h"


INT32              ControllerHealthTemps[2];
INT32              VideoHealthTemps[6][2];
INT32              PCMHealthTemps[6][2];
INT32              M1553HealthTemps[6][2];
INT32              PowerSupplyHealthTemps[2];
INT32              FirewireHealthTemps[2];

INT32              ControllerPower;
INT32              FirewirePower;
INT32              VideoPower[5];
INT32              M1553Power[5];
INT32              PCMPower[5];

// definitions
//

//#define SETTINGS_FILENAME       "/usr/Calculex/Settings"
#define SETTINGS_FILENAME       "./Settings"


#define SETTINGS_VERSION        1
#define SETTINGS_KEY            "<System Setup>"

#define CONTROLLER_OFFSET   0
#define FIREWIRE_OFFSET     1
#define POWER_SUPPLY_OFFSET 2
#define VIDEO_OFFSET        4
#define M1553_OFFSET        8 
#define PCM_OFFSET          12 

typedef struct 
{
    int temp1;
    int temp2;
    int current;
}I2C_SETTINGS;

typedef struct {
    INT8               SS_Key[15];
    UINT8              SS_Version;
    UINT32             Critical[MAX_FEATURES];
    UINT32             Setup;
    UINT32             Config;
    UINT32             Verbose;
    UINT32             ConsoleEcho;
    UART_CONFIG        CommConfig_Port1;
    UART_CONFIG        CommConfig_Port2;
    UINT8              DAC1;
    UINT32             TimeAdjust;
    UINT8              TimeDebug;
    UINT8              LiveVid;
    UINT8              Serial1;
    UINT8              Serial2;
    UINT8              Ssric;
    UINT8              Script;
    UINT32             Bingo;
    UINT32             Year;
    UINT32             Temp;
    UINT32             Power;
    INT32              RemoteTerminal;
    INT32              TimeCodeOffset;
    UINT32             IgnorePresent;
    UINT32             PCMStatusMask;
    UINT32             M1553StatusMask;
    I2C_SETTINGS       TempAndPower[20];
    INT32              PCM_DATA_DACS[4];
    INT32              PCM_CLOCK_DACS[4];
    UINT8              VolumeName[33];
    UINT8              UnitName[33];
    UINT8              Remote;
    UINT8              TailNumbers[32][31];
    UINT8              LongCable;
    UINT8              Debug;
    UINT32             OnTimeA;
    UINT32             OnTimeG;
    UINT16             AFactor;
    UINT16             BFactor;
    UINT16             GFactor;
    UINT8              Multiplier;
    UINT32             TimeMask;
    UINT8              GPSDebug;
    UINT8              Reserved[19];
} SYSTEM_SETTINGS;



static int TrainDAC;

// static declarations
//

static SYSTEM_SETTINGS  Settings;


static int get_default_settings( void );
static int restore_settings( void );

static int TailIndex;

//void M23_ProgramTimeValues();



// external functions
//


int settings_Initialize( void )
{
    int count;
    FILE *p_file;
    int return_value = 0;


    // fill in defaults
    //
    get_default_settings();

    TailIndex = 0;
    TrainDAC = 0;

    // open existing settings file
    //
    p_file = fopen( SETTINGS_FILENAME, "rb+" );
    if ( !p_file )
    {
        // settings file is missing
        //
        //printf( "system_settings: warning: missing setup file, reverting to defaults\r\n" );
        //return_value = -2;

        // fill in defaults
        //
        get_default_settings();


        // force file open -- will create if file does not exist
        // 
        p_file = fopen( SETTINGS_FILENAME, "wb+" );

        if ( !p_file )
        {
            restore_settings();

            perror( "settings_Initialize: fopen" );
            return -1;
        }


        // write default settings to disk
        //
        count = fwrite( &Settings, 1, sizeof( SYSTEM_SETTINGS ), p_file );

        if ( count != sizeof( SYSTEM_SETTINGS ) )
        {
            printf( "system_settings: warning: wrote only %d bytes out of %d to settings file\r\n", count, sizeof( SYSTEM_SETTINGS ) );
        }
    }

    // rewind to beginning of file
    //
    if ( fseek( p_file, 0, SEEK_SET ) )
        perror( "system_settings: fseek" );


    // read the file
    //
    count = fread( &Settings, 1, sizeof( SYSTEM_SETTINGS ), p_file );

    if ( count != sizeof( SYSTEM_SETTINGS ) )
    {
        printf( "system_settings: warning: read only %d bytes out of %d from settings file\r\n", count, sizeof( SYSTEM_SETTINGS ) );

    }

    if(p_file)
        fclose( p_file );

    p_file = NULL;



    // check the validity of the file
    // 
    if ( (strcmp( Settings.SS_Key, SETTINGS_KEY ) != 0 ) || (count != sizeof(SYSTEM_SETTINGS) ))
    {
        // settings file is invalid !!!!
        //
        printf( "system_settings: error: invalid setup file, reverting to defaults\r\n" );

        // fill in defaults
        //
        get_default_settings();

        // write them to disk
        //
        p_file = fopen( SETTINGS_FILENAME, "wb+" );
        if(p_file)
        {

            count = fwrite( &Settings, 1, sizeof( SYSTEM_SETTINGS ), p_file );

            if ( count != sizeof( SYSTEM_SETTINGS ) )
                printf( "system_settings: warning: wrote only %d bytes out of %d to settings file\r\n", count, sizeof( SYSTEM_SETTINGS ) );

            fclose( p_file );
        }

        return_value = -2;
    }


    //sUpdateVolumeName(Settings.VolumeName);

    /*Change where this is accomplished, Only set up channels when ready*/ 
    restore_settings();

    return return_value;
}

void Setup_Save()
{
    int count;
    int setup;
    FILE *p_file;
    SYSTEM_SETTINGS  Setup;

    // open existing settings file
    //
    p_file = fopen( SETTINGS_FILENAME, "rb+" );

    if ( !p_file )
    {
        perror( "system_settings: fopen" );
    }
    else
    {

        count = fread( &Setup, 1, sizeof( SYSTEM_SETTINGS ), p_file );
        fclose(p_file);

        // get current settings
        //
        strcpy( Setup.SS_Key, SETTINGS_KEY );
        Setup.SS_Version = SETTINGS_VERSION;


        SetupViewCurrent( &setup );
        Setup.Setup = setup;

        p_file = fopen( SETTINGS_FILENAME, "wb+" );

        if ( !p_file )
        {
            perror( "system_settings: fopen" );
        }
        else
        {
            // write updated data to disk
            //
            count = fwrite( &Setup, 1, sizeof( SYSTEM_SETTINGS ), p_file );

            if ( count != sizeof( SYSTEM_SETTINGS ) )
                printf( "system_settings: warning: wrote only %d bytes out of %d to settings file\r\n", count, sizeof( SYSTEM_SETTINGS ) );
  

            fclose( p_file );

            p_file = NULL;
        }

    }
}

void Critical_Save()
{
    int i;
    int count;
    int setup;
    int number_of_features;

    UINT32  *masks = NULL;

    FILE *p_file;
    SYSTEM_SETTINGS  Crit;

    // open existing settings file
    //
    p_file = fopen( SETTINGS_FILENAME, "rb+" );

    if ( !p_file )
    {
        perror( "system_settings: fopen" );
    }

    count = fread( &Crit, 1, sizeof( SYSTEM_SETTINGS ), p_file );
    fclose(p_file);

    // get current settings
    //
    strcpy( Crit.SS_Key, SETTINGS_KEY );
    Crit.SS_Version = SETTINGS_VERSION;



    p_file = fopen( SETTINGS_FILENAME, "wb+" );

    if ( !p_file )
    {
        perror( "system_settings: fopen" );
    }
    // write updated data to disk
    //

    FeaturesCriticalView( &number_of_features, &masks );

        //TODO : Get the rest of the channels


    for( i = 0; i < MAX_FEATURES; i++ )
    {
        if ( HealthArrayTypes[i] != NO_FEATURE)
        {
            Crit.Critical[i] = masks[i];
        }
        else
        {
            Crit.Critical[i] = 0x00000000;
        }
    }
    count = fwrite( &Crit, 1, sizeof( SYSTEM_SETTINGS ), p_file );

    if ( count != sizeof( SYSTEM_SETTINGS ) )
        printf( "system_settings: warning: wrote only %d bytes out of %d to settings file\r\n", count, sizeof( SYSTEM_SETTINGS ) );


    fclose( p_file );
    p_file = NULL;

}


void SaveDriftcal(int value)
{
    int count;
    FILE *p_file;
    SYSTEM_SETTINGS  Setup;
    M23_CHANNEL  const *config;

    SetupGet(&config);

    // open existing settings file
    //
    p_file = fopen( SETTINGS_FILENAME, "rb+" );

    if ( !p_file )
    {
        perror( "system_settings: fopen" );
    }
    else
    {

        count = fread( &Setup, 1, sizeof( SYSTEM_SETTINGS ), p_file );

        fclose(p_file);

        // get current settings
        //
        strcpy( Setup.SS_Key, SETTINGS_KEY );
        Setup.SS_Version = SETTINGS_VERSION;


        Setup.TimeCodeOffset = value;

        if(config->timecode_chan.Format == 'A')
        {
            Settings.Multiplier = 10;
        }
        else if(config->timecode_chan.Format == 'B')
        {
            Settings.Multiplier = 1;
        }
        else //This must be G
        {
            Settings.Multiplier = 100;
        }

        Setup.Multiplier = Settings.Multiplier;


        p_file = fopen( SETTINGS_FILENAME, "wb+" );

        if ( !p_file )
        {
            perror( "system_settings: fopen" );
        }
        else
        {
            // write updated data to disk
            //
            count = fwrite( &Setup, 1, sizeof( SYSTEM_SETTINGS ), p_file );
            fclose( p_file );
        }

        p_file = NULL;
    }

}


int settings_Update()
{
    int i;
    int count;
    int setup;
    int console_echo;
    UART_CONFIG *p_config;
    int number_of_features;
    FILE *p_file;
    UINT32  *masks = NULL;


    // open existing settings file
    //
    p_file = fopen( SETTINGS_FILENAME, "rb+" );

    if ( !p_file )
    {
        perror( "system_settings: fopen" );
        return(-1);
    }


    // get current settings
    //
    strcpy( Settings.SS_Key, SETTINGS_KEY );
    Settings.SS_Version = SETTINGS_VERSION;


    SetupViewCurrent( &setup );
    Settings.Setup = setup;

    FeaturesCriticalView( &number_of_features, &masks );

	//TODO : Get the rest of the channels

//PC Fix
    for( i = 0; i < MAX_FEATURES; i++ )
    {
        if ( HealthArrayTypes[i] != NO_FEATURE)
        {
            Settings.Critical[i] = masks[i];
        }
        else
        {
            Settings.Critical[i] = 0x00000000;
        }
    }


#if 0
    for( i = 0; i < MAX_FEATURES; i++ )
    {
        if ( i < number_of_features )
            Settings.Critical[i] = masks[i];
        else
            Settings.Critical[i] = 0x00000000;
    }
#endif


    ConsoleEchoView( &console_echo );
    Settings.ConsoleEcho = console_echo;

    SerialComView( &p_config,0 );
    Settings.CommConfig_Port1 = *p_config;

    SerialComView( &p_config,1 );
    Settings.CommConfig_Port2 = *p_config;

    // write updated data to disk
    //
    count = fwrite( &Settings, 1, sizeof( SYSTEM_SETTINGS ), p_file );

    if ( count != sizeof( SYSTEM_SETTINGS ) )
        printf( "system_settings: warning: wrote only %d bytes out of %d to settings file\r\n", count, sizeof( SYSTEM_SETTINGS ) );
  

    if(p_file)
        fclose( p_file );

    p_file = NULL;


    return 0;
}




// internal functions 
//

int get_default_settings( void )
{
    int i;

    strcpy( Settings.SS_Key, SETTINGS_KEY );
    Settings.SS_Version = SETTINGS_VERSION;
    memset( Settings.Critical, 0x00, sizeof( Settings.Critical ) );
    Settings.Setup = 1;
    Settings.Config = 0;
    Settings.Verbose = 0;
    Settings.ConsoleEcho = 1;

    Settings.CommConfig_Port1.Baud = BAUD_384;
    //Settings.CommConfig_Port1.Baud = BAUD_1152;
    Settings.CommConfig_Port1.Parity = NO_PARITY;
    Settings.CommConfig_Port1.Databits = EIGHT_DATA_BITS;
    Settings.CommConfig_Port1.Stopbits = ONE_STOP_BIT;
    Settings.CommConfig_Port1.Flowcontrol = NO_FLOW_CONTROL;

    Settings.CommConfig_Port2.Baud = BAUD_96;
    Settings.CommConfig_Port2.Parity = NO_PARITY;
    Settings.CommConfig_Port2.Databits = EIGHT_DATA_BITS;
    Settings.CommConfig_Port2.Stopbits = ONE_STOP_BIT;
    Settings.CommConfig_Port2.Flowcontrol = NO_FLOW_CONTROL;

    Settings.DAC1 = 0x0;

    Settings.TimeAdjust = 0x160144B;

    Settings.TimeDebug = 0;


    Settings.RemoteTerminal = -1; //Default to OFF

    Settings.LongCable = 0; //Default to OFF

    Settings.Debug = 0; //Default to OFF

    Settings.GPSDebug = 0; //Default to OFF

    Settings.Remote = 0; //Remote Receiver to Off

    Settings.TimeCodeOffset = 50000000;
    Settings.Multiplier = 1;

    Settings.LiveVid = 0;
    Settings.Serial1 = 0;
    Settings.Serial2 = 0;

    Settings.Ssric = 0;

    Settings.Script = 0;

    Settings.Bingo = 99;
    Settings.Year  = 2007;

    Settings.Temp  = 0;
    Settings.Power = 0;

    Settings.IgnorePresent = 0;
    Settings.PCMStatusMask = 0;
    Settings.M1553StatusMask = 0;

    Settings.TimeMask = 0x7FFFF00;

    memset(Settings.VolumeName,0x0,33);
    strcpy(Settings.VolumeName,"M2300V1 Cartridge");

    memset(Settings.UnitName,0x0,33);
    strcpy(Settings.UnitName,"M2300V1");

    for(i = 0; i < 4;i++)
    {
        Settings.PCM_DATA_DACS[i] = 0x80;
        Settings.PCM_CLOCK_DACS[i] = 0x80;
    }


    /*Set the Default Controller Temp And Power*/
    Settings.TempAndPower[CONTROLLER_OFFSET].temp1 = 65;
    Settings.TempAndPower[CONTROLLER_OFFSET].temp2 = 65;
    Settings.TempAndPower[CONTROLLER_OFFSET].current = 500;

    /*Set the Default Firewire Temp And Power*/
    Settings.TempAndPower[FIREWIRE_OFFSET].temp1 = 65;
    Settings.TempAndPower[FIREWIRE_OFFSET].temp2 = 65;
    Settings.TempAndPower[FIREWIRE_OFFSET].current = 300;

    /*Set the Default Power Supple Temp And Power*/
    Settings.TempAndPower[POWER_SUPPLY_OFFSET].temp1 = 65;
    Settings.TempAndPower[POWER_SUPPLY_OFFSET].temp2 = 65;
    Settings.TempAndPower[POWER_SUPPLY_OFFSET].current = 0xFF; //No Defined

    /*Set the Default Board Offsets*/
    for(i = 0; i < 4;i++)
    {
        Settings.TempAndPower[VIDEO_OFFSET+i].temp1 = 80;
        Settings.TempAndPower[VIDEO_OFFSET+i].temp2 = 80;
        Settings.TempAndPower[VIDEO_OFFSET+i].current = 300;

        Settings.TempAndPower[M1553_OFFSET+i].temp1 = 65;
        Settings.TempAndPower[M1553_OFFSET+i].temp2 = 65;
        Settings.TempAndPower[M1553_OFFSET+i].current = 300;

        Settings.TempAndPower[PCM_OFFSET+i].temp1 = 65;
        Settings.TempAndPower[PCM_OFFSET+i].temp2 = 65;
        Settings.TempAndPower[PCM_OFFSET+i].current = 300;
    }

    for(i = 0; i < 32;i++)
    {
        memset(Settings.TailNumbers[i],0x0,31);
        strcpy(Settings.TailNumbers[i],"NO TAIL NUMBER");
    }

    Settings.AFactor = 0x300;
    Settings.BFactor = 0x300;
    Settings.GFactor = 0x300;

    return 0;
}


int restore_settings( void )
{
    int i;
    int config;


    // restore the settings
    //
    if ( Settings.SS_Version >= 1 ) // SETTINGS_VERSION 1
    {

        for ( i = 0; i < MAX_FEATURES; i++ )
        {
           
            if(i == RECORDER_FEATURE)
            {
                if(Settings.Config != B1B_CONFIG)
                {
                    CriticalSet( i, Settings.Critical[i] | M23_MEMORY_NOT_PRESENT | M23_MEMORY_FULL | M23_RECORDER_CONFIGURATION_FAIL | M23_DISK_NOT_CONNECTED);
                }
                else
                {
                    CriticalSet( i, Settings.Critical[i] );
                }
            }
            else
            {
                CriticalSet( i, Settings.Critical[i] );
            }

        }

        //M23_ProgramTimeValues();

        /*Remove from this point , we will setup when we determine if an SSRIC is available/*/


        //ConsoleEchoSet( Settings.ConsoleEcho );
        SerialComSet( &Settings.CommConfig_Port1 ,0);
        SerialComSet( &Settings.CommConfig_Port2 ,1);
    }

    if ( Settings.SS_Version >= 2 ) // SETTINGS_VERSION 2
    {
        // fill this in for the settings that were added with version 2
        // ...
    }

    return 0;
}

void M23_SetTailIndex(int index)
{
    TailIndex = index;
}

void M23_SetTailNumber(int offset,char *Tail)
{
    memset(Settings.TailNumbers[offset],0x0,31);
    strncpy(Settings.TailNumbers[offset],Tail,30);
}

void M23_PrintTailNumbers()
{
    int i;

    for(i = 0; i < 32;i++)
    {
        PutResponse(0,Settings.TailNumbers[i]);
        PutResponse(0,"\r\n");
    }
}

void M23_GetTailNumber(char *tail)
{
    strncpy(tail,Settings.TailNumbers[TailIndex],31);
}

void M23_SetPCMDAC(int dac,int clock,int data)
{
    Settings.PCM_CLOCK_DACS[dac] = clock;
    Settings.PCM_DATA_DACS[dac] = data;
}

void M23_GetPCM_DAC(int dac,int *clock,int *data)
{
    *clock = Settings.PCM_CLOCK_DACS[dac];
    *data  = Settings.PCM_DATA_DACS[dac];
}


void M23_SetDAC(int dac,int value)
{
    int             fd;
    unsigned char   buf[5];

    fd = open("/dev/spi-0",O_RDWR);

    if(fd < 0)
    {
        perror("OPEN SPI");
        close(fd);
        return;
    }

    memset(buf,0x0,5);
    buf[2] = (value & 0x00FF);
    buf[0] = (value >> 8) & 0x003F;
    //printf("Writing DAC values = 0x%x 0x%x 0x%x 0x%x\r\n",buf[0],buf[1],buf[2],buf[3]);
    if(write(fd, &buf, 3) != 3)
    {
        perror("Writing DAC");
    }

    close(fd);
    Settings.DAC1 = value;
    TrainDAC = value;
}


void M23_SetTimeCodeOffset()
{
    UINT32 CSR;
    int    i;
    int    SumValue = 0;


    for(i = 0 ; i < 10; i++)
    {
        /*Program the DAC's*/
        CSR = M23_ReadControllerCSR(CONTROLLER_DRIFTCAL_CSR);
        SumValue += CSR;

        sleep(1);

    }

    SumValue = SumValue/10;

    M23_WriteControllerCSR(CONTROLLER_TIMECODE_OFFSET_CSR,SumValue);

    Settings.TimeCodeOffset = SumValue;

    SaveDriftcal(SumValue);
}

void M23_GetTimeCodeOffset(int *value)
{
    *value = Settings.TimeCodeOffset;
}

void M23_SetMultiplier(int value)
{
    if(value == 0) //IRIG B
    {
        Settings.Multiplier = 1;
    }
    else if(value == 1) //IRIG A
    {
        Settings.Multiplier = 10;
    }
    else //IRIG G
    {
        Settings.Multiplier = 100;
    }
   
}

void M23_GetMultiplier(int *value)
{
    if( (Settings.Multiplier != 1) && (Settings.Multiplier != 10) && (Settings.Multiplier != 100) )
    {
        Settings.Multiplier = 1;
    }

    *value = Settings.Multiplier;
}

void M23_SetVerbose(int value)
{
    Settings.Verbose = value;
}

void M23_GetVerbose(int *value)
{
    *value = Settings.Verbose;
}

void M23_SetTimeAdjust(int TimeAdjust,int value,int mode)
{
    if(mode == 0) //IRIG B
    {
        Settings.TimeAdjust = value;
    }
    else if(mode == 1) //IRIG A
    {
        Settings.OnTimeA = value;
    }
    else //IRIG G
    {
        Settings.OnTimeG = value;
    }
}

void M23_GetOnTime(int *value,int mode)
{
    if(mode == 0) //IRIG B
    {
        *value = Settings.TimeAdjust;
    }
    else if(mode == 1) //IRIG A
    {
        *value = Settings.OnTimeA;
    }
    else //IRIG G
    {
        *value = Settings.OnTimeG;
    }
}


void M23_StartVideoChain()
{
    /*Start the Uncompressed Video Chain*/
    M23_StartUncompressedVideo(Settings.LiveVid);

}

void M23_SetConfig(int value)
{
    Settings.Config = value;
}

void M23_GetConfig(int *value)
{
    *value = Settings.Config;
}


void M23_SetFactor(int value,int mode)
{
    if(mode == 0) //IRIG B
    {
        Settings.BFactor = value;
    }
    else if(mode == 1) //IRIG A
    {
        Settings.AFactor = value;
    }
    else //IRIG G
    {
        Settings.GFactor = value;
    }
}

void M23_GetFactor(int *value,int mode)
{
    if(mode == 0) //IRIG B
    {
        *value = Settings.BFactor;
    }
    else if(mode == 1) //IRIG A
    {
        *value = Settings.AFactor;
    }
    else //IRIG G
    {
        *value = Settings.GFactor;
    }
//printf("Mode = %d, value = 0x%x\r\n",mode,*value);
}

void M23_GetYear(int *value)
{
    *value = Settings.Year;
}

void M23_SetYear(int value)
{
     Settings.Year = value;
}

void M23_SetVolume() 
{
    sUpdateVolumeName(Settings.VolumeName);
}

void M23_SetVolumeName(UINT8 *VolName) 
{
    memset(Settings.VolumeName,0x0,33);
    strcpy(Settings.VolumeName,VolName);
    sUpdateVolumeName(Settings.VolumeName);
}

void M23_PrintVolumeName()
{
    PutResponse(0,"VOLUME ");
    PutResponse(0,Settings.VolumeName);
    PutResponse(0,"\r\n*");;
}

void M23_SetUnitName(UINT8 *UnitName) 
{
    memset(Settings.UnitName,0x0,33);
    strcpy(Settings.UnitName,UnitName);
    sUpdateVolumeName(Settings.UnitName);
}


void M23_PrintUnitName()
{
    PutResponse(0,"UNITNAME ");
    PutResponse(0,Settings.UnitName);
    PutResponse(0,"\r\n*");;
}

void M23_GetUnitName(UINT8 *UnitName) 
{
    strcpy(UnitName,Settings.UnitName);
}

void M23_SetTimeDebug(int value)
{
    Settings.TimeDebug = value;
}

void M23_GetTimeDebug(int *value)
{
    *value = Settings.TimeDebug;
}

void M23_SetRemoteTerminalValue(int value)
{
    Settings.RemoteTerminal = value;
}

void M23_GetRemoteTerminalValue(int *value)
{
    *value = Settings.RemoteTerminal;
}


void M23_GetLongCableValue(int *value)
{
    *value = Settings.LongCable;
}

void M23_SetLongCableValue(int value)
{
    Settings.LongCable = value;
}

void M23_GetDebugValue(int *value)
{
    *value = Settings.Debug;
}

void M23_SetDebugValue(int value)
{
    Settings.Debug = value;
}

void M23_GetGPSDebugValue(int *value)
{
    *value = Settings.GPSDebug;
}

void M23_SetGPSDebugValue(int value)
{
    Settings.GPSDebug = value;
}


void M23_SetRemoteValue(int value)
{
    Settings.Remote = value;
}

void M23_GetRemoteValue(int *value)
{
    *value = Settings.Remote;
}


void M23_SetTempValue(int value)
{
    Settings.Temp = value;
}

void M23_SetPowerValue(int value)
{
    Settings.Temp = value;
}

void M23_GetLiveVideo(int *value)
{
    *value = Settings.LiveVid;
}
void M23_TempLiveVideo(int value)
{
    M23_StartUncompressedVideo(value);
}

void M23_SetLiveVideo(int value)
{
    if(Settings.Debug)
        printf("Setting Livevid %d\r\n",value);

    Settings.LiveVid = value;
    /*Start the Uncompressed Video Chain*/
    M23_StartUncompressedVideo(Settings.LiveVid);
}

void M23_SetCurrentLiveVideo()
{
    M23_StartUncompressedVideo(Settings.LiveVid);
}


void M23_SetIgnorePresent(int value)
{
    Settings.IgnorePresent = value;
}

void M23_GetIgnorePresent(int *value)
{
    *value = Settings.IgnorePresent;

}

void M23_SetPCMStatusMask(int value)
{
    Settings.PCMStatusMask = value;
}

void M23_GetPCMStatusMask(int *value)
{
    *value = Settings.PCMStatusMask;
}

void M23_SetM1553StatusMask(int value)
{
    Settings.M1553StatusMask = value;
}

void M23_GetM1553StatusMask(int *value)
{
    *value = Settings.M1553StatusMask;
}

void M23_SetSsric(int value)
{
    Settings.Ssric = value;
}

void M23_GetSsric(int *value)
{
    *value = Settings.Ssric;
}

void M23_GetBingo(int *value)
{
    *value = Settings.Bingo;
}

void M23_SetBingo(int value)
{
    Settings.Bingo = value;
}

void M23_GetSetup(int *value)
{
    *value = Settings.Setup;
}

void M23_SetSerial(int index,int value)
{
    if(index == 1)
    {
        Settings.Serial1 = value;
    }
    else
    {
        Settings.Serial2 = value;
    }
}

void M23_GetSerial1(int *value)
{
    *value = Settings.Serial1;
}

void M23_GetSerial2(int *value)
{
    *value = Settings.Serial2;
}

void M23_PutTimeMask(int value)
{
    Settings.TimeMask = value;
}

void M23_GetTimeMask(int *value)
{
    *value = Settings.TimeMask;
}

void M23_PutCal()
{
    M23_CHANNEL  const *config;

    char tmp[25];

    SetupGet(&config);

    /*Note : The spacing from Name to value is currently at 14*/

    memset(tmp,0x0,25);

    if(config->timecode_chan.Format == 'A')
    {
        sprintf(tmp,"ONTIMEVAL    0x%x\r\n",Settings.OnTimeA);
    }
    else if(config->timecode_chan.Format == 'B')
    {
        sprintf(tmp,"ONTIMEVAL    0x%x\r\n",Settings.TimeAdjust);
    }
    else //This must be G
    {
        sprintf(tmp,"ONTIMEVAL    0x%x\r\n",Settings.OnTimeG);
    }

    PutResponse(0,tmp);
}


void M23_PutTimeCodeOffset()
{

    /*Note : The spacing from Name to value is currently at 14*/
    PutResponse(0,"DRIFTCAL\r\n");;
}

void M23_PutTimeCal()
{
    if(Settings.TimeDebug == 1)
    {
        PutResponse(0,"ONTIMECAL    ON\r\n");
    }
    else
    {
        PutResponse(0,"ONTIMECAL    OFF\r\n");
    }

}

void M23_PutRemoteTerm()
{
    char tmp[25];

    memset(tmp,0x0,25);
    if(Settings.RemoteTerminal == -1)
    {
        sprintf(tmp,"RTADDR       OFF\r\n");
    }
    else
    {
        sprintf(tmp,"RTADDR       %d\r\n",Settings.RemoteTerminal);
    }

    PutResponse(0,tmp);
}

void M23_PutVideoOut()
{
    char tmp[25];

    memset(tmp,0x0,25);
    if(Settings.Config == LOCKHEED_CONFIG)
    {
        sprintf(tmp,"TMOUT        %d\r\n",Settings.LiveVid);
        PutResponse(0,tmp);
    }
    else
    {
        sprintf(tmp,"LIVEVID      %d\r\n",Settings.LiveVid);
        PutResponse(0,tmp);
    } 
}

void M23_PutSettings(int mode)
{
    int  i;
    int  pcm = 0;
    int  m1553 = 0;
    int  video = 0;
    int  num_boards;
    char tmp[40];

    /*Note : The spacing from Name to value is currently at 14*/
    memset(tmp,0x0,40);

    sprintf(tmp,"BINGO        %d\r\n",Settings.Bingo);
    PutResponse(0,tmp);

#if 0
    if(Settings.Bit4)
    {
        PutResponse(0,"BIT4         OFF\r\n");
    }
    else
    {
        PutResponse(0,"BIT4         ON\r\n");
    }
#endif


    SetPutCom();

    memset(tmp,0x0,40);
    sprintf(tmp,"CONFIG       %d\r\n",Settings.Config);
    PutResponse(0,tmp);

    memset(tmp,0x0,40);
    sprintf(tmp,"DAC          0x%x\r\n",TrainDAC);
    PutResponse(0,tmp);

    if(Settings.Debug)
    {
        PutResponse(0,"DEBUG        ON\r\n");
    }
    else
    {
        PutResponse(0,"DEBUG        OFF\r\n");
    }

    M23_PutTimeCodeOffset();

    if(mode == 0) //IRIG B
    {
        sprintf(tmp,"FACTOR       0x%x\r\n",Settings.BFactor);
    }
    else if(mode == 1) //IRIG A
    {
        sprintf(tmp,"FACTOR       0x%x\r\n",Settings.AFactor);
    }
    else
    {
        sprintf(tmp,"FACTOR       0x%x\r\n",Settings.GFactor);
    }

    PutResponse(0,tmp);

    if(Settings.GPSDebug)
    {
        PutResponse(0,"GPS_DEBUG    ON\r\n");
    }
    else
    {
        PutResponse(0,"GPS_DEBUG    OFF\r\n");
    }


    if(Settings.LongCable == 0)
    {
        PutResponse(0,"IEEE1394B    S800\r\n");
    }
    else
    {
        PutResponse(0,"IEEE1394B    S400\r\n");
    }

#if 0
    if(Settings.IgnorePresent)
    {
        PutResponse(0,"IGNORE       ON\r\n");
    }
    else
    {
        PutResponse(0,"IGNORE       OFF\r\n");
    }
#endif

    if(Settings.Config != LOCKHEED_CONFIG)
    {
        M23_PutVideoOut();
    }



    M23_PutTimeCal(); 

    M23_PutCal();

#if 0
    for(i = 0; i < 4;i++)
    {
        sprintf(tmp,"PCM_DAC%d     0x%x 0x%x\r\n",i,Settings.PCM_CLOCK_DACS[i],Settings.PCM_DATA_DACS[1]);
        PutResponse(0,tmp);
    }
#endif

    if(Settings.Remote == 0)
    {
        PutResponse(0,"REMOTE       OFF\r\n");
    }
    else
    {
        PutResponse(0,"REMOTE       ON\r\n");
    }

    M23_PutRemoteTerm();


    if(Settings.Script)
    {
        PutResponse(0,"SCRIPT       ON\r\n");
    }
    else
    {
        PutResponse(0,"SCRIPT       OFF\r\n");
    }

    memset(tmp,0x0,40);
    if(Settings.Ssric == 1)
    {
        sprintf(tmp,"SSRIC        ON\r\n");
    }
    else
    {
        sprintf(tmp,"SSRIC        OFF\r\n");
    }
    PutResponse(0,tmp);


    memset(tmp,0x0,40);
    sprintf(tmp,"TIMEMASK     0x%x\r\n",Settings.TimeMask);
    PutResponse(0,tmp);

    if(Settings.Config == LOCKHEED_CONFIG)
    {
        M23_PutVideoOut();
    }

    memset(tmp,0x0,40);
    sprintf(tmp,"UNITNAME     %s\r\n",Settings.UnitName); 
    PutResponse(0,tmp);

    if(Settings.Verbose == 1)
    {
        PutResponse(0,"VERBOSE      ON\r\n"); 
    }
    else
    {
        PutResponse(0,"VERBOSE      OFF\r\n"); 
    }


    sprintf(tmp,"VOLUME       %s\r\n",Settings.VolumeName); 
    PutResponse(0,tmp);

    sprintf(tmp,"YEAR         %d\r\n",Settings.Year);
    PutResponse(0,tmp);

#if 0

    /*Put The Controller Temp and Power*/
    sprintf(tmp,"\r\nController\r\n");
    PutResponse(0,tmp);
    sprintf(tmp,"  Temp1 %d, Temp2 %d, Power %d\r\n",Settings.TempAndPower[CONTROLLER_OFFSET].temp1,Settings.TempAndPower[CONTROLLER_OFFSET].temp2,Settings.TempAndPower[CONTROLLER_OFFSET].current);
    PutResponse(0,tmp);

    /*Get the Number of MP boards*/
    M23_NumberMPBoards(&num_boards);
    for(i = 0; i < num_boards;i++)
    {
        if(M23_MP_IS_PCM(i) ) //PCM
        {
            sprintf(tmp,"PCM %d\r\n",pcm);
            PutResponse(0,tmp);
            sprintf(tmp,"  Temp1 %d, Temp2 %d, Power %d\r\n",Settings.TempAndPower[PCM_OFFSET + pcm].temp1,Settings.TempAndPower[PCM_OFFSET+pcm].temp2,Settings.TempAndPower[PCM_OFFSET+pcm].current);
            PutResponse(0,tmp);
            pcm++;
        }
        else if(M23_MP_IS_M1553(i) ) //M1553
        {
            sprintf(tmp,"M1553 %d\r\n",m1553);
            PutResponse(0,tmp);
            sprintf(tmp,"  Temp1 %d, Temp2 %d, Power %d\r\n",Settings.TempAndPower[M1553_OFFSET + m1553].temp1,Settings.TempAndPower[M1553_OFFSET+m1553].temp2,Settings.TempAndPower[M1553_OFFSET+m1553].current);
            PutResponse(0,tmp);
            m1553++;
        }
        else //This is a video Board
        {
            sprintf(tmp,"Video %d\r\n",video);
            PutResponse(0,tmp);
            sprintf(tmp,"  Temp1 %d, Temp2 %d, Power %d\r\n",Settings.TempAndPower[VIDEO_OFFSET + video].temp1,Settings.TempAndPower[VIDEO_OFFSET+video].temp2,Settings.TempAndPower[VIDEO_OFFSET+video].current);
            PutResponse(0,tmp);
            video++;
        }
    }
#endif


/**********These are not used at this time********************/
#if 0
    if(Settings.Serial1 == 0)
    {
        sprintf(tmp,"SERIAL1      NONE\r\n");
    } 
    else if(Settings.Serial1 == 1)
    {
        sprintf(tmp,"SERIAL1      PCM\r\n");
    } 
    else if(Settings.Serial1 == 2)
    {
        sprintf(tmp,"SERIAL1      VID\r\n");
    } 
    else if(Settings.Serial1 == 3)
    {
        sprintf(tmp,"SERIAL1      MIL1553\r\n");
    } 

    PutResponse(0,tmp);

    if(Settings.Serial2 == 0)
    {
        sprintf(tmp,"SERIAL2      NONE\r\n");
    } 
    else if(Settings.Serial2 == 1)
    {
        sprintf(tmp,"SERIAL2      PCM\r\n");
    } 
    else if(Settings.Serial2 == 2)
    {
        sprintf(tmp,"SERIAL2      VID\r\n");
    } 
    else if(Settings.Serial2 == 3)
    {
        sprintf(tmp,"SERIAL2      MIL1553\r\n");
    } 

    PutResponse(0,tmp);
#endif

#if 0
    sprintf(tmp,"MASK1553     0x%x\r\n",Settings.M1553StatusMask);
    PutResponse(0,tmp);
    sprintf(tmp,"MASKPCM      0x%x\r\n",Settings.PCMStatusMask);
    PutResponse(0,tmp);
#endif


}

void M23_SetScript(int value)
{
    Settings.Script= value;
}

void M23_GetScript(int *value)
{
    *value = Settings.Script;
}


void M23_ProgramTimeValues()
{
    UINT32             CSR;
    M23_CHANNEL  const *config;

    SetupGet(&config);


    /*If Time Cal on set the bit*/
    if(Settings.TimeDebug == 1)
    {
        CSR = M23_ReadControllerCSR(CONTROLLER_TIMECODE_ADJUST_CSR);
        M23_WriteControllerCSR(CONTROLLER_TIMECODE_ADJUST_CSR,CSR | CONTROLLER_TIMECODE_DEBUG);

    }

    /*Program the Time Adjustments*/

    if(config->timecode_chan.Format == 'B')
    {
        M23_WriteControllerCSR(CONTROLLER_TIMECODE_ADJUST_CSR,Settings.TimeAdjust);
    }
    else if(config->timecode_chan.Format == 'A')
    {
        M23_WriteControllerCSR(CONTROLLER_TIMECODE_ADJUST_CSR,Settings.OnTimeA);
    }
    else
    {
        M23_WriteControllerCSR(CONTROLLER_TIMECODE_ADJUST_CSR,Settings.OnTimeG);
    }


}

int SetTempValues()
{
    int    temp;
    int    i;
    int    size;
    int    NumMP;
    int    pcm_index = 0;
    int    m1553_index = 0;
    char   tmp[20];
    int    video_index = 0;
    int    total = 1; /*Always have Controller, Change to 2 with full system Power Supply*/
  
    char   buffer[8];


    M23_NumberMPBoards(&NumMP);
    total += NumMP;

    PutResponse(0,"Enter Temperature Thresholds (MIN -40C,MAX 85C)\r\n");


    memset(buffer,0x0,8);
    PutResponse(0,"Controller Temp 1->");
    SerialGetLine(0, buffer,&size);
 
    if(size > 1)
    {
        temp = atoi(buffer);
        if( (temp < -40) || (temp > 85) )
        {
            return(-1);
        }
        Settings.TempAndPower[CONTROLLER_OFFSET].temp1 = temp;
    }

    memset(buffer,0x0,8);
    PutResponse(0,"Controller Temp 2->");
    SerialGetLine(0, buffer,&size);
 
    if(size > 1)
    {
        temp = atoi(buffer);
        if( (temp < -40) || (temp > 85) )
        {
            return(-1);
        }
        Settings.TempAndPower[CONTROLLER_OFFSET].temp2 = temp;
    }
            
    for(i = 0; i < NumMP; i++)
    {
        if(M23_MP_IS_PCM(i) ) //PCM
        {
            memset(buffer,0x0,8);
            PutResponse(0,"PCM Board Temp 1 ->");
            SerialGetLine(0, buffer,&size);
 
            if(size > 1)
            {
                temp = atoi(buffer);
                if( (temp < -40) || (temp > 85) )
                {
                    return(-1);
                }
                Settings.TempAndPower[PCM_OFFSET + pcm_index].temp1 = temp;
            }

            memset(buffer,0x0,8);
            PutResponse(0,"PCM Board Temp 2 ->");
            SerialGetLine(0, buffer,&size);
 
            if(size > 1)
            {
                temp = atoi(buffer);
                if( (temp < -40) || (temp > 85) )
                {
                    return(-1);
                }
                Settings.TempAndPower[PCM_OFFSET + pcm_index].temp2 = temp;
            }
            pcm_index++;
        }
        else if(M23_MP_IS_M1553(i) ) //M1553
        {
            memset(buffer,0x0,8);
            PutResponse(0,"M1553 Board Temp 1 ->");
            SerialGetLine(0, buffer,&size);
 
            if(size > 1)
            {
                temp = atoi(buffer);
                if( (temp < -40) || (temp > 85) )
                {
                    return(-1);
                }
                Settings.TempAndPower[M1553_OFFSET + m1553_index].temp1 = temp;
            }

            memset(buffer,0x0,8);
            PutResponse(0,"M1553 Board Temp 2 ->");
            SerialGetLine(0, buffer,&size);
 
            if(size > 1)
            {
                temp = atoi(buffer);
                if( (temp < -40) || (temp > 85) )
                {
                    return(-1);
                }

                Settings.TempAndPower[M1553_OFFSET + m1553_index].temp2 = temp;
            }
            m1553_index++;
        }
        else //This is a video Board
        {
            memset(buffer,0x0,8);
            memset(tmp,0x0,20);
            sprintf(tmp,"Video %d Temp 1 ->",video_index+1);
            PutResponse(0,tmp);
            SerialGetLine(0, buffer,&size);
 
            if(size > 1)
            {
                temp = atoi(buffer);
                if( (temp < -40) || (temp > 85) )
                {
                    return(-1);
                }
                Settings.TempAndPower[VIDEO_OFFSET+video_index].temp1 = temp;
            }

            memset(buffer,0x0,8);
            sprintf(tmp,"Video %d Temp 2 ->",video_index+1);
            PutResponse(0,tmp);
            SerialGetLine(0, buffer,&size);
 
            if(size > 1)
            {
                temp = atoi(buffer);
                if( (temp < -40) || (temp > 85) )
                {
                    return(-1);
                }

                Settings.TempAndPower[VIDEO_OFFSET + video_index].temp2 = temp;
            }
            video_index++;
        }
    }

    return 0;
}

int SetPowerValues()
{
    int    temp;
    int    i;
    int    size;
    int    NumMP;
    int    pcm_offset = 0;
    int    m1553_offset = 0;
    int    video_index = 0;
    char   tmp[20];
    int    total = 1; /*Always have Controller, Change to 2 with full system Power Supply*/
  
    char   buffer[8];


    M23_NumberMPBoards(&NumMP);
    total += NumMP;

    PutResponse(0,"Enter Power Thresholds(in mA)\r\n");


    memset(buffer,0x0,8);
    PutResponse(0,"Controller Board Power->");
    SerialGetLine(0, buffer,&size);
 
    if(size > 1)
    {
        temp = atoi(buffer);
        Settings.TempAndPower[CONTROLLER_OFFSET].current = temp;
    }

    for(i = 0; i < NumMP; i++)
    {
        if(M23_MP_IS_PCM(i) ) //PCM
        {
            memset(buffer,0x0,8);
            memset(tmp,0x0,20);
            sprintf(tmp,"PCM Board %d Power ->",pcm_offset + 1);
            PutResponse(0,tmp);
            //PutResponse(0,"PCM Board Power ->");
            SerialGetLine(0, buffer,&size);
 
            if(size > 1)
            {
                temp = atoi(buffer);
                Settings.TempAndPower[PCM_OFFSET + pcm_offset].current = temp;
            }
            pcm_offset++;

        }
        else if(M23_MP_IS_M1553(i) ) //M1553
        {
            memset(buffer,0x0,8);
            //PutResponse(0,"M1553 Board Power ->");
            memset(tmp,0x0,20);
            sprintf(tmp,"M1553 Board %d Power ->",m1553_offset + 1);
            PutResponse(0,tmp);
            SerialGetLine(0, buffer,&size);
 
            if(size > 1)
            {
                temp = atoi(buffer);
                Settings.TempAndPower[M1553_OFFSET + m1553_offset].current = temp;
            }
            m1553_offset++;
        }
        else //This is a video Board
        {
            memset(buffer,0x0,8);
            memset(tmp,0x0,20);
            sprintf(tmp,"Video Board %d Power ->",video_index+1);
            PutResponse(0,tmp);
            SerialGetLine(0, buffer,&size);
 
            if(size > 1)
            {
                temp = atoi(buffer);
                Settings.TempAndPower[VIDEO_OFFSET+video_index].current = temp;
            }
            video_index++;
        }
    }

    return 0;
}

void DisplayPower()
{
    int  i;
    int  NumMP;
    int  mp_index = 0;
    int  pcm_index = 0;
    int  m1553_index = 0;
    int  video_index = 0;
    int  total = 1; /*Always have Controller, Change to 2 with full system Power Supply*/
    char tmp[20];

    struct I2c_Temp_Struct DeviceStatus[7];

    memset(tmp,0x0,20);

    M23_NumberMPBoards(&NumMP);
    total += NumMP;

    M2300_I2C_Get_DeviceTemperature(total,DeviceStatus);

    PutResponse(0,"         Power Readings           \r\n");
    PutResponse(0,"    (Current Thresholds(mA in () )      \r\n");
    PutResponse(0,"\r\nSlot   Board Type         Power   \r\n");
    PutResponse(0,"--------------------------------------\r\n");

    for(i = 0; i < total; i++)
    {
        sprintf(tmp," %d",i+1);
        PutResponse(0,tmp);

        if(DeviceStatus[i].BoardType == 0x01)
        {
            sprintf(tmp,"     CONTROLLER(%d)",Settings.TempAndPower[CONTROLLER_OFFSET].current);
            PutResponse(0,tmp);
            sprintf(tmp,"      %03d",DeviceStatus[i].current);
            PutResponse(0,tmp);
        }
        else if(DeviceStatus[i].BoardType == 0x04)
        {
            if(M23_MP_IS_PCM(mp_index) ) //PCM
            {
                sprintf(tmp,"     PCM Board(%d)",Settings.TempAndPower[PCM_OFFSET + pcm_index].current);
                PutResponse(0,tmp);
                sprintf(tmp,"    %03d",DeviceStatus[i].current);
                pcm_index++;
            }
            else if(M23_MP_IS_M1553(mp_index) ) //M1553
            {
                sprintf(tmp,"     M1553 Board(%d)",Settings.TempAndPower[M1553_OFFSET + m1553_index].current);
                PutResponse(0,tmp);
                sprintf(tmp,"  %03d",DeviceStatus[i].current);
                m1553_index++;
            }

            PutResponse(0,tmp);

            mp_index++;
        }
        else if(DeviceStatus[i].BoardType == 0x06) //Video Board
        {
            sprintf(tmp,"     VIDEO(%d)   ",Settings.TempAndPower[VIDEO_OFFSET+video_index].current);
            PutResponse(0,tmp);
            sprintf(tmp,"        %03d",DeviceStatus[i].current);
            PutResponse(0,tmp);
            video_index++;
        }
      
        PutResponse(0,"\r\n");

    }
}

void DisplayTemp()
{
    int  i;
    int  NumMP;
    int  mp_index = 0;
    int  pcm_index = 0;
    int  m1553_index = 0;
    int  video_index = 0;
    int  total = 1; /*Always have Controller, Change to 2 with full system Power Supply*/
    char tmp[80];

    struct I2c_Temp_Struct DeviceStatus[7];

    memset(tmp,0x0,80);

    M23_NumberMPBoards(&NumMP);
    total += NumMP;


    M2300_I2C_Get_DeviceTemperature(total,DeviceStatus);

    PutResponse(0,"         Temperature Readings           \r\n");
    PutResponse(0,"    (Current Thresholds(Celsius) in () )       \r\n");
    PutResponse(0,"\r\nSlot   Board Type         Temp1   Temp2  \r\n");
    PutResponse(0,"------------------------------------------\r\n");

    for(i = 0; i < total; i++)
    {
        sprintf(tmp," %d",i+1);
        PutResponse(0,tmp);

        if(DeviceStatus[i].BoardType == 0x01)
        {
            sprintf(tmp,"     CONTROLLER(%d,%d)",Settings.TempAndPower[CONTROLLER_OFFSET].temp1,Settings.TempAndPower[CONTROLLER_OFFSET].temp2);
            PutResponse(0,tmp);
            sprintf(tmp,"   %02d      %02d",DeviceStatus[i].dev1TemperatureInternal,DeviceStatus[i].dev1TemperatureExternal);
            PutResponse(0,tmp);
        }
        else if(DeviceStatus[i].BoardType == 0x04)
        {
            if(M23_MP_IS_PCM(mp_index) ) //PCM
            {
                sprintf(tmp,"     PCM Board(%d,%d)",Settings.TempAndPower[PCM_OFFSET + pcm_index].temp1,Settings.TempAndPower[PCM_OFFSET + pcm_index].temp2);
                PutResponse(0,tmp);
                sprintf(tmp,"    %02d      %02d",DeviceStatus[i].dev1TemperatureInternal,DeviceStatus[i].dev1TemperatureExternal);
                pcm_index++;
            }
            else if(M23_MP_IS_M1553(mp_index) ) //M1553
            {
                sprintf(tmp,"     M1553 Board(%d,%d)",Settings.TempAndPower[M1553_OFFSET + m1553_index].temp1,Settings.TempAndPower[M1553_OFFSET + m1553_index].temp2);
                PutResponse(0,tmp);
                sprintf(tmp,"  %02d      %02d",DeviceStatus[i].dev1TemperatureInternal,DeviceStatus[i].dev1TemperatureExternal);
                m1553_index++;
            }
            PutResponse(0,tmp);

            mp_index++;
        }
        else if(DeviceStatus[i].BoardType == 0x06)
        {
            sprintf(tmp,"     VIDEO(%d,%d)   ",Settings.TempAndPower[VIDEO_OFFSET+video_index].temp1,Settings.TempAndPower[VIDEO_OFFSET+video_index].temp2);
            PutResponse(0,tmp);
            sprintf(tmp,"     %02d      %02d",DeviceStatus[i].dev1TemperatureInternal,DeviceStatus[i].dev1TemperatureExternal);

            PutResponse(0,tmp);

            video_index++;
            mp_index++;
        }

        PutResponse(0,"\r\n");

    }

}

void CheckTemp()
{
    int  i;
    int  NumMP;
    int  mp_index = 0;
    int  pcm_index = 0;
    int  m1553_index = 0;
    int  video_index = 0;
    int  total = 1; /*Always have Controller, Power Supply and Firewire Board*/
    char tmp[20];

    struct I2c_Temp_Struct DeviceStatus[7];

    memset(tmp,0x0,20);


    M23_NumberMPBoards(&NumMP);
    total += NumMP;

    memset(ControllerHealthTemps,0x0,4*2);
    memset(PowerSupplyHealthTemps,0x0,4*2);
    memset(VideoHealthTemps,0x0,4*12);
    memset(PCMHealthTemps,0x0,4*12);
    memset(M1553HealthTemps,0x0,4*12);

    ControllerPower = 0;
    FirewirePower = 0;
    for(i = 0;i < 5;i++)
    {
        VideoPower[i] = 0;
        M1553Power[i] = 0;
        PCMPower[i] = 0;
    }

    M2300_I2C_Get_DeviceTemperature(total,DeviceStatus);

    for(i = 0; i < total; i++)
    {
        if(DeviceStatus[i].BoardType == 0x01) //Controller
        {
            if(DeviceStatus[i].dev1TemperatureInternal > Settings.TempAndPower[CONTROLLER_OFFSET].temp1)
            {
                ControllerHealthTemps[0] = 1;
            }
            if(DeviceStatus[i].dev2Temperature > Settings.TempAndPower[CONTROLLER_OFFSET].temp2)
            {
                ControllerHealthTemps[1] = 1;
            }

            if(DeviceStatus[i].current < Settings.TempAndPower[CONTROLLER_OFFSET].current)
            {
                ControllerPower = 1;
            }
        }
        else if(DeviceStatus[i].BoardType == 0x04)
        {
            if(M23_MP_IS_PCM(mp_index) ) //PCM
            {
                if(DeviceStatus[i].dev1TemperatureInternal > Settings.TempAndPower[PCM_OFFSET + pcm_index].temp1)
                {
                    PCMHealthTemps[pcm_index][0] = 1;
                }
                if(DeviceStatus[i].dev1TemperatureExternal > Settings.TempAndPower[PCM_OFFSET + pcm_index].temp2)
                {
                    PCMHealthTemps[pcm_index][1] = 1;
                }
                if(DeviceStatus[i].current < Settings.TempAndPower[PCM_OFFSET + pcm_index].current)
                {
                    PCMPower[pcm_index] = 1;
                }
                pcm_index++;
            }
            else if(M23_MP_IS_M1553(mp_index) ) //M1553
            {
                if(DeviceStatus[i].dev1TemperatureInternal > Settings.TempAndPower[M1553_OFFSET + m1553_index].temp1)
                {
                    M1553HealthTemps[m1553_index][0] = 1;
                }

                if(DeviceStatus[i].dev1TemperatureExternal > Settings.TempAndPower[M1553_OFFSET + m1553_index].temp2)
                {
                    M1553HealthTemps[m1553_index][1] = 1;
                }

                if(DeviceStatus[i].current < Settings.TempAndPower[M1553_OFFSET + m1553_index].current)
                {
                    M1553Power[m1553_index] = 1;
                }
                m1553_index ++;
            }
         
            mp_index++;
        }
        else if(DeviceStatus[i].BoardType == 0x06)
        {
            if(DeviceStatus[i].dev1TemperatureInternal > Settings.TempAndPower[VIDEO_OFFSET + video_index].temp1)
            {
                VideoHealthTemps[video_index][0] = 1;
            }

            if(DeviceStatus[i].dev1TemperatureExternal > Settings.TempAndPower[VIDEO_OFFSET + video_index].temp2)
            {
                VideoHealthTemps[video_index][1] = 1;
            }


            if(DeviceStatus[i].current < Settings.TempAndPower[VIDEO_OFFSET + video_index].current)
            {
                VideoPower[video_index] = 1;
            }

            video_index++;
            mp_index++;
        }

    }

}
