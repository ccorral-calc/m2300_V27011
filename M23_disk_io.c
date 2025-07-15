#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>

#include <errno.h>

#include "M23_Controller.h"
#include "M23_disk_io.h"

#include "M23_Typedefs.h"

#include "M23_Login.h"
#include "M23_features.h"

#include <unistd.h>
#include <linux/unistd.h>
#include <linux/fs.h>



#define  MAX_DEVICES 2
#define  MAX_ID  5  //max phy nodes on the bus

#define SEND_CMD    0xa
#define DEV_LUN     0x0 //if we set it to 0x1 CDB byte, 2 bits 5-7 hold LUN 

#define LOGIN_SUCCESSFUL   0x41

#define SENSE_BUFF_LEN  32
#define DEF_TIMEOUT 1000       /* 1,000 millisecs == 1 seconds */


static struct ohci_link_regs_t *ohci_reg_ptr;
static int phy_id;
static int Generation;
static int NumberOfNodes;

static int DiskAccess;

//local Function to see if target is Calculex device
int isCalculexDisk(int deviceHandle);

static int CartridgeIsConnected;
static int CurrentWritePointer;
static int CurrentReadPointer;
static int CurrentRecordPointer;


/////////////////////////////////////////////////////////////////////////////
// Used to determine if a cartridge has been removed or inserted.
//
// InPut:
//  
// returns:
//  0 if No Change
//  1 if Cartridge Inserted , need to perform Mount
//  2 if Cartridge Removed  , need to perform Dismount 
/////////////////////////////////////////////////////////////////////////////
int M23_GetCartridgeStatus()
{
    int status = 0;
    int return_status;
    int notpresent;
    int notlatched;

    GetCartridgeHealth(&notpresent,&notlatched);

    if(notpresent)
    {
        /*Cartridge Not Present*/
        status = 2;
    }
    else
    {
        /*Check if already logged in*/
        return_status = DiskIsConnected();
        if(return_status)
        {
            /*Cartridge is already connected*/
            status = 0;
        }
        else
        {
            /*Cartridge Not logged in,go ahead an log in*/
            status = 1;
        }
     
    }

    return(status);
}

int InitializeDevice()
{
    UINT32 base_addr;

    phy_id = -1;

    //get base address
    base_addr = get_ohci_base_address();
    if(base_addr != 0 && base_addr != -1)
    {
        //printf("OHCI base address = 0x%08lx\r\n", base_addr);
        ohci_reg_ptr = (struct ohci_link_regs_t*)base_addr;
    }

    CartridgeIsConnected = 0;

    DiskAccess = 0;

    return(0);
}

