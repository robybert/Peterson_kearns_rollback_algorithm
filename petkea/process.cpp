#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <sys/select.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/time.h>
#include "process.hpp"
#include "pet-kea.hpp"

using namespace std;

int make_pipe(int fildes[2])
{
    int status;
    status = pipe(fildes);
    if (status == -1)
    {
        perror("failure to create pipe for process");
        return -1;
    }
    return 0;
}

pid_t fork_process(int process_nr, int fildes[CHILDREN][2], bool restart)
{
    pid_t c_pid;
    c_pid = fork();
    if (c_pid == -1)
    {
        perror("failure to fork new process");
        return -1;
    }
    else if (c_pid == 0)
    { // child process
        msg_process(process_nr, fildes, restart);
    }
    return c_pid;
}

pid_t restart_process(int process_nr, pid_t pid, int fildes[CHILDREN][2])
{
    cout << "restart process " << process_nr << endl;
    pid_t new_c_pid;
    kill(pid, SIGKILL);

    close(fildes[process_nr][0]);
    close(fildes[process_nr][1]);
    if (make_pipe(fildes[process_nr]) == -1)
    {
        return -1;
    }

    new_c_pid = fork_process(process_nr, fildes, true);
    return new_c_pid;
}

// int send_err_msg(int process_nr, msg_t buffer, int fildes[CHILDREN][2])
// {
//     int ret;
//     buffer.msg_type = ERR;
//     cout << "sending ERR with fd = " << fildes[process_nr][0] << " " << fildes[process_nr][1] << endl;
//     buffer.contents.ptp_err.fildes[0] = fildes[process_nr][0];
//     buffer.contents.ptp_err.fildes[1] = fildes[process_nr][1];
//     buffer.contents.ptp_err.sending_process_nr = process_nr;
//     buffer.contents.ptp_err.pid = getpid();
//     for (int i = 0; i < CHILDREN; i++)
//     {
//         if (i == process_nr)
//             continue;
//         ret = write(fildes[i][1], &buffer, BSIZE);
//         if (ret < 0)
//         {
//             perror("failure to write ERR msg");
//             return -1;
//         }
//     }
//     return 0;
// }

int recv_err_msg(struct msg_t buffer, int fildes[CHILDREN][2])
{

    close(fildes[buffer.sending_process_nr][0]);
    close(fildes[buffer.sending_process_nr][1]);
    int pidfd = syscall(SYS_pidfd_open, buffer.contents.ptp_err.pid, 0);
    if (pidfd == -1)
    {
        perror("failure to generate pidfd");
        return -1;
    }
    fildes[buffer.sending_process_nr][0] = syscall(SYS_pidfd_getfd, pidfd, buffer.contents.ptp_err.fildes[0], 0);
    if (fildes[buffer.sending_process_nr][0] == -1)
    {
        perror("failure to recieve fd 0 from process");
        return -1;
    }

    fildes[buffer.sending_process_nr][1] = syscall(SYS_pidfd_getfd, pidfd, buffer.contents.ptp_err.fildes[1], 0);
    if (fildes[buffer.sending_process_nr][1] == -1)
    {
        perror("failure to recieve fd 1 from process");
        return -1;
    }
    return 0;
}

int send_msg(char *input, int fildes[2], Pet_kea *state)
{
    int ret;
    ret = state->send_msg(input, fildes);
    if (ret < 0)
    {
        perror("write to pipe");
        return -1;
    }
    return 0;
}

void msg_process(int process_nr, int fildes[CHILDREN][2], bool restart)
{
    cout << "msg_process " << process_nr << " started with pid = " << getpid() << endl;

    fd_set current_fd, ready_fd;
    struct timeval tv;

    Pet_kea state = Pet_kea(process_nr, CHILDREN);
    struct msg_t buffer(CHILDREN);
    int ret, dest_process_nr, msg_cnt = 0, msg_nr = 0;
    bool is_busy[CHILDREN];
    for (int i = 0; i < CHILDREN; i++)
        is_busy[i] = false;

    FD_ZERO(&current_fd);
    FD_SET(fildes[process_nr][0], &current_fd);

    srand(SEED + process_nr);
    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    if (restart)
    {
        ret = state.recovery(); // TODO: fix recovery
        if (ret == -1)
        {
            exit(EXIT_FAILURE);
        }
    }
    // close(fildes[process_nr][1]);       //close writing to your read pipe the pipe needs to stay open until the other processes have retrieved the FD

    while (1)
    {
        ready_fd = current_fd;

        // start reading messages
        ret = select(fildes[process_nr][0] + 1, &ready_fd, NULL, NULL, &tv);
        if (ret == 0)
        {
            // cout << "timeout process " << process_nr << endl;
        }
        else if (ret < 0)
        {
            perror("select error");
        }
        else if (FD_ISSET(fildes[process_nr][0], &ready_fd))
        {
            ret = state.recv_msg(fildes[process_nr], &buffer);
            if (buffer.msg_type == MSG)
            {
                msg_cnt++;
                cout << process_nr << " " << buffer.contents.ptp_msg.msg_buf << "   time_v: " << buffer.time_v[0] << "-" << buffer.time_v[1] << endl;
                continue;
            }
            else if (buffer.msg_type == ERR)
            {
                cout << process_nr << " ERR recieved from " << buffer.sending_process_nr << endl;
                ret = recv_err_msg(buffer, fildes);
                if (ret != -1)
                    is_busy[buffer.sending_process_nr] = false;
                // TODO: send confirmation
            }
        }

        // start sending messages
        while ((dest_process_nr = rand() % CHILDREN) == process_nr || is_busy[dest_process_nr])
        {
        }
        char input[64];
        sprintf(input, "message number %d", msg_nr++);

        ret = send_msg(input, fildes[dest_process_nr], &state);
        if (ret == -1)
        {
            is_busy[dest_process_nr] = true;
            // if (send_msg(process_nr, dest_process_nr, buffer, fildes) == -1)
            //     is_busy[dest_process_nr] = true;
        }
        sleep(1);
    }
}