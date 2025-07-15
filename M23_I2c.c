#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include "M23_Controller.h"
#include "M23_I2c.h"
#include "M23_MP_Handler.h"

#define I2C_TARGET "/dev/i2c-0"

#define CONTROLLER_BOARD 0x01 
#define VIDEO_BOARD      0x02 
#define PS_BOARD         0x03 
#define MP_BOARD         0x04
#define FW_BOARD         0x05
 

#define PCF8591_RD   0x90
#define PCF8591_WR   0x91
#define DS1621_RD   0x92
#define DS1621_WR   0x93
#define PCA9500     0x40 
#define PCA9548     0xE0
#define MAX6692     0x98

void perror_quit(char *str);

int init_UDS1380(int FileDe);
int init_SAA7115H(int FileDe,int channel);
int SAA7115H_Reset(int FileDe,int channel);
int init_SAA6752HS(int FileDe,int channel);     
int SAA6752HS_Reset(int FileDe,int channel);


int stat_UDS1380(int FileDe);
int stat_SAA7115H(int FileDe);
int stat_SAA6752HS(int FileDe);

static int MPEG_GOP[16];
static int MPEG_SystemBitRate[16];
static int MPEG_MaximumBitRate[16];
static int MPEG_TargetBitRate[16];
static int MPEG_SVideo[16];
static int MPEG_SVideo_SA9[16];
static int MPEG_SVideo_SA10[16];
static int MPEG_SVideo_SA88[16];

static UINT16  MPEG_AudioGain;

static int MPEG_VideoEnabled[16];
static int MPEG_AudioEnabled[16];

static int MPEG_BW[16];

static int VideoOut0 = 0xB;
static int VideoOut4 = 0x7;
static int VideoOut5 = 0x38;
static int VideoOut6 = 0x0;

void M23_SetupVideoI2c_Init()
{
    int i;
    int index = 0;
    int NumMPBoards;

    M23_NumberMPBoards(&NumMPBoards);

    for(i = 0; i < NumMPBoards;i++)
    {
        if(M23_MP_IS_Video(i))
        {
            M23_MP_Initialize_Video_I2c((MP_DEVICE_0 + i) ,index);
            index++;
        }
    }

}


void M23_I2c_SetRegisters(int reg0,int reg4,int reg5, int reg6)
{
    VideoOut0 = reg0;
    VideoOut4 = reg4;
    VideoOut5 = reg5;
    VideoOut6 = reg6;
}

/*-----------------------------------------------------------------------------------*/
//
//  M2300_I2C_Get_DeviceTemperature()
//
//  Function to get the temperatures from devices
//  Input   : Number of busses available
//  OutPut  : Structure that holds the temperature on all the devices
//  Returns : 0 if Pass
//            1 if Failed  
//
/*-----------------------------------------------------------------------------------*/
int M2300_I2C_Get_DeviceTemperature(int numOfBusses,struct I2c_Temp_Struct* DeviceStatus)
{
    int fd;                                 //File Descriptor 
    int i = 0;
    int j = 0;
    int averageHolder;

    int numBoardsAvailable = numOfBusses;   // number of boards on the backplane
    int s_addr = 0;                         // Slave Address

    unsigned char ch;                       // Unsigned Temperature value holder 
    signed char   ch2;                        // signed Temperature value holder
    int           state;
    int           loops;
 
 
    //Check if the number of busses is 
    //withing the valid range

    if(numBoardsAvailable < 1 || numBoardsAvailable > 7)
    {
        printf("ERROR: Number of boards out of range\r\n");
        return(1);
    }

    /*----------------------------------------------------*/

    //Initialize the structure with zero's
    memset(DeviceStatus,0,sizeof(DeviceStatus));

    /*----------------------------------------------------*/

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
	perror_quit("Error opening I2C Device");
        return(1);
    }
    /*----------------------------------------------------*/
    
    //Go through all the boards on the busses
    for(i = 0 ; i < numBoardsAvailable ; i++)
    {

        if(SlotContainsBoard[i] == 1)
        {
            /*----------------------------------------------------*/
        
            s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

	    if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
            {
    	        printf("ioctl->Setting Slave E0\r\n");
                break;
            }

            /*----------------------------------------------------*/


            //Based on which bus is being talked to 
            //we set the control register value to 
            //enable that bus
            ch = (1 << i);
         
            if(write(fd, &ch, 1) != 1)
            {
    	        printf("writing chan %d\r\n",ch);
            }

            /*----------------------------------------------------*/

            //Read to be sure right value got written
	    if(read(fd, &ch, 1) != 1)
            {
	        printf("reading chan\r\n");
            }

            /*This is needed for the Video Board*/
            s_addr = 0xE2;//0xE0 is the address for Switch PCA9548

	    if(ioctl(fd, 0x0703, s_addr >> 1) < 0)
            {
    	        printf("ioctl->Setting Slave E2\r\n");
            }
            ch = 1;
            write(fd, &ch, 1);
            /*Done with stuff needed for Video Board*/

            /*----------------------------------------------------*/
            //GET BOARD TYPE FROM PCA9500
            /*----------------------------------------------------*/
            //Set slave address to PCA9500 chip -> read   
            s_addr = PCA9500;

            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
            {
    	        printf("ioctl-> Getting Board type\r\n");
            }

            //Read the Board Type
            if(read(fd, &ch, 1) != 1)
            {
                //Device read unsuccessfull store 0xFF to the structure
                DeviceStatus[i].BoardType = 0xFF;

            }
            else
            {

                //Device read successfull store the temperature to the structure
                DeviceStatus[i].BoardType = ( ch >> 3);
            }

            
            if(DeviceStatus[i].BoardType != FW_BOARD)
            {

                /*----------------------------------------------------*/
                //  GET TEMPERATURE 1 FROM MAX6692
                /*----------------------------------------------------*/
                //Set slave address to MAX6692 chip -> read  
                s_addr = MAX6692;

                /*----------------------------------------------------*/

                if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
	            perror_quit("ioctl");

                /*----------------------------------------------------*/

                //Write 0x00 to the Get internal temperature at offset 0x00

                ch = 0x00;

   	        if(write(fd, &ch, 1) != 1)
                {
            
                    //perror_quit("write");
                    //printf("Failed Writing offset 0x%02x on bus %d (Base 0)\r\n",s_addr,i);
        
                }
                else
                {

                    if(read(fd, &ch, 1) != 1)
                    {
    	                //perror_quit("read");

                        //printf("Failed reading offset 0x%02x on bus %d (Base 0)\r\n",s_addr,i);

                        //Device read un success full store 0xFF to the structure
                        DeviceStatus[i].dev1TemperatureInternal = 0xFF;

                    }
                    else
                    {
                        DeviceStatus[i].dev1TemperatureInternal = ch;
                        ch = 0;
                    }

                }

                /*----------------------------------------------------*/

                //Write 0x01 to the Get external temperature at offset 0x01

                ch = 0x01;

                if(write(fd, &ch, 1) != 1)
                {
            
                    //perror_quit("write");
                    //printf("Failed Writing offset 0x%02x on bus %d (Base 0)\r\n",s_addr,i);
        
                }
                else
                {

       	            if(read(fd, &ch, 1) != 1)
                    {
    	                //perror_quit("read");

                        //printf("Failed reading offset 0x%02x on bus %d (Base 0)\r\n",s_addr,i);

                        //Device read un success full store 0xFF to the structure
                        DeviceStatus[i].dev1TemperatureExternal = 0xFF;
                    }
                    else
                    {

                        //Device read successfull store the temperature to the structure
                        DeviceStatus[i].dev1TemperatureExternal = ch;
                        ch = 0;
                    }
                }
            }


            /*----------------------------------------------------*/
            //GET TEMPERATURE 2 FROM DS1621
            /*----------------------------------------------------*/

            //Set the device to write DS1631 DS1621 0x93
            s_addr = DS1621_WR;

            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
	        perror_quit("ioctl");


        /*----------------------------------------------------*/

            //Write 0xaa to the Control register 
            //to allow reading Temperature

            ch = 0xaa;

            if(write(fd, &ch, 1) != 1)
            {
            
                //perror_quit("write");
                //printf("Failed Writing offset 0x%02x on bus %d (Base 0)\r\n",s_addr,i);
        
            }

            /*----------------------------------------------------*/

            //Set the device to read DS1631 DS1621 0x92
            s_addr = DS1621_RD;

            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
            {
    	        perror_quit("ioctl");
            }

            /*----------------------------------------------------*/

	    if(read(fd, &ch2, 1) != 1)
            {
    	        //perror_quit("read");

                //printf("Failed reading offset 0x%02x on bus %d (Base 0)\r\n",s_addr,i);
                DeviceStatus[i].dev2Temperature = 0xFF;
            }
            else
            {

                //Store the signed temperature to the structure
                DeviceStatus[i].dev2Temperature = ch2;
                ch2 = 0;

            }

            if( (DeviceStatus[i].BoardType != VIDEO_BOARD) && (DeviceStatus[i].BoardType != MP_BOARD) )
            {
                /*----------------------------------------------------*/
                //GET CURRENT FROM PCF8591
                /*----------------------------------------------------*/

                //Set the device to write PCF8591_WR 0x91
                s_addr = PCF8591_WR;

                if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
	            perror_quit("ioctl");


                /*----------------------------------------------------*/
                //Write 0x00 to the Control register 
                //to allow reading Current

                ch = 0x00;
                write(fd,&ch,1);

                /*----------------------------------------------------*/

                //Set the device to read PCF8591_RD 0x90
                s_addr = PCF8591_RD;

                if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                {
    	            perror_quit("ioctl");
                }
                /*----------------------------------------------------*/

                if(read(fd, &ch2, 1) != 1)
                {
    	            //perror_quit("read");

                    //printf("Failed reading offset 0x%02x on bus %d (Base 0)\r\n",s_addr,i);
                    DeviceStatus[i].current = 0xFF;
                }
                else
                {

                    read(fd, &ch2, 1);

                    //printf("****mAMPS = %d ***0x%x \r\n",ch2,ch2);

                    if(ch2 == (-1))
                    {
                        DeviceStatus[i].current = (int)0xFF;
                    }
                    else
                    {

                        averageHolder = 0;
    
                        //Paul Add If Record State
                        M23_RecorderState(&state);
                        if(state == STATUS_RECORD)
                        {
                            loops = 10;
                        }
                        else
                        {
                            loops = 100;
                        }



                        //for(j = 0 ; j < 100 ; j++)
                        for(j = 0 ; j < loops ; j++)
                        {
                            if(read(fd, &ch2, 1) != 1)
                            {
    	                        //perror_quit("read");

                                //printf("Failed reading offset 0x%02x on bus %d (Base 0)\r\n",s_addr,i);
                                DeviceStatus[i].current = (int)0xFF;
                                break;
                            }
                            else
                            {
                                averageHolder = averageHolder + (int)ch2;
                           }
                        }

                        if(DeviceStatus[i].current == (int)0xFF)
                        {
                        }
                        else
                        {
                            //Store the current in Milli AMPS to the structure
                            //DeviceStatus[i].current = (int)((averageHolder * 8)/100);
                            DeviceStatus[i].current = (int)((averageHolder * 8)/loops);
                            averageHolder = 0;
                        }
                    }
                }
            }
            else
            {
                DeviceStatus[i].current = (int)0xFF;
            }

            /*----------------------------------------------------*/
            //Once the temperatures are stored to 
            //the Structure we initialize the Variables to 0xFF 
            ch2 = 0xFF;
            ch  = 0xFF;
        }
     
    }

    close(fd);

    return(0);
}


