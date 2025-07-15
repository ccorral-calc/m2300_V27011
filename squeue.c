

#include "squeue.h"



void SQ_Init(SERIAL_QUEUE_INFO *sq, UINT8 *buffer, UINT32 bufsize)
//******************************************************************************
{
  sq->buffer        = buffer;
  sq->size          = bufsize;
  sq->buffer_limit  = buffer + bufsize;
  sq->in            = buffer;
  sq->out           = buffer;
  sq->count         = 0;
  sq->msg_state     = MSG_STX;
  sq->msg_SOM_in    = buffer;
  sq->msg_data_size = 0;
  sq->msg_count     = 0;
  sq->msg_bytes_remaining = 0;
  sq->total_byte_count = 0;
  sq->total_packet_count = 0;
  sq->msg_header[0] = STX;
} // SQ_Init

BOOL SQ_AddByte(SERIAL_QUEUE_INFO *sq, UINT8 byte)
//******************************************************************************
{
    if (sq->size == sq->count) return FALSE;
    sq->count++;
    *sq->in++ = byte;
    sq->total_byte_count++;
    if (sq->in == sq->buffer_limit) sq->in = sq->buffer;
    return TRUE;
}


BOOL SQ_SpaceAvailable(SERIAL_QUEUE_INFO *sq, UINT16 count)
//******************************************************************************
{
    if ((UINT32) count > (sq->size - sq->count))
      return FALSE;
    else
      return TRUE;
}


BOOL SQ_AddBytes(SERIAL_QUEUE_INFO *sq, UINT8 *buffer, UINT16 count)
//******************************************************************************
{
    if ((UINT32) count > (sq->size - sq->count)) return FALSE;
    sq->count += count;
    sq->total_byte_count += count;
    while(count--)
    {
        *sq->in++ = *buffer++;
        if (sq->in == sq->buffer_limit) sq->in = sq->buffer;
    }
    return TRUE;
}

BOOL SQ_GetBytes(SERIAL_QUEUE_INFO *sq, UINT8 *buffer, UINT16 count)
//******************************************************************************
{
    if ((UINT32) count > sq->count) return FALSE;
    sq->count -= count;
    while(count--)
    {
        *buffer++ = *sq->out++;
        if (sq->out == sq->buffer_limit) sq->out = sq->buffer;
    }
    return TRUE;
}

BOOL SQ_PeekBytes(SERIAL_QUEUE_INFO *sq, UINT8 *buffer, UINT16 count, UINT16 offset)
//******************************************************************************
{
    UINT8 *p_out;
    if ((UINT32) count > sq->count) return FALSE;

    p_out = sq->out;

        //---  This allows the transmission of a message without
        //--- removing the message from the queue.
        //---  Why do this?  In order to retransmit the message should
        //--- the M-Bus lose arbitration.
    while (offset--) {
      p_out++;
      if (p_out == sq->buffer_limit) p_out = sq->buffer;
    }

    while (count--)
    {
        *buffer++ = *p_out++;
        if (p_out == sq->buffer_limit) p_out = sq->buffer;
    }
    return TRUE;
}


#if 0

BOOL SQ_Snd_Msg(SERIAL_QUEUE_INFO *sq, UINT16 Cmd, UINT16 count, UINT8 *data)
//******************************************************************************
{
    if (((UINT32) count + NUM_OVHD_BYTES) > (sq->size - sq->count)) return FALSE;
    SQ_AddByte(sq, STX);
    SQ_AddBytes(sq, (UINT8 *) &count, 2);   // Endian dependent
    SQ_AddBytes(sq, (UINT8 *) &Cmd, 2);     // Endian dependent
    SQ_AddBytes(sq, data, count);
    SQ_AddByte(sq, ETX);
    sq->msg_count++;
    sq->total_packet_count++;
    return TRUE;
}

