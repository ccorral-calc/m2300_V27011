#include <unistd.h>
#include <sys/time.h>
#include "M23_Controller.h"
#include "M23_disk_io.h"
#include "M23_Stanag.h"
#include "time.h"
#include "M23_Typedefs.h"

int NodeList[512];

static STANAG_DIR_BLOCK_HOLDER FileTable[MAX_STANAG_BLOCKS];
static STANAG_DIR_BLOCK_HOLDER Stanag_Swaped[MAX_STANAG_BLOCKS];

static UINT32 CurrentFileIndex;    /*0 - MAX_NUM_FILES-1 ->excludes Event Files*/

static UINT32 CurrentFileNumber;    /*0 - MAX_NUM_FILES-1*/
static BOOL   CurrentFileState;     /*0 open 1 close*/
static UINT64 CurrentWriteBlock;    /*this value holds the block number on Disk to write*/
static UINT64 TotalBytesWritten;
static UINT64 CurrentReadBlock;     /*this value holds the block number on Disk to read*/
static UINT64 CurrentPlayBlock;     /*this value holds the block number on Disk to play*/
static UINT64 NumberOfBlocksWritten;

static UINT32 TotalAvailableBlocks;

static UINT32 CurrentStanagBlock;

static int InitializeStanag();
static int FileStoreFileTable();
static int FileRetrieveFileTable();
static int swap_Stanag();

static int GetSystemDateStr( char *Date );
static int GetSystemTimeStr( char *Time );

static int FixLastEntry(UINT32 LastBlock , UINT32 BytesWritten);

static int UpdateStanag(int mode);

static UINT32 currentFileBlock;

/******************************************************************************/
//                          int sinit()
//
//    This function is called when ever the Cartridge is connected
//    It performs a connect operation and gets a handler to the disk
//    reads and writes and seek are performed on this handler
//    Performs a Retrieve File Table from Disk ...if fails it simply
//    initializes the file table to contain no files 
//
//    Return Value:
//      0 = Passed  
//      1 = Failed
//
/******************************************************************************/
int sinit()
{

    InitializeDevice();

    if(DiskConnect() == -1)
    {
      //printf("Failed Connecting to Disk...\r\n");
      return(-1);
    }

    DiskInfo( &TotalAvailableBlocks );

  
    CurrentWriteBlock = RESERVED_BLOCKS;
    CurrentReadBlock  = RESERVED_BLOCKS;
    CurrentPlayBlock  = RESERVED_BLOCKS;

    CurrentFileNumber = 0;
    CurrentFileIndex  = 0;
    CurrentFileState  = 1;

    //Whenever Perform a System Initialize, Load the previous Stanag to the memory
    //Later on add capability to determine if the Stanag is valid else just init to 0
    if(FileRetrieveFileTable() == -1)
    {
      if(InitializeStanag() == -1)
      {
          printf("FileTable Could not be initialized\r\n");
          return(-1);
      }
      CurrentStanagBlock = 0;
    }

    DiskRecordSeek(CurrentWriteBlock);


    return(0);

}

/******************************************************************************/
//                              int UpdateStanagAndBlocks()
//
//  This Function is updates the Stanag entry on the cartridge and the total 
//  blocks used.
//  
//  Return Value:
//      0 = Passed
//
/******************************************************************************/
void UpdateStanagAndBlocks()
{
    int    notpresent;
    int    notlatched;

    UINT32 CSR;
    UINT64 Blocks;

    GetCartridgeHealth(&notpresent,&notlatched);

    if(notpresent)
    {
        return;
    }
    else
    {
        /*Read Number of Blocks Record from the start of this recording*/
        CSR = M23_ReadControllerCSR(IEEE1394B_RECORD_BLOCKS_RECORDED);
        NumberOfBlocksWritten = CSR;
        Blocks = CSR;
        TotalBytesWritten = (UINT64)(Blocks * 512);

        UpdateStanag(0);
    }

}
/******************************************************************************/
//                              int UpdateStanagBlocks(int offset)
//
//  This Function is updates the Stanag entry on the cartridge and the total 
//  blocks used.
//  
//  Return Value:
//      0 = Passed
//
/******************************************************************************/
void UpdateStanagBlocks(UINT64 offset,int pkt_len)
{
    int    notpresent;
    int    notlatched;
    int    count;

    UINT32 blocks;
    UINT32 CSR;

    UINT64 RootBlocks;
    UINT64 Length;

    GetCartridgeHealth(&notpresent,&notlatched);

    if(notpresent)
    {
        return;
    }
    else
    {

        count = 0;
  
#if 0
        while(1)
        {
            CSR = M23_ReadControllerCSR(IEEE1394B_RECORD_BLOCKS_RECORDED);
            Blocks = CurrentWriteBlock + CSR;
            RootBlocks = CurrentWriteBlock + offset;
            if(RootBlocks >= Blocks)
            {
                break;
            }
            else
            {
                count++;
                if(count > 100)
                {
                    printf("Blocks Not Written %lld < %lld\r\n",RootBlocks,Blocks);
                }
                else
                {
                    usleep(100);
                }
            }
        }
#endif

        TotalBytesWritten = (UINT64)(offset + pkt_len);
        if(TotalBytesWritten % 4)
        {
             printf("ERROR - Odd Length offset = %lld, len %d\r\n",offset,pkt_len);
        }

        UpdateStanag(0);

        if(TotalBytesWritten)
        {
            if(TotalBytesWritten % BLOCK_SIZE)
            {
                NumberOfBlocksWritten = (TotalBytesWritten/BLOCK_SIZE) + 1;
            }
            else
            {
                NumberOfBlocksWritten = (TotalBytesWritten/BLOCK_SIZE);
            }
        }

    }

}
void UpdateTmatsRecordStanag(UINT64 offset,int pkt_len)
{
    int    count;

    UINT32 blocks;
    UINT32 CSR;

    UINT64 RootBlocks;
    UINT64 Length;


    TotalBytesWritten = (UINT64)(offset + pkt_len);
    if(TotalBytesWritten % 4)
    {
         printf("ERROR - Odd Length offset = %lld, len %d\r\n",offset,pkt_len);
    }

    UpdateStanag(0);

    if(TotalBytesWritten)
    {
        if(TotalBytesWritten % BLOCK_SIZE)
        {
            NumberOfBlocksWritten = (TotalBytesWritten/BLOCK_SIZE) + 1;
        }
        else
        {
            NumberOfBlocksWritten = (TotalBytesWritten/BLOCK_SIZE);
        }
   }

}


/******************************************************************************/
//                              int InitializeStanag()
//
//  This Function is called by sinit() all it does is creates an
//  Empty stanag for Max number of file entries all the empty Stanag blocks are
//  linked together at this point.
//  
//  Return Value:
//      0 = Passed
//
/******************************************************************************/
static int InitializeStanag()
{
  int i = 0;
  int j = 0;
  int k = 0;

  //int StanagFirstBlock = 1;
  /*initialization of STANAG has been done using NATO STANAG4575 document*/
  //Later on change code to only init first blosk and appent as we record files


  for(k = 0; k < MAX_STANAG_BLOCKS; k++)
  {

      memset(&FileTable[k],0,sizeof(FileTable));
      FileTable[k].STANAG_MagicNumber[0] = 'F';
      FileTable[k].STANAG_MagicNumber[1] = 'O';
      FileTable[k].STANAG_MagicNumber[2] = 'R';
      FileTable[k].STANAG_MagicNumber[3] = 'T';
      FileTable[k].STANAG_MagicNumber[4] = 'Y';
      FileTable[k].STANAG_MagicNumber[5] = 't';
      FileTable[k].STANAG_MagicNumber[6] = 'w';
      FileTable[k].STANAG_MagicNumber[7] = 'o';

      FileTable[k].STANAG_RevisionNumber = STANAG_REV_NUM;	 

      FileTable[k].STANAG_ShutDown = 0x00;//Initialize volume to improper shutdown
                                          //On proper Shutdown we set it to 0xFF 

      FileTable[k].STANAG_NumberOfFilesEntries=0x00;	

      FileTable[k].STANAG_CurrBlock = 512; /*Change for 106-7*/
  
      memset(FileTable[k].STANAG_VolumeName,0x00,sizeof(FileTable[k].STANAG_VolumeName));
  
      if(k==0)
      {
        FileTable[k].STANAG_ForwardLink=1;
        FileTable[k].STANAG_ReverseLink=1;
      }
      else
      {
          FileTable[k].STANAG_ForwardLink=FileTable[k-1].STANAG_ForwardLink + 1;
          FileTable[k].STANAG_ReverseLink=FileTable[k-1].STANAG_ForwardLink - 1;
      }

      if( k == MAX_STANAG_BLOCKS-1)
      {
        FileTable[k].STANAG_ForwardLink=FileTable[k-1].STANAG_ForwardLink;
        FileTable[k].STANAG_ReverseLink=FileTable[k-1].STANAG_ForwardLink - 1;
      }

      for (j = 0; j < MAX_NUM_FILES_IN_STANAG_BLOCK; j++)
      {
          memset(FileTable[k].STANAG_FileEntries[j].FILE_FileName,0xFF,56);

          //FileTable[k].STANAG_FileEntries[j].FILE_StartAddress = 0xFFFFFFFFFFFFFFFF;
          memset(&FileTable[k].STANAG_FileEntries[j].FILE_StartAddress,0xFF,8);

          //FileTable[k].STANAG_FileEntries[j].FILE_BlockCount = 0xFFFFFFFFFFFFFFFF;
          memset(&FileTable[k].STANAG_FileEntries[j].FILE_BlockCount,0xFF,8);

          //FileTable[k].STANAG_FileEntries[j].FILE_Size = 0xFFFFFFFFFFFFFFFF;
          memset(&FileTable[k].STANAG_FileEntries[j].FILE_Size,0xFF,8);
  
          for( i = 0 ; i < 8 ; i++)
          {
             FileTable[k].STANAG_FileEntries[j].FILE_CreateDate[i] = 0xFF; 
          }

          for( i = 0 ; i < 8 ; i++)
          {
             FileTable[k].STANAG_FileEntries[j].FILE_CreateTime[i] = 0xFF;
          }

          FileTable[k].STANAG_FileEntries[j].FILE_TimeType=0xFF;
      
          for( i = 0 ; i < 7 ; i++ )
          {
             FileTable[k].STANAG_FileEntries[j].FILE_Reserved[i] = 0xFF;
          }

          for( i = 0 ; i < 8 ; i++ )
          {
             FileTable[k].STANAG_FileEntries[j].FILE_CloseTime[i] = 0xFF;
          }
      }

  }

  return(0);

}

