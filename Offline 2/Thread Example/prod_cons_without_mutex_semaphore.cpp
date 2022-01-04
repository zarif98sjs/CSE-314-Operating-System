#include<cstdio>
#include<pthread.h>
#include<semaphore.h>
#include<queue>
#include <unistd.h>

using namespace std;


//semaphore to control sleep and wake up
sem_t empty;
sem_t full;
queue<int> q;



void * ProducerFunc(void * arg)
{	
	printf("%s\n",(char*)arg);
	int i;
	for(i=1;i<=10;i++)
	{
		
			
		sleep(1);
		
		q.push(i);
		printf("producer produced item %d\n",i);
		
		
	
		
	}
}

void * ConsumerFunc(void * arg)
{
	printf("%s\n",(char*)arg);
	int i;
	for(i=1;i<=10;i++)
	{	
		
 		
		sleep(1);
		

		int item = q.front();
		q.pop();
		printf("consumer consumed item %d\n",item);	

			
		
	}
}





int main(void)
{	
	pthread_t thread1;
	pthread_t thread2;
	
	//init_semaphore();
	
	char * message1 = "i am producer";
	char * message2 = "i am consumer";	
	
	pthread_create(&thread1,NULL,ProducerFunc,(void*)message1 );
	pthread_create(&thread2,NULL,ConsumerFunc,(void*)message2 );

	while(1);
	return 0;
}
