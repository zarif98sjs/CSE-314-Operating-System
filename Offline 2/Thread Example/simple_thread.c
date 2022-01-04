#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>


void * threadFunc1(void * arg)
{
	int i;
	for(i=1;i<=5;i++)
	{
		printf("%s\n",(char*)arg);
		// sleep(1);
	}
}

void * threadFunc2(void * arg)
{
	int i;
	for(i=1;i<=5;i++)
	{
		printf("%s\n",(char*)arg);
		// sleep(1);
	}
}

int main(void)
{	
	pthread_t thread1;
	pthread_t thread2;
	
	char * message1 = "i am thread 1";
	char * message2 = "i am thread 2";	
	
	pthread_create(&thread1,NULL,threadFunc1,(void*)message1 );
	pthread_create(&thread2,NULL,threadFunc2,(void*)message2 );

	pthread_join(thread1,NULL);
	pthread_join(thread2,NULL);
	// exit(0);

	// while(1);
	return 0;
}
