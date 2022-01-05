#include<bits/stdc++.h>
using namespace std;

default_random_engine generator;
poisson_distribution<int>distribution(1.0/20);

double nextTime(float rateParameter)
{
	double ret = -logf(1.0f - (double) random() / ((double)RAND_MAX + 1)) / rateParameter;
    return  ret;
}

int main()
{

    int nroll = 10000;
    double sum = 0;
    for(int i=0;i<nroll;i++){
        sum += nextTime(1.0/3);
    }

    cout<<sum/nroll<<endl;

    // int nrolls = 10000;
    // int nstar = 60;

    // int p[10] = {};

    // for(int i=0;i<nrolls;i++)
    // {
    //     int num = distribution(generator);
    //     if(num < 10) ++p[num];
    // }

    // vector<int>v;

    // for(int i=0;i<10;i++)
    // {
    //     // cout<<i<<": "<<string(p[i]*nstar/nrolls,'*')<<endl; 
    //     v.push_back(p[i]*nstar/nrolls);
    //     // cout<<i<<": "<<p[i]*nstar/nrolls<<endl;    
    // }

    // for(int i=0;i<10;i++)
    // {
    //     // cout<<i<<": "<<string(p[i]*nstar/nrolls,'*')<<endl; 
    //     if(i>0) v[i] += v[i-1];
    //     if(i>0) cout<<i<<": "<<v[i]<<" --> diff : "<<v[i]-v[i-1]<<endl;    
    // }

    // int n;
    // cin>>n;

    // while(n--)
    // {
    //     int number = distribution(generator);
    //     cout<<number<<endl;
    // }
    
    return 0;
}