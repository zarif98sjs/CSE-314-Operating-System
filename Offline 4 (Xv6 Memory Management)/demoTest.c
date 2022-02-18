#include "types.h"
#include "stat.h"
#include "user.h"

/**
 * @brief TEST
 * 
 * 1. properly reallocate page in disk and free memory : ok
 * 2. In FIFO, check if code and stack segments are brought back just after reallocatiion finishes : ok
 * 3. In AGING, check if code and stack segments are never swapped out : ok
 * 4. raw FIFO will have more page fault than AGING : ok
 * 5. implement LIFO to double check that FIFO gets more page fault : ok -> extra
 * 6. compare LIFO and AGING , AGING should perform better in the long run as it never swaps code and stack pages : 
 * 7. access every page once, write to that, reaccess that page to check if it can find the same : ok 
 * 8. fork and write differently to child and parent
 */

// check fork
int sanityCheck4()
{
    printf(1,"//////////////////////////////////////\n");
    printf(1,"//////////// Test Case 4 /////////////\n");
    printf(1,"//////////////////////////////////////\n");

    int numPage = 17;
    char * arr = malloc (numPage<<12);
    arr[0] = 'Z';
    arr[1] = 'A';
    arr[2] = 'R';
    arr[3] = 'I';
    arr[4] = 'F';
    arr[5] = 0;

    if (fork() == 0){ // fork successful , child process
    
        arr[5] = 'U';
        arr[6] = 'L';
        arr[7] = 0;
        printf(1, "Child Process : %s\n",&arr[0]); // should print ZARIFUL
        free(arr);

    }
    else{
        wait();
        arr[5] = 'A';
        arr[6] = 'L';
        arr[7] = 'A';
        arr[8] = 'M';
        arr[9] = 0;
        printf(1, "Parent Process : %s\n",&arr[0]); // should print ZARIFALAM
        free(arr);
    }
    
    // while (1)
    // {
    //     /* code */
    // }

    return 1;
}

int sanityCheck3()
{
    printf(1,"//////////////////////////////////////\n");
    printf(1,"//////////// Test Case 3 /////////////\n");
    printf(1,"//////////////////////////////////////\n");

    int numPage = 17;
    char * arr = malloc (numPage<<12);
    arr[0] = 'Z';
    arr[1] = 'A';
    arr[2] = 'R';
    arr[3] = 'I';
    arr[4] = 'F';
    arr[5] = 0;

    // FIFO : should have one extra page fault except of course for code and stack , gave more page number (17) to force the first page eviction
    // Aging : can only have page fault for first page , depends on page size
    // LIFO : should NOT have page fault
    printf(1, "<<<<< %s >>>>> \n",&arr[0]); 
    free(arr);

    return 1;
}

int sanityCheck2()
{
    printf(1,"//////////////////////////////////////\n");
    printf(1,"//////////// Test Case 2 /////////////\n");
    printf(1,"//////////////////////////////////////\n");

    int numPage = 13;
    char * arr = malloc (numPage<<12);
    int lastByte = (numPage<<12) - 1;
    arr[lastByte-5] = 'Z';
    arr[lastByte-4] = 'A';
    arr[lastByte-3] = 'R';
    arr[lastByte-2] = 'I';
    arr[lastByte-1] = 'F';
    arr[lastByte] = 0;
    printf(1, "<<<<< %s >>>>> \n",&arr[lastByte-5]); // in all cases, should print ZARIF without any page fault after mem alloc 
    free(arr);
    return 1;
}

/**
 * @brief strong test
 * check every possible byte
 */
int sanityCheck1()
{

    printf(1,"//////////////////////////////////////\n");
    printf(1,"//////////// Test Case 1 /////////////\n");
    printf(1,"//////////////////////////////////////\n");

    int numPage = 17;
    int* ara = (int*)malloc(numPage<<12);
    printf(1,"Numpage = %d \n",numPage);

    int numInt = (numPage<<12)/4;
    for(int i = 0 , m = 0 ;i < numInt ; i++ , m += 1)
    {
        ara[m] = i;
    }

    int diff = 0;
    int ok_count = 0;

    for(int i = 0 , m = 0 ;i < numInt ; i++ , m += 1)
    {
        if(ara[m]!=i) diff++;
        else ok_count++;
    }

    free((void*)ara);

    if(!diff)
    {
        printf(1,"SANITY CHECK 1 PASEED\n");
        printf(1,"num of int %d , ok %d\n",numInt,ok_count);
        return 1;
    }
    else
    {
        printf(1,"WA ON TC 1\n");
        printf(1,"num of int %d , not ok %d\n",numInt,diff);
        return 0;
    }
}