/////////////////////////////////////////////////////////////////////////////
// Used to connect to the device in both READ and WRITE mode.
//
// InPut:
//  
// returns:
//  0 if successful
// -1 on error
/////////////////////////////////////////////////////////////////////////////
int DiskConnect(void)
{
    int status = DiskIsConnected();
    int i;
    int loops;
    int target_id = -1;
    int ret;
    int debug;
    int config;
    int isRemote;

    PHY_NODE node[MAX_ID];

    if(status == 0)
    {
         M23_GetConfig(&config);
        if(config == LOCKHEED_CONFIG)
        {
            if(IEEE1394_Lockheed_Bus_Speed == BUS_SPEED_S400)
            {
                ret = set_bus_speed(Speed_400);
                if(ret != 0)
                {
                    //printf("cannot set speed\r\n");
                    return 0;
                }
            }
            else
            {
                ret = set_bus_speed(Speed_800);
                //ret = set_bus_speed(Speed_400);
                if(ret != 0)
                {
                    //printf("cannot set speed\r\n");
                    return 0;
                }
            }
        }
        else
        {

            if(IEEE1394_Bus_Speed == BUS_SPEED_S400)
            {
                ret = set_bus_speed(Speed_400);
                if(ret != 0)
                {
                    //printf("cannot set speed\r\n");
                    return 0;
                }
            }
            else
            {
                //ret = set_bus_speed(Speed_800);
                ret = set_bus_speed(Speed_400);
                if(ret != 0)
                {
                    //printf("cannot set speed\r\n");
                    return 0;
                }
            }
        }

        M23_GetDebugValue(&debug);
        for(loops = 0; loops < 15; loops++)
        {
            ret = initialize_ohci(ohci_reg_ptr);
            force_root(ohci_reg_ptr);
            usleep(1000000);

             // get topology map
            for(i=0; i<MAX_ID; i++)
            {
                node[i].type = Unknown_Device;
            }

            ret = get_node_map(node,&isRemote);
//printf("Number of Nodes Received = %d\r\n",ret);
            if(ret == 0) //if we get node 0 on bus, we failed
            {
                force_root(ohci_reg_ptr);
                usleep(1000000);
                continue;
            }

            for(i = 0;i < ret;i++)
            {
                if(node[i].type == Calculex_Controller)
                {
                    phy_id = node[i].node_id;
                    //printf("node %d is controller\r\n", phy_id);
                    break;
                }
            }
            for(i = 0;i < ret;i++)
            {
               if(node[i].type == Calculex_Cartridge)
               {
                    target_id = node[i].node_id;


                    if(config == LOCKHEED_CONFIG)
                    {
                        if(IEEE1394_Lockheed_Bus_Speed == BUS_SPEED_S400)
                        {
                            M23_WriteControllerCSR(IEEE1394B_BUS_SPEED,BUS_SPEED_S400 | BUS_PAYLOAD_S400);
                        }
                        else
                        {
                            //M23_WriteControllerCSR(IEEE1394B_BUS_SPEED,BUS_SPEED_S400 | BUS_PAYLOAD_S400);
                            M23_WriteControllerCSR(IEEE1394B_BUS_SPEED,BUS_SPEED_S800 | BUS_PAYLOAD_S800);
                        }
                    }
                    else
                    {

                        if(IEEE1394_Bus_Speed == BUS_SPEED_S400)
                        {
                            //printf("Set to S400\r\n");
                            M23_WriteControllerCSR(IEEE1394B_BUS_SPEED,BUS_SPEED_S400 | BUS_PAYLOAD_S400);
                        }
                        else
                        {
                            //printf("Set to S800\r\n");
                            M23_WriteControllerCSR(IEEE1394B_BUS_SPEED,BUS_SPEED_S400 | BUS_PAYLOAD_S400);
                            //M23_WriteControllerCSR(IEEE1394B_BUS_SPEED,BUS_SPEED_S800 | BUS_PAYLOAD_S800);
                        }
                    }
#if 0

                    /*Change this to use the TMATS variable*/
                    if(isRemote == 1)
                    {
                        //printf("Set to S400\r\n");
                        M23_WriteControllerCSR(IEEE1394B_BUS_SPEED,BUS_SPEED_S400 | BUS_PAYLOAD_S400);
                    }
                    else
                    {
                        //printf("Set to S800\r\n");
                        M23_WriteControllerCSR(IEEE1394B_BUS_SPEED,BUS_SPEED_S800 | BUS_PAYLOAD_S800);
                    }
#endif

                    i = ret;
                }
            }

            if(target_id == -1)
            {
                //printf("there is no cartridge connected, will not do login\r\n");
                 return -1;
            }
           
            //check to see if controller in the root
            if(phy_id == (ret - 1) ) //we found the root
            {
                break;
            }

            //Otherwise retry
            force_root(ohci_reg_ptr);
            //usleep(1000000);
            usleep(2000000);
        }

        if(phy_id == -1)
        {
          //printf("cannot get topology map after 15 retries\r\n");
          return -1;
        }

        //soft reset
        link_soft_reset( ohci_reg_ptr);

#if 0
        //set bus speed
        //if(isRemote == 1)
        if(IEEE1394_Bus_Speed == BUS_SPEED_S400)
        {
            ret = set_bus_speed(Speed_400);
            if(ret != 0)
            {
                //printf("cannot set speed\r\n");
                return 0;
            }
        }
        else
        {
            ret = set_bus_speed(Speed_400);
            //ret = set_bus_speed(Speed_800);
            if(ret != 0)
            {
                //printf("cannot set speed\r\n");
                return 0;
            }
        }
#endif

        //still do some initialize after bus reset
        ret = initialize_ohci(ohci_reg_ptr);
        if(ret != 0)
        {
            //printf("failed to initialize ohci\r\n");
            status = -1;
        }
        else
        {
            //do login
            if(phy_id != -1)
            {
//printf("Sending Login Orb ,Phy %d,Target %d\r\n",phy_id,target_id);
                ret = send_login_orb(ohci_reg_ptr, phy_id,target_id);
                if(debug)
                {
                    printf("Send_Login_ORB 0x%08x\r\n",ret);
                }

                //if(ret == LOGIN_SUCCESSFUL)
                if( (ret & 0xFF48) == 0x40)
                {
                    initialize_scsi_orb(phy_id);

                    CartridgeIsConnected = 1;
                    NumberOfNodes = (get_selfID_size() - 1) / 2;
                    M23_Set1394BCommandGo();
                    //printf("Cartridge Is Connected\r\n");

                }
                else
                {
                    status = -1;
                    //printf("Cartridge Not Connected, Login Failed\r\n");
                }

                Generation    = get_selfID_generation();
            }
            else
            {
                //printf("phy_id %d\r\n",phy_id);
                status = -1;
            }
        }
    }
  
    return(status);
}