void M23_SetAudioGain(int gain,int whichone)
{
    int temp = MPEG_AudioGain;

    if(whichone) //Right Channel
    {
        temp = temp & 0xFF00;
        temp |= gain;
    }
    else  //Left Channel
    {
        temp = temp & 0x00FF;
        temp |= (gain << 8);
    }

    MPEG_AudioGain = (UINT16)temp;
}

void M23_SetI2cDefault()
{
    int i;

    for(i=0;i<16;i++)
    {
        MPEG_GOP[i]            = 0x010F;
        MPEG_SystemBitRate[i]  = 4000;
        MPEG_MaximumBitRate[i] = 3600;
        MPEG_TargetBitRate[i]  = 3200;
        MPEG_VideoEnabled[i]   = 0x0; //Video Enabled
        MPEG_AudioEnabled[i]   = 0x0; //Audio Enabled
        MPEG_SVideo[i]         = 0xC0;
        MPEG_SVideo_SA9[i]     = 0x40;
        MPEG_SVideo_SA88[i]    = 0x68;
        MPEG_SVideo_SA10[i]    = 0x06;
        MPEG_BW[i]             = 0x0;


    }

    MPEG_AudioGain = 0x0404;

}


/***********************************************************************************************
 *
 * This will fix initialization of the I2c using multiple reads
 *
 **********************************************************************************************/
void M23_FixI2cInitialization()
{
    int fd;                                 //File Descriptor
    int s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    int i = 0;
    /*----------------------------------------------------*/

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        perror_quit("Error opening I2C Device");
    }
    else
    {
        s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
        {
            perror_quit("ioctl");
        }
        else
        {
            for(i = 0; i<10;i++)
            {
                if(read(fd, &ch, 1) != 1)
                {
                }
                else
                {
                    i = 10; 
                }
            }

            if( i == 10)
            {
                printf("I2C Failed 10 times!!!!\r\n");
            }
        }
    }

}

/***********************************************************************************************
 *
 * This will Disable the Video for this channel. 
 *
 **********************************************************************************************/
void M23_I2C_SetVideoMode(int channel,int mode)
{
    if(mode == 0) //Composite
    {
        MPEG_SVideo[channel]      = 0xC0;
        MPEG_SVideo_SA9[channel]  = 0x40;
        MPEG_SVideo_SA10[channel] = 0x06;
        MPEG_SVideo_SA88[channel]  = 0x68;
    }
    else //SVIDEO
    {
        MPEG_SVideo[channel] = 0xC8;
        MPEG_SVideo_SA9[channel] = 0x80;
        MPEG_SVideo_SA10[channel] = 0x0E;
        MPEG_SVideo_SA88[channel]  = 0xE8;
    }
}

/***********************************************************************************************
 *
 * This will Disable the Video for this channel. 
 *
 **********************************************************************************************/
void M23_I2C_DisableVideo(int channel)
{
    MPEG_VideoEnabled[channel] = 0x1; //Video Disabled
}


/***********************************************************************************************
 *
 * This will Disable the Audio for this channel. 
 *
 **********************************************************************************************/
void M23_I2C_DisableAudio(int channel)
{
    MPEG_AudioEnabled[channel] = 0x1; //Audio Disabled
}


/***********************************************************************************************
 *
 * This will set the Bit Rate via the I2c Bus. 
 *
 **********************************************************************************************/
int M2300_I2C_SetMPEG_Bitrate(int channel,int BitRate) //In Kb/s 0x1000 = 4096 bit/s
{
    int overhead;


    if((1000 <= BitRate)  && (20000 >=  BitRate ))
    {

        overhead = 320 + 110 + 256 + (100 * (BitRate/4000));

        MPEG_SystemBitRate[channel]  = BitRate;
        //MPEG_MaximumBitRate[channel] = (MPEG_SystemBitRate[channel] - 256 - 128);
        MPEG_MaximumBitRate[channel] = (MPEG_SystemBitRate[channel] - overhead);
        MPEG_TargetBitRate[channel]  = ((MPEG_MaximumBitRate[channel] * 80)/100);

    }
    else
    {
        perror_quit("Bit Rate Out of Range\n");
        return(1);
    }

    return(0);
}

/***********************************************************************************************
 *
 * This will set the GOP via the I2c Bus. 
 *
 **********************************************************************************************/
int M2300_I2C_SetMPEG_GOP(int channel,int M_val,int N_val)
{
    if(((0 <= M_val)  && (3 >=  M_val )) && ((0 <= N_val)  && (19 >=  N_val )))
    {
        MPEG_GOP[channel] = (M_val << 8);
        MPEG_GOP[channel] |=  N_val;
//printf("Setting GOP for channel %d, 0x%x,%d,%d\r\n",channel,MPEG_GOP[channel],M_val,N_val);
    }
    else
    {
        perror_quit("GOP Values Out of Range\n");
        return(1);
    }

    return(0);
}

/***********************************************************************************************
 *
 * This will start the compressor chip via the I2c Bus. 
 *
 **********************************************************************************************/
int M23_I2C_StartCompressor(int channel,int BusNumber)
{

    int fd;                                 //File Descriptor
    int s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    unsigned char buff[2];

    channel++;

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        perror_quit("Error opening I2C Device");
        return(1);
    }
    else
    {
        s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
        {
            perror_quit("ioctl");
        }
        else
        {
            ch = BusNumber;

            if(write(fd, &ch, 1) != 1)
            {
                //perror_quit("write");
                if(write(fd, &ch, 1) != 1)
                {
                    perror_quit("second write - Start Compressor");
                }
            }
            else
            {
                //Read to be sure right valur got written
                if(read(fd, &ch, 1) != 1)
                {
                    //perror_quit("read");
                    if(read(fd, &ch, 1) != 1)
                    {
                        perror_quit("second read - 2");
                    }
                }
                else
                {
                     s_addr = 0xE2;//0xE2 is the address for Switch

                     if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                     {
                        perror_quit("ioctl");
                     }
                     else
                     {
                        ch = (1 << channel);

                        if(write(fd, &ch, 1) != 1)
                        {
                            if(write(fd, &ch, 1) != 1)
                            {
                                perror_quit("second write");
                            }
                        }
                        else
                        {
                            //Set slave address to MPEG2 chip -> read
                            s_addr = 0x40;//SAA6752HS

                            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                            {
                                perror_quit("ioctl");
                            }
                            else
                            {
                                /*Stop The Compressor*/
                                memset(buff,0,sizeof(buff));

                                buff[0] = 0x02; //This sets the compressor to Encode Mode

                                if(write(fd, &buff, 1) != 1)
                                {
                                    if(write(fd, &buff, 1) != 1)
                                    {
                                        perror_quit("SAA6752HS write");
                                        return(1);
                                    }
                            
                                }
                            }
                        }
                     }
                }
            }
        }
    }

    return(0);
}

/***********************************************************************************************
 *
 * This will read the compressor state. 
 *
 **********************************************************************************************/
int M23_I2C_GetCompressorState(int channel,int BusNumber)
{

    int fd;                                 //File Descriptor
    int state = 0;
    int s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    unsigned char buff[2];

    channel++;

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        perror_quit("Error opening I2C Device");
        state = -1;
    }
    else
    {
        s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
        {
            perror_quit("ioctl");
            state = -1;
        }
        else
        {
            ch = BusNumber;

            if(write(fd, &ch, 1) != 1)
            {
                //perror_quit("write");
                if(write(fd, &ch, 1) != 1)
                {
                    perror_quit("second write");
                    state = -1;
                }
            }
            else
            {
                //Read to be sure right valur got written
                if(read(fd, &ch, 1) != 1)
                {
                    //perror_quit("read");
                    if(read(fd, &ch, 1) != 1)
                    {
                        perror_quit("second read - 3");
                        state = -1;
                    }
                }
                else
                {
                     s_addr = 0xE2;//0xE2 is the address for Switch

                     if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                     {
                        perror_quit("ioctl");
                        state = -1;
                     }
                     else
                     {
                        ch = (1 << channel);

                        if(write(fd, &ch, 1) != 1)
                        {
                            if(write(fd, &ch, 1) != 1)
                            {
                                perror_quit("second write");
                                state = -1;
                            }
                        }
                        else
                        {
                            //Set slave address to MPEG2 chip -> read
                            s_addr = 0x40;//SAA6752HS

                            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                            {
                                perror_quit("ioctl");
                                state = -1;
                            }
                            else
                            {
                                /*Read The Compressor state*/

                                memset(buff,0,sizeof(buff));
                                buff[0] = 0x10;

                                if(write(fd, &buff, 1) != 1)
                                {
                                    if(write(fd, &buff, 1) != 1)
                                    {
                                        perror_quit("SAA6752HS write");
                                        return(1);
                                    }
                            
                                }

                                if(read(fd, &ch, 1) != 1)
                                {
                                    state = -1;
                                }
                                else
                                {
                                    state = ch;
                                }

                            }
                        }
                    }
                }
            }
        }
    }

    return(state);
}

