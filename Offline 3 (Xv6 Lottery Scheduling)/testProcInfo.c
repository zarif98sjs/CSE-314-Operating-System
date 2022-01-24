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