/**
 * @brief easy test
 */
int sanityCheck0()
{

    printf(1,"//////////////////////////////////////\n");
    printf(1,"//////////// Test Case 0 /////////////\n");
    printf(1,"//////////////////////////////////////\n");

    int numPage = 17;
    int* ara = (int*)malloc(numPage<<12);
    printf(1,"Numpage = %d \n",numPage);

    for(int i = 0 , m = 0 ;i < numPage ; i++ , m += 1024)
    {
        printf(1,"Accessing page %d\n",i);
        ara[m] = i;
    }

    int diff = 0;

    for(int i = 0 , m = 0 ;i < numPage ; i++ , m += 1024)
    {
        printf(1,"Accessing page %d\n",i);
        // printf(1,"Number got %d\n",ara[m]);
        if(ara[m]!=i) diff++;
    }

    free((void*)ara);
    

    if(!diff)
    {
        printf(1,"SANITY CHECK 0 PASEED\n");
        return 1;
    }
    else
    {
        printf(1,"WA ON TC 0\n");
        return 0;
    }
}

int stressTest()
{

    printf(1,"//////////////////////////////////////\n");
    printf(1,"//////////// Stress Test /////////////\n");
    printf(1,"//////////////////////////////////////\n");

    int numPage = 24;
    int* ara = (int*)malloc(numPage<<12);
    printf(1,"Numpage = %d \n",numPage);

    for(int i = 0 , m = 0 ;i < numPage ; i++ , m += 1024)
    {
        printf(1,"Accessing page %d\n",i);
        ara[m] = i;
    }

    int diff = 0;

    for(int i = 0 , m = 0 ;i < numPage ; i++ , m += 1024)
    {
        printf(1,"Accessing page %d\n",i);
        // printf(1,"Number got %d\n",ara[m]);
        if(ara[m]!=i) diff++;
    }

    free((void*)ara);
    

    if(!diff)
    {
        printf(1,"SANITY CHECK 0 PASEED\n");
        return 1;
    }
    else
    {
        printf(1,"WA ON TC 0\n");
        return 0;
    }
}

void optimizationTest()
{
    printf(1,"////////////////////////////////////////////\n");
    printf(1,"//////////// Optimization Test /////////////\n");
    printf(1,"////////////////////////////////////////////\n");

    int numPage = 17;
    int* ara = (int*)malloc(numPage<<12);
    printf(1,"Numpage = %d \n",numPage);

    int repeat = 20;
    while(repeat--)
    {
        for(int i = 0 , m = 0 ;i < numPage ; i++ , m += 1024)
        {
            // printf(1,"Accessing page %d\n",i);
            ara[m] = i;
        }
    }    
    
    free((void*)ara);
    return;
} 

/**
 * @brief TESTING STATS
 * 
 * 
- sanitycheck 0
    - FIFO : 51
    - Aging : 21
    - Optimized FIFO : 43
- sanitycheck 1
    - FIFO : 52
    - Aging : 24
    - Optimized Fifo : 45
- sanitycheck 2
    - FIFO : 8
    - Aging : 7
    - Optimized FIFO : 5
- sanitycheck 3
    - FIFO : 12 (3 for code and stack , 1 for later retireval)
    - Aging : 9 (will be 3 less, as expected)
- sanitycheck 4
    - FIFO
        - parent : 12
        - child : 0
    - Aging
        - parent : 9
        - child : 0
 *
 * 
 */

int main(int argc, char *argv[])
{

    

    int ok = 0;
    ok += sanityCheck0();
    ok += sanityCheck1();

    // sanityCheck2();
    // sanityCheck3();
    // sanityCheck4();

    if(ok == 2)
    {
        printf(1,"/////////////////////////////////////////////////\n");
        printf(1,"//////////// All Test Case Passed ///////////////\n");
        printf(1,"///////////////////    AC    ////////////////////\n");
        printf(1,"/////////////////////////////////////////////////\n");
    }

    // stressTest();
    // optimizationTest();

    exit();
}