/***********************************************************************************************
 *
 * This will stop the compressor chip via the I2c Bus. 
 *
 **********************************************************************************************/
int M23_I2C_StopCompressor(int channel,int BusNumber)
{

    int fd;                                 //File Descriptor
    int s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    unsigned char buff[2];

    channel++;

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        perror_quit("Error opening I2C Device");
        return(1);
    }
    else
    {
        s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
        {
            perror_quit("ioctl");
        }
        else
        {
            ch = BusNumber;

            if(write(fd, &ch, 1) != 1)
            {
                //perror_quit("write");
                if(write(fd, &ch, 1) != 1)
                {
                    perror_quit("second write");
                }
            }
            else
            {
                //Read to be sure right valur got written
                if(read(fd, &ch, 1) != 1)
                {
                    //perror_quit("read");
                    if(read(fd, &ch, 1) != 1)
                    {
                        perror_quit("second read - 4");
                    }
                }
                else
                {
                     s_addr = 0xE2;//0xE2 is the address for Switch

                     if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                     {
                        perror_quit("ioctl");
                     }
                     else
                     {
                        ch = (1 << channel);

                        if(write(fd, &ch, 1) != 1)
                        {
                            if(write(fd, &ch, 1) != 1)
                            {
                                perror_quit("second write");
                            }
                        }
                        else
                        {
                            //Set slave address to MPEG2 chip -> read
                            s_addr = 0x40;//SAA6752HS

                            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                            {
                                perror_quit("ioctl");
                            }
                            else
                            {
                                /*Stop The Compressor*/
                                memset(buff,0,sizeof(buff));

                                buff[0] = 0x03; //This sets the compressor to Encode Mode

                                if(write(fd, &buff, 1) != 1)
                                {
                                    if(write(fd, &buff, 1) != 1)
                                    {
                                        perror_quit("SAA6752HS write");
                                        return(1);
                                    }
                            
                                }
                            }
                        }
                     }
                }
            }
        }
    }

    return(0);
}


/***********************************************************************************************
 *
 * This will Set the reconfigure bit so the compressor goes to Idle . 
 *
 **********************************************************************************************/
int M23_I2C_ReconfigCompressor(int channel,int BusNumber)
{

    int fd;                                 //File Descriptor
    int s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    unsigned char buff[2];


    channel++;
    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        perror_quit("Error opening I2C Device");
        return(1);
    }
    else
    {
        s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
        {
            perror_quit("ioctl");
        }
        else
        {
            ch = BusNumber;

            if(write(fd, &ch, 1) != 1)
            {
                //perror_quit("write");
                if(write(fd, &ch, 1) != 1)
                {
                    perror_quit("second write");
                }
            }
            else
            {
                //Read to be sure right valur got written
                if(read(fd, &ch, 1) != 1)
                {
                    //perror_quit("read");
                    if(read(fd, &ch, 1) != 1)
                    {
                        printf("second read -5\r\n");
                    }
                }
                else
                {
                     s_addr = 0xE2;//0xE2 is the address for Switch

                     if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                     {
                        perror_quit("ioctl");
                     }
                     else
                     {
                        ch = (1 << channel);

                        if(write(fd, &ch, 1) != 1)
                        {
                            if(write(fd, &ch, 1) != 1)
                            {
                                perror_quit("second write");
                            }
                        }
                        else
                        {
                            //Set slave address to MPEG2 chip -> read
                            s_addr = 0x40;//SAA6752HS

                            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                            {
                                perror_quit("ioctl");
                            }
                            else
                            {
                                /*Stop The Compressor*/
                                memset(buff,0,sizeof(buff));

                                buff[0] = 0x05; //This sets the compressor to idle Mode

                                if(write(fd, &buff, 1) != 1)
                                {
                                    if(write(fd, &buff, 1) != 1)
                                    {
                                        perror_quit("SAA6752HS write");
                                        return(1);
                                    }
                            
                                }
                            }
                        }
                     }
                }
            }
        }
    }

    return(0);
}

/***********************************************************************************************
 *
 * This will initialize the Compressor,Video and Audio chips via the I2c Bus. 
 *
 **********************************************************************************************/
int M2300_I2C_Init_VideoChannel(int BusNumber,int channel)
{

    int fd;                                 //File Descriptor
    int state;
    int s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    int debug;

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        printf("Error opening I2C Device\r\n");
        return(1);
    }

    M23_GetDebugValue(&debug);

    /*----------------------------------------------------*/

    s_addr = PCA9548;//0xE0 is the address for Switch PCA9548


    if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
    {
        printf("ioctl\r\n");
    }
    else
    {

        ch = BusNumber;

        if(write(fd, &ch, 1) != 1)
        {
            //perror_quit("write");
            if(write(fd, &ch, 1) != 1)
            {
                printf("second write - Init Video Channel\r\n");
            }
        }
        else
        {

            //Read to be sure right value got written
            if(read(fd, &ch, 1) != 1)
            {
                //perror_quit("read");
                if(read(fd, &ch, 1) != 1)
                {
                    printf("second read - Init Video Channel\r\n");
                }
            }
            else
            {

                s_addr = 0xE2;//0xE2 is the address for Switch

                if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                {
                    printf("ioctl %d\r\n",s_addr >> 1);
                }
                else
                {
                    ch = (1 << channel);

                    if(write(fd, &ch, 1) != 1)
                    {
                        if(write(fd, &ch, 1) != 1)
                        {
                            printf("second write - 2\r\n");
                        }
                    }
                    else
                    {
                        //Set slave address to MPEG2 chip -> read
                        s_addr = 0x40;//SAA6752HS

                        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                        {
                            printf("ioctl\r\n");
                        }
                        else
                        {
                            //INIT THE MPEG CHIP HERE ******************
                            SAA6752HS_Reset(fd,(channel - 1) );

                            if(init_SAA6752HS(fd,(channel-1) ))
                            {
                                printf("SAA6752HS not found for channel %d\r\n",channel);
                            }
                            else
                            {
                                //printf("SAA6752HS Init Success\r\n");
                               state = M23_I2C_GetCompressorState(channel,BusNumber);
                               printf("SAA6752HS Init Success,state = 0x%x\r\n",state);
                            }

                        }


                        //Set slave address to VIDEO chip -> read
                        s_addr = 0x42;//SAA7115H

                        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                        {
                            printf("ioctl\r\n");
                        }
                        else
                        {
                            //INIT THE VIDEO CHIP HERE ******************
                            SAA7115H_Reset(fd,(channel-1) );

                            if(init_SAA7115H(fd,(channel-1)))
                            {
                                printf("SAA7115H %d not found\r\n",channel);
                            }
                            else
                            {
                                //printf("SAA7115H Init Success\r\n\n");
                            }

                        }

                    }

                }

            }

        }

    }

    close(fd);

    return(0);

}

/***********************************************************************************************
 *
 * This will initialize the Compressor,Video and Audio chips via the I2c Bus.
 *
 **********************************************************************************************/
int M2300_I2C_Get_VideoBoard_Version(int BusNumber,int board)
{
    int           fd;                                 //File Descriptor
    int           s_addr = 0;                         // Slave Address
    int           debug;
    int           NumChips = 0;

    unsigned char ch;                       // Unsigned Temperature value holder

    M23_GetDebugValue(&debug);

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        printf("Error opening I2C Device\r\n");
        return(1);
    }


    /*----------------------------------------------------*/

    s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

    if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
    {
        printf("ioctl\r\n");
    }
    else
    {


        /*----------------------------------------------------*/

        ch = BusNumber;

        if(write(fd, &ch, 1) != 1)
        {
            //perror_quit("write");
            if(write(fd, &ch, 1) != 1)
            {
                printf("second write - Init Video Board\r\n");
            }
        }
        else
        {
            //Read to be sure right value got written
            if(read(fd, &ch, 1) != 1)
            {
                //perror_quit("read");
                if(read(fd, &ch, 1) != 1)
                {
                    printf("second read - Init Video 2\r\n");
                }
            }
            else
            {

                s_addr = 0xE2;//0xE2 is the address for Switch

                if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                {
                    printf("ioctl %d\r\n",s_addr >> 1);
                    NumChips = 0;
                }
                else
                {
                    ch = 1;
                    write(fd, &ch, 1);

                    /*----------------------------------------------------*/
                    //GET BOARD TYPE FROM PCA9500
                    /*----------------------------------------------------*/
                    //Set slave address to PCA9500 chip -> read   
                    s_addr = PCA9500;

                    if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                    {
    	                printf("ioctl-> Getting Board type\r\n");
                    }

                    //Read the Board Type
                    if(read(fd, &ch, 1) != 1)
                    {
                        //Device read unsuccessfull store 0xFF to the structure
                        NumChips = 0;
                    }
                    else
                    {
if(debug)
{
    printf("Video version from I2c 0x%x\r\n",ch);
}
                        if((ch & 0x7) < 0x3)
                        {
                            NumChips = 2;
                        }
                        else
                        {
                            NumChips = 4;
                        }
                    }

                }

            }

        }
    }

    close(fd);

    return(NumChips);

}


/***********************************************************************************************
 *
 * This will initialize the Compressor,Video and Audio chips via the I2c Bus. 
 *
 **********************************************************************************************/
