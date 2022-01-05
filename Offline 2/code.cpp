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

int L_to_R = 0;
int R_to_L = 0;

bool VIP_lane_lock = false;

/*************** time **************/

time_t start_time, record_time;

/*************** pthread **************/
pthread_t passenger_threads[MAX_PASSENGER];
pthread_t conditional_thread;

/*************** mutex **************/
pthread_mutex_t boarding_mutex;
pthread_mutex_t special_kiosk_mutex;

/*************** semaphore **************/
sem_t kiosk_capacity_sem;
vector<sem_t>belt_capacity_sem;

/*************** conditional (to block R->L) **************/

pthread_mutex_t L_to_R_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t R_to_L_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  condition_cond  = PTHREAD_COND_INITIALIZER;

/*************** conditional (to block L->R) **************/
pthread_mutex_t VIP_lane_lock_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t VIP_lane_lock_condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  VIP_lane_lock_condition_cond  = PTHREAD_COND_INITIALIZER;

/*************** code **************/

template <class T>
string to_str(T x)
{
    stringstream ss;
    ss<<x;
    return ss.str();
}

struct Passenger{
	int pid;
	bool isVIP;

	string getPassengerId()
	{
		if(isVIP) return to_str(pid) + " (VIP)";
		else return to_str(pid);
	}
};

int get_current_time()
{
	// Recording current time.
    time(&record_time);
  
    // Calculating total time taken by the program.
    double time_taken = double(record_time - start_time);
	return (int) time_taken;
}

int random(int min, int max) //range : [min, max]
{
   return min + (rand() % static_cast<int>(max - min + 1));
}

bool coin_toss()
{
	return rand() % 2;
}

void recheckin_at_special_kiosk(Passenger &p)
{
	/*************** hop off VIP lane and unlock it **************/

	pthread_mutex_lock(&VIP_lane_lock_mutex);
	VIP_lane_lock = false;
	pthread_mutex_unlock(&VIP_lane_lock_mutex);

	pthread_mutex_lock(&R_to_L_mutex);
	R_to_L--;
	int cur_time = get_current_time();
	printf("Passenger %s has started waiting at [SPECIAL] kiosk ... at time %d\n",p.getPassengerId().c_str(),cur_time);
	pthread_mutex_unlock(&R_to_L_mutex);

	/*************** check-in at special kiosk **************/

	pthread_mutex_lock(&special_kiosk_mutex);

	cur_time = get_current_time();
	printf("Passenger %s has started self-check in at [SPECIAL] kiosk ... at time %d\n",p.getPassengerId().c_str(),cur_time);
	
	sleep(checkin_at_kiosk_w);
	
	cur_time = get_current_time();
	printf("Passenger %s has finished check in at [SPECIAL] kiosk at time %d\n",p.getPassengerId().c_str(),cur_time);

	L_to_R++; // everyone will come back to boarding through VIP channel

	pthread_mutex_unlock(&special_kiosk_mutex);
}


void go_back(Passenger &p)
{
	/*************** put in R->L queue **************/

	pthread_mutex_lock(&R_to_L_mutex);
	R_to_L++;
	printf("[GO BACK] Passenger %s\n",p.getPassengerId().c_str());
	pthread_mutex_unlock(&R_to_L_mutex);

	printf("R to L : %d ... Passenger %s\n",R_to_L,p.getPassengerId().c_str());

	/*************** wait until there is a signal that no (L->R) person is on the opposite side **************/

	pthread_mutex_lock( &condition_mutex );
	while( L_to_R > 0 )
	{
		pthread_cond_wait( &condition_cond, &condition_mutex );
	}
	pthread_mutex_unlock( &condition_mutex );

	/*************** hop on VIP lane and lock it **************/

	pthread_mutex_lock(&VIP_lane_lock_mutex);
	VIP_lane_lock = true;
	pthread_mutex_unlock(&VIP_lane_lock_mutex);

	int cur_time = get_current_time();
	printf("Passenger %s has started walking through VIP channel at time %d ... [R to L] |||| L to R : %d \n",p.getPassengerId().c_str(),cur_time,L_to_R);
	// sleep(30); // VIP WALK : R->L
	sleep(vip_walk_z); // VIP WALK : R->L

	/*************** re check-in **************/

	recheckin_at_special_kiosk(p);

	/*************** wait until there is a signal that no (R->L) person is on the opposite side **************/

	pthread_mutex_lock( &VIP_lane_lock_condition_mutex);
	while( VIP_lane_lock )
	{
		pthread_cond_wait( &VIP_lane_lock_condition_cond, &VIP_lane_lock_condition_mutex );
	}
	pthread_mutex_unlock( &VIP_lane_lock_condition_mutex );

	/*************** hop on VIP lane **************/

	cur_time = get_current_time();
	printf("Passenger %s has started walking through VIP channel at time %d ... [L to R] |||| R to L : %d \n",p.getPassengerId().c_str(),cur_time,R_to_L);
	sleep(vip_walk_z); // VIP WALK : L->R
}