BOOL SQ_Rcv_Msg(SERIAL_QUEUE_INFO *sq, UINT16 *Cmd, UINT16 *Count, UINT8 *data)
//******************************************************************************
{
    UINT8 byte;

    if (sq->msg_count == 0) return FALSE;

    SQ_GetBytes(sq, &byte, 1);
    if (byte != STX) return FALSE;

    SQ_GetBytes(sq, (UINT8 *) Count, 2); // Endian dependent
    SQ_GetBytes(sq, (UINT8 *) Cmd,   2); // Endian dependent
    SQ_GetBytes(sq, data,  *Count);

    SQ_GetBytes(sq, &byte, 1);
    if (byte != ETX) return FALSE;

    sq->msg_count--;

    return TRUE;
}

BOOL SQ_Peek_Msg(SERIAL_QUEUE_INFO *sq, UINT16 *cmd, UINT16 *count)
//******************************************************************************
{
  UINT8 peek_buf[5];

  if (sq->msg_count == 0) return FALSE;

  SQ_PeekBytes(sq, peek_buf, sizeof(peek_buf), 0);
  if (peek_buf[0] != STX) return FALSE;

  *count = (UINT16) ((peek_buf[1] << 8) | peek_buf[2]);
  *cmd   = (UINT16) ((peek_buf[3] << 8) | peek_buf[4]);
  return TRUE;
}

BOOL SQ_Add_Msg_Byte(SERIAL_QUEUE_INFO *sq, UINT8 byte)
//******************************************************************************
{
     return(SQ_Native_Msg_Rcv_Protocol(sq, byte));

#if 0
    switch(sq->msg_state) {
        case MSG_STX :
            if (byte != STX) return FALSE;
            sq->msg_state = MSG_CNTH;
            sq->msg_SOM_in = sq->in;
            break;

        case MSG_CNTH :
            sq->msg_bytes_remaining = (UINT16) (((UINT16) byte) << 8);
            sq->msg_state = MSG_CNTL;
            break;

        case MSG_CNTL :
            sq->msg_bytes_remaining |= (UINT16) byte;
            sq->msg_state = MSG_CMDH;
            sq->msg_data_size = sq->msg_bytes_remaining;
            break;

        case MSG_CMDH :
            sq->msg_state = MSG_CMDL;
            break;

        case MSG_CMDL :
            sq->msg_state = (sq->msg_bytes_remaining) ? MSG_DATA : MSG_ETX;
            break;

        case MSG_DATA :
            if (--sq->msg_bytes_remaining == 0)
                sq->msg_state = MSG_ETX;
            break;

        case MSG_ETX :
            sq->msg_state = MSG_STX;
            if (byte != ETX)
            {
                sq->in = sq->msg_SOM_in;
                                sq->count -= sq->msg_data_size + (NUM_OVHD_BYTES-1); // -1 because no ETX queued
                return FALSE;
            }
            sq->msg_count++;
            sq->total_packet_count++;
            break;
    }//***END switch


    return SQ_AddByte(sq, byte);

#endif
}



BOOL SQ_Snd_Native_Msg(SERIAL_QUEUE_INFO *sq
                       , UINT8 MBusDest
                       ,IP_NATIVE_MSG_HEADER *p_header
                       ,IP_MESSAGE_DATA *data)