int M2300_I2C_Init_VideoBoard(int BusNumber,int board,int video_index,int NumAudio)
{

    int fd;                                 //File Descriptor
    int state;
    int s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    int i = 0;
    int debug;
    /*----------------------------------------------------*/

    M23_GetDebugValue(&debug);

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        printf("Error opening I2C Device\r\n");
        return(1);
    }


        /*----------------------------------------------------*/

        s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
        {
            printf("ioctl\r\n");
        }
        else
        {


        /*----------------------------------------------------*/

             ch = BusNumber;

            if(write(fd, &ch, 1) != 1)
            {
                //perror_quit("write");
                if(write(fd, &ch, 1) != 1)
                {
                    printf("second write - Init Video Board\r\n");
                }
            }
            else
            {
                /*----------------------------------------------------*/


                //Read to be sure right valur got written
                if(read(fd, &ch, 1) != 1)
                {
                    //perror_quit("read");
                    if(read(fd, &ch, 1) != 1)
                    {
                        printf("second read - Init Video 2\r\n");
                    }
                }
                else
                {
                    //Init chips on 5 sub busses
                    //**************************
                    for(i = 1 ; i < 5 ; i++)
                    //for(i = 3 ; i < 4 ; i++)
                    {

                        /*----------------------------------------------------*/

                         s_addr = 0xE2;//0xE2 is the address for Switch

                        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                        {
                            printf("ioctl %d\r\n",s_addr >> 1);
                            break;
                        }
                        else
                        {
                            ch = (1 << i);
                            if(write(fd, &ch, 1) != 1)
                            {
                                if(write(fd, &ch, 1) != 1)
                                {
                                    printf("second write - 2 - Init Video\r\n");
                                }
                            }
                            else
                            {
                                //if( (ch == 0x02) || (ch == 0x08))
                                if(1)
                                {
                                    //Set slave address to AUDIO chip -> read
                                    s_addr = 0x34;//UDA1380

                                    if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                                    {
                                        printf("ioctl\r\n");
                                    }
                                    else
                                    {
                                        //INIT THE AUDIO CHIP HERE ******************
                                        if(init_UDS1380(fd))
                                        {
                                            if(debug)
                                                printf("UDS1380 %d not found\r\n",i);
                                        }
                                        else
                                        {
                                            if(debug)
                                                printf("UDS1380 %d Init Success\r\n",i);
                                        }

                                    }
                                }

                                //Set slave address to MPEG2 chip -> read
                                s_addr = 0x40;//SAA6752HS

                                if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                                {
                                    printf("ioctl\r\n");
                                }
                                else
                                {
                                    //INIT THE MPEG CHIP HERE ******************
                                    SAA6752HS_Reset(fd,((i-1)+(4*video_index)));
                                    if(NumAudio == 4)
                                    {
                                        if(init_SAA6752HS(fd,((i-1)+(4*video_index)) ))
                                        {
                                            if(debug)
                                                printf("SAA6752HS %d not found\r\n",i);
                                        }
                                        else
                                        {
#if 0
                                            state = M23_I2C_GetCompressorState(i,BusNumber);
                                            if(debug)
                                                printf("SAA6752HS %d Init Success, state = 0x%x\r\n",i,state);
#endif
                                        }
                                    }
                                    else
                                    {
                                        if(init_SAA6752HS_TwoAudio(fd,((i-1)+(4*video_index)) ))
                                        {
                                            if(debug)
                                                printf("SAA6752HS %d not found\r\n",i);
                                        }
                                        else
                                        {
#if 0
                                            state = M23_I2C_GetCompressorState(i,BusNumber);
                                            if(debug)
                                                printf("SAA6752HS %d Init Success, state = 0x%x\r\n",i,state);
#endif
                                        }
                                    }

                                }


                                //Set slave address to VIDEO chip -> read
                                s_addr = 0x42;//SAA7115H
#if(DEBUG_ENABLE)

        printf("Setting slave addr 0x%02x\n", s_addr);

#endif

                                if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                                {
                                    printf("ioctl\r\n");
                                }
                                else
                                {
                                    //INIT THE VIDEO CHIP HERE ******************
                                    SAA7115H_Reset(fd, ((i-1)+(4*video_index)));
                                    if(init_SAA7115H(fd,((i-1)+(4*video_index)) ))
                                    {
                                        if(debug)
                                            printf("SAA7115H %d not found\r\n",i);

                                    }
                                    else
                                    {
                                        if(debug)
                                            printf("SAA7115H %d Init Success\r\n\n",i);
                                    }

                                }

                            }

                        }

                    }

                }

            }

        }

    close(fd);

    return(0);

}

int M2300_I2C_VideoBoard_SetEncode(int BusNumber,int board,int video_index)
{

    int fd;                                 //File Descriptor
    int state;
    int s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    int i = 0;
    int debug;
    /*----------------------------------------------------*/

    M23_GetDebugValue(&debug);

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        printf("Error opening I2C Device\r\n");
        return(1);
    }


        /*----------------------------------------------------*/

        s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
        {
            printf("ioctl\r\n");
        }
        else
        {


        /*----------------------------------------------------*/

             ch = BusNumber;

            if(write(fd, &ch, 1) != 1)
            {
                //perror_quit("write");
                if(write(fd, &ch, 1) != 1)
                {
                    printf("second write - Init Video Board\r\n");
                }
            }
            else
            {
                /*----------------------------------------------------*/


                //Read to be sure right valur got written
                if(read(fd, &ch, 1) != 1)
                {
                    //perror_quit("read");
                    if(read(fd, &ch, 1) != 1)
                    {
                        printf("second read - Init Video 2\r\n");
                    }
                }
                else
                {
                    //Init chips on 5 sub busses
                    //**************************
                    for(i = 1 ; i < 5 ; i++)
                    {

                        /*----------------------------------------------------*/

                         s_addr = 0xE2;//0xE2 is the address for Switch

                        if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                        {
                            printf("ioctl %d\r\n",s_addr >> 1);
                            break;
                        }
                        else
                        {
                            ch = (1 << i);
                            if(write(fd, &ch, 1) != 1)
                            {
                                if(write(fd, &ch, 1) != 1)
                                {
                                    printf("second write - 2 - Init Video\r\n");
                                }
                            }
                            else
                            {

                                //Set slave address to MPEG2 chip -> read
                                s_addr = 0x40;//SAA6752HS

                                if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                                {
                                    printf("ioctl\r\n");
                                }
                                else
                                {
                                    //INIT THE MPEG CHIP HERE ******************
                                    SAA6752HS_Encode(fd,((i-1)+(4*video_index)));
                                }
                            }

                        }

                    }

                }

            }

        }

    close(fd);

    return(0);

}


int init_UDS1380(int FileDe) //AUDIO CHIP
{
    int numberOfDefaultRegisters = 14;
    unsigned char buff[3];
    unsigned char buffrd[2];
    int i = 0;

    //printf("Initializing UDS1380\r\n");

    unsigned short UDS1380_default[2 * 14] =
    {
//    0x00,0x0c08,
    //0x00,0x0c00,
    0x00,0x0F12,
    //0x00,0x0E12,
    0x01,0x0010,
    //0x02,0x010f,
    0x02,0x850F,
    0x03,0x3f3f,
    0x04,0x0202,
    0x10,0x0000,
//    0x11,0xff00,
    0x11,0xFFFF,
    0x12,0x0000,
    0x13,0x0000,
    0x14,0x0000,
//    0x18,0xADAB,
    0x20,0x0000,
    0x21,0x0404,
    0x22,0x0001,
    0x23,0x0000
//    0x7f,0x0000
    };

    memset(buff,0,sizeof(buff));

    for(i = 0; i < numberOfDefaultRegisters ; i++)
    {

        buff[0] = (unsigned char)UDS1380_default[(i*2)+0];
        buff[1] = (unsigned char)((UDS1380_default[(i*2)+1] & 0xFF00) >> 8);
        buff[2] = (unsigned char)(UDS1380_default[(i*2)+1] & 0x00FF);

        if(write(FileDe, &buff, 3) != 3)
        {

           if(write(FileDe, &buff, 3) != 3)
           {
               perror_quit("UDS1380 write");
           }
           return(1);
        }
        else
        {
                //printf("AT 0x%.2x WRITE 0x%.2x %.2x\r\n",buff[0],buff[1],buff[2]);
                //usleep(1000);

                if(write(FileDe, &buff, 1) != 1)
                {

                    if(write(FileDe, &buff, 1) != 1)
                    {
                        perror_quit("second write");
                    }
                }
                else
                {

                    //Read to be sure right valur got written
                    if(read(FileDe, &buffrd, 2) != 2)
                    {
                        if(read(FileDe, &buffrd, 2) != 2)
                        {
                            perror_quit("read");
                        }
                    }
                    else
                    {

                        //printf("FROM 0x%.2x READ 0x%.2x %.2x\r\n",buff[0],buffrd[0],buffrd[1]);

                    }

                }

         }

        sleep(.25);

    }

    return(0);
}


int SAA7115H_Reset(int FileDe,int channel) //VIDEO CHIP
{
    unsigned char buff[2];

    memset(buff,0x0,2);

    buff[0] = 0x88;
    buff[1] = 0x0;
    if(write(FileDe, &buff, 2) != 2)
    {
        perror("Resetting A/D\r\n");            
    }

    usleep(1000);

    return(0);
}

