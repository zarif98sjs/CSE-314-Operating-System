#include<bits/stdc++.h>
using namespace std;

default_random_engine generator;
poisson_distribution<int>distribution(4.1);

int main()
{
    int n;
    cin>>n;

    while(n--)
    {
        int number = distribution(generator);
        cout<<number<<endl;
    }
    
    return 0;
}