/******************************************************************************/
//                              int M23_AddNewStanag()
//
//  This Function will add Stanag Parameters used when moving the Tmats File
//  
//  Return Value:
//      0 = Passed
//
/******************************************************************************/
void M23_AddNewStanag(int size,int blocks)
{
    FileTable[0].STANAG_FileEntries[0].FILE_BlockCount = blocks;
    FileTable[0].STANAG_FileEntries[0].FILE_Size = size;
}

/******************************************************************************/
//                              int InitializeStanagErase()
//
//  This Function is called by searse() all it does is creates an
//  Empty stanag after the first File Entry
//  
//  Return Value:
//      0 = Passed
//
/******************************************************************************/
static int InitializeStanagErase()
{
    int i = 0;
    int j = 1;
    int k = 1;

    int debug;

    M23_GetDebugValue(&debug);
    //int StanagFirstBlock = 1;
    /*initialization of STANAG has been done using NATO STANAG4575 document*/
    //Later on change code to only init first blosk and appent as we record files


    for(k = 1; k < MAX_STANAG_BLOCKS; k++)
    {
        memset(&FileTable[k],0x0,sizeof(FileTable));

        FileTable[k].STANAG_MagicNumber[0] = 'F';
        FileTable[k].STANAG_MagicNumber[1] = 'O';
        FileTable[k].STANAG_MagicNumber[2] = 'R';
        FileTable[k].STANAG_MagicNumber[3] = 'T';
        FileTable[k].STANAG_MagicNumber[4] = 'Y';
        FileTable[k].STANAG_MagicNumber[5] = 't';
        FileTable[k].STANAG_MagicNumber[6] = 'w';
        FileTable[k].STANAG_MagicNumber[7] = 'o';

        FileTable[k].STANAG_RevisionNumber = STANAG_REV_NUM;	 

        FileTable[k].STANAG_ShutDown = 0x00;//Initialize volume to improper shutdown
                                          //On proper Shutdown we set it to 0xFF 

        FileTable[k].STANAG_NumberOfFilesEntries=0x00;	

        FileTable[k].STANAG_CurrBlock = 512; /*Change for 106-7*/
  
        memset(FileTable[k].STANAG_VolumeName,0x00,sizeof(FileTable[k].STANAG_VolumeName));
  
        if(k==0)
        {
            FileTable[k].STANAG_ForwardLink=1;
            FileTable[k].STANAG_ReverseLink=1;
        }
        else
        {
            FileTable[k].STANAG_ForwardLink=FileTable[k-1].STANAG_ForwardLink + 1;
            FileTable[k].STANAG_ReverseLink=FileTable[k-1].STANAG_ForwardLink - 1;
        }

        if( k == MAX_STANAG_BLOCKS-1)
        {
            FileTable[k].STANAG_ForwardLink=FileTable[k-1].STANAG_ForwardLink;
            FileTable[k].STANAG_ReverseLink=FileTable[k-1].STANAG_ForwardLink - 1;
        }

        for (j = 0; j < MAX_NUM_FILES_IN_STANAG_BLOCK; j++)
        {
            memset(FileTable[k].STANAG_FileEntries[j].FILE_FileName,0xFF,56);

            //FileTable[k].STANAG_FileEntries[j].FILE_StartAddress = 0xFFFFFFFFFFFFFFFF;
            memset(&FileTable[k].STANAG_FileEntries[j].FILE_StartAddress,0xFF,8);

            //FileTable[k].STANAG_FileEntries[j].FILE_BlockCount = 0xFFFFFFFFFFFFFFFF;
            memset(&FileTable[k].STANAG_FileEntries[j].FILE_BlockCount,0xFF,8);

            //FileTable[k].STANAG_FileEntries[j].FILE_Size = 0xFFFFFFFFFFFFFFFF;
            memset(&FileTable[k].STANAG_FileEntries[j].FILE_Size,0xFF,8);
  
            for( i = 0 ; i < 8 ; i++)
            {
                FileTable[k].STANAG_FileEntries[j].FILE_CreateDate[i] = 0xFF; 
            }

            for( i = 0 ; i < 8 ; i++)
            {
                FileTable[k].STANAG_FileEntries[j].FILE_CreateTime[i] = 0xFF;
            }

            FileTable[k].STANAG_FileEntries[j].FILE_TimeType=0xFF;
      
            for( i = 0 ; i < 7 ; i++ )
            {
                FileTable[k].STANAG_FileEntries[j].FILE_Reserved[i] = 0xFF;
            }

            for( i = 0 ; i < 8 ; i++ )
            {
                FileTable[k].STANAG_FileEntries[j].FILE_CloseTime[i] = 0xFF;
            }
        }
    }

    /*Clear all file entries except for the first one, should be TMATS configuration file*/
    FileTable[0].STANAG_ForwardLink = 1;
    FileTable[0].STANAG_ReverseLink = 1;
    FileTable[0].STANAG_NumberOfFilesEntries = 0x01;	
    FileTable[0].STANAG_ShutDown = 0xFF;//Initialize volume to proper shutdown
    FileTable[0].STANAG_FileEntries[0].FILE_StartAddress = 129;

    for (j = 1; j < MAX_NUM_FILES_IN_STANAG_BLOCK; j++)
    {
        memset(FileTable[0].STANAG_FileEntries[j].FILE_FileName,0xFF,56);

        memset(&FileTable[0].STANAG_FileEntries[j].FILE_StartAddress,0xFF,8);

        memset(&FileTable[0].STANAG_FileEntries[j].FILE_BlockCount,0xFF,8);

        memset(&FileTable[0].STANAG_FileEntries[j].FILE_Size,0xFF,8);
 
        for( i = 0 ; i < 8 ; i++)
        {
            FileTable[0].STANAG_FileEntries[j].FILE_CreateDate[i] = 0xFF; 
        }

        for( i = 0 ; i < 8 ; i++)
        {
            FileTable[0].STANAG_FileEntries[j].FILE_CreateTime[i] = 0xFF;
        }

        FileTable[0].STANAG_FileEntries[j].FILE_TimeType=0xFF;
      
        for( i = 0 ; i < 7 ; i++ )
        {
            FileTable[0].STANAG_FileEntries[j].FILE_Reserved[i] = 0xFF;
        }

        for( i = 0 ; i < 8 ; i++ )
        {
            FileTable[0].STANAG_FileEntries[j].FILE_CloseTime[i] = 0xFF;
        }
    }

    CurrentWriteBlock = FileTable[0].STANAG_FileEntries[0].FILE_StartAddress +  FileTable[0].STANAG_FileEntries[0].FILE_BlockCount;
    swap_Stanag();

    for(k = 0; k < MAX_STANAG_BLOCKS; k++)
    {
       DiskWriteSeek(SYSTEM_RESERVE_BLOCKS + k);
       DiskWrite( (UINT8*)&Stanag_Swaped[k] , 512);
    }

#if 0
    if(FileStoreFileTable()==-1)
    {
      printf("Error: Storing filetable\r\n");
      return(-1);
    }
#endif

  return(0);
}


