#ifndef _SQUEUE_H_
#define _SQUEUE_H_

#include "M23_Typedefs.h"
//#include "ip_comm.h"
//#include "ip_msg.h"


#ifdef __cplusplus
extern "C" {
#endif




//
//--- Defines ------------------------------------------------------------------
//
#define NUM_OVHD_BYTES (6)
#define NUM_NATIVE_OVHD_BYTES (11)
#define STX     ((UINT8) 0x02)
#define ETX     ((UINT8) 0x03)

/*#define MAX_BYTES_IN_QUEUE  (16*1024)*/
#define MAX_BYTES_IN_QUEUE  (16)


//
//--- Typedefs -----------------------------------------------------------------
//
typedef enum
{ MSG_STX,
  MSG_DEST, MSG_SRC,
  MSG_MBUS, MSG_PORT,
  MSG_CMDH, MSG_CMDL,
  MSG_CNTH, MSG_CNTL,
  MSG_HDR_CHKSUM,
  MSG_DATA,
  MSG_CHKSUM,
  MSG_ETX
} MESSAGE_STATES;

typedef struct __SERIAL_QUEUE_INFO_tag
{
  UINT8 *buffer;
  UINT8 *buffer_limit;
  UINT8 *in;
  UINT8 *out;
  UINT32 size;
  UINT32 count;

  MESSAGE_STATES msg_state;
  UINT8 *msg_SOM_in;
  UINT32 msg_data_size;
  UINT16 msg_SOM_count;
  UINT16 msg_count;
  UINT16 msg_bytes_remaining;
  UINT8  msg_chksum;
  UINT8  msg_flush;

  UINT32 total_byte_count;
  UINT32 total_packet_count;
  UINT8  msg_header[NUM_NATIVE_OVHD_BYTES + 1];

} SERIAL_QUEUE_INFO;


typedef struct {
    UINT8   Dest;
    UINT8   Src;
    UINT8   MBusAddr;
    UINT8   Port;
    UINT16  Command;
    UINT16  Count;
    UINT8   HeaderChksum;
//!!!! WARNING  !!!!  WARNING !!!!  
//---  Do NOT use the sizeof operator on this
//--- structure to obtained its length.  You will
//--- not be happy.
//---  Use the following 'define' when you need 
//--- the length of the structure
//---  Of course, you will need to change this 'define'
//--- whenever you add or delete members in the structure.
} IP_NATIVE_MSG_HEADER;

#define IP_NATIVE_HEADER_SIZE  9


typedef UINT16   IP_MESSAGE_CODE;  // Interprocess command/response code (two bytes)
typedef UINT8    IP_MESSAGE_DATA;   // Interprocess data (zero or more bytes)




//
//--- Functions ----------------------------------------------------------------
//
void SQ_Init(SERIAL_QUEUE_INFO *sq, UINT8 *buffer, UINT32 bufsize);

BOOL SQ_SpaceAvailable(SERIAL_QUEUE_INFO *sq, UINT16 count);
BOOL SQ_AddByte(SERIAL_QUEUE_INFO *sq, UINT8 byte);
BOOL SQ_AddBytes(SERIAL_QUEUE_INFO *sq, UINT8 *buffer, UINT16 count);
BOOL SQ_GetBytes(SERIAL_QUEUE_INFO *sq, UINT8 *buffer, UINT16 count);

#define SQ_GetByteCount(sq)     ((sq)->count)

BOOL SQ_PeekBytes(SERIAL_QUEUE_INFO *sq, UINT8 *buffer
                 ,UINT16 count, UINT16 offset);

BOOL SQ_Snd_Msg(SERIAL_QUEUE_INFO *sq, UINT16 Cmd, UINT16 Count, UINT8 *data);
BOOL SQ_Rcv_Msg(SERIAL_QUEUE_INFO *sq, UINT16 *Cmd, UINT16 *Count, UINT8 *data);
BOOL SQ_Peek_Msg(SERIAL_QUEUE_INFO *sq, UINT16 *cmd, UINT16 *count);

BOOL SQ_Add_Msg_Byte(SERIAL_QUEUE_INFO *sq, UINT8 byte);

 
BOOL SQ_Snd_Native_Msg(SERIAL_QUEUE_INFO *sq
                      ,UINT8 MBusDest
                      ,IP_NATIVE_MSG_HEADER *p_header
                      ,IP_MESSAGE_DATA *data);
                      
BOOL SQ_Add_Native_Msg_Byte(SERIAL_QUEUE_INFO *sq, UINT8 byte);
BOOL SQ_Native_Msg_Rcv_Protocol(SERIAL_QUEUE_INFO *sq, UINT8 byte);

UINT16 SQ_Get_Full_Snd_Native_Msg(SERIAL_QUEUE_INFO *sq, UINT8 *data);

BOOL SQ_Peek_Native_Msg(SERIAL_QUEUE_INFO *sq, IP_NATIVE_MSG_HEADER *header);
BOOL SQ_Rcv_Native_Msg(SERIAL_QUEUE_INFO *sq, IP_NATIVE_MSG_HEADER *header, UINT8 *data);

#ifdef __cplusplus
}
#endif

#endif
