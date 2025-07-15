#include "M23_Typedefs.h"

#define sFILE int 

#define MAX_NUM_FILES 512 /*The number of file entries FileTable can hold*/
#define MAX_NUM_FILES_IN_STANAG_BLOCK 4/*The number of file entries STANAG BLOCK can hold*/
#define MAX_STANAG_BLOCKS (MAX_NUM_FILES/MAX_NUM_FILES_IN_STANAG_BLOCK)
#define SYSTEM_RESERVE_BLOCKS 1 
#define STANAG_RESERVEDBLOCKS  MAX_STANAG_BLOCKS
#define RESERVED_BLOCKS ( SYSTEM_RESERVE_BLOCKS + STANAG_RESERVEDBLOCKS ) /*total MBs reserved b4 the data gets written*/
//#define STANAG_REV_NUM	1 
#define STANAG_REV_NUM	0xF  //Support for 106-7
#define BLOCK_SIZE 512

typedef struct 
{
    char   FILE_FileName[56];
    UINT64 FILE_StartAddress;
    UINT64 FILE_BlockCount;
    UINT64 FILE_Size;
    char   FILE_CreateDate[8];
    char   FILE_CreateTime[8];
    UINT8  FILE_TimeType;
    char   FILE_Reserved[7];
    char   FILE_CloseTime[8];
}STANAG_FILE_ENTRY;

typedef struct 
{
    char   STANAG_MagicNumber[8];
    UINT8  STANAG_RevisionNumber;
    UINT8  STANAG_ShutDown;
    UINT16 STANAG_NumberOfFilesEntries;
    UINT32 STANAG_CurrBlock;
    char   STANAG_VolumeName[32];
    UINT64 STANAG_ForwardLink;
    UINT64 STANAG_ReverseLink;
    STANAG_FILE_ENTRY STANAG_FileEntries[4];
}STANAG_DIR_BLOCK_HOLDER;


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
int sinit();

/******************************************************************************/
//                       int sprint()
//  This function prints the file table to the screen. later we can have
//  it return a string that holds the File Table to the calling function. 
//   
/******************************************************************************/
int sprint();

/******************************************************************************/
//                          serase()
//  This function is called to erase the contents of stanag File table
//  Stanag is initialized and then Stored to the disk
//
/******************************************************************************/
int serase(int mode);

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
sFILE sopen( const char *filename, const char *mode );

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
size_t swrite( char *buffer, size_t size, size_t count, sFILE stream );

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
size_t sread( char *buffer, size_t size, size_t count, sFILE stream );

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
int sclose( sFILE stream ,int increment,int fill);

/******************************************************************************/
//                      int sdisk();
//
//  
//  Return Value:
//      0 = Passed
//
/******************************************************************************/
int sdisk(UINT32 *TotalDiskBlocks, UINT32 *BlocksUsed);

/******************************************************************************/
//                const STANAG_DIR_BLOCK_HOLDER *sGetFileTable(int *totalFileCount);
//
//  This function returns the total number of file entries and the stanag File table  
//
//  Return Value:
//      0 = Passed
//
/******************************************************************************/
const STANAG_DIR_BLOCK_HOLDER *sGetFileTable(int* totalFileCount);


/******************************************************************************/
//                      int sseek(UINT64 seekOffset);
//
//  This function sets the pointer to any offset given. sinit() must be called prior
//  to issueing sseek      
//  
//  Return Value:
//      0 = Passed
//     -1 = fail 
//
/******************************************************************************/
int sseek(UINT64 seekOffset);

