#include<bits/stdc++.h>
#include<pthread.h>
#include<semaphore.h>
#include <unistd.h>
using namespace std;

#define READ        freopen("in.txt", "r", stdin)
#define WRITE       freopen("out.txt", "w", stdout)

#define MAX_PASSENGER 5
#define endl "\n"

int kiosks,belts,passengers_per_belt;
int checkin_at_kiosk_w, security_x, boarding_y , vip_walk_z;
pthread_t passenger_threads[MAX_PASSENGER];

sem_t kiosk_capacity_sem;
vector<sem_t>belt_capacity_sem;
pthread_mutex_t boarding_mutex;

time_t start_time, record_time;

int random(int min, int max) //range : [min, max]
{
   return min + (rand() % static_cast<int>(max - min + 1));
}

struct Passenger{
	int pid;
	bool isVIP;
};

int get_current_time()
{
	// Recording current time.
    time(&record_time);
  
    // Calculating total time taken by the program.
    double time_taken = double(record_time - start_time);
	return (int) time_taken;
}


void boarding_on_plane(Passenger &p)
{
	int cur_time = get_current_time();
	printf("Passenger %d has started waiting to be boarded at time %d\n",p.pid,cur_time);
	// cout<<"Passenger "<<p.pid<<" : has started waiting to be boarded at time "<<cur_time<<endl;

	pthread_mutex_lock(&boarding_mutex);

	cur_time = get_current_time();
	printf("Passenger %d has started boarding the plane at time %d\n",p.pid,cur_time);
	// cout<<"Passenger "<<p.pid<<" : has started boarding the plane at time "<<cur_time<<endl;
	
	sleep(boarding_y);
	
	cur_time = get_current_time();
	printf("Passenger %d has boarded the plane at time %d\n",p.pid,cur_time);
	// cout<<"Passenger "<<p.pid<<" : has boarded the plane at time "<<cur_time<<endl;
	
	pthread_mutex_unlock(&boarding_mutex);
}


void security_check(Passenger &p)
{
	int belt_id = random(0,belts-1);
	int cur_time = get_current_time();
	printf("Passenger %d has started waiting for security check in belt %d from time %d\n",p.pid,belt_id,cur_time);
	// cout<<"Passenger "<<p.pid<<" : has started waiting for security check in belt " <<belt_id<< " from time "<<cur_time<<endl;

	sem_wait(&belt_capacity_sem[belt_id]); //down

	cur_time = get_current_time();
	printf("Passenger %d has started the security check at time %d\n",p.pid,cur_time);
	// cout<<"Passenger "<<p.pid<<" : has started the security check at time "<<cur_time<<endl;
	
	sleep(security_x);

	cur_time = get_current_time();
	printf("Passenger %d crossed the security check at time %d\n",p.pid,cur_time);
	// cout<<"Passenger "<<p.pid<<" : has crossed the security check at time "<<cur_time<<endl;

	sem_post(&belt_capacity_sem[belt_id]); //up
}

void checkin_at_kiosk(Passenger &p)
{
	sem_wait(&kiosk_capacity_sem); //down

	int cur_time = get_current_time();
	printf("Passenger %d has started self-check in at kiosk ... at time %d\n",p.pid,cur_time);
	// cout<<"Passenger "<<p.pid<<" : has started self-check in at kiosk ... at time "<<cur_time<<endl;
	
	sleep(checkin_at_kiosk_w);
	
	cur_time = get_current_time();
	printf("Passenger %d has finished check in at time %d\n",p.pid,cur_time);
	// cout<<"Passenger "<<p.pid<<" : has finished check in at time "<<cur_time<<endl;
	
	sem_post(&kiosk_capacity_sem); // up	
}

void * arrival(void* p)
{
	struct Passenger *passenger = (struct Passenger*)p;
	int cur_time = get_current_time();
	printf("Passenger %d has arrived at time %d\n",(*passenger).pid,cur_time);
	// cout<<"Passenger "<<(*passenger).pid<<" has arrived at time "<<get_current_time()<<endl;

	// checkin_at_kiosk(*passenger);
	security_check(*passenger);
	boarding_on_plane(*passenger);
}

bool give_VIP_status()
{
	return rand() % 2;
}


int main(void)
{	
	srand ( time(NULL) );

	READ;
	// WRITE;

	/*************** input **************/

    cin>>kiosks>>belts>>passengers_per_belt;
	cin>>checkin_at_kiosk_w>>security_x>>boarding_y>>vip_walk_z;

	cout<<kiosks<<endl;
	cout<<belts<<endl;
	cout<<passengers_per_belt<<endl;

	cout<<checkin_at_kiosk_w<<endl;
	cout<<security_x<<endl;
	cout<<boarding_y<<endl;
	cout<<vip_walk_z<<endl;

	/*************** time **************/
	time(&start_time);
	

	sem_init(&kiosk_capacity_sem, 0, kiosks);

	belt_capacity_sem = vector<sem_t>(belts);
	for(int i=0;i<belts;i++) sem_init(&belt_capacity_sem[i], 0, passengers_per_belt);

	struct Passenger *passenger;

	for(int i=0;i<MAX_PASSENGER;i++)
	{
		// passenger = malloc(sizeof(struct Passenger));
		passenger = new Passenger();
		(*passenger).pid = i+1;
		(*passenger).isVIP = give_VIP_status();

		pthread_create(&passenger_threads[i],NULL,arrival,(void*) passenger);
	}

	for(int i=0;i<MAX_PASSENGER;i++)
	{
		pthread_join(passenger_threads[i],NULL);
	}

	return 0;
}