void boarding_on_plane(Passenger &p)
{
	pthread_mutex_lock(&L_to_R_mutex);
	if(p.isVIP) L_to_R--;

	int cur_time = get_current_time();
	
	printf("Passenger %s has started waiting to be boarded at time %d\n",p.getPassengerId().c_str(),cur_time);
	pthread_mutex_unlock(&L_to_R_mutex);

	if(coin_toss()) 
	{
		go_back(p); // has lost boarding pass

		pthread_mutex_lock(&L_to_R_mutex);
		L_to_R--;
		printf("[DEC L_to_R] Passenger %s (VIP) has started waiting to be boarded AGAIN at time %d\n",p.getPassengerId().c_str(),cur_time);
		pthread_mutex_unlock(&L_to_R_mutex);
	}
	
	pthread_mutex_lock(&boarding_mutex);

	cur_time = get_current_time();
	
	printf("Passenger %s has started boarding the plane at time %d\n",p.getPassengerId().c_str(),cur_time);
	
	sleep(boarding_y);
	
	cur_time = get_current_time();
	
	printf("Passenger %s has boarded the plane at time %d ---------------------------------------- DONE\n",p.getPassengerId().c_str(),cur_time);
	
	pthread_mutex_unlock(&boarding_mutex);
}


void security_check(Passenger &p)
{
	int belt_id = random(0,belts-1);
	int cur_time = get_current_time();
	printf("Passenger %s has started waiting for security check in belt %d from time %d\n",p.getPassengerId().c_str(),belt_id,cur_time);

	sem_wait(&belt_capacity_sem[belt_id]); //down

	cur_time = get_current_time();
	printf("Passenger %s has started the security check at time %d\n",p.getPassengerId().c_str(),cur_time);
	
	sleep(security_x);

	cur_time = get_current_time();
	printf("Passenger %s crossed the security check at time %d\n",p.getPassengerId().c_str(),cur_time);

	sem_post(&belt_capacity_sem[belt_id]); //up
}

void checkin_at_kiosk(Passenger &p)
{
	sem_wait(&kiosk_capacity_sem); //down

	int cur_time = get_current_time();
	printf("Passenger %s has started self-check in at kiosk ... at time %d\n",p.getPassengerId().c_str(),cur_time);
	
	sleep(checkin_at_kiosk_w);
	
	cur_time = get_current_time();
	printf("Passenger %s has finished check in at time %d\n",p.getPassengerId().c_str(),cur_time);
	
	if(p.isVIP) L_to_R++; // only VIP will go through the VIP channel
	
	sem_post(&kiosk_capacity_sem); // up	
}



void * arrival(void* p)
{
	struct Passenger *passenger = (struct Passenger*)p;
	int cur_time = get_current_time();

	printf("Passenger %s has arrived at time %d\n",(*passenger).getPassengerId().c_str(),cur_time);

	checkin_at_kiosk(*passenger);
	if(!(*passenger).isVIP) security_check(*passenger);

	if(!(*passenger).isVIP) boarding_on_plane(*passenger);
	else{

		// CONSIDER PROBLEM WITH VIP LANE

		printf("L to R : %d ... Passenger %s\n",L_to_R,(*passenger).getPassengerId().c_str());

		pthread_mutex_lock( &VIP_lane_lock_condition_mutex);
		while( VIP_lane_lock )
		{
			pthread_cond_wait( &VIP_lane_lock_condition_cond, &VIP_lane_lock_condition_mutex );
		}
		pthread_mutex_unlock( &VIP_lane_lock_condition_mutex );

		cur_time = get_current_time();
		printf("Passenger %s has started walking through VIP channel at time %d ... [L to R] |||| R to L : %d \n",(*passenger).getPassengerId().c_str(),cur_time,R_to_L);
		sleep(vip_walk_z); // VIP WALK : L->R

		boarding_on_plane(*passenger); // starts boarding 
	}	
}

void *broadcast_function(void* arg)
{
	while (true)
	{
		// CONDITIONAL SIGNAL HERE
		pthread_mutex_lock( &condition_mutex );
		if( L_to_R == 0 )
		{	
			// printf("Broadcasting....");
			pthread_cond_broadcast( &condition_cond );
		}
		pthread_mutex_unlock( &condition_mutex );

		// CONDITIONAL SIGNAL HERE
		pthread_mutex_lock( &VIP_lane_lock_condition_mutex );
		if( !VIP_lane_lock )
		{	
			// printf("Broadcasting....");
			pthread_cond_broadcast( &VIP_lane_lock_condition_cond );
		}
		pthread_mutex_unlock( &VIP_lane_lock_condition_mutex );
	}
	
}


int main(void)
{	
	srand ( time(NULL) );
	// srand ( 15 );

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
	
	/*************** mutex init **************/
	
	pthread_mutex_init(&boarding_mutex, NULL);
	pthread_mutex_init(&special_kiosk_mutex, NULL);

	/*************** semaphore init **************/

	sem_init(&kiosk_capacity_sem, 0, kiosks);

	belt_capacity_sem = vector<sem_t>(belts);
	for(int i=0;i<belts;i++) sem_init(&belt_capacity_sem[i], 0, passengers_per_belt);

	/*************** passenger generation **************/

	struct Passenger *passenger;

	for(int i=0;i<MAX_PASSENGER;i++)
	{
		passenger = new Passenger();
		(*passenger).pid = i+1;
		(*passenger).isVIP = coin_toss();

		pthread_create(&passenger_threads[i],NULL,arrival,(void*) passenger);
	}

	pthread_create(&conditional_thread,NULL,broadcast_function,NULL);

	for(int i=0;i<MAX_PASSENGER;i++)
	{
		pthread_join(passenger_threads[i],NULL);
	}

	return 0;
}