int init_SAA7115H(int FileDe,int channel) //VIDEO CHIP
{

    int numberOfDefaultRegisters = 202;
    unsigned char buff[2];
    int i = 0;

    //printf("Initializing SAA7115H\r\n");

//0x02,0xc0, //c6 for SVideo
//0x09,0x40,
//0x10,0x06,
//0x88,0x68


    unsigned char SAA7115H_default[2 * 202] =
    {

0x01,0x08,
0x02,MPEG_SVideo[channel], //c6 for SVideo
0x03,0x20,
0x04,0x90,
0x05,0x90,
0x06,0xeb,
0x07,0xe0,
0x08,0xb0,
0x09,MPEG_SVideo_SA9[channel],
0x0a,0x80,
0x0b,0x44,
0x0c,0x40,
0x0d,0x00,
0x0e,0x07,
0x0f,0x2a,
0x10,MPEG_SVideo_SA10[channel],
0x11,0x00,
0x12,0x00,
0x13,0x00,
0x14,0x11, /*was 0x01*/
//0x14,0x01,
0x15,0x11,
0x16,0xfe,
0x17,0x58,
0x18,0x40,
0x19,0x80,
0x1a,0x77,
0x1b,0x42,
0x1c,0xa9,
0x1d,0x01,
0x30,0xbc,
0x31,0xdf,
0x32,0x02,
0x33,0x00,
0x34,0xcd,
0x35,0xcc,
0x36,0x3a,
0x37,0x00,
0x38,0x03,
0x39,0x10,
0x3a,0x00,
0x40,0x40,
0x41,0x00,
0x42,0x00,
0x43,0x00,
0x44,0x00,
0x45,0x00,
0x46,0x00,
0x47,0x00,
0x48,0x00,
0x49,0x00,
0x4a,0x00,
0x4b,0x00,
0x4c,0x00,
0x4d,0x00,
0x4e,0x00,
0x4f,0x00,
0x50,0x00,
0x51,0x00,
0x52,0x00,
0x53,0x00,
0x54,0x00,
0x55,0x00,
0x56,0x00,
0x57,0x00,
0x58,0x00,
0x59,0x47,
0x5a,0x06,
0x5b,0x83,
0x5d,0x80,
0x5e,0x00,
0x5f,0x00,
0x63,0x00,
0x64,0x00,
0x65,0x00,
0x80,0x00,
0x81,0x00,
0x82,0x00,
0x83,0x21,
0x84,0x80,
0x85,0x00,
0x86,0xc5,
0x87,0x00,
0x89,0x00,
0x8a,0x00,
0x8b,0x00,
0x8c,0x00,
0x8d,0x00,
0x8e,0x00,
0x8f,0x48,
0x90,0x00,
0x91,0x08,
0x92,0x00,
0x93,0xc0,
0x94,0x04,
0x95,0x00,
0x96,0xd0,
0x97,0x02,
0x98,0x07,
0x99,0x01,
0x9a,0x06,
0x9b,0x01,
0x9c,0xd0,
0x9d,0x02,
0x9e,0x07,
0x9f,0x01,
0xa0,0x01,
0xa1,0x00,
0xa2,0x00,
0xa3,0x00,
0xa4,0x80,
0xa5,0x40,
0xa6,0x40,
0xa7,0x00,
0xa8,0x00,
0xa9,0x04,
0xaa,0x00,
0xab,0x00,
0xac,0x00,
0xad,0x02,
0xae,0x00,
0xaf,0x00,
0xb0,0x00,
0xb1,0x04,
0xb2,0x00,
0xb3,0x04,
0xb4,0x00,
0xb5,0x00,
0xb6,0x00,
0xb7,0x00,
0xb8,0x00,
0xb9,0x00,
0xba,0x00,
0xbb,0x00,
0xbc,0x00,
0xbd,0x00,
0xbe,0x00,
0xbf,0x00,
0xc0,0x00,
0xc1,0x08,
0xc2,0x10,
0xc3,0x80,
0xc4,0x10,
0xc5,0x00,
0xc6,0xd0,
0xc7,0x02,
0xc8,0x0a,
0xc9,0x00,
0xca,0xf2,
0xcb,0x00,
0xcc,0xd0,
0xcd,0x02,
0xce,0xf0,
0xcf,0x00,
0xd0,0x01,
0xd1,0x00,
0xd2,0x00,
0xd3,0x00,
0xd4,0x00,
0xd5,0x08,
0xd6,0x10,
0xd7,0x00,
0xd8,0x00,
0xd9,0x04,
0xda,0x00,
0xdb,0x00,
0xdc,0x00,
0xdd,0x02,
0xde,0x00,
0xdf,0x00,
0xe0,0x00,
0xe1,0x04,
0xe2,0x00,
0xe3,0x04,
0xe4,0x00,
0xe5,0x00,
0xe6,0x00,
0xe7,0x00,
0xe8,0x00,
0xe9,0x00,
0xea,0x00,
0xeb,0x00,
0xec,0x00,
0xed,0x00,
0xee,0x00,
0xef,0x00,
0xf0,0x00,
0xf1,0x00,
0xf2,0x00,
0xf3,0x00,
0xf4,0x01,
0xf5,0x00,
0xf6,0x00,
0xf7,0x00,
0xf8,0x00,
0xf9,0x00,
0xfa,0x00,
0xfb,0x00,
0xfc,0x00,
0xfd,0x00,
0xfe,0x00,
0xff,0x00,
0x88,MPEG_SVideo_SA88[channel]

    };
    memset(buff,0,sizeof(buff));

//printf("Channel %d = [SA2]0x%x,[SA9]0x%x,[SA10]0x%x\r\n",channel,MPEG_SVideo[channel],MPEG_SVideo_SA9[channel],MPEG_SVideo_SA10[channel]);

    for(i = 0; i < numberOfDefaultRegisters ; i++)
    {

        buff[0] = (unsigned char)SAA7115H_default[(i*2)+0];
        buff[1] = (unsigned char)((SAA7115H_default[(i*2)+1] & 0xFF));

        if(write(FileDe, &buff, 2) != 2)
        {
            if(write(FileDe, &buff, 2) != 2)
            {
                printf("SAA7115H write\r\n");
                return(1);
            }
        }
        else
        {

        }

    }


    return(0);

}


int SAA6752HS_Reset(int FileDe,int channel) //MPEG COMPRESSOR CHIP
{
    unsigned char buff[4];           
    unsigned char ch;
    int      i;
    UINT16   w;

    memset(buff,0x0,4);                     
    //buff[0] = 0x03; //This will reset the COMPRESSOR
    buff[0] = 0x07; //This will reconfigure the COMPRESSOR

    if(write(FileDe, &buff, 1) != 1)
    {
        if(write(FileDe, &buff, 1) != 1)
        {
            printf("9-SAA6752HS write");
            return(1);
        }        

    }
    else
    { 
        /*Now Wait for Idle State*/

        for(i = 0;i < 100;i++)   
        {
            buff[0] = 0x12; //This will wait for the IDLE state
            write(FileDe, &buff, 1);

            read(FileDe, &w, 2);

            if( w == 0x200)
            {
                buff[0] = 0x10; //This will wait for the IDLE state
                write(FileDe, &buff, 1);

                read(FileDe, &ch, 1);
                if(ch & 0x1 )
                {
                    //printf("idle @ %d\r\n",i);
                    i = 101;
                }
            }

            usleep(1000);
        }

    }

    return(0);
}                                 

int SAA6752HS_Encode(int FileDe,int channel) //MPEG COMPRESSOR CHIP
{
    unsigned char buff[4];           
    unsigned char ch;
    int      i;
    UINT16   w;

    memset(buff,0x0,4);                     
    buff[0] = 0x02; //This will reconfigure the COMPRESSOR

    if(write(FileDe, &buff, 1) != 1)
    {
        if(write(FileDe, &buff, 1) != 1)
        {
            printf("9-SAA6752HS write");
            return(1);
        }        

    }
    else
    { 
        /*Now Wait for Idle State*/

        for(i = 0;i < 100;i++)   
        {
            buff[0] = 0x12; //This will wait for the IDLE state
            write(FileDe, &buff, 1);

            read(FileDe, &w, 2);

            if( w == 0x200)
            {
                buff[0] = 0x10; //This will wait for the Encode state
                write(FileDe, &buff, 1);

                read(FileDe, &ch, 1);
                if(ch & 0x2 )
                {
                    i = 101;
                }
            }

            usleep(1000);
        }

    }

    if(i == 0)
        printf("Never went to Encode\r\n");

    return(0);
}


