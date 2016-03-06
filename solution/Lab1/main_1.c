#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include "uDataTypes.h"

#define BUS_IN_QUEUE 	0
#define BUS_OUT_QUEUE1 	1
#define BUS_OUT_QUEUE2	2
#define BUS_OUT_QUEUE3	3

//Global Variables
long msgId = 0;
long numMsgsSent = 0;
long numMsgsReceived = 0;
int fd[NUM_DEVICES];

pthread_mutex_t queueMutex;
pthread_mutex_t msgIdMutex;
pthread_mutex_t sendMutex;
pthread_mutex_t receiveMutex;

pthread_t senders[3];
pthread_t receivers[3];
pthread_t busDaemon;

int exiting = 0;

//Function to generate random numbers
int randomNoGenerator(int lo, int hi)
{
	return ((rand() % (hi-lo+1)) + lo);
}

//Thread for sending Messages
void *senderThread (void *arg)
{
	Message msg;
	int res=-1;
	int i;
	long senderId = (long) arg;
	int strLength;
	char randomMessage[MAX_MESSAGE_LENGTH];
	while (1)
	{
		pthread_mutex_lock(&msgIdMutex);
		msg.msgId = msgId;
		msgId++;
		pthread_mutex_unlock(&msgIdMutex);
		
		msg.srcId = senderId;
		msg.dstId = randomNoGenerator(1,3);
		msg.queingTime=0;
		msg.lastInsertTime=0;
		strLength = randomNoGenerator(1,79);
		for(i=0; i<strLength; ++i)
		{
			msg.message[i] = (char)randomNoGenerator(48,127);
		}
		for(;i<MAX_MESSAGE_LENGTH; ++i)
		{
			msg.message[i] = '\0';
		}
		
		
		while ((res<0) && (exiting==0))
		{
			pthread_mutex_lock (&queueMutex);
			res = write(fd[BUS_IN_QUEUE], &msg, sizeof(Message));
			if(res >= 0)
				numMsgsSent++;
			pthread_mutex_unlock (&queueMutex);
			usleep(randomNoGenerator(1,10)*10000);
		}
		if(res>=0)
		{
			printf("Msg Sent by %d!!! MsgId=%d, Source=%d, Dest=%d\n",
						(int) senderId, msg.msgId, msg.srcId, msg.dstId);
		}
		res=-1;
		if(exiting == 1)
		{
			break;
		}
	}
	printf("Sender Exiting....%d\n", (int)senderId);
	pthread_exit(NULL);
}

//Thread to receive messages
void *receiverThread (void *arg)
{
	int i;
	Message msg;
	int res=-1;
	long receiverId = (long) arg;
	while (1)
	{
		while((res<0) && (exiting==0))
		{
			pthread_mutex_lock (&queueMutex);
			res = read(fd[receiverId], &msg, sizeof(Message));
			if(res >=0)
				numMsgsReceived++;
			pthread_mutex_unlock (&queueMutex);
			usleep(randomNoGenerator(1,10)*1000);
		}
		if(res>=0)
		{
			printf("Msg Received by %d!!! MsgId=%d, Source=%d, Dest=%d QTime=%ld\n",
						(int) receiverId, msg.msgId, msg.srcId, msg.dstId, msg.queingTime);
		}
		res=-1;
		
		if(exiting == 1)
		{
			break;
		}
		
	}
		
	printf("Receiver Exiting....%d\n", (int)receiverId);
	pthread_exit(NULL);
}

//Reading from bus_in_q and write to bus_out_q
void *busDaemonThread (void *arg)
{
	Message msg;
	int i;
	int res1=-1;
	int res2=-1;
	while (1)
	{
		while ((res1<0) && (exiting==0))
		{
			pthread_mutex_lock (&queueMutex);
			res1 = read(fd[BUS_IN_QUEUE], &msg, sizeof(Message));
			pthread_mutex_unlock (&queueMutex);
			if(res1 <0 )
				usleep(randomNoGenerator(1,10)*1000);
		}	
		res1=-1;
		while ((res2<0) && (exiting==0))
		{
			pthread_mutex_lock(&queueMutex);
			res2 = write(fd[msg.dstId], &msg, sizeof(Message));
			pthread_mutex_unlock (&queueMutex);
			if(res2 <0 )
				usleep(randomNoGenerator(1,10)*1000);
		}
		res2=-1;
		if(exiting == 1)
		{
			break;
		}
	}
	printf("Daemon Exiting....\n");
	pthread_exit(NULL);
}

int main()
{
	long i;
	
	pthread_mutex_init(&queueMutex, NULL);
	pthread_mutex_init(&msgIdMutex, NULL);
	pthread_mutex_init(&sendMutex, NULL);
	pthread_mutex_init(&receiveMutex, NULL);
	
	fd[0] = open("/dev/bus_in_q", O_RDWR);
	fd[1] = open("/dev/bus_out_q1", O_RDWR);
	fd[2] = open("/dev/bus_out_q2", O_RDWR);
	fd[3] = open("/dev/bus_out_q3", O_RDWR);
	for(i=0; i<NUM_DEVICES; ++i)
	{
		if (fd[i] < 0 )
		{
			printf("Can not open device file. %d\n", fd[i]);		
			return -1;
		}
	}
	
	printf("Creating sender Threads....\n");
	for(i=0; i<3;++i)
	{
		pthread_create ( &senders[i], NULL, senderThread, (void *)i+1);
	}
	
	printf("Creating BusDaemon Thread....\n");
	pthread_create ( &busDaemon, NULL, busDaemonThread, NULL);
	
	printf("Creating receiver Threads....\n");
	for(i=0; i<3;++i)
	{
		pthread_create ( &receivers[i], NULL, receiverThread, (void *)i+1);
	}
	
	usleep(10000000);
	exiting = 1;
	for(i=0; i<3;++i)
	{
		pthread_join(senders[i], NULL);
		pthread_join(receivers[i], NULL);
	}
	pthread_join(busDaemon, NULL);
	
	printf("All Threads Exited.....\n");
	usleep(1000000);
	
	for(i=0; i<NUM_DEVICES; ++i)
	{
		close(fd[i]);
	}
	
	pthread_mutex_destroy(&queueMutex);
	pthread_mutex_destroy(&sendMutex);
	pthread_mutex_destroy(&receiveMutex);
	pthread_mutex_destroy(&msgIdMutex);
	
	printf ("Exiting... \n");
	return 0;
}

