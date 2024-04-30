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
    int sv[CHILDREN][2];
    pid_t c_pid[CHILDREN];

    // establish pipes
    for (int i = 0; i < CHILDREN; i++)
    {
        make_pipe(fildes[i], sv[i]);
    }
    int ret;

    // fork all child processes
    for (int i = 0; i < CHILDREN; i++)
    {
        ret = fork_process(i, fildes, sv, false);
        if (ret == -1)
            return 0;
        c_pid[i] = ret;
        cout << "parent created child " << i << " with c_pid " << c_pid[i] << endl;
    }

    int to_restart = 1;

    // rng for time to wait
    this_thread::sleep_for(chrono::seconds(16));

    // restart the selected process
    c_pid[to_restart] = restart_process(to_restart, c_pid[to_restart], fildes, sv);

    this_thread::sleep_for(chrono::seconds(16));

    c_pid[to_restart] = restart_process(to_restart, c_pid[to_restart], fildes, sv);

    while (0)
    {

        if (c_pid[to_restart] != -1)
        {
            // rng for process selection if the restart was succesful
            to_restart = rand() % CHILDREN;
        }
    }
    return 0;
}