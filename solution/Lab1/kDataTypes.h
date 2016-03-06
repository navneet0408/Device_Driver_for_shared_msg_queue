#ifndef _KDATATYPES_H
#define _KDATATYPES_H

#define QUEUE_SIZE 10

#include "uDataTypes.h"
struct mutex;

typedef struct DeviceData_t
{
	Message *messages[QUEUE_SIZE];
	int head;
	int tail;
	int numMsgs;
}DeviceData;

typedef struct SQueue_t 
{
	struct cdev cdev;               /* The cdev structure */
	char name[20];                  /* Name of device*/
	DeviceData data;
	struct mutex sQueueMutex;
}SQueue;

#endif
