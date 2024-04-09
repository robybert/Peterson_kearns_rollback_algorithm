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

void serialize(struct msg_t *msg, char *data)
{
    int *q = (int *)data;
    *q = msg->msg_type;
    q++;
    *q = msg->sending_process_nr;
    q++;
    if (msg->msg_type == MSG)
    {
        char *p = (char *)q;
        for (int i = 0; i < (int)sizeof(msg->contents.ptp_msg.msg_buf); i++)
        {
            *p = msg->contents.ptp_msg.msg_buf[i];
            p++;
        }
    }
    else
    {
        *q = msg->contents.ptp_err.pid;
        q++;
        *q = msg->contents.ptp_err.fildes[0];
        q++;
        *q = msg->contents.ptp_err.fildes[1];
        q++;
    }
}

void deserialize(char *data, struct msg_t *msg)
{
    int *q = (int *)data;
    msg->msg_type = (message_type)*q;
    q++;
    msg->sending_process_nr = *q;
    q++;
    if (msg->msg_type == MSG)
    {
        char *p = (char *)q;
        for (int i = 0; i < (int)sizeof(msg->contents.ptp_msg.msg_buf); i++)
        {
            msg->contents.ptp_msg.msg_buf[i] = *p;
            p++;
        }
    }
    else
    {
        // TODO: serialize err msg
        msg->contents.ptp_err.pid = (pid_t)*q;
        q++;
        msg->contents.ptp_err.fildes[0] = *q;
        q++;
        msg->contents.ptp_err.fildes[1] = *q;
        q++;
    }
}

int send_err_msg(int process_nr, msg_t *buffer, int fildes[CHILDREN][2])
{
    int ret;
    buffer->msg_type = ERR;
    cout << "sending ERR with fd = " << fildes[process_nr][0] << " " << fildes[process_nr][1] << endl;
    buffer->contents.ptp_err.fildes[0] = fildes[process_nr][0];
    buffer->contents.ptp_err.fildes[1] = fildes[process_nr][1];
    buffer->sending_process_nr = process_nr;
    buffer->contents.ptp_err.pid = getpid();

    char input[sizeof(msg_t)];
    serialize(buffer, input);

    for (int i = 0; i < CHILDREN; i++)
    {
        if (i == process_nr)
            continue;
        ret = write(fildes[i][1], input, sizeof(msg_t));
        if (ret < 0)
        {
            perror("failure to write ERR msg");
            return -1;
        }
    }
    return 0;
}

int recv_err_msg(struct msg_t *buffer, int fildes[CHILDREN][2])
{
    cout << "recieving ERR with fd = " << fildes[buffer->sending_process_nr][0] << " " << fildes[buffer->sending_process_nr][1] << " with pid: " << buffer->contents.ptp_err.pid << endl;
    close(fildes[buffer->sending_process_nr][0]);
    close(fildes[buffer->sending_process_nr][1]);
    int pidfd = syscall(SYS_pidfd_open, buffer->contents.ptp_err.pid, 0);
    if (pidfd == -1)
    {
        perror("failure to generate pidfd");
        return -1;
    }
    fildes[buffer->sending_process_nr][0] = syscall(SYS_pidfd_getfd, pidfd, buffer->contents.ptp_err.fildes[0], 0);
    if (fildes[buffer->sending_process_nr][0] == -1)
    {
        perror("failure to recieve fd 0 from process");
        return -1;
    }

    fildes[buffer->sending_process_nr][1] = syscall(SYS_pidfd_getfd, pidfd, buffer->contents.ptp_err.fildes[1], 0);
    if (fildes[buffer->sending_process_nr][1] == -1)
    {
        perror("failure to recieve fd 1 from process");
        return -1;
    }
    return 0;
}

int send_msg(char *input, int fildes[2], int process_nr)
{
    struct msg_t msg;
    msg.msg_type = MSG;
    msg.sending_process_nr = process_nr;
    sprintf(msg.contents.ptp_msg.msg_buf, input);
    int ret;
    char data[sizeof(msg_t)];
    serialize(&msg, data);
    ret = write(fildes[1], data, sizeof(msg_t));
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

    struct msg_t buffer;
    char data[sizeof(msg_t)];
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
        send_err_msg(process_nr, &buffer, fildes);
        // buffer.msg_type = ERR;
        // buffer.sending_process_nr = process_nr;
        // buffer.contents.ptp_err.fildes[0] = fildes[process_nr][0];
        // buffer.contents.ptp_err.fildes[1] = fildes[process_nr][1];
        // buffer.contents.ptp_err.pid = getpid();

        // serialize(&buffer, data);
        // for (int i = 0; i < CHILDREN; i++)
        // {
        //     if (i == process_nr)
        //         continue;
        //     ret = write(fildes[i][1], data, sizeof(buffer));
        //     if (ret == -1)
        //     {
        //         exit(EXIT_FAILURE);
        //     }
        // }
    }
    // close(fildes[process_nr][1]);       //close writing to your read pipe the pipe needs to stay open until the other processes have retrieved the FD

    while (1)
    {
        ready_fd = current_fd;

        // start reading messages
        ret = select(fildes[process_nr][0] + 1, &ready_fd, NULL, NULL, &tv);
        if (ret == 0)
        {
            cout << "timeout process " << process_nr << endl;
        }
        else if (ret < 0)
        {
            perror("select error");
        }
        else if (FD_ISSET(fildes[process_nr][0], &ready_fd))
        {
            ret = read(fildes[process_nr][0], data, sizeof(msg_t));
            if (ret < 0)
            {
                // handle error
            }
            deserialize(data, &buffer);
            if (buffer.msg_type == MSG)
            {
                msg_cnt++;
                cout << process_nr << " " << buffer.contents.ptp_msg.msg_buf << endl;
            }
            else if (buffer.msg_type == ERR)
            {
                cout << process_nr << " ERR recieved from " << buffer.sending_process_nr << endl;
                ret = recv_err_msg(&buffer, fildes);
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

        ret = send_msg(input, fildes[dest_process_nr], process_nr);
        if (ret == -1)
        {
            is_busy[dest_process_nr] = true;
            // if (send_msg(process_nr, dest_process_nr, buffer, fildes) == -1)
            //     is_busy[dest_process_nr] = true;
        }
        sleep(1);
    }
}