//******************************************************************************
//******************************************************************************
//**
//**
//**   This routine will queue a native message for transmission.
//**
//**
//******************************************************************************
//******************************************************************************
{
  UINT16 i, length;
  UINT8  header_chksum = 0;
  UINT8  chksum = 0;



  length = p_header->Count + NUM_NATIVE_OVHD_BYTES;


  //---  If this is for the M-Bus, take in account slave address byte.
  if ( MBusDest ) length++;


  //---  Is there room in the queue for this message?
  if (((UINT32) length) > (sq->size - sq->count)) return FALSE;


  //---  Queue up the M-Bus slave address is required
  if ( MBusDest )  SQ_AddByte(sq, MBusDest);


  //---  Queue the header information
 SQ_AddByte(sq, STX);

  SQ_AddByte(sq, p_header->Dest);
  header_chksum  = p_header->Dest;

  SQ_AddByte(sq, p_header->Src);
  header_chksum += p_header->Src;

  SQ_AddByte(sq, p_header->MBusAddr);
  header_chksum += p_header->MBusAddr;

  SQ_AddByte(sq, p_header->Port);
  header_chksum += p_header->Port;

  SQ_AddBytes(sq, (UINT8*)(&p_header->Command), 2);
  header_chksum += (p_header->Command >> 8) & 0xff;
  header_chksum += p_header->Command & 0xff;

  SQ_AddBytes(sq, (UINT8*)(&p_header->Count), 2);
  header_chksum += (p_header->Count >> 8) & 0xff;
  header_chksum += p_header->Count & 0xff;

  /*
  SQ_AddByte(sq, (UINT8)((p_header->Command >> 8) & 0xff));
  header_chksum += (p_header->Command >> 8) & 0xff;

  SQ_AddByte(sq, (UINT8)(p_header->Command & 0xff));
  header_chksum += p_header->Command & 0xff;

  SQ_AddByte(sq, (UINT8)((p_header->Count >> 8) & 0xff));
  header_chksum += (p_header->Count >> 8) & 0xff;

  SQ_AddByte(sq, (UINT8)(p_header->Count & 0xff));
  header_chksum += p_header->Count & 0xff;
*/

  //---  Queue the Header Checksum
  SQ_AddByte(sq, header_chksum);


  //---  Queue the data information
  SQ_AddBytes(sq, data, p_header->Count);
  for ( i = 0; i < p_header->Count ; i++ )
    chksum += *data++;


  //---  Queue the trailer information
  SQ_AddByte(sq, chksum);
  SQ_AddByte(sq, ETX);

    
  sq->msg_count++;
  sq->total_packet_count++;


  return TRUE;
}

