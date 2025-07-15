//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: test.c
//    Version: 1.0
//     Author: dm
//
//            Test Application for MONSSTR 2300 MP Driver
//
//    Revisions:   
//

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "cxmp.h"



unsigned char command[40][80];

int main( int args, char * argv[] )
{
    int i;
    int bytes;
    int cmd;
    int index = 0;;
    unsigned char byte;
    FILE *fp;

    fp = fopen("Script.txt","rb+");

    memset(command,0x0,40*80);
    if(!fp){
        printf("File Does not exist\r\n");

    }else {

        cmd = 0;
        while( !feof(fp) ) {
            bytes = fread(&byte,1,1,fp);
            if( byte == 0xA) {
                cmd++;
                index = 0;
            }else{
                if( (byte != 0xA) && (byte != 0xD) ){
printf("%c",byte);
                    command[cmd][index++] = byte;
                    byte = 0;
                }
            }

        }

        fclose(fp);

        if(cmd == 0) {
                printf("[%d] -> %s",cmd,command[0]);
        }else{
       
            for(i = 0; i < cmd ;i++) {
                strncat(command[i],"\r\n",2);
                printf("[%d] -> %s",i,command[i]);
            }
        }
    }

}
