#ifndef _PETKEA_H_
#define _PETKEA_H_

#include <unistd.h>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>

using namespace std;

const int MAX_LOG = 500;
const int SAVE_CNT = 5;
// const int STATE_SIZE =

typedef enum message_type
{
    MSG,
    ERR
} msg_type;

struct ptp_msg
{
    char msg_buf[64];
};

struct fail_log_t
{
    int id;
    int fail_nr;
    int res_time;
};

struct ctrl_msg_t
{
    struct fail_log_t log_entry;
    int recieved_cnt;
    pair<int, vector<int>> *recieved;
};

struct ptp_err
{
    pid_t pid;
    int fildes[2];
    struct ctrl_msg_t *msg;
};

struct msg_t
{
    message_type msg_type;
    int sending_process_nr;
    vector<int> time_v;
    vector<int> fail_v;
    union contents
    {
        struct ptp_msg ptp_msg;
        struct ptp_err ptp_err;
    } contents;
    msg_t(int v_size) : time_v(v_size, 0),
                        fail_v(v_size, 0) {}
};

struct msg_log_t
{
    char msg_buf[64];
    bool recipient;
    int process_id;
    vector<int> time_v;
    vector<int> fail_v;
};

class Pet_kea
{
private:
    int id;
    vector<int> time_v;
    vector<int> fail_v;
    msg_log_t *msg_log;
    int msg_cnt;
    int last_checkpoint;
    ofstream msg_out;
    void serialize_ctrl(struct ctrl_msg_t *msg, char *data);
    void deserialize_ctrl(char *data, struct ctrl_msg_t *msg);
    void serialize(struct msg_t *msg, char *data);
    void deserialize(char *data, struct msg_t *msg);
    void send_ctrl(int fildes[][2], struct msg_t *msg);
    void recv_ctrl();
    int store_msg(struct msg_t *msg, bool recipient);

public:
    Pet_kea(int process_nr, int process_cnt);
    Pet_kea(int process_nr, int process_cnt, int fildes[][2], struct msg_t *msg);
    ~Pet_kea();
    int checkpoint();
    int send_msg(char *input, int fildes[2]);
    int recv_msg(int fildes[2], struct msg_t *ret_buf);
    int recovery();
};

#endif