BOOL SQ_Native_Msg_Rcv_Protocol(SERIAL_QUEUE_INFO *sq, UINT8 byte)
//******************************************************************************
//******************************************************************************
//**
//**
//**   This routine implements the native mode protocol.
//**
//**   Valid messages will placed on the appropriate queue.  Invalid messages
//**  are discarded.
//**
//**
//******************************************************************************
//******************************************************************************
{
    UINT8 *header_ptr;
    UINT32 i;




    switch(sq->msg_state) {


        case MSG_STX:
         if (byte != STX) 
         {
            return FALSE;
         } 
         else
         {
           //---  Native Mode Message
           sq->msg_flush = TRUE;
           sq->msg_state = MSG_DEST;
           sq->msg_SOM_in = sq->in;
           sq->msg_SOM_count = 0;
         }

            break;

        case MSG_DEST :
            sq->msg_state = MSG_SRC;
            sq->msg_header[1] = byte;
            sq->msg_chksum = byte;
            break;

        case MSG_SRC :
            sq->msg_state = MSG_MBUS;
            sq->msg_header[2] = byte;
            sq->msg_chksum += byte;
            break;

        case MSG_MBUS :
            sq->msg_state = MSG_PORT;
            sq->msg_header[3] = byte;
            sq->msg_chksum += byte;
            break;

        case MSG_PORT :
            sq->msg_state = MSG_CMDH;
            sq->msg_header[4] = byte;
            sq->msg_chksum += byte;
            break;

        case MSG_CMDH :
            sq->msg_state = MSG_CMDL;
            sq->msg_header[5] = byte;
            sq->msg_chksum += byte;
            break;

        case MSG_CMDL :
            sq->msg_state = MSG_CNTH;
            sq->msg_header[6] = byte;
            sq->msg_chksum += byte;
            break;          

        case MSG_CNTH :
            sq->msg_state = MSG_CNTL;
            sq->msg_header[7] = byte;
            sq->msg_bytes_remaining = byte;
            sq->msg_chksum += byte;
            break;          

        case MSG_CNTL :
            sq->msg_state = MSG_HDR_CHKSUM;
            sq->msg_header[8] = byte;
            sq->msg_bytes_remaining = (sq->msg_bytes_remaining << 8) + byte;
            sq->msg_chksum += byte;
            break;          



        case MSG_HDR_CHKSUM :
            sq->msg_header[9] = byte;

            if ( sq->msg_chksum == byte )
            {

              sq->msg_data_size = sq->msg_bytes_remaining;

              sq->msg_state = (sq->msg_bytes_remaining) ? MSG_DATA : MSG_CHKSUM;
              sq->msg_chksum = 0;


               //---  Check for maximum data length
              if ( (sq->msg_data_size+NUM_NATIVE_OVHD_BYTES) <= MAX_BYTES_IN_QUEUE ) {

                //---  Checksum & data length is okay so try and queue
                  if ( SQ_AddBytes(sq, &(sq->msg_header[0]), IP_NATIVE_HEADER_SIZE+1) )
                  {
                    sq->msg_SOM_count = IP_NATIVE_HEADER_SIZE+1;
                    sq->msg_flush = FALSE;
                    return TRUE;
                  }
                  else
                  {
                    goto ABORT_MSG;
                  }

              }else{
                  //--- 
                  //--- Receive the rest of the message and
                  //--- and the flush it.
                  goto ABORT_MSG;
              }


            }else{
ABORT_MSG:
              //---  Error!  Bad checksum
              sq->msg_state = MSG_STX;
              header_ptr = &(sq->msg_header[1]);
              sq->msg_flush = FALSE;


              //---
              //---  Is there a STX is header?
              //---
              for ( i = 0;  i < IP_NATIVE_HEADER_SIZE; i++ ) {

                byte = *header_ptr++;

                switch ( sq->msg_state )  {

                  case MSG_STX :
                    if (byte == STX) {
                      sq->msg_flush = TRUE;
                      sq->msg_state = MSG_DEST;
                    }
                    break;

                 case MSG_DEST :
                    sq->msg_state = MSG_SRC;
                    sq->msg_header[1] = byte;
                    sq->msg_chksum = byte;
                    break;

                  case MSG_SRC :
                    sq->msg_state = MSG_MBUS;
                    sq->msg_header[2] = byte;
                    sq->msg_chksum += byte;
                    break;

                  case MSG_MBUS :
                    sq->msg_state = MSG_PORT;
                    sq->msg_header[3] = byte;
                    sq->msg_chksum += byte;
                    break;

                  case MSG_PORT :
                    sq->msg_state = MSG_CMDH;
                    sq->msg_header[4] = byte;
                    sq->msg_chksum += byte;
                    break;

                  case MSG_CMDH :
                    sq->msg_state = MSG_CMDL;
                    sq->msg_header[5] = byte;
                    sq->msg_chksum += byte;
                    break;

                  case MSG_CMDL :
                    sq->msg_state = MSG_CNTH;
                    sq->msg_header[6] = byte;
                    sq->msg_chksum += byte;
                    break;          

                  case MSG_CNTH :
                    sq->msg_state = MSG_CNTL;
                    sq->msg_bytes_remaining = byte;
                    sq->msg_header[7] = byte;
                    sq->msg_chksum += byte;
                    break;          

                  case MSG_CNTL :
                    sq->msg_state = MSG_HDR_CHKSUM;
                    sq->msg_bytes_remaining = (sq->msg_bytes_remaining << 8) + byte;
                    sq->msg_header[8] = byte;
                    sq->msg_chksum += byte;
                    break;

                }//**END  switch ( sq->msg_state )


              }//**END  for ( i = 0;    i < IP_NATIVE_HEADER_SIZE; i++ )


            }
            break;



        case MSG_DATA :
            if (--sq->msg_bytes_remaining == 0)
                sq->msg_state = MSG_CHKSUM;

            sq->msg_chksum += byte;
            if ( SQ_AddByte(sq, byte) )
              sq->msg_SOM_count++;
            else
              sq->msg_flush = TRUE;

            break;


        case MSG_CHKSUM :
            if ( sq->msg_chksum == byte )
            {
              sq->msg_state = MSG_ETX;
              if ( SQ_AddByte(sq, byte) )
                sq->msg_SOM_count++;
              else
              {
                //---  Error!  Bad checksum
                sq->msg_state = MSG_STX;
                sq->in = sq->msg_SOM_in;
                sq->count -= sq->msg_SOM_count;
              }
            }
            else
            {
              //---  Error!  Bad checksum
              sq->msg_state = MSG_STX;
              sq->in = sq->msg_SOM_in;
              sq->count -= sq->msg_SOM_count;
            }
            break;

        case MSG_ETX :
            sq->msg_state = MSG_STX;
            if ( (byte != ETX)
                 ||
                 (sq->msg_flush == TRUE) )
            {
              sq->in = sq->msg_SOM_in;
              sq->count -= sq->msg_SOM_count;
            }
            else
            {
              if ( SQ_AddByte(sq, byte) )
              {
               sq->msg_count++;
              }
              else
              {
               sq->in = sq->msg_SOM_in;
               sq->count -= sq->msg_SOM_count;
              }
            }
            break;


       
        
    } // switch



//  return SQ_AddByte(sq, byte);
    return TRUE;
   

}