/************************************************************************************/
//              void sUpdateVolumeName(char *VolName)
//
//  This function will update the volume name in the first STANAG directory block
/***********************************************************************************/
void sUpdateVolumeName(char *VolName)
{
    int i;

    if( DiskIsConnected() )
    {
        for(i = 0; i < (CurrentStanagBlock + 1);i++)
        {
            memset(FileTable[i].STANAG_VolumeName,0x00,sizeof(FileTable[i].STANAG_VolumeName));
            strcpy(FileTable[i].STANAG_VolumeName,VolName);
        }
    }
}

void sopenNode()
{
    DiskWriteSeek(CurrentWriteBlock);
}


/******************************************************************************/
//              sFILE sopen(const char *filename , const char *mode)
//
//  This function takes a file name parameter and a mode (read write etc)
//  moad is ignored at this point, file name will be loaded to the stanag
//  if no file name is entered a generic filename will be generated.
//  Main goal of this function is to append an entry in the stanag and load
//  the current time and current block offset. 
//  To be safe a Diskseek is performed to the current write address.         
//
//  This new entry is not written to the disk yet its done at close time.
//
//  Return Value:
//      it returns sFILE (int) that is not of any use at this point.
//      this is the file number of this entry inn Stanag.
//
/******************************************************************************/
sFILE sopen(const char *filename , const char *mode)
{
    int    verbose;
    int i = 0;
    UINT32 stanag_table_block = 0;

    char TimeString[8];
    char DateString[8];

    unsigned char name[80];

    memset(name,0x0,80);


    if(CurrentFileState == 0)
    {
        //printf("File already open\r\n");
        return(-1);
    }
    else
    {
        CurrentFileState = 0; /*0 open 1 close*/
    }

    TotalBytesWritten = 0;

    DiskRecordSeek(CurrentWriteBlock);

    stanag_table_block =  (CurrentFileNumber / MAX_NUM_FILES_IN_STANAG_BLOCK);

    FileTable[stanag_table_block].STANAG_ShutDown = 0x00;//Initialize volume to improper shutdown

    if(stanag_table_block==0)
    {
        FileTable[stanag_table_block].STANAG_ForwardLink=1;
        FileTable[stanag_table_block].STANAG_ReverseLink=1;
    }
    else
    {   
        FileTable[stanag_table_block].STANAG_ForwardLink = stanag_table_block + 1;
        FileTable[stanag_table_block].STANAG_ReverseLink = FileTable[stanag_table_block].STANAG_ForwardLink - 1;
        FileTable[stanag_table_block - 1 ].STANAG_ForwardLink = FileTable[stanag_table_block].STANAG_ReverseLink + 1;

        strcpy(FileTable[stanag_table_block].STANAG_VolumeName,FileTable[stanag_table_block -1].STANAG_VolumeName);
    }

    if((CurrentFileNumber % 4) == 0)
    {
        FileTable[stanag_table_block].STANAG_MagicNumber[0] = 'F';
        FileTable[stanag_table_block].STANAG_MagicNumber[1] = 'O';
        FileTable[stanag_table_block].STANAG_MagicNumber[2] = 'R';
        FileTable[stanag_table_block].STANAG_MagicNumber[3] = 'T';
        FileTable[stanag_table_block].STANAG_MagicNumber[4] = 'Y';
        FileTable[stanag_table_block].STANAG_MagicNumber[5] = 't';
        FileTable[stanag_table_block].STANAG_MagicNumber[6] = 'w';
        FileTable[stanag_table_block].STANAG_MagicNumber[7] = 'o';

        FileTable[stanag_table_block].STANAG_RevisionNumber = STANAG_REV_NUM;	 
        FileTable[stanag_table_block].STANAG_CurrBlock = 512; /*Change for 106-7*/
        if(stanag_table_block > 0)
        {
            strcpy(FileTable[stanag_table_block].STANAG_VolumeName,FileTable[stanag_table_block -1].STANAG_VolumeName);
        }
    }

    CurrentStanagBlock = stanag_table_block;

    FileTable[stanag_table_block].STANAG_NumberOfFilesEntries = FileTable[stanag_table_block].STANAG_NumberOfFilesEntries + 1;

    memset(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_FileName,0x0,56);

    if(strlen(filename) == 0)
    {

        if(LoadedTmatsFromCartridge)
        {
            sprintf(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_FileName,"File%03d",(UINT32)CurrentFileIndex);
        }
        else
        {
            sprintf(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_FileName,"File%03d",(UINT32)CurrentFileIndex+1);
        }
    }
    else
    {
        sprintf(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_FileName,"%s",filename);
    }

    M23_GetVerbose(&verbose);

    if(verbose)
    {
        sprintf(name,"\r\nRecording %s\r\n",FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_FileName);
        PutResponse(0,name);
    }
  

    FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_StartAddress=CurrentWriteBlock;

    FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_BlockCount=0;  

    FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_Size=0;  

    FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_TimeType = 0x01;  


    for( i = 0 ; i < 7 ; i++ )
    {
        FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_Reserved[i] = 0xFF;  

    }

    //Add capability to get right system time
    GetSystemDateStr( &DateString[0] );

    sprintf(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_CreateDate,"%s",DateString);

    GetSystemTimeStr( &TimeString[0] );

    sprintf(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_CreateTime,"%s",TimeString);    

    sprintf(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_CloseTime,"%s",TimeString);    

    NumberOfBlocksWritten = 0;

    M23_UpdateRecordBlock(CurrentWriteBlock);

    return(CurrentFileNumber);

}

/******************************************************************************/
//                size_t swrite(buffer, size,count,stream)
//
//  this function can be called repetedly after sopen() 
//  user passes a buffer a size (data size) and count (times to write)    
//  file pointer (stream) is not used at this point. 
//  For each file there is a Variable TotalBytesWritten that keeps incrementing
//  by this function. 
//
//  Return Value:
//      Number of bytes Written
//
/******************************************************************************/
size_t swrite(char *buffer, size_t size, size_t count, sFILE stream)
{

    int    num_blocks;
    UINT32 byteswritten = 0;
    UINT32 bytestowrite = size * count;

    if( !DiskIsConnected() )
    {
      //printf("handle invalid:\n");
      return( byteswritten );
    }

    num_blocks = bytestowrite/512;
    if(bytestowrite % 512)
    {
        num_blocks++;
    }

    DiskWrite( buffer, bytestowrite );

    byteswritten = num_blocks * 512;

    if( byteswritten > 0 )
    {
        TotalBytesWritten = TotalBytesWritten +  byteswritten;
    }

    return(byteswritten);

}

/******************************************************************************/
//         size_t sread(buffer, size , count, stream)
//
//  this function reads the requested amount int the buffer provided.
//  File pointer to read from is ignored at this point
//  
//  Return Value:
//      Bytes read
//
/******************************************************************************/
size_t sread(char *buffer, size_t size, size_t count, sFILE stream)
{

    UINT32 bytestoread = size * count;
    UINT32 bytesRead = 0;

    if( !DiskIsConnected() )
    {
        return( -1 );
    }

    if((bytestoread % 512) == 0)
    {
        DiskReadSeek(CurrentReadBlock);

        bytesRead = DiskRead( buffer, (UINT32)bytestoread );
//printf("Asking for %d bytes, read %d bytes\r\n",bytestoread,bytesRead);
    
        if(bytesRead)
        {
            if((bytesRead % BLOCK_SIZE) == 0)
            {
                CurrentReadBlock = CurrentReadBlock + (bytesRead/BLOCK_SIZE);
            }
        }
    }

    return(bytesRead);

}

void sUpdateNode(int block)
{
    int fileIndex;

    fileIndex = CurrentFileNumber -1;
    NodeList[fileIndex] = block;
}

void scloseNodesFile()
{
    UINT64 BlocksWritten = 0;

    if(TotalBytesWritten)
    {
        /*Pad to the end of a block*/
        if(TotalBytesWritten % BLOCK_SIZE)
        {
            BlocksWritten = TotalBytesWritten/BLOCK_SIZE + 1;
        }
        else
        {
            BlocksWritten = TotalBytesWritten/BLOCK_SIZE;
        }
    }
   
    CurrentWriteBlock = CurrentWriteBlock + BlocksWritten;
   
    TotalBytesWritten = 0;

    /*Get the Number of Blocks used and remaining on the Drive*/
    //DiskInfo( &TotalAvailableBlocks );


}

