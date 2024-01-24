#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include "process.cpp"

// const int CHILDREN = 5;
const int PARENT_SEED = 463523;
using namespace std;

int make_pipe(int fildes[2], int pipe_nr) {
    int status;
    status = pipe(fildes);
    if (status == -1) {
        fprintf(stderr, "failure to create pipe %d\n", pipe_nr);
        // TODO: handle error
        return -1;
    }
    return 0;
}

pid_t fork_process(int process_nr, int fildes[CHILDREN][2], bool restart) {
    pid_t c_pid;
    c_pid= fork();
    if (c_pid == -1){
        fprintf(stderr, "failure to fork child %d\n", c_pid);
        exit(EXIT_FAILURE);
        //TODO: handle exit failure
    } else if (c_pid == 0) {
        //child
        cout << "child process created " << getpid() << endl;

        //let child exit w/o making more children
        return msg_process(process_nr,fildes, restart);
    }
    return c_pid;
}


// simulates a failure and restarts the process
pid_t restart_process(int process_nr, pid_t pid, int fildes[CHILDREN][2]){
    //TODO: kill the process
    pid_t new_c_pid;
    kill(pid, SIGKILL);
    //TODO: make new pipes and close the old pipes
    if (make_pipe(fildes[process_nr], process_nr) == -1) {
        //TODO: make error
    }

    //TODO: refork the process and let it send its fd to other processes 
    new_c_pid = fork_process(process_nr, fildes, true);
    return new_c_pid;
}

int main(int argc, char const *argv[])
{
    // setup RNG
    srand(PARENT_SEED);
    //establish pipes
    int fildes[CHILDREN][2];

    for (int i = 0; i < CHILDREN; i++) {
        make_pipe(fildes[i], i);
    }
    //fork all child processes
    pid_t c_pid[CHILDREN];

    for (int i = 0; i < CHILDREN; i++){
        c_pid[i] = fork_process(i, fildes, false);
        //parent
        cout << "parent created child " << i << " with c_pid " << c_pid[i] << endl;



    }
    while(1) {
        //rng for time to wait 
        this_thread::sleep_for(chrono::seconds(3));
        //rng for process selection
        //int to_restart = rand() % CHILDREN;
        //restart the selected process
        //c_pid[to_restart] = restart_process(to_restart, c_pid[to_restart], fildes);
        
    }
    return 0;
}