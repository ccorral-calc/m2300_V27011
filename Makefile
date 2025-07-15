# M2300 Makefile
#
#

# CC = /eldk/usr/bin/ppc_82xx-gcc
#CC = /home/bdu/buildroot/build_nios2/staging_dir/bin/nios2-linux-gcc
CC = /usr/local/nios2-linux-tools/bin/nios2-linux-gcc

CFLAGS = -O2 -elf2flt="s 64000"
//CFLAGS = -Wall  -elf2flt="s 64000"
//CFLAGS =  -elf2flt="s 64000"
#optional flags
# -g	debug
# -Wall show all warnings (highest warning level)
# -D_DEBUG

#DEFS = -I. -I/home/lwbuss/linux-2.6.16/include -D_FILE_OFFSET64 -D_FILE_OFFSET_BITS=64 
DEFS = -I.  -D_FILE_OFFSET64 -D_FILE_OFFSET_BITS=64 

LIB = -lpthread 
#LIB = 
OS = uname -s



OBJS = M23_Controller.o M23_Main.o M23_Utilities.o squeue.o M2X_cmd_ascii.o M2X_serial.o \
       M2X_time.o M23_setup.o  M23_disk_io.o M2X_system_settings.o M23_EventProcessor.o\
       M23_Tmats.o M2X_FireWire_io.o M23_HealthProcessor.o  M23_Stanag.o M23_M1553_cmd.o\
       M23_Discretes.o M23_MP_Handler.o M23_Ssric.o M23_PCM_sizing.o M23_I2c.o M23_Login.o M23_Control.o\
       M23_Ethernet.o M23_GPS.o M23_DataConversion.o

m2300_012319: $(OBJS) Makefile
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -o m2300_012319

M23_Controller.o: M23_Controller.c M23_Controller.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_Controller.c

M23_Main.o: M23_Main.c
	$(CC) $(CFLAGS) $(DEFS) -c -D_REENTRANT M23_Main.c

squeue.o: squeue.c squeue.h
	$(CC) $(CFLAGS) $(DEFS) -c -D_REENTRANT squeue.c

M2X_cmd_ascii.o: M2X_cmd_ascii.c M2X_cmd_ascii.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M2X_cmd_ascii.c

M23_M1553_cmd.o: M23_M1553_cmd.c M23_M1553_cmd.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_M1553_cmd.c

M2X_serial.o: M2X_serial.c M2X_serial.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M2X_serial.c

M23_Stanag.o: M23_Stanag.c M23_Stanag.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_Stanag.c

M2X_time.o: M2X_time.c M2X_time.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M2X_time.c

M23_setup.o: M23_setup.c M23_setup.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_setup.c

M23_disk_io.o: M23_disk_io.c M23_disk_io.h
	$(CC) $(CFLAGS) $(DEFS) -c -D_REENTRANT M23_disk_io.c

M2X_system_settings.o: M2X_system_settings.c M2X_system_settings.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M2X_system_settings.c

M23_Tmats.o: M23_Tmats.c
	$(CC) $(CFLAGS) $(DEFS) -c -D_REENTRANT M23_Tmats.c
	
M23_Utilities.o: M23_Utilities.c M23_Utilities.h 
	$(CC) $(CFLAGS) $(DEFS) -c -D_REENTRANT  M23_Utilities.c

M23_HealthProcessor.o: M23_HealthProcessor.c M23_Status.h M23_features.h M2X_Const.h
	$(CC) $(CFLAGS) $(DEFS) -c -D_REENTRANT M23_HealthProcessor.c

M23_EventProcessor.o: M23_EventProcessor.c M23_EventProcessor.h
	$(CC) $(CFLAGS) $(DEFS) -c -D_REENTRANT M23_EventProcessor.c

M2X_FireWire_io.o: M2X_FireWire_io.c
	$(CC) $(CFLAGS) $(DEFS) -c -D_REENTRANT M2X_FireWire_io.c

M23_Discretes.o: M23_Discretes.c M23_Discretes.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_Discretes.c

M23_MP_Handler.o: M23_MP_Handler.c M23_MP_Handler.h cxmp.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_MP_Handler.c

M23_Ssric.o: M23_Ssric.c M23_Ssric.h squeue.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_Ssric.c

M23_PCM_sizing.o: M23_PCM_sizing.c M23_PCM_sizing.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_PCM_sizing.c

M23_I2c.o: M23_I2c.c M23_I2c.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_I2c.c

M23_Login.o: M23_Login.c M23_Login.h M23_LinkDriver.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_Login.c

M23_Control.o: M23_Control.c
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_Control.c

M23_Ethernet.o: M23_Ethernet.c
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_Ethernet.c

M23_GPS.o: M23_GPS.c M2X_serial.h
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_GPS.c

M23_DataConversion.o: M23_DataConversion.c
	$(CC) $(CFLAGS) $(DEFS) -c  -D_REENTRANT M23_DataConversion.c

clean:
	rm $(OBJS) 