/******************************************************************************/
//                      int sclose(sFILE stream)
//
//  This function is called to close the file. file pointer is ignored 
//  at this point. sClose() function adds closeing information to the 
//  stanag table like, Total Blocks Written, Size of File etc. 
//  Marks the next available write block and writes the stanag to the disk.
//  Before writing the stanag to the disk it swaps the contents of stanag.
//  
//  Return Value:
//      0 = Passed
//
/******************************************************************************/
int sclose(sFILE stream,int increment,int fill)
{
   UINT64 BlocksWritten = 0;
   UINT64 tmp;
   //UINT32 BlocksWritten = 0;
   UINT32 stanag_table_block = 0;
   char   buffer[BLOCK_SIZE];

   memset(buffer, 0x00, sizeof(buffer));
    
   CurrentFileState = 1; /*0 open 1 close*/ 

   stanag_table_block =  (CurrentFileNumber / MAX_NUM_FILES_IN_STANAG_BLOCK);

   tmp = TotalBytesWritten - fill;
   //TotalBytesWritten -= fill;
   TotalBytesWritten = tmp;
//printf("Total recorded bytes %lld with %d fill\r\n",TotalBytesWritten,fill);

#if 0

   FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_Size = TotalBytesWritten ;

   BlocksWritten = (TotalBytesWritten/BLOCK_SIZE) + 1;

   FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_BlockCount = BlocksWritten;

   //Store the close time of file
   GetSystemTimeStr( &CloseTimeString[0] );
   sprintf(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_CloseTime,"%s",CloseTimeString);    

   //FileTable properly shutdown
   FileTable[stanag_table_block].STANAG_ShutDown = 0xFF;
#endif
   
   if(FileTable[stanag_table_block].STANAG_NumberOfFilesEntries < 4)
   {
       memset(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries].FILE_FileName,0xFF,56);    
   }


    BlocksWritten = UpdateStanag(1);
    CurrentFileNumber++; 

    if(increment)
    {
        CurrentFileIndex++;
    }
   CurrentWriteBlock = CurrentWriteBlock + BlocksWritten;
   
   TotalBytesWritten = 0;
   NumberOfBlocksWritten = 0;

   return(0);

}


/******************************************************************************/
//                      int emergency_close(sFILE stream)
//
//  This function is called to close the file. file pointer is ignored 
//  at this point. sClose() function adds closeing information to the 
//  stanag table like, Total Blocks Written, Size of File etc. 
//  Marks the next available write block and writes the stanag to the disk.
//  Before writing the stanag to the disk it swaps the contents of stanag.
//  
//  Return Value:
//      0 = Passed
//
/******************************************************************************/
int Emergency_Close(sFILE stream,int increment)
{
   UINT64 BlocksWritten = 0;
   UINT64 tmp;
   UINT32 stanag_table_block = 0;

    
   CurrentFileState = 1; /*0 open 1 close*/ 

   stanag_table_block =  (CurrentFileNumber / MAX_NUM_FILES_IN_STANAG_BLOCK);
   
   if(FileTable[stanag_table_block].STANAG_NumberOfFilesEntries < 4)
   {
       memset(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries].FILE_FileName,0xFF,56);    
   }

    if(TotalBytesWritten)
    {
        if(TotalBytesWritten % BLOCK_SIZE)
        {
            BlocksWritten = (TotalBytesWritten/BLOCK_SIZE) + 1;
        }
        else
        {
            BlocksWritten = (TotalBytesWritten/BLOCK_SIZE);
        }
    }

    CurrentFileNumber++; 

    if(increment)
    {
        CurrentFileIndex++;
    }

    CurrentWriteBlock = CurrentWriteBlock + BlocksWritten;
   
    TotalBytesWritten = 0;
    NumberOfBlocksWritten = 0;

    return(0);

}


int sGetCurrentFileNumber()
{
    return(CurrentFileIndex);
}


void sResetCurrentFileNumber()
{
    CurrentFileNumber = 0;
    CurrentFileIndex = 0;
}

/******************************************************************************/
//                          sCurrentBlcok()
//  This function will return the current Write Block
//
/******************************************************************************/
int sCurrentBlock(int *block)
{
  *block = CurrentWriteBlock;

  return(0);
}
/******************************************************************************/
//                          serase()
//  This function is called to erase the contents of stanag File table
//  Stanag is initialized and then Stored to the disk
//
/******************************************************************************/
int serase(int mode)
{
    int status = 0;

    if( (LoadedTmatsFromCartridge == 0) || (CH10_TMATS == 1) )
    {
        CurrentWriteBlock   = RESERVED_BLOCKS;
        CurrentReadBlock   = RESERVED_BLOCKS;

        CurrentFileNumber = 0;
        CurrentFileIndex = 0;
        CurrentFileState= 1;

        /*initialize FileTable  */
        if(InitializeStanag() == -1)
        {
           //printf("Error: initializing filetable\n" );
           return( -1 );
        }

        M23_SetVolume();

    
        status = M2x_EraseCartridge(mode);
        swap_Stanag( );
    }
    else
    {
        CurrentFileNumber = 1;
        CurrentFileIndex = 1;
        CurrentFileState= 1;
        InitializeStanagErase();
    }


    return(status);
}


/******************************************************************************/
//
//
/******************************************************************************/
static int FileStoreFileTable()
{
    UINT32 stanag_table_block = 0;
    char  StanagSwappedString[512];
  
    if(!DiskIsConnected())
    {
        return(-1);
    }

    stanag_table_block =  (CurrentFileNumber / MAX_NUM_FILES_IN_STANAG_BLOCK);

    if(stanag_table_block != 0)
    {
        /*Write previous block to get the latest information*/
        DiskWriteSeek((SYSTEM_RESERVE_BLOCKS + (stanag_table_block - 1) ));
   

        if(DiskWrite( (UINT8*)&Stanag_Swaped[stanag_table_block - 1] , 512) != 512 )
        {
            //printf("Error Writing File Table - 1 %d\r\n",(SYSTEM_RESERVE_BLOCKS + (stanag_table_block - 1) ));
            return(-1);
         }

    }

    /*the file table should always sit at block 1 and not 0*/
    DiskWriteSeek((SYSTEM_RESERVE_BLOCKS + stanag_table_block ));


    /*write current STANAG block*/

    if(DiskWrite( (UINT8*)&Stanag_Swaped[stanag_table_block] , 512) != 512 )
    {
        //printf("Error Writing File Table -1\r\n");
        return(-1);
    }

    M2x_FlushCartridge();

#if 0
    /*Adding this to see if we can fix trashing the data*/
    DiskWriteSeek((SYSTEM_RESERVE_BLOCKS + stanag_table_block + 1 ));

    /*Write zeros to the next stanag block*/   
    memset ( StanagSwappedString , 0x0 , 512);
    if(DiskWrite( StanagSwappedString , 512 ) != 512)
    {
        //printf("Error Writing File Table\n");
        return(-1);
    }

#endif

    //DiskWriteSeek(CurrentWriteBlock + TotalBytesWritten);

    return(0);

}

