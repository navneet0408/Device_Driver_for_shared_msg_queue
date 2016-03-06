
#ifndef _UDATATYPES_H
#define _UDATATYPES_H

#define MAX_MESSAGE_LENGTH 80
#define NUM_DEVICES 4
const char *DEVICE_NAMES[] = {"bus_in_q", "bus_out_q1", "bus_out_q2", "bus_out_q3"};

typedef struct Message_t
{
	int msgId;
	int srcId;
	int dstId;
	unsigned long queingTime;
	unsigned long lastInsertTime;
	char message[MAX_MESSAGE_LENGTH];
}Message;

#endif