BOOL SQ_Rcv_Native_Msg(SERIAL_QUEUE_INFO *sq, IP_NATIVE_MSG_HEADER *header, UINT8 *data)
//******************************************************************************
//******************************************************************************
//**
//**
//**   This routine will dequeue a native message that was received and copy
//** the message information into the header and data buffers. 
//**
//**
//******************************************************************************
//******************************************************************************
{


#if 0
    UINT8 byte;


    if (sq->msg_count == 0) return FALSE;


    //---
    //---  Need to disable interrupts because the
    //--- function SQ_GetBytes modifies 'sq->count'.
    //---
   MCF_DisableInterrupts();

    
    //---  Get the start sync byte
    if ( !SQ_GetBytes(sq, &byte, 1) ) {
      sq->msg_count = 0;
      MCF_EnableInterrupts();
      return FALSE;
     }




   //---  Check for Native or ASCII message       
    if (byte != STX) {

      //---  ASCII Message 
      header->Src = 0xff;
      header->Command = IP_D_ASCII_CMD;
      header->Count = 1;
     
      //---  Store the byte 
      *data = byte;


   
   }else {

     //---  Native Mode Message
     
     //--- Copy over header information
      SQ_GetBytes(sq, (UINT8 *)header,  IP_NATIVE_HEADER_SIZE);

      //-- Copy over data bytes
      SQ_GetBytes(sq, data,  header->Count);

      SQ_GetBytes(sq, &byte, 1);       // Throw checksum byte away for now????
      SQ_GetBytes(sq, &byte, 1);
      if (byte != ETX) {
        MCF_EnableInterrupts();
        return FALSE;
      }

   }


   //---
   //---  1 message has been removed from the queue.
   //---
   sq->msg_count--;
   

   MCF_EnableInterrupts();

   
#endif
   return TRUE;


}


UINT16 SQ_Get_Full_Snd_Native_Msg(SERIAL_QUEUE_INFO *sq, UINT8 *data)
//******************************************************************************
//******************************************************************************
//**
//**
//**   This routine will dequeue a native message from the 'transmit' queuue
//**   and copy the message into the buffer. 
//**
//**   A full native message consists of routing byte, STX, message header,
//**   message data, checksum and ETX.
//**
//**
//******************************************************************************
//******************************************************************************
{
    IP_NATIVE_MSG_HEADER  *header;


    if (sq->msg_count == 0) return 0;


    //--- Copy over M-bus routing information
    if ( !SQ_GetBytes(sq, (UINT8 *)data,  1) ) {
        sq->msg_count = 0;
        return( 0 );
    }


    //---  Copy the STX
    data++;
    if ( !SQ_GetBytes(sq, data,  1) )
          return( 0 );


    //---  Copy over header information
    data++;
    header = (IP_NATIVE_MSG_HEADER *)data;
    if ( !SQ_GetBytes(sq, (UINT8 *)header,  IP_NATIVE_HEADER_SIZE) )
          return( 0 );


    //---  Copy over data bytes, checksum & ETX
    data += IP_NATIVE_HEADER_SIZE;
    if ( !SQ_GetBytes(sq, data, (UINT16)(header->Count + 2) ) )
        return( 0 );


    sq->msg_count--;

    return(  1 + header->Count + NUM_NATIVE_OVHD_BYTES );
}




BOOL SQ_Peek_Native_Msg(SERIAL_QUEUE_INFO *sq, IP_NATIVE_MSG_HEADER *header)
//******************************************************************************
//** 
//** 
//**   The M-Bus Transmit Queue cannot be peeked using this function. 
//** 
//** 
//******************************************************************************
{
  if (sq->msg_count == 0) return FALSE;


  SQ_PeekBytes(sq, (UINT8 *)header, IP_NATIVE_HEADER_SIZE, 1);

  
  return TRUE;
}

#endif