/******************************************************************************/
//
//
/******************************************************************************/
static int FileRetrieveFileTable()
{

   int j;
   int i;
   int k;
   int block;
   int size;
   int numblock = 0;
   int debug;
   int bytes_read;

   int config;

   int setup;
   char tmp[3];

   char buffer[512];

   M23_GetConfig(&config);
   M23_GetDebugValue(&debug);

   DiskReadSeek(SYSTEM_RESERVE_BLOCKS);
   CurrentFileNumber = 0;

   /*the file table should always sit at block 1 and not 0*/     
   for(i = 0; i < MAX_STANAG_BLOCKS;i++)
   {
       bytes_read = DiskRead((UINT8 *)&FileTable[i] ,512);

       if(bytes_read < 1)
       {
           return(-1);
       }
   }

   if(memcmp(FileTable[0].STANAG_MagicNumber,"FORTYtwo",8) != 0)
   {
     //printf("Stanag not loaded...MagicNumber Missing\r\n");
     return(-1);
   }

   //Stanag stored on Disk is swaped so we re swaped the info to be usable
   //we will swap it again before storeing it to the disk
   swap_Stanag();
   memcpy(FileTable,Stanag_Swaped,sizeof( FileTable ));

   //here we load all the information to start 
   //the new recording appending to the old one

   for(j = 0 ; j < MAX_STANAG_BLOCKS ; j ++)
   {
       if(FileTable[j].STANAG_NumberOfFilesEntries == 0)
       {
           if(j==0)
           {
             CurrentWriteBlock = RESERVED_BLOCKS;
           }
           else
           {

             CurrentWriteBlock = FileTable[j-1].STANAG_FileEntries[(CurrentFileNumber - 1) % 4 ].FILE_StartAddress +  FileTable[j-1].STANAG_FileEntries[(CurrentFileNumber - 1) % 4].FILE_BlockCount;

             if(debug)
                 printf("ft[%d] %d  - %d\r\n",j,CurrentFileNumber,CurrentWriteBlock);

#if 0
              //check if the STANAG was shutdown improperly    
              if( FileTable[j-1].STANAG_ShutDown == 0x00 )
              {
                
                
                //writes zeros to the garbage part of last entry
                if(0 > FixLastEntry((UINT32) (CurrentWriteBlock-1) , (UINT32)(FileTable[j-1].STANAG_FileEntries[(CurrentFileNumber - 1) % 4 ].FILE_Size % BLOCK_SIZE) ))
                {
                    //printf("Failed to fix last entry\r\n");
                }
                else
                {
                    //Sets the shutdown flag to 0xFF (proper shutdown)
                    FileTable[j-1].STANAG_ShutDown = 0xFF;
                    
                    //Write the fixed entry back to the STANAG
                    swap_Stanag();
                    if(FileStoreFileTable()==-1)
                    {
                        //printf("Error: Storing filetable\n");
                        return(-1);
                    }
                }
              }
#endif

           }  
           
           break;
       }
       else
       {
           CurrentFileNumber = CurrentFileNumber +  FileTable[j].STANAG_NumberOfFilesEntries;
           CurrentStanagBlock = j;

       }

   }   

   /*We now need to find out If an Update file exists, or fill in Node Information*/
   for(k = 0 ; k < MAX_STANAG_BLOCKS ; k ++)
   {
       for( i=0 ; i < FileTable[k].STANAG_NumberOfFilesEntries ; i++)
       {
           if( (k == 0) && (i == 0) )
           {
               if(strncmp(FileTable[k].STANAG_FileEntries[i].FILE_FileName,"recorder_configuration_file_SAVE",32)  == 0)
               {
                   memset(tmp,0x0,3);
                   if(strlen(FileTable[k].STANAG_FileEntries[i].FILE_FileName) == 34)
                   {
                       sprintf(tmp,"%c",FileTable[k].STANAG_FileEntries[i].FILE_FileName[33]);
                   }
                   else
                   {
                       sprintf(tmp,"%c%c",FileTable[k].STANAG_FileEntries[i].FILE_FileName[33],FileTable[k].STANAG_FileEntries[i].FILE_FileName[34]);
                   }
                   setup = atoi(tmp);
                   if(TmatsLoaded == 0)
                   {
                       M23_LoadTmatsFromRMM(setup,(UINT32)FileTable[k].STANAG_FileEntries[i].FILE_StartAddress,FileTable[k].STANAG_FileEntries[i].FILE_FileName);

                       if(FileTable[0].STANAG_NumberOfFilesEntries == 1)
                       {
                           M2x_EraseCartridge(0);
                           CurrentFileNumber = 1;
                           CurrentFileState= 1;
                           InitializeStanagErase();
                           usleep(100);
                           M23_ReWriteTmatsFile();
                           CurrentFileIndex = 0;
                       }
                   }
               }
               else if(strncmp(FileTable[k].STANAG_FileEntries[i].FILE_FileName,"recorder_firmware_file",22)  == 0)
               {
                   M23_UpgradeFromRMM((int)FileTable[k].STANAG_FileEntries[i].FILE_StartAddress);
                   //Erase();
               }
           }


           CurrentFileIndex++;
       }
   }
   
    if( (config == 0) || (config == P3_CONFIG) )
    {
        for(k = 0 ; k < MAX_STANAG_BLOCKS ; k ++)
        {
            for( i=0 ; i < FileTable[k].STANAG_NumberOfFilesEntries ; i++)
            {
                 block = FileTable[k].STANAG_FileEntries[i].FILE_StartAddress + FileTable[k].STANAG_FileEntries[i].FILE_BlockCount;
                 memset(buffer,0x0,512);
                 DiskReadSeek(block);
                 bytes_read = DiskRead(buffer,512);
                 if(bytes_read > 0)
                 {
                     if(strncmp(buffer,"NODEFILE",8) == 0)
                     {
                         NodeList[i+(k*4)] = block;
                         memcpy(&numblock,(buffer+8),4);
                         if(debug)
                             printf("Found node Entry @ %d with num nodes %d,index %d\r\n",block,numblock,i+(k*4));
                     }
                 }
                 else
                 {
                     return(-1);
                 }
             }
        }

        if(numblock > 0)
        {
             size = (numblock * 16) + 12;
             if( size % 512)
             {
                 size += (512 - (size%512) );
             }
             numblock = size / 512;

            CurrentWriteBlock += numblock;
        }
    }

    if(debug)
        printf("Current Write Block %d\r\n",CurrentWriteBlock);

   DiskReadSeek(RESERVED_BLOCKS);

   return(0);
}

/******************************************************************************/
//                       int M23_GetNodeBlock()
//    Get the block offset for the Node Entries for this filer
//   
/******************************************************************************/
int M23_GetNodeBlock(int FileNum)
{

 return(NodeList[FileNum]);

}


/******************************************************************************/
//                       int M23_GetBlockFromFile()
//    Get the block offset from the filename and offset. If no file of this name
//    exists or the offset is not in the file range, return an error
//   
/******************************************************************************/
int M23_GetBlockFromFile(char *FileName,int offset,int *Block,int *BlockCount)
{
    int i,k;

    for(k = 0 ; k < MAX_STANAG_BLOCKS ; k ++)
    {
        for( i=0 ; i < FileTable[k].STANAG_NumberOfFilesEntries ; i++)
        {
            if(strcasecmp(FileTable[k].STANAG_FileEntries[i].FILE_FileName,FileName)  == 0)
            {
                if( ((UINT32)FileTable[k].STANAG_FileEntries[i].FILE_StartAddress + offset) <= ((UINT32)FileTable[k].STANAG_FileEntries[i].FILE_StartAddress + (UINT32)FileTable[k].STANAG_FileEntries[i].FILE_BlockCount) ) 
                {
                    *Block = (UINT32)FileTable[k].STANAG_FileEntries[i].FILE_StartAddress + offset;
                    *BlockCount = (UINT32)FileTable[k].STANAG_FileEntries[i].FILE_BlockCount;
                    return(0);
                }
            }
        }
    }

    return(1);
}

int M23_GetStartBlock()
{
    return(currentFileBlock);
}
/******************************************************************************/
//                       int M23_GetFileFromTime()
//    Get the File Number from the time Requested
//   
/******************************************************************************/
int M23_GetFileFromTime(INT64 SearchTime)
{
    int    i,k;
    int    file_no = -1;
    int    day;
    int    closeHour;
    int    createHour;
    int    state;
    int    debug;
    char   CloseHour[3];
    char   CreateHour[3];
    char   tmp[20];
    char   tmp2[20];
    INT64  FileTime;
    INT64  FileCreateTime;

    MicroSecTimeTag Time;
    MicroSecTimeTag CreateTime;


    M23_GetDebugValue(&debug);

    for(k = 0 ; k < MAX_STANAG_BLOCKS ; k ++)
    {
        for( i=0 ; i < FileTable[k].STANAG_NumberOfFilesEntries ; i++)
        {
            memset(tmp,0x0,20);
            memset(CloseHour,0x0,3);
            memset(CreateHour,0x0,3);
            day = GetDayFromTime(FileTable[k].STANAG_FileEntries[i].FILE_CreateDate);
          
            sprintf(CreateHour,"%c%c",FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[0]
                                     ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[1]);
            createHour = atoi(CreateHour);

            sprintf(CloseHour,"%c%c",FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[0]
                                    ,FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[1]);
            closeHour = atoi(CloseHour);

            if(closeHour < createHour)
            {
                day++;
            }

            sprintf(tmp,"%03d %c%c:%c%c:%c%c.0%c%c",day,FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[0]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[1]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[2]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[3]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[4]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[5]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[6]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[7]);

            //Time_ASCIIToTimeTag( (char const *)&FileTable[k].STANAG_FileEntries[i].FILE_CloseTime, &Time);
            Time_ASCIIToTimeTag( (char const *)tmp, &Time);

            FileTime = ((INT64)(Time.Upper) << 32);
            FileTime |= (INT64)Time.Lower;

            sprintf(tmp2,"%03d %c%c:%c%c:%c%c.0%c%c",day,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[0]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[1]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[2]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[3]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[4]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[5]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[6]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[7]);



            Time_ASCIIToTimeTag( (char const *)tmp2, &CreateTime);
            FileCreateTime = ((INT64)(CreateTime.Upper) << 32);
            FileCreateTime |= (INT64)CreateTime.Lower;
           

            if((SearchTime < FileTime) && (SearchTime >= FileCreateTime) )
            {
                file_no = i + (k*4);

                if(debug)
                    printf("Search %lld, Close %lld, Create %lld,file %d,CurrentFileIndex %d\r\n",SearchTime,FileTime,FileCreateTime,file_no,CurrentFileIndex);

                M23_RecorderState(&state);
                if( (file_no == CurrentFileIndex) && ( (state == STATUS_RECORD) || (state == STATUS_REC_N_PLAY) ) )
                {
                    file_no = -1;
                }
                return(file_no);
            }
        }
    }

    return(file_no);
}