/////////////////////////////////////////////////////////////////////////////
// Used to write data to disk after positioning with DiskSeek().
//
// InPut:
//  p_data      pointer to data to write
//  DataCount  number of bytes to write (must be multiple of block size)
//
// returns:
//  (byteswritten) if successful
//  (-1) on error
/////////////////////////////////////////////////////////////////////////////
int DiskWrite(UINT8 *p_data, UINT32 DataCount)
{
    int loops;
    int cdb[3];
    int num_blocks;
    int debug;
    int byteswritten;
    int state;

    UINT32 CSR;

    M23_GetDebugValue(&debug);
    M23_RecorderState(&state);

    /*Wait for DiskRead to complete*/
    if(state == STATUS_RECORD)
    {
        while(1)
        {
            if(DiskAccess == 0)
            {
                DiskAccess = 1;
                break;
            }
            else
            {
                usleep(1000);
            }
        }
    }


    M23_WriteControllerCSR(IEEE1394B_HOST_STATUS,0x0);

    num_blocks = DataCount/512;

    cdb[0] = (0x2A << 24) | ((CurrentWritePointer >> 16) & 0xFFFF);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_0,cdb[0]);
    cdb[1] = (CurrentWritePointer & 0xFFFF) << 16;
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_1,cdb[1]);
    cdb[2] =  1 << 24;   //always 1  block
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_2,cdb[2]);

    /*Write the size*/
    M23_WriteControllerCSR(IEEE1394B_HOST_ORB_SIZE,512);

    //M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,p_data);

    M23_UpdateDPRBlock(p_data);
    M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,HOST_ORB_ADDRESS);
   
    /*Set the direction for Host to Target, and clear  go*/
    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    CSR &= ~IEEE1394B_HOST_TO_TARGET;
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR | IEEE1394B_HOST_GO);

    usleep(10000);
     
    /*Now Wait for Host busy to clear*/
    loops = 0;
    while(1)
    {
        CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);

        if(CSR & IEEE1394B_HOST_BUSY)
        {
            usleep(1000);
            if(debug)
            {
                if(loops == 1000)
                   printf("DiskWrite - Host Busy Never Cleared\r\n");
            }
        }
        else
        {
            break;
        }

        loops++;
    }

    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    CSR &= ~IEEE1394B_HOST_GO;

    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR);

    byteswritten =  512;

    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_0,0x0);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_1,0x0);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_2,0x0);

    DiskAccess = 0;
    return(byteswritten);
}

