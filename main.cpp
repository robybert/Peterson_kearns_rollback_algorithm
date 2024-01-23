#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <time.h>
#include "process.cpp"

// const int CHILDREN = 5;
const int PARENT_SEED = 463523;
using namespace std;


// simulates a failure and restarts the process
int restart_process(int process_nr, pid_t pid){
    return 0;
}

int main(int argc, char const *argv[])
{
    // setup RNG
    srand(PARENT_SEED);
    //establish pipes
    int status[CHILDREN];
    int fildes[CHILDREN][2];

    for (int i = 0; i < CHILDREN; i++) {
        status[i] = pipe(fildes[i]);//check if working
        if (status[i] == -1) {
            fprintf(stderr, "failure to create pipe %d\n", i);
            exit(EXIT_FAILURE);
        }
    }
    //fork all child processes
    pid_t c_pid[CHILDREN];

    for (int i = 0; i < CHILDREN; i++){
        c_pid[i] = fork();
        if (c_pid[i] == -1){
            fprintf(stderr, "failure to fork child %d\n", c_pid[i]);
            exit(EXIT_FAILURE);
        } else if (c_pid[i] == 0) {
            //child
            cout << "child process created " << getpid() << endl;

            //let child exit w/o making more children
            return msg_process(i,fildes);
        }
        //parent
        cout << "parent created child " << i << " with c_pid " << c_pid[i] << endl;



    }
    while(1) {
        //rng for time to wait 
        this_thread::sleep_for(chrono::seconds(rand()%3));
        //rng for process selection
        int to_restart = rand() % CHILDREN;
        //restart the selected process
        restart_process(to_restart,c_pid[to_restart]);
        
    }
    return 0;
}