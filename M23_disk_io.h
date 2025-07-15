#include "stdio.h"
#include "stdlib.h"
#include "memory.h" 
#include "string.h" 
#include "M23_Typedefs.h"


int InitializeDevice(void);

int DiskConnect( void );
int DiskIsConnected(void);
int DiskDisconnect( void );
int DiskPath( char *recoedDiskPath );


int DiskInfo(UINT32 *TotalAvailableBlocks);

int DiskSeek( UINT64 start_offset );

int DiskWrite( UINT8 *p_data, UINT32 byte_count );
int DiskRead( UINT8 *p_data, UINT32 byte_count );