/////////////////////////////////////////////////////////////////////////////
// Used to read data from disk after positioning with DiskSeek().
//
// InPut:
//  p_data     pointer to data to read
//  DataCount  number of bytes to read (must be multiple of block size)
//
// returns:
//  (bytesread) if successful
//  (-1) on error
/////////////////////////////////////////////////////////////////////////////
int DiskRead(UINT8 *p_data, UINT32 DataCount)
{
    int i;
    int debug;
    int cdb[3];
    int num_blocks;
    int loops;
    int state;

    unsigned long buffer[128];

    UINT32 CSR;
    UINT32 Status;

    INT32 bytesread=0;

    M23_GetDebugValue(&debug);
    M23_RecorderState(&state);


    /*Wait for DiskWrite to complete*/
    if(state == STATUS_RECORD)
    {
        while(1)
        {
            if(DiskAccess == 0)
            {
                DiskAccess = 1;
                break;
            }
            else
            {
                usleep(1000);
            }
        }
    }

    M23_WriteControllerCSR(IEEE1394B_HOST_STATUS,0x0);


    num_blocks = DataCount/512;

    /*Write the CDB to the Logic*/
    cdb[0] = (0x28 << 24) | ((CurrentReadPointer >> 16) & 0xFFFF);
    cdb[1] = (CurrentReadPointer & 0xFFFF) << 16;
    cdb[2] =  1 << 24;   //num of blocks

    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_0,cdb[0]);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_1,cdb[1]);
    M23_WriteControllerCSR(IEEE1394B_HOST_CDB_2,cdb[2]);


    /*Write the size*/
    M23_WriteControllerCSR(IEEE1394B_HOST_ORB_SIZE,512);

    /*Write the address*/
    //M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,(p_data + (i*512)) );
    //M23_WriteControllerCSR(IEEE1394B_HOST_DEST_ADDRESS,HOST_ORB_ADDRESS);
    //M23_ReadMemorySpace((p_data + (i*512) ),512);

    /*Set the direction for Host to Target, and clear  go*/
    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR | IEEE1394B_HOST_GO | IEEE1394B_HOST_TO_TARGET);

    usleep(1000);

    /*Now Wait for Host busy to clear*/
    loops = 0;
    while(1)
    {
        CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);

        if(CSR & IEEE1394B_HOST_BUSY)
        {
            loops++;
            if(loops == 900)
            {

                if(debug)
                    printf("Disk Read - Host Busy Never Cleared 0x%x from 0x%x\r\n",CSR,IEEE1394B_HOST_CONTROL);

                bytesread = 0;

                break;
            }
            else
            {
                usleep(1000);
            }
        }
        else
        {
            Status = M23_ReadControllerCSR(IEEE1394B_HOST_STATUS);
            if(Status == 0x4100)
            {
                for(i = 0; i < 128; i++)
                {
                    buffer[i] = READ32(HOST_ORB_READ_ADDRESS + (i*4));
                }

                /*Get the data*/
                memcpy(p_data ,(unsigned char*)buffer ,512);

                bytesread = 512;

                CurrentReadPointer++;
            }
            else
            {
                if(debug)
                    printf("Disk Read - Bad Status 0x%x\r\n",Status);
            }

            break;
        }
    }

    CSR = M23_ReadControllerCSR(IEEE1394B_HOST_CONTROL);
    CSR &= ~IEEE1394B_HOST_GO;
    M23_WriteControllerCSR(IEEE1394B_HOST_CONTROL,CSR);


    DiskAccess = 0;
    return(bytesread);

}

/////////////////////////////////////////////////////////////////////////////
// Used to set a block offset to allow reading from that point.
//
// InPut:
//  startOffset     pointer to start byte offset on disk
//  
// returns:
//  (0) if successful
//  (-1) on error
/////////////////////////////////////////////////////////////////////////////
int DiskReadSeek(UINT32 startBlock)
{

    CurrentReadPointer = startBlock;
    return(0);

}

/////////////////////////////////////////////////////////////////////////////
// Used to set a block offset to allow Writing from that point.
//
// InPut:
//  startOffset     pointer to start byte offset on disk
//  
// returns:
//  (0) if successful
//  (-1) on error
/////////////////////////////////////////////////////////////////////////////
int DiskWriteSeek(UINT32 startBlock)
{

    CurrentWritePointer = startBlock;


    return(0);

}

