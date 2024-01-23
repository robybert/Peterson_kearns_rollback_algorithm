#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

using namespace std;

struct msg_t{
    int sending_process_nr;
    char msg_buf[50];
};

const int BSIZE = sizeof(msg_t);
const int SEED = 555432;
const int CHILDREN = 5;



int msg_process(int process_nr,int fildes[CHILDREN][2])
{
    cout << "msg_process " << process_nr << " started with pid = " << getpid() << endl;

    close(fildes[process_nr][1]);       //close writing to your read pipe
    fd_set current_fd, ready_fd;
    struct timeval tv;

    struct msg_t buffer;
    struct msg_t sbuffer;
    int dest_process_nr, ret, msg_cnt = 0;

    FD_ZERO(&current_fd);
    FD_SET(fildes[process_nr][0],&current_fd);
    srand(SEED);
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    
    while(1) {
        ready_fd = current_fd;
        //start reading messages
        //cout << "reading messages " << process_nr << endl;
        ret = select(fildes[process_nr][0]+1, &ready_fd, NULL, NULL, &tv);
        if (ret == 0) {
           // cout << "timeout process " << process_nr << endl;
        } else if (ret < 0) {
            perror("select error");
        } else if (FD_ISSET(fildes[process_nr][0], &ready_fd)){
            ret = read(fildes[process_nr][0], &buffer, BSIZE);
            msg_cnt++;
            cout << process_nr << " " << buffer.msg_buf << " " << msg_cnt << endl;
            continue;
        }
        //start sending messages
        //cout << "sending message " << process_nr << endl;
        sbuffer.sending_process_nr = process_nr;
        sprintf(sbuffer.msg_buf, "message sent form process %d      ", process_nr);
        while ((dest_process_nr = rand() % CHILDREN) == process_nr) {}
        ret = write(fildes[dest_process_nr][1], &sbuffer, BSIZE);
        if (ret < 0) {
            cout << process_nr << " failure to write to " << dest_process_nr << endl;
        }
    }
    return 0;
}