int M23_IsTimeFromStartOfFile(INT64 SearchTime)
{
    int    i,k;
    int    file_no = -1;
    int    day;
    int    closeHour;
    int    createHour;
    int    state;
    char   CloseHour[3];
    char   CreateHour[3];
    char   tmp[20];
    INT64  FileTime;

    MicroSecTimeTag Time;



    for(k = 0 ; k < MAX_STANAG_BLOCKS ; k ++)
    {
        for( i=0 ; i < FileTable[k].STANAG_NumberOfFilesEntries ; i++)
        {
            memset(tmp,0x0,20);
            memset(CloseHour,0x0,3);
            memset(CreateHour,0x0,3);
            day = GetDayFromTime(FileTable[k].STANAG_FileEntries[i].FILE_CreateDate);
          
            sprintf(CreateHour,"%c%c",FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[0]
                                     ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[1]);
            createHour = atoi(CreateHour);

            sprintf(CloseHour,"%c%c",FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[0]
                                    ,FileTable[k].STANAG_FileEntries[i].FILE_CloseTime[1]);
            closeHour = atoi(CloseHour);

            if(closeHour < createHour)
            {
                day++;
            }

            sprintf(tmp,"%03d %c%c:%c%c:%c%c.0%c%c",day,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[0]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[1]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[2]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[3]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[4]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[5]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[6]
                                                       ,FileTable[k].STANAG_FileEntries[i].FILE_CreateTime[7]);

            //Time_ASCIIToTimeTag( (char const *)&FileTable[k].STANAG_FileEntries[i].FILE_CloseTime, &Time);
            Time_ASCIIToTimeTag( (char const *)tmp, &Time);

            FileTime = ((INT64)(Time.Upper) << 32);
            FileTime |= (INT64)Time.Lower;

            if(SearchTime == FileTime) //The Time is at the start of the File
            {
                currentFileBlock = (UINT32)FileTable[k].STANAG_FileEntries[i].FILE_StartAddress; 
                return(-2);
            }
        }
    }

    return(0);
}



/******************************************************************************/
//                       int M23_GetBlockFromFile()
//    Get the block offset from the filename and offset. If no file of this name
//    exists or the offset is not in the file range, return an error
//   
/******************************************************************************/
int M23_GetBlockFromFileNumber(int offset,int *Block)
{
    int i,k;

    for(k = 0 ; k < MAX_STANAG_BLOCKS ; k ++)
    {
        for( i=0 ; i < FileTable[k].STANAG_NumberOfFilesEntries ; i++)
        {
            if( offset == ( (i + (k*4)) + 1) )
            {
                *Block = (UINT32)FileTable[k].STANAG_FileEntries[i].FILE_StartAddress;
                return(0);
            }
        }
    }

    return(-1);
}
/******************************************************************************/
//                       int M23_GetEndBlock(int offset))
//    Get the End block offset from the offset. 
//   
/******************************************************************************/
int M23_GetEndBlock(int offset)
{
    int i,k;
    int block = 0;

    block = (int)CurrentWriteBlock;
    return(block);
#if 0
    for(k = 0 ; k < MAX_STANAG_BLOCKS ; k ++)
    {
        for( i=0 ; i < FileTable[k].STANAG_NumberOfFilesEntries ; i++)
        {
            block =  (UINT32)(FileTable[k].STANAG_FileEntries[i].FILE_StartAddress + FileTable[k].STANAG_FileEntries[i].FILE_BlockCount);
#if 0
            if(offset >= (UINT32)FileTable[k].STANAG_FileEntries[i].FILE_StartAddress)
            {
                block = FileTable[k].STANAG_FileEntries[i].FILE_BlockCount;
            }
#endif
        }
    }

    return(block);
#endif
}


/******************************************************************************/
//                       int M23_FindBlockinFile()
//  This function will find the request block in the file
//   
/******************************************************************************/
int M23_FindFirstInLastFile()
{
    int i;
    int j;
    int bof = 129;

    for(j = 0 ; j < MAX_STANAG_BLOCKS ; j ++)
    {

        if(FileTable[j].STANAG_NumberOfFilesEntries  == 0)
        {
            j = MAX_STANAG_BLOCKS;
        }
        else
        {

            for( i=0 ; i < FileTable[j].STANAG_NumberOfFilesEntries ; i++);

            bof = (UINT32)FileTable[j].STANAG_FileEntries[i -1].FILE_StartAddress;
        }
    }

    return(bof);

}


/******************************************************************************/
//                       int sprint()
//  This function prints the file table to the screen. 
//  it needs to return a string to the calling function that holds the File Table
//   
/******************************************************************************/
int sprint()
{

int i = 0;
int j = 0;

    /*print filt table*/
    printf("File_Name   Start_Block  Block_Count   Size_in_Bytes  create_Date  Creae_Time  Close_Time \n");
    printf("==========================================================================================\n");

    for(j = 0 ; j < MAX_STANAG_BLOCKS ; j ++)
    {

      for( i=0 ; i < FileTable[j].STANAG_NumberOfFilesEntries ; i++)
      {

           printf("%s   %10u  %10u    %11llu        %c%c/%c%c/%c%c%c%c  %c%c:%c%c:%c%c.%c%c  %c%c:%c%c:%c%c.%c%c\n"
  								        ,FileTable[j].STANAG_FileEntries[i].FILE_FileName
								        ,(UINT32)FileTable[j].STANAG_FileEntries[i].FILE_StartAddress
								        ,(UINT32)FileTable[j].STANAG_FileEntries[i].FILE_BlockCount
								        ,FileTable[j].STANAG_FileEntries[i].FILE_Size
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateDate[0]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateDate[1]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateDate[2]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateDate[3]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateDate[4]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateDate[5]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateDate[6]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateDate[7]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[0]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[1]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[2]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[3]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[4]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[5]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[6]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[7]
                                        ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[0]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[1]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[2]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[3]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[4]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[5]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[6]
								        ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[7]);



      }
    }
    return(0);
}
/******************************************************************************/
//    STANAG in memory needs to be swaped before we write it to the disk
//    this function performs the swap in memory and sores the swaped version
//    to structure called Stanag_Swaped
/******************************************************************************/
int swap_Stanag()
{

int i = 0;
int j = 0;

    memset(Stanag_Swaped,0,sizeof(Stanag_Swaped));

    for( i = 0 ; i < MAX_STANAG_BLOCKS ; i++ )
    {

        memcpy(Stanag_Swaped[i].STANAG_MagicNumber,FileTable[i].STANAG_MagicNumber,sizeof(FileTable[i].STANAG_MagicNumber));

        Stanag_Swaped[i].STANAG_RevisionNumber=FileTable[i].STANAG_RevisionNumber;

        Stanag_Swaped[i].STANAG_ShutDown=FileTable[i].STANAG_ShutDown;
        Stanag_Swaped[i].STANAG_NumberOfFilesEntries=FileTable[i].STANAG_NumberOfFilesEntries;
        SWAP_TWO(Stanag_Swaped[i].STANAG_NumberOfFilesEntries);

        Stanag_Swaped[i].STANAG_CurrBlock=FileTable[i].STANAG_CurrBlock;
        SWAP_FOUR(Stanag_Swaped[i].STANAG_CurrBlock);

        memcpy(Stanag_Swaped[i].STANAG_VolumeName,FileTable[i].STANAG_VolumeName,sizeof(Stanag_Swaped[i].STANAG_VolumeName));
        
        Stanag_Swaped[i].STANAG_ForwardLink=FileTable[i].STANAG_ForwardLink;
        SWAP_EIGHT(Stanag_Swaped[i].STANAG_ForwardLink);

        Stanag_Swaped[i].STANAG_ReverseLink=FileTable[i].STANAG_ReverseLink;
        SWAP_EIGHT(Stanag_Swaped[i].STANAG_ReverseLink);


        for(j=0; j <  4; j++)
        {
        
           strncpy(Stanag_Swaped[i].STANAG_FileEntries[j].FILE_FileName,FileTable[i].STANAG_FileEntries[j].FILE_FileName,56); 

            
            Stanag_Swaped[i].STANAG_FileEntries[j].FILE_StartAddress= FileTable[i].STANAG_FileEntries[j].FILE_StartAddress;
            SWAP_EIGHT(Stanag_Swaped[i].STANAG_FileEntries[j].FILE_StartAddress);

            Stanag_Swaped[i].STANAG_FileEntries[j].FILE_BlockCount=FileTable[i].STANAG_FileEntries[j].FILE_BlockCount;
            SWAP_EIGHT(Stanag_Swaped[i].STANAG_FileEntries[j].FILE_BlockCount);

            Stanag_Swaped[i].STANAG_FileEntries[j].FILE_Size=FileTable[i].STANAG_FileEntries[j].FILE_Size;
            SWAP_EIGHT(Stanag_Swaped[i].STANAG_FileEntries[j].FILE_Size);

            strncpy(Stanag_Swaped[i].STANAG_FileEntries[j].FILE_CreateDate,FileTable[i].STANAG_FileEntries[j].FILE_CreateDate,8);//sizeof(FileTable[i].STANAG_FileEntries[j].FILE_CreateDate));

            strncpy(Stanag_Swaped[i].STANAG_FileEntries[j].FILE_CreateTime,FileTable[i].STANAG_FileEntries[j].FILE_CreateTime,8);//sizeof(FileTable[i].STANAG_FileEntries[j].FILE_CreateTime));
            
            Stanag_Swaped[i].STANAG_FileEntries[j].FILE_TimeType=FileTable[i].STANAG_FileEntries[j].FILE_TimeType;
    
            strncpy(Stanag_Swaped[i].STANAG_FileEntries[j].FILE_Reserved,FileTable[i].STANAG_FileEntries[j].FILE_Reserved,7); //sizeof(FileTable[i].STANAG_FileEntries[j].FILE_Reserved));
            
            strncpy(Stanag_Swaped[i].STANAG_FileEntries[j].FILE_CloseTime,FileTable[i].STANAG_FileEntries[j].FILE_CloseTime,8);//sizeof(FileTable[i].STANAG_FileEntries[j].FILE_CloseTime));
        }

    }

    return(0);
}

