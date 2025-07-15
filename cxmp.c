//////////////////////////////////////////////////////////////////////////////
//
//    Copyright © 2004 CALCULEX Inc.
//
//    The material herein contains information that is proprietary and 
//    confidential to CALCULEX, Inc. and it is not to be copied or disclosed 
//    for any purpose except as specifically authorized in writing.
//
//       File: cxmp.c
//    Version: 1.0
//     Author: dm
//
//            MONSSTR 2300 MP Driver
//
//    Revisions:   
//

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <errno.h>
#include <asm/uaccess.h>

#include "cxmp.h"





/* Storage for information about CALCULEX MP (MIL-1553/PCM) cards */

static struct mp_device mp_devices[MAX_DEVICES];
static int num_mp_devices = 0;

/* PCI side of the driver -- declarations */

#define PCI_VENDOR_ID_CALCULEX  0x13e4
#define PCI_DEVICE_ID_MP        0x2301


static char *mp_device_names[] ={
                "CALCULEX MP"
};

static struct pci_device_id mp_pci_tbl[] = {
    { PCI_VENDOR_ID_CALCULEX, PCI_DEVICE_ID_MP, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
    { 0,}
};

MODULE_DEVICE_TABLE(pci, mp_pci_tbl);

int mp_probe_device(struct pci_dev * pdev, const struct pci_device_id * ent);
void mp_remove_device(struct pci_dev * pdev);

static struct pci_driver mp_driver = {
    name:     "cxmp",
    id_table: mp_pci_tbl,
    probe:    mp_probe_device,
    remove:   mp_remove_device,
};

/* User side of the driver (device file operations) -- declarations */

int mp_open(struct inode * inode, struct file * filp);
int mp_release(struct inode * inode, struct file * filp);
int mp_ioctl(struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg);

static struct file_operations mp_fops = {
    open:       mp_open,
    release:    mp_release,
    ioctl:      mp_ioctl
};

static int mp_major = 0;

/* PCI side of the driver -- implementations */

int pci_read_config(struct pci_dev * pdev, u32 offset, void * ptr, u32 size);
int pci_write_config(struct pci_dev * pdev, u32 offset, void * ptr, u32 size);
int pci_read_bar(struct mp_bar * bar, u32 offset, void * ptr, u32 size, u8 access_size, u8 increment);
int pci_write_bar(struct mp_bar * bar, u32 offset, void * ptr, u32 size, u8 access_size, u8 increment);
void * read_iomemory(u32 address, u8 access_size);
void * read_ioport(u32 address, u8 access_size);
void write_iomemory(void * data, void * destination , u8 access_size);
void write_ioport(void * data, void * destination, u8 access_size);

int mp_probe_device(struct pci_dev * pdev, const struct pci_device_id * ent)
{
    int i;
    int device_num = num_mp_devices;
    
    printk(KERN_INFO "cxmp: Found device %d: %s\n", device_num, mp_device_names[ent->driver_data]);
    if(device_num >= MAX_DEVICES){
        printk(KERN_ERR "cxmp: max devices reached (%d)!\n", MAX_DEVICES);
        return -ENOMEM;
    }
    pci_enable_device(pdev);
    pdev->driver_data = (void*)device_num;
    mp_devices[device_num].pdev = pdev;
    mp_devices[device_num].irq_connected = 0;
    
    for( i = 0; i < MAX_BARS; i++ ) {
        unsigned long start = pci_resource_start(pdev, i);
        unsigned long len = pci_resource_len(pdev, i);
        if(len == 0)
            goto bar_loop_none;
        
        if((pci_resource_flags(pdev, i) & IORESOURCE_MEM)){
            if(!check_mem_region(start, len)){
                request_mem_region(start, len, "mp");
                mp_devices[device_num].bars[i].ptr = (u32)ioremap_nocache(start, len);
                mp_devices[device_num].bars[i].is_port = 0;
            } else{
                goto bar_loop_none;
            }
        } else{     /* IORESOURCE_IO */
            if(!check_region(start, len)){
                request_region(start, len, "mp");
                mp_devices[device_num].bars[i].ptr = start;
                mp_devices[device_num].bars[i].is_port = 1;
            } else{
                goto bar_loop_none;
            }
        }
        continue;
        
    bar_loop_none:
        mp_devices[device_num].bars[i].ptr = 0;
    }
    pci_set_master(pdev);
    
    num_mp_devices++;
    return 0;
}

void mp_remove_device(struct pci_dev * pdev)
{
    int device_num = (int)pdev->driver_data;
    int i;
    
    printk(KERN_INFO "cxmp: Releasing device %d\n", device_num);
    for( i=0; i<MAX_BARS; i++ ){
        unsigned long start = pci_resource_start(pdev, i);
        unsigned long len = pci_resource_len(pdev, i);
        if(!mp_devices[device_num].bars[i].ptr)
            continue;
        if(mp_devices[device_num].bars[i].is_port){
            release_region(start, len);
        }
        else{
            iounmap((void*)mp_devices[device_num].bars[i].ptr);
            release_mem_region(start, len);
        }
    }
}

/* Device file operations -- implementations */

int mp_open(struct inode * inode, struct file * filp)
{
    int device_num = MP_DEVICE_NUM(MINOR(inode->i_rdev));
    
    if(device_num >= num_mp_devices){
        return -ENODEV;
    }
    printk(KERN_DEBUG "cxmp: Opening device %d\n", device_num);
    MOD_INC_USE_COUNT;
    return 0;
}

int mp_release(struct inode * inode, struct file * filp)
{
    int device_num = MP_DEVICE_NUM(MINOR(inode->i_rdev));

    printk(KERN_DEBUG "cxmp: Closing device %d\n", device_num);
    MOD_DEC_USE_COUNT;
    return 0;
}

int mp_ioctl(struct inode * inode, struct file * filp,
                                unsigned int cmd, unsigned long arg)
{
    int device_num = MP_DEVICE_NUM(MINOR(inode->i_rdev));
    int rv = 0;
    struct mp_driver_S *mp_driver_ctl;
    struct mp_reg_S *reg_ctl;
    struct mp_buf_S *buf_ctl;
    struct mp_loc_S *loc_ctl;

    /* Convenience variable to check each time whether it's I/O access */
    
    if(_IOC_TYPE(cmd) != _MP_IOC_MAGIC) return -ENOTTY;
    if(_IOC_NR(cmd) > _MP_IOC_MAXNR) return -ENOTTY;
    if(_IOC_DIR(cmd) & _IOC_READ)
        rv = !access_ok(VERIFY_WRITE, (void*)(arg), _IOC_SIZE(cmd));
    if(!rv && _IOC_DIR(cmd) & _IOC_WRITE)
        rv = !access_ok(VERIFY_READ, (void*)(arg), _IOC_SIZE(cmd));
    if(rv){
        printk(KERN_ERR "cxmp: access_ok failed for ioctl()\n");
        return -EFAULT;
    }
    
    switch(cmd){
    case MP_IOC_GET_DRIVER_STATUS:
        mp_driver_ctl = (struct mp_driver_S*)arg;
        mp_driver_ctl->num_devices = num_mp_devices;
        break;
        
    case MP_IOC_REGREAD:
        reg_ctl = (struct mp_reg_S*)arg;
        pci_read_config(mp_devices[device_num].pdev, reg_ctl->offset, reg_ctl->ptr, reg_ctl->size);
        break;
        
    case MP_IOC_REGWRITE:
        reg_ctl = (struct mp_reg_S*)arg;
        pci_write_config(mp_devices[device_num].pdev, reg_ctl->offset, reg_ctl->ptr, reg_ctl->size);
        break;
        
    case MP_IOC_READ:
        buf_ctl = (struct mp_buf_S*)arg;
        if(!access_ok(VERIFY_WRITE, buf_ctl->ptr, buf_ctl->size))
            return -EFAULT;
        pci_read_bar(&mp_devices[device_num].bars[buf_ctl->bar], buf_ctl->offset, buf_ctl->ptr, buf_ctl->size, buf_ctl->access_size, buf_ctl->increment);
        break;
        
    case MP_IOC_WRITE:
        buf_ctl = (struct mp_buf_S*)arg;
        if(!access_ok(VERIFY_READ, buf_ctl->ptr, buf_ctl->size))
            return -EFAULT;
        pci_write_bar(&mp_devices[device_num].bars[buf_ctl->bar], buf_ctl->offset, buf_ctl->ptr, buf_ctl->size, buf_ctl->access_size, buf_ctl->increment);
        break;

    case MP_IOC_GETLOC:
        loc_ctl = (struct mp_loc_S*)arg;
        loc_ctl->dev = PCI_SLOT(mp_devices[device_num].pdev->devfn);
        loc_ctl->fn = PCI_FUNC(mp_devices[device_num].pdev->devfn);
        loc_ctl->bus_number = mp_devices[device_num].pdev->bus->number;
        break;

    default:
        return -ENOTTY;
    }

    return rv;
}

int pci_read_config(struct pci_dev * pdev, u32 offset, void * ptr, u32 size)
{
    u32 ptr_address = (u32)ptr;
    u32 i;
    for(i = 0; size > 3 && i < size - 3; i += 4){
        u32 data;
        pci_read_config_dword(pdev, offset + i, &data);
        copy_to_user((void*)(ptr_address + i), &data, 4);
    }
    if(size > 1 && i < size - 1){
        u16 data;
        pci_read_config_word(pdev, offset + i, &data);
        copy_to_user((void*)(ptr_address + i), &data, 2);
        i += 2;
    }
    if(i < size){
        u8 data;
        pci_read_config_byte(pdev, offset + i, &data);
        copy_to_user((void*)(ptr_address + i), &data, 1);
    }
    return 0;
}


int pci_write_config(struct pci_dev * pdev, u32 offset, void * ptr, u32 size)
{
    u32 ptr_address = (u32)ptr;
    u32 i;
    for(i = 0; size > 3 && i < size - 3; i += 4){
        u32 data;
        copy_from_user(&data, (void*)(ptr_address + i), 4);
        pci_write_config_dword(pdev, offset + i, data);
    }
    if(size > 1 && i < size - 1){
        u16 data;
        copy_from_user(&data, (void*)(ptr_address + i), 2);
        pci_write_config_word(pdev, offset + i, data);
        i += 2;
    }
    if(i < size){
        u8 data;
        copy_from_user(&data, (void*)(ptr_address + i), 1);
        pci_write_config_byte(pdev, offset + i, data);
    }
    return 0;
}

int pci_read_bar(struct mp_bar * bar, u32 offset, void * ptr, u32 size, u8 access_size, u8 increment)
{
    u32 ptr_address = (u32)ptr;
    u32 i;
    if(size % access_size || (access_size != 4 && access_size != 2 && access_size != 1) || !bar->ptr) {
        printk(KERN_ERR "cxmp: Unused BAR or access size not equal to 1, 2 or 4\n");
        return -EINVAL;
    }
    if(bar->is_port){
        switch(access_size){
        case 4:
            for(i = 0; i < size; i += access_size){
                u32 data;
                data = inl(bar->ptr + offset + (increment ? i : 0));
                copy_to_user((u32*)(ptr_address + i), &data, access_size);
            }
            break;
        case 2:
            for(i = 0; i < size; i += access_size){
                u16 data;
                data = inw(bar->ptr + offset + (increment ? i : 0));
                copy_to_user((u16*)(ptr_address + i), &data, access_size);
            }
            break;
        default:
            for(i = 0; i < size; i += access_size){
                u8 data;
                data = inb(bar->ptr + offset + (increment ? i : 0));
                copy_to_user((u8*)(ptr_address + i), &data, access_size);
            }
            break;
        };
        return 0;
    }
    else{
        switch(access_size){
        case 4:
            for(i = 0; i < size; i += access_size){
                u32 data;
                data = readl(bar->ptr + offset + (increment ? i : 0));
                copy_to_user((u32*)(ptr_address + i), &data, access_size);
            }
            break;
        case 2:
            for(i = 0; i < size; i += access_size){
                u16 data;
                data = readw(bar->ptr + offset + (increment ? i : 0));
                copy_to_user((u16*)(ptr_address + i), &data, access_size);
            }
            break;
        default:
            for(i = 0; i < size; i += access_size){
                u8 data;
                data = readb(bar->ptr + offset + (increment ? i : 0));
                copy_to_user((u8*)(ptr_address + i), &data, access_size);
            }
            break;
        };
        return 0;
    }
}

int pci_write_bar(struct mp_bar * bar, u32 offset, void * ptr, u32 size, u8 access_size, u8 increment)
{
    u32 ptr_address = (u32)ptr;
    u32 i;
    if(size % access_size || (access_size != 4 && access_size != 2 && access_size != 1) || !bar->ptr) {
        printk(KERN_ERR "cxmp: Unused BAR or access size not equal to 1, 2 or 4\n");
        return -EINVAL;
    }
    if(bar->is_port){
        switch(access_size){
        case 4:
            for(i = 0; i < size; i += access_size){
                u32 data;
                copy_from_user(&data, (u32*)(ptr_address + i), access_size);
                outl(data, (bar->ptr + offset + (increment ? i : 0)));
            }
            break;
        case 2:
            for(i = 0; i < size; i += access_size){
                u16 data;
                copy_from_user(&data, (u16*)(ptr_address + i), access_size);
                outw(data, (bar->ptr + offset + (increment ? i : 0)));
            }
            break;
        default:
            for(i = 0; i < size; i += access_size){
                u8 data;
                copy_from_user(&data, (u8*)(ptr_address + i), access_size);
                outb(data, (bar->ptr + offset + (increment ? i : 0)));
            }
            break;
        };
        return 0;
    }
    else{
        switch(access_size){
        case 4:
            for(i = 0; i < size; i += access_size){
                u32 data;
                copy_from_user(&data, (u32*)(ptr_address + i), access_size);
                writel(data, (bar->ptr + offset + (increment ? i : 0)));
            }
            break;
        case 2:
            for(i = 0; i < size; i += access_size){
                u16 data;
                copy_from_user(&data, (u16*)(ptr_address + i), access_size);
                writew(data, (bar->ptr + offset + (increment ? i : 0)));
            }
            break;
        default:
            for(i = 0; i < size; i += access_size){
                u8 data;
                copy_from_user(&data, (u8*)(ptr_address + i), access_size);
                writeb(data, (bar->ptr + offset + (increment ? i : 0)));
            }
            break;
        };
        return 0;
    }
}

/* Main module initialization functions */

int mp_module_init(void)
{
    int rv;

    printk(KERN_INFO "cxmp: Initializing\n");

    /* General initializations */
    SET_MODULE_OWNER(&mp_fops);
    EXPORT_NO_SYMBOLS;
    memset(mp_devices, 0, sizeof(mp_devices));

    /* Initialize PCI side of driver */
    rv = pci_module_init(&mp_driver);
    if(rv < 0){
        printk(KERN_WARNING "cxmp: No recognized devices\n");
        return rv;
    }

    /* Initialize device file side of driver */
    rv = devfs_register_chrdev(mp_major, "mp", &mp_fops);
    if(rv < 0){
        printk(KERN_ERR "cxmp: Can't register device with major number %d\n", mp_major);
        return rv;
    }
    if(mp_major == 0) mp_major = rv; /* dynamic */
    return 0;
}

void mp_module_exit(void)
{
    int rv, i;


    for(i = 0; i < num_mp_devices; i++) {
        if(mp_devices[i].irq_connected) {

            free_irq(mp_devices[i].pdev->irq, &mp_devices[i]);
        }
    }
    pci_unregister_driver(&mp_driver);
    rv = devfs_unregister_chrdev(mp_major, "mp");
    if(rv < 0){
        printk(KERN_ERR "cxmp: Can't unregister device with major number %d: error %d\n", mp_major, rv);
    }
}

module_init(mp_module_init);
module_exit(mp_module_exit);