int init_SAA6752HS(int FileDe,int channel) //MPEG COMPRESSOR CHIP
{

    int numberOfDefaultRegisters1Byte = 47;
    int numberOfDefaultRegisters2Byte = 14;
    int debug;
    unsigned char buff[190];
    int i = 0;

    unsigned char SAA6752HS_default_1Byte[2*47] =
    {
        0x07,0x00,
        0x08,0x00,
        //0x08,0x01,
        0x20,0x02,
        0x30,0x03,
        0x31,0x00,
        0x32,0x00,
        0x33,0x00,
        0x34,0x00,
        0x40,0x01,
        0x41,0x00,
        0x42,0x00,
        0x43,0x00,
        0x46,0x00,
        0x47,0x00,
        0x50,0x00,
        0x53,0x00,
        0x55,0x08,
        0x61,0x00,
        0x63,0x00,
        0x65,0x00,
        0x70,MPEG_VideoEnabled[channel],
        /*0x70,0x00,*/
        0x71,0x01,
        0x74,0x00,
        0x75,0x00,
        0x78,0x20,
        0x7B,0x00,
        0x7C,0x00,
        0x7D,0x00,
        0x82,0x04,
        0x83,0x0C,
        0x84,0x80,
        0x90,MPEG_AudioEnabled[channel],
        /*0x90,0x00,*/
        0x91,0x00,
        0x92,0x01,
        0x93,0x00,
        0x94,0x00,
        0x95,0x00,
        0x96,0x00,
        0xB0,0x05,
        0xB3,0xE0,
        0xB4,0xC0,
        0xB5,0x03,
        0xB6,0x02,
        0xB7,0x00,
        0xB9,0x00,
        0xC3,0x00,
        0xD0,0x07
    };


    unsigned short SAA6752HS_default_2Byte[2*14] =
    {
        0x11,0x0000,
        0x44,0x0404,
        0x45,0x0000,
        0x62,0xFFFF,
        0x72,MPEG_GOP[channel],
        /*0x72,0x010F,*/
        0x77,0x0000,
        0x80,MPEG_TargetBitRate[channel],/*Enter the Video Target Bit rate here*/
        0x81,MPEG_MaximumBitRate[channel],/*Enter the Video Max Bit rate here*/
        0xB1,MPEG_SystemBitRate[channel],/*Enter the Total Bit rate here*/

        /*0x80,0x0BB8,Enter the Video Target Bit rate here*/
        /*0x81,0x0DAC,Enter the Video Max Bit rate here*/
        /*0xB1,0x1000,Enter the Total Bit rate here*/

        0xB2,0x0000,

        //Requested Values
        //0xC0,0x03E8,
        //0xC1,0x03E9,
        //0xC4,0x03E7,

        //Change to default
        0xC0,0x0100,
        0xC1,0x0103,
        0xC4,0x0104,
        0xF6,0x0000

    };



    M23_GetDebugValue(&debug);
    if(debug)
        printf("Channel %d -> GOP 0x%x,System Rate %d,Max %d,Target %d\r\n",channel+1,MPEG_GOP[channel],MPEG_SystemBitRate[channel],MPEG_MaximumBitRate[channel],MPEG_TargetBitRate[channel]);

    memset(buff,0,sizeof(buff));

    for(i = 0; i < numberOfDefaultRegisters1Byte ; i++)
    {

        buff[0] = (unsigned char)SAA6752HS_default_1Byte[(i*2)+0];
        buff[1] = (unsigned char)(SAA6752HS_default_1Byte[(i*2)+1] & 0xFF);

        if(write(FileDe, &buff, 2) != 2)
        {
            if(write(FileDe, &buff, 2) != 2)
            {
                printf("SAA6752HS write -> Reg %d\r\n",i);
                return(1);
            }

        }
        else
        {

        }

    }
    memset(buff,0,sizeof(buff));

    for(i = 0; i < numberOfDefaultRegisters2Byte ; i++)
    {

        buff[0] = (unsigned char)SAA6752HS_default_2Byte[(i*2)+0];
        buff[1] = (unsigned char)((SAA6752HS_default_2Byte[(i*2)+1] & 0xFF00) >> 8);
        buff[2] = (unsigned char)(SAA6752HS_default_2Byte[(i*2)+1] & 0x00FF);

        if(write(FileDe, &buff, 3) != 3)
        {

            if(write(FileDe, &buff, 3) != 3)
            {
                printf("1-SAA6752HS write\r\n");
                return(1);
            }

        }
        else
        {

        }

    }

    //Removed upon Stuarts request
    /*
    memset(buff,0,sizeof(buff));

    buff[0] = 0xA4;
    buff[1] = 0x01;

    if(write(FileDe, &buff, 2) != 2)
    {
        if(write(FileDe, &buff, 2) != 2)
        {
            printf("2-SAA6752HS write\r\n");
            return(1);
        }

    }
    else
    {

    }
    */

    memset(buff,0,sizeof(buff));

    //Noise clear
    buff[0] = 0xA4;
    buff[1] = 0x00;

    if(write(FileDe, &buff, 2) != 2)
    {
        if(write(FileDe, &buff, 2) != 2)
        {
            printf("3-SAA6752HS write\r\n");
            return(1);
        }

    }
    else
    {

    }


    //Noise pre filter coefficients

    memset(buff,0,sizeof(buff));

    buff[0] = 0x51;
    buff[1] = 0x33;
    buff[2] = 0x20;
    buff[3] = 0x16;
    buff[4] = 0x0D;

    if(write(FileDe, &buff, 5) != 5)
    {
        if(write(FileDe, &buff, 5) != 5)
        {
            printf("4-SAA6752HS write");
            return(1);
        }

    }
    else
    {

    }


    //Noise pre filter thresh hold

    memset(buff,0,sizeof(buff));

    buff[0] = 0x52;
    buff[1] = 0x0A;
    buff[2] = 0x0F;
    buff[3] = 0x14;

    if(write(FileDe, &buff, 4) != 4)
    {
        if(write(FileDe, &buff, 4) != 4)
        {
            printf("5-SAA6752HS write");
            return(1);
        }
    }
    else
    {

    }


    // SET 0x54 here

    // SET 0x60 VBI Mode select

    // SET 0x73

    // SET 0x73
    // SET 0x76
    // SET 0x79
/*************************************************/

    //PAT

    memset(buff,0,sizeof(buff));

    buff[0] = 0xC2;
    buff[1] = 0x00;
    buff[2] = 0x47;
    buff[3] = 0x40;
    buff[4] = 0x00;
    buff[5] = 0x10;
    buff[6] = 0x00;
    buff[7] = 0x00;
    buff[8] = 0xB0;
    buff[9] = 0x0D;
    buff[10] = 0x00;
    buff[11] = 0x01;
    buff[12] = 0xC1;
    buff[13] = 0x00;
    buff[14] = 0x00;
    buff[15] = 0x00;
    buff[16] = 0x01;
    buff[17] = 0xE0;
    buff[18] = 0x10;
    buff[19] = 0x76;
    buff[20] = 0xF1;
    buff[21] = 0x44;
    buff[22] = 0xD1;

    if(write(FileDe, &buff, 23) != 23)
    {
        if(write(FileDe, &buff, 23) != 23)
        {
            printf("6-SAA6752HS write");
            return(1);
        }
    }
    else
    {

    }

/*************************************************/

        //PMT 

    memset(buff,0,sizeof(buff));

    buff[0] = 0xC2;
    buff[1] = 0x01;
    buff[2] = 0x47;
    buff[3] = 0x40;
    buff[4] = 0x10;
    buff[5] = 0x10;
    buff[6] = 0x00;
    buff[7] = 0x02;
    buff[8] = 0xB0;
    buff[9] = 0x17;
    buff[10] = 0x00;
    buff[11] = 0x01;
    buff[12] = 0xC1;
    buff[13] = 0x00;
    buff[14] = 0x00;

// Requested Values
//    buff[15] = 0xE3;
//    buff[16] = 0xE7;
// Default Values PCR PID
    buff[15] = 0xE1;
    buff[16] = 0x04;

    buff[17] = 0xF0;
    buff[18] = 0x00;


    if(MPEG_VideoEnabled[channel] == 0) //Enabled
    {
        buff[19] = 0x02;
    }
    else
    {
        buff[19] = 0xFF;
    }

// Requested Values
//    buff[20] = 0xE3;
//    buff[21] = 0xE8;
// Default Values Video PID
    buff[20] = 0xE1;
    buff[21] = 0x00;

    buff[22] = 0xF0;
    buff[23] = 0x00;

    if(MPEG_AudioEnabled[channel] == 0) //Enabled
    {
        buff[24] = 0x04;
    }
    else
    {
        buff[24] = 0xFF;
    }

// Requested Values
//    buff[25] = 0xE3;
//    buff[26] = 0xE9;
// Default Values Audio PID
    buff[25] = 0xE1;
    buff[26] = 0x03;

    buff[27] = 0xF0;
    buff[28] = 0x00;
    buff[29] = 0xA1;
    buff[30] = 0xCA;
    buff[31] = 0x0F;
    buff[32] = 0x82;


    if(write(FileDe, &buff, 33) != 33)
    {
        if(write(FileDe, &buff, 33) != 33)
        {
            printf("7-SAA6752HS write");
            return(1);
        }
    }
    else
    {

    }
/********************************************/

    //STUART REQUESTED TO JUST WRITE BYTE 2
    memset(buff,0,sizeof(buff));

    buff[0] = 0x02; //This sets the compressor to Encode Mode

    if(write(FileDe, &buff, 1) != 1)
    {
        if(write(FileDe, &buff, 1) != 1)
        {
            printf("8-SAA6752HS write");
            return(1);
        }

    }

    return(0);

}