/////////////////////////////////////////////////////////////////////////////
// Used to set a block offset to allow Recording from that point.
//
// InPut:
//  startOffset     pointer to start byte offset on disk
//  
// returns:
//  (0) if successful
//  (-1) on error
/////////////////////////////////////////////////////////////////////////////
int DiskRecordSeek(UINT32 startBlock)
{

    /*Set the Record Pointer in the Logic*/
    M23_WriteControllerCSR(IEEE1394B_RECORD_POINTER_IN,startBlock);
    CurrentRecordPointer = startBlock;
    
    return(0);

}

/////////////////////////////////////////////////////////////////////////////
// Used to set a block offset to allow Playback from that point.
//
// InPut:
//  startOffset     pointer to start byte offset on disk
//  
// returns:
//  (0) if successful
//  (-1) on error
/////////////////////////////////////////////////////////////////////////////
int DiskPlaybackSeek(UINT32 startBlock)
{

    /*Set the Playback Pointer in the Logic*/
    M23_WriteControllerCSR(IEEE1394B_PLAYBACK_POINTER_IN,startBlock);
    sSetPlayLocation(startBlock);
    
    return(0);

}

/////////////////////////////////////////////////////////////////////////////
// Used to disconnect from the device.
//
// InPut:
//  
// returns:
//  (0) if successful
//  (-1) on error
/////////////////////////////////////////////////////////////////////////////
int DiskDisconnect(void)
{
    CartridgeIsConnected = 0;

    return(0);
}

/////////////////////////////////////////////////////////////////////////////
// Used to find if device is connected or disconnected.
//
// InPut:
//  
// returns:
//  (DeviceHandler) -1 if not connected, 
/////////////////////////////////////////////////////////////////////////////
int DiskIsConnected(void)
{
    int status = 0;
    if(CartridgeIsConnected)
    {
        if(!(HealthStatusArr[RECORDER_FEATURE] & M23_DISK_NOT_CONNECTED))
        {
            status = 1;
        }

    }

    return(status);
}

/////////////////////////////////////////////////////////////////////////////
// Used to set device to connected .
//
// InPut:
//  
// returns:
//  (DeviceHandler) -1 if not connected, 
/////////////////////////////////////////////////////////////////////////////
void DiskSetConnect(void)
{
    CartridgeIsConnected = 1;
}

/////////////////////////////////////////////////////////////////////////////
// Used to find if Total disk blocks and Blocks used till last closed file
//
// InPut:
//  
// returns:
//  (DeviceHandler) -1 if not connected, 
/////////////////////////////////////////////////////////////////////////////
int DiskInfo(UINT32 *TotalAvailableBlocks )
{
    int previous = 0;
    int blocks = 0;
    int i = 0;

    while(1)
    {
        blocks = M2x_ReadCapacity();
        if(blocks == previous)
        {
            break;
        }
        else
        {
            i++;
            previous = blocks;
            if(i == 10)
            {
                break;
            }
        }
        usleep(10000);
    }


   //*TotalAvailableBlocks = (UINT32)M2x_ReadCapacity();
   *TotalAvailableBlocks = (UINT32)blocks;

   return(0);
}
/////////////////////////////////////////////////////////////////////////////
// Local function. It is used to find if the target device path is CALCULEX or NOT
// InPut:  deviceHandle                  handle to the device
//           
// returns:
//          0 if not calculex
//          1 if calculex 
/////////////////////////////////////////////////////////////////////////////
int isCalculexDisk(int deviceHandle)
{

    return(0);//it is NOT CALCULEX disk

}

/////////////////////////////////////////////////////////////////////////////
//     int M23_GetRecordPointer()
//           
// returns:
//         StartOfLastRecord 
/////////////////////////////////////////////////////////////////////////////
int M23_GetRecordPointer()
{

    return(CurrentRecordPointer);

}




