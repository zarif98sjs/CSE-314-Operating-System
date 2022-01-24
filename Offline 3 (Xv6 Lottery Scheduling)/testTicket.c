#include "types.h"
#include "stat.h"
#include "user.h"


int main(int argc, char * argv[])
{
    printf(1, "test_Ticket\n");
    
    int number = atoi(argv[1]);
    settickets(number);

    while (1)
    {
        /* code */
    }
    
    exit();//eq to return zero

    /**
     * 
     * fork test
     * 
     **/


    // printf(1, "testFork\n");
    
    // int number = atoi(argv[1]);
    // settickets(number);

    // int val = fork();

    // if(val == 0) printf(1,"\nFork successful\n");
    // else if (val < 0) printf(1,"\nFork unsuccessful\n");

    // while (1)
    // {
    //     /* code */
    // }
    
    // exit();//eq to return zero

}