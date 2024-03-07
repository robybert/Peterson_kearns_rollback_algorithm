#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <time.h>
#include "process.hpp"

const int PARENT_SEED = 463523;
using namespace std;

int main(int argc, char const *argv[])
{
    // setup RNG
    srand(PARENT_SEED);

    int fildes[CHILDREN][2];
    pid_t c_pid[CHILDREN];

    // establish pipes
    for (int i = 0; i < CHILDREN; i++)
    {
        make_pipe(fildes[i]);
    }

    // fork all child processes
    for (int i = 0; i < CHILDREN; i++)
    {
        c_pid[i] = fork_process(i, fildes, false);
        cout << "parent created child " << i << " with c_pid " << c_pid[i] << endl;
    }

    int to_restart = 1;

    while (1)
    {
        // rng for time to wait
        this_thread::sleep_for(chrono::seconds(15));

        // restart the selected process
        // c_pid[to_restart] = restart_process(to_restart, c_pid[to_restart], fildes);

        if (c_pid[to_restart] != -1)
        {
            // rng for process selection if the restart was succesful
            to_restart = rand() % CHILDREN;
        }
    }
    return 0;
}