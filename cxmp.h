//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: cxmp.h
//    Version: 1.0
//     Author: dm
//
//            MONSSTR 2300 MP Driver
//
//    Revisions:   
//

#ifndef __CXMP_H
#define __CXMP_H


/* Device number bitfield division */
#define MP_DEVICE_NUM(a) (a)

#define MAX_DEVICES 64
#define MAX_BARS 6

struct mp_bar{
  unsigned int ptr;
  unsigned char is_port;
};

struct mp_device{
  struct pci_dev *pdev;
  struct mp_bar bars[MAX_BARS];
  unsigned char irq_connected;
  void * int_data;
};

struct mp_driver_S{
  int num_devices;
};

struct mp_buf_S{
  void *ptr;
  int size;
  int offset;
  int access_size;
  int increment;
  int bar;
};

struct mp_reg_S{
  void * ptr;
  int offset;
  int size;
};

struct mp_loc_S{
  unsigned short dev;
  unsigned short fn;
  unsigned char bus_number;
};

/* ioctl() commands */
#define _MP_IOC_MAGIC  'z'

#define MP_IOC_GET_DRIVER_STATUS _IOR(_MP_IOC_MAGIC, 0x80, struct mp_driver_S)
#define MP_IOC_READ _IOW(_MP_IOC_MAGIC, 0x81, struct mp_buf_S)
#define MP_IOC_WRITE _IOW(_MP_IOC_MAGIC, 0x82, struct mp_buf_S)
#define MP_IOC_REGREAD _IOW(_MP_IOC_MAGIC, 0x83, struct mp_reg_S)
#define MP_IOC_REGWRITE _IOW(_MP_IOC_MAGIC, 0x84, struct mp_reg_S)
#define MP_IOC_GETLOC _IOWR(_MP_IOC_MAGIC, 0x85, struct mp_loc_S)
#define MP_IOC_ISRCONNECT _IOWR(_MP_IOC_MAGIC, 0x86, struct mp_isr_connect_S)
#define MP_IOC_ISRDISCONNECT _IOW(_MP_IOC_MAGIC, 0x87, struct mp_isr_disconnect_S)
#define _MP_IOC_MAXNR 0x87

#endif /* __CXMP_H */
