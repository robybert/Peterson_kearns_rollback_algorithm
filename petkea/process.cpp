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

int send_err_msg(int process_nr, Pet_kea::State *state, int fildes[CHILDREN][2])
{
    int ret;
    struct msg_t msg;
    msg.type = ERR;
    msg.sending_process_nr = process_nr;
    cout << "sending ERR with fd = " << fildes[process_nr][0] << " " << fildes[process_nr][1] << endl;
    msg.ptp_err.fildes[0] = fildes[process_nr][0];
    msg.ptp_err.fildes[1] = fildes[process_nr][1];
    msg.ptp_err.pid = getpid();

    char input[sizeof(msg_t)];
    serialize(&msg, input);

    for (int i = 0; i < CHILDREN; i++)
    {
        if (i == process_nr)
            continue;
        ret = write(fildes[i][1], input, sizeof(msg_t)); // state->send_void(input, fildes[i], SER_ERR_MSG_SIZE);
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
    cout << "recieving ERR with fd = " << fildes[buffer->sending_process_nr][0] << " " << fildes[buffer->sending_process_nr][1] << " with pid: " << buffer->ptp_err.pid << endl;
    close(fildes[buffer->sending_process_nr][0]);
    close(fildes[buffer->sending_process_nr][1]);
    int pidfd = syscall(SYS_pidfd_open, buffer->ptp_err.pid, 0);
    // cout << "recieving ERR with fd = " << fildes[buffer->sending_process_nr][0] << " " << fildes[buffer->sending_process_nr][1] << " with pidfd: " << pidfd << endl;
    if (pidfd == -1)
    {
        perror("failure to generate pidfd");
        return -1;
    }
    fildes[buffer->sending_process_nr][0] = syscall(SYS_pidfd_getfd, pidfd, buffer->ptp_err.fildes[0], 0);
    if (fildes[buffer->sending_process_nr][0] == -1)
    {
        perror("failure to recieve fd 0 from process");
        return -1;
    }

    fildes[buffer->sending_process_nr][1] = syscall(SYS_pidfd_getfd, pidfd, buffer->ptp_err.fildes[1], 0);
    if (fildes[buffer->sending_process_nr][1] == -1)
    {
        perror("failure to recieve fd 1 from process");
        return -1;
    }
    return 0;
}

void serialize(struct msg_t *msg, char *data)
{
    int *q = (int *)data;
    *q = msg->type;
    q++;
    *q = msg->sending_process_nr;
    q++;
    if (msg->type == MSG)
    {
        char *p = (char *)q;
        for (int i = 0; i < (int)sizeof(msg->ptp_msg.msg); i++)
        {
            *p = msg->ptp_msg.msg[i];
            p++;
        }
    }
    else
    {
        *q = msg->ptp_err.pid;
        q++;
        *q = msg->ptp_err.fildes[0];
        q++;
        *q = msg->ptp_err.fildes[1];
        q++;
    }
}

void deserialize(char *data, struct msg_t *msg)
{
    int *q = (int *)data;
    msg->type = (message_type)*q;
    q++;
    msg->sending_process_nr = *q;
    q++;
    if (msg->type == MSG)
    {
        char *p = (char *)q;
        for (int i = 0; i < (int)sizeof(msg->ptp_msg.msg); i++)
        {
            msg->ptp_msg.msg[i] = *p;
            p++;
        }
    }
    else
    {
        // TODO: serialize err msg
        msg->ptp_err.pid = (pid_t)*q;
        q++;
        msg->ptp_err.fildes[0] = *q;
        q++;
        msg->ptp_err.fildes[1] = *q;
        q++;
    }
}

int send_msg(struct msg_t *msg, int fildes[2], Pet_kea::State *state)
{
    char input[sizeof(msg_t)];
    serialize(msg, input);
    int ret = write(fildes[1], input, sizeof(msg_t)); // state->send_msg(input, fildes, sizeof(msg_t));
    if (ret < 0)
    {
        perror("write to pipe");
        return -1;
    }
    return 0;
}

int recv_msg(struct msg_t *msg, int fildes[2], Pet_kea::State *state)
{
    char *output = (char *)malloc(sizeof(msg_t));

    int ret = read(fildes[0], output, sizeof(msg_t)); // state->recv_msg(fildes, output, sizeof(msg_t));
    deserialize(output, msg);
    if (ret < 0)
    {
        perror("read from pipe");
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

    int ret, dest_process_nr, msg_cnt = 0, msg_nr = 0;
    bool is_busy[CHILDREN];
    for (int i = 0; i < CHILDREN; i++)
        is_busy[i] = false;

    FD_ZERO(&current_fd);
    FD_SET(fildes[process_nr][0], &current_fd);

    srand(SEED + process_nr);
    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    Pet_kea::State state = Pet_kea::State(process_nr, CHILDREN, restart);

    if (restart)
    {
        ret = send_err_msg(process_nr, &state, fildes);
        if (ret == -1)
        {

            exit(EXIT_FAILURE);
        }
        // state.send_ctrl(fildes);
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

            ret = recv_msg(&buffer, fildes[process_nr], &state);
            if (buffer.type == MSG)
            {
                msg_cnt++;
                cout << process_nr << " " << buffer.ptp_msg.msg << endl;
                continue;
            }
            else if (buffer.type == ERR)
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

        buffer.type = MSG;
        buffer.sending_process_nr = process_nr;
        sprintf(buffer.ptp_msg.msg, "message number %d", msg_nr++);

        ret = send_msg(&buffer, fildes[dest_process_nr], &state);
        if (ret == -1)
        {
            is_busy[dest_process_nr] = true;
            // if (send_msg(process_nr, dest_process_nr, buffer, fildes) == -1)
            //     is_busy[dest_process_nr] = true;
        }
        sleep(1);
    }
}