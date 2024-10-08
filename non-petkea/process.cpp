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
#include <sys/socket.h>
#include <sys/time.h>
#include "process.hpp"

using namespace std;
bool is_active;

static void wyslij(int sending_process_nr, int socket, int fd) // send fd by socket
{
    cout << sending_process_nr << " sending fd: " << fd << endl;
    struct msghdr msg = {0};

    char buf[CMSG_SPACE(sizeof(fd))];
    memset(buf, '\0', sizeof(buf));
    struct iovec io = {.iov_base = (void *)"", .iov_len = 1};

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);
    // msg.msg_name = (void *)&sending_process_nr;
    // msg.msg_namelen = sizeof(int);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

    memmove(CMSG_DATA(cmsg), &fd, sizeof(fd));

    msg.msg_controllen = CMSG_SPACE(sizeof(fd));

    if (sendmsg(socket, &msg, 0) < 0)
        cout << "failed to send fd" << endl;
}

static int odbierz(int socket) // receive fd from socket
{
    struct msghdr msg = {0};

    char m_buffer[1];
    struct iovec io = {.iov_base = m_buffer, .iov_len = sizeof(m_buffer)};
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    char c_buffer[256];
    msg.msg_control = c_buffer;
    msg.msg_controllen = sizeof(c_buffer);

    if (recvmsg(socket, &msg, 0) < 0)
        cerr << "Failed to receive message" << endl;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

    // unsigned char *data = CMSG_DATA(cmsg);

    cerr << "About to extract fd" << endl;
    int fd;
    memmove(&fd, CMSG_DATA(cmsg), sizeof(fd));
    cout << "Extracted fd: " << fd << endl;

    return fd;
}

void signalHandler(int signum)
{
    cout << "Interrupt signal (" << signum << ") received.\n";

    // cleanup and close up stuff here
    // terminate program

    is_active = false;
}

int make_pipe(int fildes[2], int sv[2])
{
    int status;
    status = pipe(fildes);
    if (status == -1)
    {
        perror("failure to create pipe for process");
        return -1;
    }

    if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, sv) != 0)
        cerr << "Failed to create Unix-domain socket pair" << endl;
    return 0;
}

pid_t fork_process(int process_nr, int fildes[CHILDREN][2], int sv[CHILDREN][2], bool restart)
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
        signal(SIGINT, signalHandler);
        is_active = true;
        msg_process(process_nr, fildes, sv, restart);
        cout << "child EXIT" << endl;
        exit(0);
    }
    return c_pid;
}

pid_t restart_process(int process_nr, pid_t pid, int fildes[CHILDREN][2], int sv[CHILDREN][2])
{
    cout << "restart process " << process_nr << endl;
    pid_t new_c_pid;
    kill(pid, SIGINT);

    close(fildes[process_nr][0]);
    close(fildes[process_nr][1]);
    if (make_pipe(fildes[process_nr], sv[process_nr]) == -1)
    {
        return -1;
    }

    sleep(3);

    new_c_pid = fork_process(process_nr, fildes, sv, true);
    return new_c_pid;
}

void send_err_msg_over_socket(int process_nr, int fildes[CHILDREN][2], int sv[CHILDREN][2])
{
    char data[SER_ERR_MSG_SIZE];
    struct msg_t msg;
    msg.type = ERR;
    msg.sending_process_nr = process_nr;
    serialize(&msg, data);
    for (int i = 0; i < CHILDREN; i++)
    {
        if (i == process_nr)
            continue;
        wyslij(process_nr, sv[i][0], fildes[process_nr][1]);
        // wyslij(process_nr, sv[i][0], fildes[i][1]);
        write(fildes[i][1], data, SER_ERR_MSG_SIZE);
        // Pet_kea::send_void(data, fildes[i], SER_ERR_MSG_SIZE);
    }
}

// int send_err_msg(int process_nr, int fildes[CHILDREN][2])
// {
//     int ret;
//     struct msg_t msg;
//     msg.type = ERR;
//     msg.sending_process_nr = process_nr;
//     cout << "sending ERR with fd = " << fildes[process_nr][0] << " " << fildes[process_nr][1] << endl;
//     msg.ptp_err.fildes[0] = fildes[process_nr][0];
//     msg.ptp_err.fildes[1] = fildes[process_nr][1];
//     msg.ptp_err.pid = getpid();

//     char input[SER_ERR_MSG_SIZE];
//     serialize(&msg, input);

//     for (int i = 0; i < CHILDREN; i++)
//     {
//         if (i == process_nr)
//             continue;
//         ret = write(fildes[i][1], input, SER_ERR_MSG_SIZE);
//         if (ret < 0)
//         {
//             perror("failure to write ERR msg");
//             return -1;
//         }
//     }
//     return 0;
// }

