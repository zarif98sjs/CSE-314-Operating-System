#include<stdio.h>
#include<pthread.h>
#include<semaphore.h>

sem_t bin_sem;
pthread_mutex_t mtx;
//char message[100];



void * thread_function(void * arg)
{	
	int x;
	char message2[10];
	while(1)
	{	
		//pthread_mutex_lock(&mtx);		
		printf("thread2:waiting..\n");
		sem_wait(&bin_sem);		
		printf("hi i am the new thread waiting inside critical..\n");
		scanf("%s",message2);
		printf("You entered in Thread2:%s\n",message2);
		sem_post(&bin_sem);
		//pthread_mutex_unlock(&mtx); 
	
	}
	
}

int main(void)
{
	pthread_t athread;
	pthread_attr_t ta;
	char message[10];
	int x;
	sem_init(&bin_sem,0,1);
	pthread_mutex_init(&mtx,NULL);
	
	pthread_attr_init(&ta);
	pthread_attr_setschedpolicy(&ta,SCHED_RR);	                                                                                                                                                                                                     

	pthread_create(&athread,&ta,thread_function,NULL);
	while(1)
	{	
		//pthread_mutex_lock(&mtx);
		printf("main waiting..\n");
		sem_wait(&bin_sem);	
		printf("hi i am the main thread waiting inside critical..\n");
		scanf("%s",message);
		printf("You entered in Main:%s\n",message);
		sem_post(&bin_sem);
		//pthread_mutex_unlock(&mtx); 
	}
	sleep(5);		
}
