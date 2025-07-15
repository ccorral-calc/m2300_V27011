//Lase Update: 06/10/2005

/*-----------------------------------------------------------------------------------*/
//  Debug flag  : 1 = ON , 0 = OFF

#define DEBUG_ENABLE 0

/*Define the Slots which contain boards structure*/
int SlotContainsBoard[7];


/*-----------------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------------*/
//  the structure to hold the temperature 
//  from devices on all boards 

struct I2c_Temp_Struct {
	int dev1TemperatureInternal; //MAX6648/MAX6692
	int dev1TemperatureExternal; //MAX6648/MAX6692
	int dev2Temperature; //DS1631/DS1621 
    int current;         //PCF8591 In mAmps   
    int BoardType;       //Controller = 0x01, Video = 0x02, MP = 0x03, PS = 0x04 UNKNOWN = 0xFF
};
/*-----------------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------------*/
//  Function to get the temperatures from devices
//  Input   : Number of busses available
//  OutPut  : Structure that holds the temperature on all the devices
//  Returns : 0 if Pass
//            1 if Failed  
/*-----------------------------------------------------------------------------------*/

int M2300_I2C_Get_DeviceTemperature(int numOfBusses ,struct I2c_Temp_Struct* DeviceStatus);

