# **`Xv6-OS-Process-Scheduler-Lottery-Scheduling`**
 
# **Problem**

<details>
<summary>  
<b>Details</b>
</summary>
In this offline, you'll be putting a new scheduler into xv6. It is called a lottery scheduler, and the full version is described in this chapter of the online book; you'll be building a simpler one. The basic idea is simple: assign each running process a slice of the processor based on the number of tickets it has; the more tickets a process has, the more it runs. Each time slice, a randomized lottery determines the winner of the lottery; that winning process is the one that runs for that time slice.


You'll need two new system calls to implement this scheduler. The first is `int settickets(int number)`, which sets the number of tickets of the calling process. By default, each process should get one ticket; calling this routine makes it such that a process can raise the number of tickets it receives, and thus receive a higher proportion of CPU cycles. This routine should return 0 if successful, and -1 otherwise (if, for example, the caller passes in a number less than one).

The second is `int getpinfo(struct pstat *)`. This routine returns some information about all running processes, including how many times each has been chosen to run and the process ID of each. You can use this system call to build a variant of the command line program ps, which can then be called to see what is going on. The structure pstat is defined below; note, you cannot change this structure and must use it exactly as-is. This routine should return 0 if successful, and -1 otherwise (if, for example, a bad or NULL pointer is passed into the kernel).

Most of the code for the scheduler is quite localized and can be found in proc.c; the associated header file, proc.h is also quite useful to examine. To change the scheduler, not much needs to be done; study its control flow and then try some small changes.

You'll need to assign tickets to a process when it is created. Specifically, you'll need to make sure a child process inherits the same number of tickets as its parents. Thus, if the parent has 10 tickets, and calls `fork()` to create a child process, the child should also get 10 tickets.

You'll also need to figure out how to generate random numbers in the kernel; some searching should lead you to a simple pseudo-random number generator, which you can then include in the kernel and use as appropriate.

Finally, you'll need to understand how to fill in the structure pstat in the kernel and pass the results to userspace. The structure should look like what you see here, in a file you'll have to include called `pstat.h`:
```cpp
#ifndef _PSTAT_H_

#define _PSTAT_H_

#include "param.h"

struct pstat {

  int inuse[NPROC];   // whether this slot of the process table is in use (1 or 0)

  int tickets[NPROC]; // the number of tickets this process has

  int pid[NPROC];     // the PID of each process

  int ticks[NPROC];   // the number of ticks each process has accumulated

};

#endif // _PSTAT_H_
```
Good examples of how to pass arguments into the kernel are found in existing system calls. In particular, follow the path of `read()`, which will lead you to `sys_read()`, which will show you how to use `argptr()` (and related calls) to obtain a pointer that has been passed into the kernel. Note how careful the kernel is with pointers passed from userspace -- they are a security threat(!), and thus must be checked very carefully before usage.

**For this particular offline set CPUS =1 in the makefile of xv6.**
</details>

&nbsp;

# **Solution**

- `settickets`
    ```cpp
    int settickets(int number)
    {
        struct proc* pr = myproc();
        int pid = pr->pid;

        acquire(&ptable.lock);

        // Find and assign the tickets to the process
        struct proc* p;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->pid == pid) {
                p->ticket = number;
                release(&ptable.lock);
                return 0;
            }
        }

        release(&ptable.lock);

        return 0;
    }
    ```

- `getpinfo`

    ```cpp
    int getpinfo(struct pstat* ps)
    {

        acquire(&ptable.lock);

        struct proc *p;
        int i=0;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            ps->pid[i] = p->pid;
            ps->inuse[i] = p->state != UNUSED;
            ps->ticket[i] = p->ticket;
            ps->tick[i] = p->tick;
            i++;
        }

        release(&ptable.lock);

        return 0;
    }
    ```


- `scheduler`

    ```cpp
    int getRunnableProcTickets(void)
    {
        struct proc *p;
        int total = 0;

        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        {
            if (p->state == RUNNABLE)
            {
            total += p->ticket;
            }
        }
        return total;
    }

    void scheduler(void)
    {
        struct proc *p;
        struct cpu *c = mycpu();
        c->proc = 0;

        for (;;)
        {
            // Enable interrupts on this processor.
            sti();

            long cur_total = 0;

            // Loop over process table looking for process to run.
            acquire(&ptable.lock);
            long total = getRunnableProcTickets() * 1LL;
            long win_ticket = random_at_most(total);

            for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
            {

                if (p->state == RUNNABLE)
                    cur_total += p->ticket;
                else
                    continue;

                if (cur_total > win_ticket) // winner process
                {
                    // Switch to chosen process.  It is the process's job
                    // to release ptable.lock and then reacquire it
                    // before jumping back to us.
                    c->proc = p;
                    switchuvm(p);
                    p->state = RUNNING;
                    // p->tick++;
                    int tick_start = ticks;
                    swtch(&(c->scheduler), p->context);
                    int tick_end = ticks;
                    p->tick += (tick_end - tick_start);

                    switchkvm();

                    // Process is done running for now.
                    // It should have changed its p->state before coming back.
                    c->proc = 0;

                    break;
                }
                else
                    continue;
            }
            release(&ptable.lock);
        }
    }
    ```
# **Test**

- Test Program for **Ticket Assignment**

    ```cpp
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
         * comment the part above and uncomment bellow to test fork
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
    ```

- Test Program for **Lottery Scheduler**

    ```cpp
    #include "types.h"
    #include "stat.h"
    #include "user.h"
    #include "pstat.h"

    int main(int argc, char *argv[])
    {
        struct pstat ps;
        getpinfo(&ps);
        printf(1, "\nPID\tINUSE\tTICKETS\t\tTICKS\n");
        for (int i = 0; i < NPROC; i++)
        {
            // if (ps.inuse[i])
            // if (ps.pid[i])
            if (ps.pid[i] && ps.ticket[i] > 0)
            {
                printf(1, "%d\t%d\t%d\t\t%d\n", ps.pid[i], ps.inuse[i], ps.ticket[i], ps.tick[i]);
            }
                
        }
        exit();
    }
    ```


- test commands

    - use `&;` command to run in parallel, give processes different tickets

        ```bash
        testTicket 10 &; testTicket 20 &; testTicket 40 &;
        ```
    - use `testProcInfo` to test how many ticks have passed for the processes.

        ```bash
        testProcInfo
        ```

        **output**
        ```
        PID     INUSE   TICKETS         TICKS
        5       1       10              411
        7       1       20              843
        9       1       40              1609
        ```

        we can see that the processes have proper distribution of ticks which almost converges to `10:20:40` as time goes to infinity