/****************************************************************************************/

/****************************************************************************************/
int GetSystemDateStr( char *Date )
{
    GSBTime gsb_time;
    int Month = 0;
    int Day = 0;
    int year;

    //struct tm *gmt;

    if ( Date == NULL )
        return -1;

    TimeView(&gsb_time);

    M23_GetYear(&year);

    //gettimeofday( &time, NULL );

    // convert to straight microseconds since the start of the current year
    //
    //gmt = gmtime( &time.tv_sec );
    //gsb_time.Days = gmt->tm_yday ;              // IRIG defines Jan 1 as day 1, not day 0

    //if((gmt->tm_year + 1900) % 4 == 0)
    if((year% 4) == 0)
    {
       //Feb = 29 Days -> Leap year

       if(gsb_time.Days <= 31)
       {
            Month = 1;
            Day = gsb_time.Days;
       }
       else if(gsb_time.Days > 31 && gsb_time.Days <= 60)
       {
            Month = 2;
            Day = gsb_time.Days - 31;
       }
       else if(gsb_time.Days > 60 && gsb_time.Days <= 91)
       {
            Month = 3;
            Day = gsb_time.Days - 60;
       }
       else if(gsb_time.Days > 91 && gsb_time.Days <= 121)
       {
            Month = 4;
            Day = gsb_time.Days - 91;
       }
       else if(gsb_time.Days > 121 && gsb_time.Days <= 152)
       {
            Month = 5;
            Day = gsb_time.Days - 121;
       }
       else if(gsb_time.Days > 152 && gsb_time.Days <= 182)
       {
            Month = 6;
            Day = gsb_time.Days - 152;
       }
       else if(gsb_time.Days > 182 && gsb_time.Days <= 213)
       {
            Month = 7;
            Day = gsb_time.Days - 182;
       }
       else if(gsb_time.Days > 213 && gsb_time.Days <= 244)
       {
            Month = 8;
            Day = gsb_time.Days - 213;
       }
       else if(gsb_time.Days > 244 && gsb_time.Days <= 274)
       {
            Month = 9;
            Day = gsb_time.Days - 244;
       }
       else if(gsb_time.Days > 274 && gsb_time.Days <= 305)
       {
            Month = 10;
            Day = gsb_time.Days - 274;
       }
       else if(gsb_time.Days > 305 && gsb_time.Days <= 335)
       {
            Month = 11;
            Day = gsb_time.Days - 305;
       }
       else if(gsb_time.Days > 335 && gsb_time.Days <= 366)
       {
            Month = 12;
            Day = gsb_time.Days - 335;
       }

   }
   else // {31,28,31,30,31,30,31,31,30,31,30,31};
   {
       //Feb = 28 Days -> Not Leap Year

       if(gsb_time.Days <= 31)
       {
            Month = 1;
            Day = gsb_time.Days;
       }
       else if( gsb_time.Days > 31 && gsb_time.Days <= 59 )
       {
            Month = 2;
            Day = gsb_time.Days - 31;
       }
       else if(gsb_time.Days > 59 && gsb_time.Days <= 90)
       {
            Month = 3;
            Day = gsb_time.Days - 59;
       }
       else if(gsb_time.Days > 90 && gsb_time.Days <= 120)
       {
            Month = 4;
            Day = gsb_time.Days - 90;
       }
       else if(gsb_time.Days > 120 && gsb_time.Days <= 151)
       {
            Month = 5;
            Day = gsb_time.Days - 120;
       }
       else if(gsb_time.Days > 151 && gsb_time.Days <= 181)
       {
            Month = 6;
            Day = gsb_time.Days - 151;
       }
       else if(gsb_time.Days > 181 && gsb_time.Days <= 212)
       {
            Month = 7;
            Day = gsb_time.Days - 181;
       }
       else if(gsb_time.Days > 212 && gsb_time.Days <= 243)
       {
            Month = 8;
            Day = gsb_time.Days - 212;
       }
       else if(gsb_time.Days > 243 && gsb_time.Days <= 273)
       {
            Month = 9;
            Day = gsb_time.Days - 243;
       }
       else if(gsb_time.Days > 273 && gsb_time.Days <= 304)
       {
            Month = 10;
            Day = gsb_time.Days - 273;
       }
       else if(gsb_time.Days > 304 && gsb_time.Days <= 334)
       {
            Month = 11;
            Day = gsb_time.Days - 304;
       }
       else if(gsb_time.Days > 334 && gsb_time.Days <= 365)
       {
            Month = 12;
            Day = gsb_time.Days - 334;
       }

   }



    //sprintf(Date,"%02d%02d%04d", Day , Month , (gmt->tm_year + 1900));  // this is offset by 1900?? 1970.
    sprintf(Date,"%02d%02d%04d", Day , Month ,year); 

   
   return 0;
}

int GetDayFromTime(char *str)
{
    int Month;
    int Day;
    int Year;
    char tmp[6];

    memset(tmp,0x0,6);
    sprintf(tmp,"%c%c",str[0],str[1]);
    Day = atoi(tmp);

    memset(tmp,0x0,6);
    sprintf(tmp,"%c%c",str[2],str[3]);
    Month = atoi(tmp);

    memset(tmp,0x0,6);
    sprintf(tmp,"%c%c%c%c",str[4],str[5],str[6],str[7]);
    Year = atoi(tmp);

    //if((Year + 1900) % 4 == 0)
    if( (Year %4) == 0 )
    {
       //Feb = 29 Days -> Leap year

       if(Month == 1)
       {
            Day = Day;
       }
       else if(Month == 2)
       {
            Day = Day + 31;
       }
       else if(Month == 3)
       {
            Day = Day + 60;
       }
       else if(Month == 4)
       {
            Day = Day + 91;
       }
       else if(Month == 5)
       {
            Day = Day + 121;
       }
       else if(Month == 6)
       {
            Day = Day + 152;
       }
       else if(Month == 7)
       {
            Day = Day + 182;
       }
       else if(Month == 8)
       {
            Day = Day + 213;
       }
       else if(Month == 9)
       {
            Day = Day + 244;
       }
       else if(Month == 10)
       {
            Day = Day + 274;
       }
       else if(Month == 11)
       {
            Day = Day + 305;
       }
       else if(Month == 12)
       {
            Day = Day + 335;
       }

   }
   else // {31,28,31,30,31,30,31,31,30,31,30,31};
   {
       //Feb = 28 Days -> Not Leap Year

       if(Month == 1)
       {
            Day = Day;
       }
       else if( Month == 2)
       {
            Day = Day + 31;
       }
       else if(Month == 3)
       {
            Day = Day + 59;
       }
       else if(Month == 4)
       {
            Day = Day + 90;
       }
       else if(Month == 5)
       {
            Day = Day + 120;
       }
       else if(Month == 6)
       {
            Day = Day + 151;
       }
       else if(Month == 7)
       {
            Day = Day + 181;
       }
       else if(Month == 8)
       {
            Day = Day + 212;
       }
       else if(Month == 9)
       {
            Day = Day + 243;
       }
       else if(Month == 10)
       {
            Day = Day + 273;
       }
       else if(Month == 11)
       {
            Day = Day + 304;
       }
       else if(Month == 12)
       {
            Day = Day + 334;
       }

    }
  
    return(Day);
}
/****************************************************************************************/