// int recv_err_msg(struct msg_t *buffer, int fildes[CHILDREN][2])
// {
//     cout << "recieving ERR with fd = " << fildes[buffer->sending_process_nr][0] << " " << fildes[buffer->sending_process_nr][1] << " with pid: " << buffer->ptp_err.pid << endl;
//     close(fildes[buffer->sending_process_nr][0]);
//     close(fildes[buffer->sending_process_nr][1]);
//     int pidfd = syscall(SYS_pidfd_open, buffer->ptp_err.pid, 0);
//     // cout << "recieving ERR with fd = " << fildes[buffer->sending_process_nr][0] << " " << fildes[buffer->sending_process_nr][1] << " with pidfd: " << pidfd << endl;
//     if (pidfd == -1)
//     {
//         perror("failure to generate pidfd");
//         return -1;
//     }
//     fildes[buffer->sending_process_nr][0] = syscall(SYS_pidfd_getfd, pidfd, buffer->ptp_err.fildes[0], 0);
//     if (fildes[buffer->sending_process_nr][0] == -1)
//     {
//         perror("failure to recieve fd 0 from process");
//         return -1;
//     }

//     fildes[buffer->sending_process_nr][1] = syscall(SYS_pidfd_getfd, pidfd, buffer->ptp_err.fildes[1], 0);
//     if (fildes[buffer->sending_process_nr][1] == -1)
//     {
//         perror("failure to recieve fd 1 from process");
//         return -1;
//     }
//     return 0;
// }

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
        *q = 0;
        q++;
        *q = 0;
        q++;
        *q = 0;
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
        msg->ptp_err.pid = (pid_t)*q;
        q++;
        msg->ptp_err.fildes[0] = *q;
        q++;
        msg->ptp_err.fildes[1] = *q;
        q++;
    }
}

int send_msg(char *input, int fildes[2], int process_nr)
{
    struct msg_t msg;
    msg.type = MSG;
    msg.sending_process_nr = process_nr;
    sprintf(msg.ptp_msg.msg, input);
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

void msg_process(int process_nr, int fildes[CHILDREN][2], int sv[CHILDREN][2], bool restart)
{
    cout << "msg_process " << process_nr << " started with pid = " << getpid() << endl;

    fd_set current_fd, ready_fd;
    struct timeval tv;

    struct msg_t buffer;
    char data[sizeof(msg_t)];
    int ret, dest_process_nr, msg_cnt = 0, msg_nr = 0;
    int new_fd[2];
    bool is_busy[CHILDREN];
    for (int i = 0; i < CHILDREN; i++)
        is_busy[i] = false;

    FD_ZERO(&current_fd);
    FD_SET(fildes[process_nr][0], &current_fd);
    FD_SET(sv[process_nr][1], &current_fd);
    int fdmax = max(fildes[process_nr][0], sv[process_nr][1]);

    srand(SEED + process_nr);
    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    if (restart)
    {
        send_err_msg_over_socket(process_nr, fildes, sv);
    }

    while (1)
    {
        ready_fd = current_fd;

        // start reading messages
        ret = select(fdmax + 1, &ready_fd, NULL, NULL, &tv);
        if (ret == 0)
        {
            cout << "timeout process " << process_nr << endl;
        }
        else if (ret < 0)
        {
            perror("select error");
        }
        else if (FD_ISSET(sv[process_nr][1], &ready_fd))
        {
            new_fd[1] = odbierz(sv[process_nr][1]);
            new_fd[0] = 0;
            // state.update_fd(ret_pair.second, fildes[ret_pair.second]);
        }
        else if (FD_ISSET(fildes[process_nr][0], &ready_fd))
        {
            ret = read(fildes[process_nr][0], data, sizeof(msg_t));
            if (ret < 0)
            {
                // handle error
            }
            deserialize(data, &buffer);
            if (buffer.type == MSG)
            {
                msg_cnt++;
                cout << process_nr << " " << buffer.ptp_msg.msg << endl;
            }
            else if (buffer.type == ERR)
            {
                cout << process_nr << " ERR recieved from " << buffer.sending_process_nr << endl;
                // ret = recv_err_msg(&buffer, fildes);
                // if (ret != -1)
                //     is_busy[buffer.sending_process_nr] = false;
                // TODO: send confirmation
                fildes[buffer.sending_process_nr][0] = new_fd[0];
                fildes[buffer.sending_process_nr][1] = new_fd[1];
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

        if (!is_active)
        {
            return;
        }
    }
}