int init_SAA6752HS_TwoAudio(int FileDe,int channel) //MPEG COMPRESSOR CHIP
{

    int numberOfDefaultRegisters1Byte = 47;
    int numberOfDefaultRegisters2Byte = 14;
    int debug;
    unsigned char buff[190];
    int i = 0;

    unsigned char SAA6752HS_default_1Byte[2*47] =
    {
        0x07,0x00,
        0x08,0x00,
        //0x08,0x01,
        0x20,0x02,
        0x30,0x03,
        0x31,0x01, /*Change to external Clock for Lockheed per Stuarts request*/
        0x32,0x00,
        0x33,0x00,
        0x34,0x00,
        0x40,0x01,
        0x41,0x00,
        0x42,0x00,
        0x43,0x00,
        0x46,0x00,
        0x47,0x00,
        0x50,0x00,
        0x53,0x00,
        0x55,0x08,
        0x61,0x00,
        0x63,0x00,
        0x65,0x00,
        0x70,MPEG_VideoEnabled[channel],
        /*0x70,0x00,*/
        0x71,0x01,
        0x74,0x00,
        0x75,0x00,
        0x78,0x20,
        0x7B,0x00,
        0x7C,0x00,
        0x7D,0x00,
        0x82,0x04,
        0x83,0x0C,
        0x84,0x80,
        0x90,MPEG_AudioEnabled[channel],
        /*0x90,0x00,*/
        0x91,0x00,
        0x92,0x01,
        0x93,0x00,
        0x94,0x00,
        0x95,0x00,
        0x96,0x00,
        0xB0,0x05,
        0xB3,0xE0,
        0xB4,0xC0,
        0xB5,0x03,
        0xB6,0x02,
        0xB7,0x00,
        0xB9,0x00,
        0xC3,0x00,
        0xD0,0x07
    };


    unsigned short SAA6752HS_default_2Byte[2*14] =
    {
        0x11,0x0000,
        0x44,0x0404,
        0x45,0x0000,
        0x62,0xFFFF,
        0x72,MPEG_GOP[channel],
        /*0x72,0x010F,*/
        0x77,0x0000,
        0x80,MPEG_TargetBitRate[channel],/*Enter the Video Target Bit rate here*/
        0x81,MPEG_MaximumBitRate[channel],/*Enter the Video Max Bit rate here*/
        0xB1,MPEG_SystemBitRate[channel],/*Enter the Total Bit rate here*/

        /*0x80,0x0BB8,Enter the Video Target Bit rate here*/
        /*0x81,0x0DAC,Enter the Video Max Bit rate here*/
        /*0xB1,0x1000,Enter the Total Bit rate here*/

        0xB2,0x0000,

        //Requested Values
        //0xC0,0x03E8,
        //0xC1,0x03E9,
        //0xC4,0x03E7,

        //Change to default
        0xC0,0x0100,
        0xC1,0x0103,
        0xC4,0x0104,
        0xF6,0x0000

    };



    M23_GetDebugValue(&debug);
    if(debug)
        printf("Channel %d -> GOP 0x%x,System Rate %d,Max %d,Target %d\r\n",channel+1,MPEG_GOP[channel],MPEG_SystemBitRate[channel],MPEG_MaximumBitRate[channel],MPEG_TargetBitRate[channel]);

    memset(buff,0,sizeof(buff));

    for(i = 0; i < numberOfDefaultRegisters1Byte ; i++)
    {

        buff[0] = (unsigned char)SAA6752HS_default_1Byte[(i*2)+0];
        buff[1] = (unsigned char)(SAA6752HS_default_1Byte[(i*2)+1] & 0xFF);

        if(write(FileDe, &buff, 2) != 2)
        {
            if(write(FileDe, &buff, 2) != 2)
            {
                printf("SAA6752HS write -> Reg %d\r\n",i);
                return(1);
            }

        }
        else
        {

        }

    }
    memset(buff,0,sizeof(buff));

    for(i = 0; i < numberOfDefaultRegisters2Byte ; i++)
    {

        buff[0] = (unsigned char)SAA6752HS_default_2Byte[(i*2)+0];
        buff[1] = (unsigned char)((SAA6752HS_default_2Byte[(i*2)+1] & 0xFF00) >> 8);
        buff[2] = (unsigned char)(SAA6752HS_default_2Byte[(i*2)+1] & 0x00FF);

        if(write(FileDe, &buff, 3) != 3)
        {

            if(write(FileDe, &buff, 3) != 3)
            {
                printf("1-SAA6752HS write\r\n");
                return(1);
            }

        }
        else
        {

        }

    }

    //Removed upon Stuarts request
    /*
    memset(buff,0,sizeof(buff));

    buff[0] = 0xA4;
    buff[1] = 0x01;

    if(write(FileDe, &buff, 2) != 2)
    {
        if(write(FileDe, &buff, 2) != 2)
        {
            printf("2-SAA6752HS write\r\n");
            return(1);
        }

    }
    else
    {

    }
    */

    memset(buff,0,sizeof(buff));

    //Noise clear
    buff[0] = 0xA4;
    buff[1] = 0x00;

    if(write(FileDe, &buff, 2) != 2)
    {
        if(write(FileDe, &buff, 2) != 2)
        {
            printf("3-SAA6752HS write\r\n");
            return(1);
        }

    }
    else
    {

    }


    //Noise pre filter coefficients

    memset(buff,0,sizeof(buff));

    buff[0] = 0x51;
    buff[1] = 0x33;
    buff[2] = 0x20;
    buff[3] = 0x16;
    buff[4] = 0x0D;

    if(write(FileDe, &buff, 5) != 5)
    {
        if(write(FileDe, &buff, 5) != 5)
        {
            printf("4-SAA6752HS write");
            return(1);
        }

    }
    else
    {

    }


    //Noise pre filter thresh hold

    memset(buff,0,sizeof(buff));

    buff[0] = 0x52;
    buff[1] = 0x0A;
    buff[2] = 0x0F;
    buff[3] = 0x14;

    if(write(FileDe, &buff, 4) != 4)
    {
        if(write(FileDe, &buff, 4) != 4)
        {
            printf("5-SAA6752HS write");
            return(1);
        }
    }
    else
    {

    }


    // SET 0x54 here

    // SET 0x60 VBI Mode select

    // SET 0x73

    // SET 0x73
    // SET 0x76
    // SET 0x79
/*************************************************/

    //PAT

    memset(buff,0,sizeof(buff));

    buff[0] = 0xC2;
    buff[1] = 0x00;
    buff[2] = 0x47;
    buff[3] = 0x40;
    buff[4] = 0x00;
    buff[5] = 0x10;
    buff[6] = 0x00;
    buff[7] = 0x00;
    buff[8] = 0xB0;
    buff[9] = 0x0D;
    buff[10] = 0x00;
    buff[11] = 0x01;
    buff[12] = 0xC1;
    buff[13] = 0x00;
    buff[14] = 0x00;
    buff[15] = 0x00;
    buff[16] = 0x01;
    buff[17] = 0xE0;
    buff[18] = 0x10;
    buff[19] = 0x76;
    buff[20] = 0xF1;
    buff[21] = 0x44;
    buff[22] = 0xD1;

    if(write(FileDe, &buff, 23) != 23)
    {
        if(write(FileDe, &buff, 23) != 23)
        {
            printf("6-SAA6752HS write");
            return(1);
        }
    }
    else
    {

    }

/*************************************************/

        //PMT

    memset(buff,0,sizeof(buff));

    buff[0] = 0xC2;
    buff[1] = 0x01;
    buff[2] = 0x47;
    buff[3] = 0x40;
    buff[4] = 0x10;
    buff[5] = 0x10;
    buff[6] = 0x00;
    buff[7] = 0x02;
    buff[8] = 0xB0;
    buff[9] = 0x17;
    buff[10] = 0x00;
    buff[11] = 0x01;
    buff[12] = 0xC1;
    buff[13] = 0x00;
    buff[14] = 0x00;

// Requested Values
//    buff[15] = 0xE3;
//    buff[16] = 0xE7;
// Default Values PCR PID
    buff[15] = 0xE1;
    buff[16] = 0x04;

    buff[17] = 0xF0;
    buff[18] = 0x00;


    if(MPEG_VideoEnabled[channel] == 0) //Enabled
    {
        buff[19] = 0x02;
    }
    else
    {
        buff[19] = 0xFF;
    }

// Requested Values
//    buff[20] = 0xE3;
//    buff[21] = 0xE8;
// Default Values Video PID
    buff[20] = 0xE1;
    buff[21] = 0x00;

    buff[22] = 0xF0;
    buff[23] = 0x00;

    if(MPEG_AudioEnabled[channel] == 0) //Enabled
    {
        buff[24] = 0x04;
    }
    else
    {
        buff[24] = 0xFF;
    }

// Requested Values
//    buff[25] = 0xE3;
//    buff[26] = 0xE9;
// Default Values Audio PID
    buff[25] = 0xE1;
    buff[26] = 0x03;

    buff[27] = 0xF0;
    buff[28] = 0x00;
    buff[29] = 0xA1;
    buff[30] = 0xCA;
    buff[31] = 0x0F;
    buff[32] = 0x82;


    if(write(FileDe, &buff, 33) != 33)
    {
        if(write(FileDe, &buff, 33) != 33)
        {
            printf("7-SAA6752HS write");
            return(1);
        }
    }
    else
    {

    }
/********************************************/

    //STUART REQUESTED TO JUST WRITE BYTE 2
    memset(buff,0,sizeof(buff));

    buff[0] = 0x02; //This sets the compressor to Encode Mode

    if(write(FileDe, &buff, 1) != 1)
    {
        if(write(FileDe, &buff, 1) != 1)
        {
            printf("8-SAA6752HS write");
            return(1);
        }

    }

    return(0);
}



int M2300_I2C_Status_VideoBoard(int BusNumber,int *stat1,int *stat2,int *stat3,int *stat4,int *stat5, int *stat6, int *stat7,int *stat8)
{

    static int fd;                                 //File Descriptor
    int s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    int status = 0;
    int status1 = 0;
    int i = 0;


    *stat1 = 1;
    *stat2 = 1;
    *stat3 = 1;
    *stat4 = 1;

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        //perror_quit("Error opening I2C Device");
        status = 1;
    }

    s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

    if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
    {
       //perror_quit("ioctl");
        status = 1;
    }
    else
    {

        ch = BusNumber;

        if(write(fd, &ch, 1) != 1)
        {
            if(write(fd, &ch, 1) != 1)
            {
                //perror_quit("write");
                status = 1;
            }
        }
        else
        {

            //Read to be sure right valur got written
            if(read(fd, &ch, 1) != 1)
            {
                if(read(fd, &ch, 1) != 1)
                {
                    //perror_quit("read");
                    status = 1;
                }
            }
            else
            {
                //status chips on 5 sub busses
                //**************************
                for(i = 1 ; i < 5 ; i++)
                {
                    s_addr = 0xE2;//0xE2 is the address for Switch PCA9548

                    if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                    {
                        //perror_quit("ioctl");
                        status = 1;
                    }
                    else
                    {
                        ch = (1 << i);

                        if(write(fd, &ch, 1) != 1)
                        {
                            if(write(fd, &ch, 1) != 1)
                            {
                                //perror_quit("write");
                                status = 1;
                            }
                        }
                        else
                        {
                            //Set slave address to MPEG2 chip -> read
                            s_addr = 0x40;//SAA6752HS

                            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                            {
                                printf("ioctl\r\n");
                            }
                            else
                            {
                                //STATUS OF THE CompressorCHIP HERE ******************
                                status1 = stat_SAA6752HS(fd);
                            }


                            //Set slave address to VIDEO chip -> read
                            s_addr = 0x42;//SAA7115H

                            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
                            {
                                //perror_quit("ioctl");
                                status = 1;
                            }
                            else
                            {
                                //STATUS OF THE A/D CHIP HERE ******************
                                status = stat_SAA7115H(fd);
                            }
                        }
                    }
                    switch(i)
                    {
                        case 1:
                            *stat1 = status;
                            *stat5 = status1;
                             break;
                        case 2:
                            *stat2 = status;
                            *stat6 = status1;
                             break;
                        case 3:
                            *stat3 = status;
                            *stat7 = status1;
                             break;
                        case 4:
                            *stat4 = status;
                            *stat8 = status1;
                             break;
                    }
                }
            }
        }
    }

    close(fd);


    return(status);

}


int stat_UDS1380(int FileDe)
{
    unsigned char buff[2];
    unsigned char val=0;

    //printf("===========Status stat_UDS1380==========\r\n");


    memset(buff,0,sizeof(buff));

    buff[0] = 0x28;
    buff[1] = 0x00;

    if(write(FileDe, &buff, 1) != 1)
    {
       perror_quit("stat_UDS1380 write");
       return(1);

    }
    else
    {

    }

    memset(buff,0,sizeof(buff));

    if(read(FileDe, &buff, 2) != 2)
    {
       perror_quit("stat_UDS1380 write");
       return(1);

    }
    else
    {
        val = buff[1];
        if(val & 0x01)
        {
            printf("**ERROR: Overflow Detected\r\n");
        }
        else
        {
            printf("**No Overflow Detected\r\n");
        }

        val = buff[1];
        if(val & 0x04)
        {
            printf("**ERROR: Decimator has muted\r\n");

        }
        else
        {

            printf("**Decimator has not muted\r\n");

        }

        val = buff[1];
        if(val & 0x10)
        {

            printf("**ERROR: AGC Gain greater than or equal to 8 dB\r\n");

        }
        else
        {

            printf("**AGC Gain less than 8 dB\r\n");

        }

    }

    //printf("\r\n");

    return(0);
}