/****************************************************************************************/
int GetSystemTimeStr( char *Time )
{
    GSBTime gsb_time;

    if ( Time == NULL )
        return -1;

    TimeView(&gsb_time);

   sprintf(Time,"%02d%02d%02d%02d",(UINT32)gsb_time.Hours,(UINT32)gsb_time.Minutes,(UINT32)gsb_time.Seconds,(UINT32)gsb_time.Microseconds>>16);
   
   return 0;

}
/****************************************************************************************/
/****************************************************************************************/
int UpdateStanag(int mode)
{
   UINT64 BlocksWritten=0;
   UINT32 stanag_table_block=0;
   char   CloseTimeString[8];
    
   UINT32 fill;
   UINT32 blocks;
   UINT64 Blocks;

   stanag_table_block =  (CurrentFileNumber / MAX_NUM_FILES_IN_STANAG_BLOCK);

   if(mode == 1) //We are Closing the File
   {
       if((TotalBytesWritten % 32768) == 0) //We may have a proble,check Values again
       {
           fill   = M23_ReadControllerCSR(CONTROLLER_BTM_FILL_CSR);
	   blocks = M23_ReadControllerCSR(IEEE1394B_RECORD_BLOCKS_RECORDED);
           Blocks = blocks;
	   TotalBytesWritten = (UINT64)(Blocks *512);
	   TotalBytesWritten = TotalBytesWritten - (UINT64)fill;
       }
   }

   FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_Size = TotalBytesWritten ;

   if(TotalBytesWritten)
   {
       if(TotalBytesWritten % BLOCK_SIZE)
       {
           BlocksWritten = (TotalBytesWritten/BLOCK_SIZE) + 1;
       }
       else
       {
           BlocksWritten = (TotalBytesWritten/BLOCK_SIZE);
       }
   }

   FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_BlockCount = BlocksWritten;

//printf("Total Bytes Written %lld,Blocks %d\r\n",TotalBytesWritten,BlocksWritten);
   //Store The close time of file
   GetSystemTimeStr( &CloseTimeString[0] );
   sprintf(FileTable[stanag_table_block].STANAG_FileEntries[FileTable[stanag_table_block].STANAG_NumberOfFilesEntries-1].FILE_CloseTime,"%s",CloseTimeString);    

   FileTable[stanag_table_block].STANAG_ShutDown = 0xFF;//Initialize volume to proper shutdown


   swap_Stanag();

   if(FileStoreFileTable()==-1)
   {
     //printf("Error: Storing filetable\r\n");
     return(-1);
   }

return(BlocksWritten);

}

/****************************************************************************************/
/****************************************************************************************/
int UpdateBlocksWritten(UINT32 BlocksWritten)
{

    NumberOfBlocksWritten = BlocksWritten;

    TotalBytesWritten = (UINT64) (NumberOfBlocksWritten * 512);

    return(0);

}

/****************************************************************************************/
/****************************************************************************************/
int sdisk(UINT32 *TotalDiskBlocks, UINT32 *BlocksUsed)
{

    //*BlocksUsed = (CurrentWriteBlock - RESERVED_BLOCKS) + (NumberOfBlocksWritten) ;
    *BlocksUsed = (UINT32)(CurrentWriteBlock + NumberOfBlocksWritten) ;
    if( (TotalAvailableBlocks - RESERVED_BLOCKS) < 0 )
    {
        *TotalDiskBlocks = 0;
    }
    else
    {
        *TotalDiskBlocks = (TotalAvailableBlocks - RESERVED_BLOCKS);
    }

    return(0);

}

/****************************************************************************************/
//    This function takes LastBlock offset of the file that is closed improperly
//    and also takes the valid number of bytes written to the last block
//    it then over writes the garbage with filler pattern (0x00)
/****************************************************************************************/
int FixLastEntry(UINT32 LastBlock , UINT32 BytesWritten)
{

    char filler_pattern[BLOCK_SIZE];

    memset(filler_pattern,0x00,sizeof(filler_pattern));

    DiskWriteSeek( LastBlock + BytesWritten);

    if( (BLOCK_SIZE-BytesWritten) != DiskWrite(filler_pattern, (BLOCK_SIZE-BytesWritten)) )
    {
        return(-1);
    }

    return(0);
}

/****************************************************************************************/
//    This function takes two parameters one is the pointer to the filetable struct and
//    other a pointer to and intiger to holde the file count. it returns the file table 
//    and filecount to the calling function
/****************************************************************************************/

const STANAG_DIR_BLOCK_HOLDER *sGetFileTable(int* totalFileCount)
{

    *totalFileCount = CurrentFileNumber;

    return(&FileTable[0]);
}

void STANAG_GetNumberOfFiles(int *totalFileCount)
{
    *totalFileCount = CurrentFileNumber;
}


void sGetReadLocation(UINT32 *blocklocation)
{
    *blocklocation = (UINT32)CurrentReadBlock;
}

void sGetPlayLocation(UINT32 *blocklocation)
{
    *blocklocation = (UINT32)CurrentPlayBlock;
}

void sSetPlayLocation(UINT32 blocklocation)
{
    CurrentPlayBlock = (UINT64)blocklocation ;
}


/******************************************************************************/
//                       int sPrintFiles()
//  This function prints the file table to the screen. 
//  it needs to return a string to the calling function that holds the File Table
//   
/******************************************************************************/
int sPrintFiles()
{

    int  i = 0;
    int  j = 0;
    int  day;
    int  closeday;
    int  closeHour;
    int  createHour;
    int  count = 0;
    char tmp[160];
    char CloseHour[3];
    char CreateHour[3];

    /*print file table*/
    for(j = 0 ; j < MAX_STANAG_BLOCKS ; j ++)
    {

      if(FileTable[j].STANAG_NumberOfFilesEntries == 0)
      {
          break;
      }

        for( i=0 ; i < FileTable[j].STANAG_NumberOfFilesEntries ; i++)
        {
            memset(tmp,0x0,160);
            memset(CloseHour,0x0,3);
            memset(CreateHour,0x0,3);

            day = GetDayFromTime(FileTable[j].STANAG_FileEntries[i].FILE_CreateDate);
          
            sprintf(CreateHour,"%c%c",FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[0]
                                     ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[1]);
            createHour = atoi(CreateHour);

            sprintf(CloseHour,"%c%c",FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[0]
                                    ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[1]);
            closeHour = atoi(CloseHour);

            closeday = day;
            if(closeHour < createHour)
            {
                closeday++;
            }
            

            count++;
            sprintf(tmp,"%4d %-35s %11d  %12lld  %03d-%c%c:%c%c:%c%c.0%c%c  %03d-%c%c:%c%c:%c%c.0%c%c"
                    ,count
	            ,FileTable[j].STANAG_FileEntries[i].FILE_FileName
	            ,(UINT32)FileTable[j].STANAG_FileEntries[i].FILE_StartAddress
	            ,FileTable[j].STANAG_FileEntries[i].FILE_Size
                    ,day
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[0]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[1]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[2]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[3]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[4]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[5]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[6]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CreateTime[7]
                    ,closeday
                    ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[0]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[1]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[2]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[3]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[4]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[5]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[6]
	            ,FileTable[j].STANAG_FileEntries[i].FILE_CloseTime[7]);

           PutResponse(0,tmp);
           PutResponse(0,"\r\n");

      }
    }
    return(0);
}

int GetNumberOfFiles()
{
    int i;
    int count = 0;

    for(i = 0 ; i < MAX_STANAG_BLOCKS ; i++)
    {
        if(FileTable[i].STANAG_NumberOfFilesEntries == 0)
        {
            break;
        }
        else
        {
            count += FileTable[i].STANAG_NumberOfFilesEntries;
        }
    }

    return(count);
}

int GetFileEntry(STANAG_FILE_ENTRY *entry,int fileNum)
{
    int i;
    int j;
    int count = 1;
    int status = 0;

    for(j = 0 ; j < MAX_STANAG_BLOCKS ; j ++)
    {

        if(FileTable[j].STANAG_NumberOfFilesEntries == 0)
        {
            break;
        }

        for( i=0 ; i < FileTable[j].STANAG_NumberOfFilesEntries ; i++)
        {
            if(fileNum == count)
            {
                memcpy(entry ,&FileTable[j].STANAG_FileEntries[i],sizeof(STANAG_FILE_ENTRY));
                i = MAX_STANAG_BLOCKS;
                j = MAX_STANAG_BLOCKS;
                status = 1;
            }
            count++;
        }
    }

    return(status);
}

