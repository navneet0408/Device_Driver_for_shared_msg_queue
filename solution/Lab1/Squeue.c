#include <linux/module.h>  // Module Defines and Macros (THIS_MODULE)
#include <linux/kernel.h>  // 
#include <linux/fs.h>	   // Inode and File types
#include <linux/cdev.h>    // Character Device Types and functions.
#include <linux/types.h>
#include <linux/slab.h>	   // Kmalloc/Kfree
#include <asm/uaccess.h>   // Copy to/from user space
#include <linux/string.h>
#include <linux/device.h>  // Device Creation / Destruction functions
#include "kDataTypes.h"

//Data Structures
SQueue *sQueue[NUM_DEVICES];
static dev_t sQueueDeviceNo[NUM_DEVICES];
struct class *sQueueDeviceClass[NUM_DEVICES];
static struct device *sQueueDevice[NUM_DEVICES];


// Device Driver Function prototypes
int squeueDriverOpen(struct inode *inode, struct file *file);
int squeueDriverRelease(struct inode *inode, struct file *file);
ssize_t squeueDriverWrite(struct file *file, const char *buf, size_t count, loff_t *ppos);
ssize_t squeueDriverRead(struct file *file, char *buf, size_t count, loff_t *ppos);
static int __init squeueDriverInit (void);
static void __exit squeueDriverExit (void);

//Enqueue a message
int add(Message *msg, DeviceData *deviceData);
//Dequeue a message
int delete (Message **msg, DeviceData *deviceData);

//Timestamp
static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
};

//Open a device
int squeueDriverOpen(struct inode *inode, struct file *file)
{
	SQueue *sq;
	sq = container_of(inode->i_cdev, SQueue, cdev);

	file->private_data = sq;
	return 0;
}

//Close the device
int squeueDriverRelease(struct inode *inode, struct file *file)
{
	Message *msg;
	SQueue *sq = file->private_data;
	while (delete(&msg, &sq->data) >=0)
	{
		kfree(msg);
	}
	return 0;
}

//Write to the device
ssize_t squeueDriverWrite(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int res;
	SQueue *sq = file->private_data;
	Message *dstMsg;
	Message *srcMsg = (Message *) buf;
	
	if(count != sizeof(Message))	//Check if the size of buffer is correct
	{
		int si = sizeof(Message);
		printk(KERN_DEBUG "Wrong size: count=%d req=%d\n", (int) count, si);
		return -1;
	}		
	
	dstMsg = kmalloc(sizeof(Message), GFP_KERNEL);	
	if (!dstMsg)
	{
		printk(KERN_DEBUG "Memory allocation failed\n");
		return -ENOMEM;
	}
	res = copy_from_user(dstMsg, srcMsg, count);
	if(res < 0)
	{
		printk(KERN_DEBUG "copy_from_user Failed\n");
		return -ENOMEM;
	}
	
	//Timestamping
	dstMsg->lastInsertTime=0;	
	dstMsg->lastInsertTime=rdtsc();
	
	//Enqueing
	res = add(dstMsg, &sq->data);
	
	if(res < 0)
	{
		return res;
	}
	return 0;
}


//Reading from the device
ssize_t squeueDriverRead(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int res;
	unsigned long timeStamp=0;
	SQueue *sq = file->private_data;
	Message *dstMsg = (Message *) buf;
	Message *srcMsg;
	if(count != sizeof(Message))	//Check if the size of buffer is correct
	{
		printk(KERN_DEBUG "Wrong size\n");
		return -1;
	}
	
	//Dequeueing
	res = delete (&srcMsg, &sq->data);
	
	if (res < 0)
	{
		//printk("Read failed for %s!!!\n", sq->name);
		return res;
	}
	
	//Timestamping
	timeStamp=rdtsc();
	srcMsg->queingTime = srcMsg->queingTime + timeStamp - srcMsg->lastInsertTime;
	
	res = copy_to_user(dstMsg, srcMsg, count);
	if (res < 0)
	{
		printk(KERN_DEBUG "copy_to_user failed\n");
		return res;
	}
	
	kfree(srcMsg);
	return count;
}

//Defining file_operations structure
static struct file_operations squeue_fops = {
    .owner		= THIS_MODULE,           	/* Owner */
    .open		= squeueDriverOpen,        	/* Open method */
    .release		= squeueDriverRelease,     	/* Release method */
    .write		= squeueDriverWrite,       	/* Write method */
    .read		= squeueDriverRead,		/* Read method */
};

//Initializing the driver
static int __init squeueDriverInit (void)
{
	int ret,i;
	
	for(i=0; i<NUM_DEVICES; ++i)
	{
		if (alloc_chrdev_region(&sQueueDeviceNo[i], 0, 1, DEVICE_NAMES[i]) < 0) 
		{
			printk(KERN_DEBUG "Can't register device\n"); 
			return -1;
		}
		sQueueDeviceClass[i] = class_create(THIS_MODULE, DEVICE_NAMES[i]);
		sQueue[i] = kmalloc(sizeof(SQueue), GFP_KERNEL);
		if (!sQueue[i])
		{
			return -ENOMEM;
		}
		sprintf(sQueue[i]->name, DEVICE_NAMES[i]);
		cdev_init(&sQueue[i]->cdev, &squeue_fops);
		sQueue[i]->cdev.owner = THIS_MODULE;
	
		ret = cdev_add(&sQueue[i]->cdev, sQueueDeviceNo[i], 1);
		if (ret) 
		{
			printk(KERN_DEBUG "Bad cdev\n");
			return ret;
		}
	
		sQueueDevice[i] = device_create(sQueueDeviceClass[i], NULL, 
					MKDEV(MAJOR(sQueueDeviceNo[i]), 0), NULL, DEVICE_NAMES[i]);
		memset(&sQueue[i]->data, 0, sizeof(DeviceData));
		mutex_init (&sQueue[i]->sQueueMutex);
		printk("SQueue driver initialized.\n");
	}
	return 0;
}

static void __exit squeueDriverExit (void)
{
	int i;
	for(i=0; i<NUM_DEVICES; ++i)
	{
		unregister_chrdev_region(sQueueDeviceNo[i], 1);

		/* Destroy device */
		device_destroy (sQueueDeviceClass[i], MKDEV(MAJOR(sQueueDeviceNo[i]), 0));
		cdev_del(&sQueue[i]->cdev);
		kfree(sQueue[i]);
	
		/* Destroy driver_class */
		class_destroy(sQueueDeviceClass[i]);

		printk("SQueue driver removed.\n");
	}
}

//Function to enqueue a message
int add(Message *msg, DeviceData *deviceData)
{
	if(deviceData->numMsgs >= QUEUE_SIZE)
	{
		return -1;
	}
	
	deviceData->messages[deviceData->tail] = msg;
	deviceData->tail = (deviceData->tail+1)%10;
	deviceData->numMsgs++;
	return 0;
}

//Function to dequeue a message
int delete (Message **msg, DeviceData *deviceData)
{
	if(deviceData->numMsgs <= 0)
	{
		return -1;
	}
	(*msg) = deviceData->messages[deviceData->head];
	deviceData->messages[deviceData->head]=NULL;
	deviceData->head = (deviceData->head+1)%10;
	deviceData->numMsgs--;
	return 0;
}
module_init (squeueDriverInit);
module_exit (squeueDriverExit);
MODULE_LICENSE("GPL v2");