int stat_SAA7115H(int FileDe)
{

    int status = 0;     

    unsigned char buff[1];         


    memset(buff,0,sizeof(buff));            

    buff[0] = 0x1F;                

    if(write(FileDe, &buff, 1) != 1)        
    {
        if(write(FileDe, &buff, 1) != 1)
        {            
            perror_quit("SAA7115H write");
            status = 1;
        }               
    }         

    memset(buff,0,sizeof(buff));

    if(read(FileDe, &buff, 1) != 1)
    {
        if(read(FileDe, &buff, 1) != 1)
        {
            perror_quit("SAA7115H write");
            status = 1;
        }
    }
    else
    {
#if 0
        val = buff[0];
        if(val & 0x40)
        {
            status = 1;
        }
#endif
        status = buff[0];
    }

    buff[0] = 0x1E;

    if(write(FileDe, &buff, 1) != 1)
    {
        if(write(FileDe, &buff, 1) != 1)
        {
            perror_quit("SAA7115H write");
            status = 1;
        }
    }

    memset(buff,0,sizeof(buff));

    if(read(FileDe, &buff, 1) != 1)
    {
        if(read(FileDe, &buff, 1) != 1)
        {
            perror_quit("SAA7115H write");
        }
    }
    else
    {
        status |= (buff[0] << 8);
    }

    //printf("\r\n");

    return(status);
}


int stat_SAA6752HS(int FileDe)
{
    unsigned char buff[2];
    unsigned int val=-1;
    unsigned short value=0;

    memset(buff,0,sizeof(buff));

    buff[0] = 0x12;

    if(write(FileDe, &buff, 1) != 1)
    {
       perror_quit("SAA6752HS write");
       return(-1);

    }

    memset(buff,0,sizeof(buff));

    if(read(FileDe, &buff, 2) != 2)
    {
       perror_quit("SAA6752HS write");
       return(-1);

    }
    else
    {
        value = buff[1];
        value = ((value << 8) | buff[0]);
        val = value;
    }

    return(val);

}


int M23_InitializeControllerAudioI2c()
{
    int           fd;                                 //File Descriptor
    int           s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    int           i = 0;
    int           BusNumber = 0x1;
    unsigned char buff[4];

    int numberOfaudioRegisters = 14;
    unsigned short controller_audio_2Byte[2*14] =
    {
        //0x00,0x0c00, 
        0x00,0x0F12, 
        0x01,0x0010, 
        //0x02,0x010F, 
        0x02,0x850F, 
        0x03,0x3F3F, 
        0x04,0x0202,
        0x10,0x0000, 
        0x11,0xFFFF, 
        0x12,0x0000,
        0x13,0x0000, 
        0x14,0x0000,
        0x20,0x0000, 
        0x21,MPEG_AudioGain,
        //0x21,0x0404,
        0x22,0x0001,
        0x23,0x0000
        //0x23,0x0001
    };

//printf("Programmed Audio Gain 0x%x\r\n",MPEG_AudioGain);
    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        printf("Error opening I2C Device\r\n");
        return(1);
    }


    s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

    if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
    {
        printf("ioctl\r\n");
    }
    else
    {

        ch = BusNumber;

        if(write(fd, &ch, 1) != 1)
        {
            //perror_quit("write");
            if(write(fd, &ch, 1) != 1)
            {
                printf("second write - Controller Audio\r\n");
            }
        }
        else
        {
            //Read to be sure right value got written
            if(read(fd, &ch, 1) != 1)
            {
                if(read(fd, &ch, 1) != 1)
                {
                    printf("second read - Controller Audio\r\n");
                }
            }

            s_addr = 0x34;//0x30 is audio
            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
            {
                printf("audio ioctl\r\n");
            }
            else
            {
                for(i = 0; i < numberOfaudioRegisters ; i++)
                {

                    memset(buff,0x0,4);
                    buff[0] = (unsigned char)controller_audio_2Byte[(i*2)+0];
                    buff[1] = (unsigned char)((controller_audio_2Byte[(i*2)+1] & 0xFF00) >> 8);
                    buff[2] = (unsigned char)(controller_audio_2Byte[(i*2)+1] & 0x00FF);

                    if(write(fd, &buff, 3) != 3)
                    {
                        if(write(fd, &buff, 3) != 3)
                        {
                            perror("writing Audio I2c Registers");
                            i = numberOfaudioRegisters;
                            //return(1);
                        }

                    }

                }
            }

        }
    }

    close(fd);

    return(0);
}

int M23_InitializeControllerVideoI2c(int channel)
{
    int           fd;                                 //File Descriptor
    int           s_addr = 0;                         // Slave Address
    unsigned char ch;                       // Unsigned Temperature value holder
    int           i = 0;
    int           BusNumber = 0x1;
    unsigned char buff[4];

    int numberOfvideoRegisters = 53;

    unsigned char ControllerVideo1Byte[2*53] =
    {
        0x00,0x00,
        0x25,0x00,
        0x26,0x00,
        0x27,0x00,
        0x28,0x19,
        0x29,0x1D,
        0x2A,0x00,
        0x2B,0x00,
        0x2C,0x00,
        0x2D,0x00,
        0x2E,0x00,
        0x2F,0x00,
        0x39,0x00,
        0x3A,0x13,
        0x5A,0x88,
        0x5B,0x7D,
        0x5C,0xAF,
        0x5D,0x2A,
        0x5E,0x2E,
        0x5F,0x2E,
        0x60,0x00,
        0x61,0x05,
        0x62,0x43,
        0x63,0x1F,
        0x64,0x7C,
        0x65,0xF0,
        0x66,0x2A,
        0x66,0x21,
        0x67,0x00,
        0x68,0x00,
        0x69,0x00,
        0x6A,0x00,
        0x6B,0x12,
        0x6C,0xFD,
        0x6D,0x07,
        0x6E,0x80,
        0x6F,0x00,
        0x70,0x1A,
        0x71,0x87,
        0x72,0x61,
        0x73,0x00,
        0x74,0x00,
        0x75,0x03,
        0x76,0x00,
        0x77,0x00,
        0x78,0x00,
        0x79,0x00,
        0x7A,0x00,
        0x7B,0x00,
        0x7C,0x00,
        0x7D,0x00,
        0x7E,0x00,
        0x7F,0x00
    };

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        printf("Error opening I2C Device\r\n");
        return(1);
    }


    s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

    if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
    {
        printf("ioctl\r\n");
    }
    else
    {

        ch = BusNumber;

        if(write(fd, &ch, 1) != 1)
        {
            //perror_quit("write");
            if(write(fd, &ch, 1) != 1)
            {
                printf("second write - Controller Video\r\n");
            }
        }
        else
        {
            //Read to be sure right valur got written
            if(read(fd, &ch, 1) != 1)
            {
                if(read(fd, &ch, 1) != 1)
                {
                    printf("second read - Controller Video\r\n");
                }
            }

            s_addr = channel;
            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
            {
                printf("video ioctl\r\n");
            }
            else
            {
                for(i = 0; i < numberOfvideoRegisters ; i++)
                {
                    memset(buff,0x0,4);
                    buff[0] = (unsigned char)ControllerVideo1Byte[(i*2)+0];
                    buff[1] = (unsigned char)(ControllerVideo1Byte[(i*2)+1] & 0xFF);

                    if(write(fd, &buff, 2) != 2)
                    {
                        if(write(fd, &buff, 2) != 2)
                        {
                            //perror("Writing Video1 I2c Registers");
                            return(1);
                        }

                    }
                }
            }

        }
    }

    close(fd);

    return(0);
}

int M23_InitializeControllerRGBI2c()
{
    int           fd;                                 //File Descriptor
    int           s_addr = 0;                         // Slave Address
    int           i = 0;
    int           BusNumber = 0x1;
    unsigned char ch;                       
    unsigned char buff[4];

    int numberOfvideoRegisters = 4;

    unsigned char ControllerVideo1Byte[2*4] =
    {
        //0x00,0x03,
        //0x00,0x0B,
        0x00,VideoOut0,
        0x04,VideoOut4,
        0x05,VideoOut5,
        0x06,VideoOut6
    };

    //Open the i2c target device
    fd = open(I2C_TARGET, O_RDWR);

    if(fd < 0)
    {
        printf("Error opening I2C Device\r\n");
        return(1);
    }


    s_addr = PCA9548;//0xE0 is the address for Switch PCA9548

    if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
    {
        printf("ioctl\r\n");
    }
    else
    {

        ch = BusNumber;

        if(write(fd, &ch, 1) != 1)
        {
            //perror_quit("write");
            if(write(fd, &ch, 1) != 1)
            {
                printf("second write - Controller Video\r\n");
            }
        }
        else
        {
            //Read to be sure right valur got written
            if(read(fd, &ch, 1) != 1)
            {
                if(read(fd, &ch, 1) != 1)
                {
                    printf("second read - Controller Video\r\n");
                }
            }

            s_addr = 0x0;
            if(ioctl(fd, /*I2C_SLAVE*/ 0x0703, s_addr >> 1) < 0)
            {
                printf("video ioctl\r\n");
            }
            else
            {
                for(i = 0; i < numberOfvideoRegisters ; i++)
                {
                    memset(buff,0x0,4);
                    buff[0] = (unsigned char)ControllerVideo1Byte[(i*2)+0];
                    buff[1] = (unsigned char)(ControllerVideo1Byte[(i*2)+1] & 0xFF);
                    if(write(fd, &buff, 2) != 2)
                    {
                        if(write(fd, &buff, 2) != 2)
                        {
                            //perror("Writing Video1 I2c Registers");
                            return(1);
                        }

                    }
                }
            }

        }
    }

    close(fd);

    return(0);
}

/*-----------------------------------------------------------------------------------*/
//
//  void perror_quit(char *str)
//
//  Function to print the error string
//  Input   : Error String
//  OutPut  : 
//  Returns : 
//            
/*-----------------------------------------------------------------------------------*/
void perror_quit(char *str)
{
    /*----------------------------------------------------*/

	//fprintf(stderr, "FATAL ERROR: ");
	//perror(str);

    /*----------------------------------------------------*/

    //Report an error but do not quit program.
//	exit(1);

}

