#ifndef _PETKEA_H_
#define _PETKEA_H_

#include <unistd.h>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <cstring>

// using namespace std;

const int MAX_LOG = 500;
const int SAVE_CNT = 5;
// const int STATE_SIZE =

namespace Pet_kea
{
    // int SER_SIZE_CTRL_MSG_T(int i) = 6 * sizeof(int);
    //  const SER_SIZE_MSG_T = ;
    inline size_t SER_SIZE_CTRL_MSG_T(int recvd_cnt, int v_size) { return (6 * sizeof(int) + recvd_cnt * (sizeof(int) + v_size * sizeof(int))); };

    typedef enum message_type
    {
        MSG,
        CTRL,
        VOID
    } msg_type;

    struct fail_log_t
    {
        int id;
        int fail_nr;
        int res_time;
    };

    struct ctrl_msg_t
    {
        message_type msg_type;
        int sending_process_nr;
        struct fail_log_t log_entry;
        int recieved_cnt;
        std::pair<int, std::vector<int>> *recieved;
    };

    // struct ptp_err
    // {
    //     pid_t pid;
    //     int fildes[2];
    //     struct ctrl_msg_t *msg;
    // };

    struct msg_t
    {
        message_type msg_type;
        int sending_process_nr;
        std::vector<int> time_v;
        std::vector<int> fail_v;
        int msg_size;
        char *msg_buf;
        msg_t(int v_size) : time_v(v_size, 0),
                            fail_v(v_size, 0) {}
    };

    struct msg_log_t
    {
        int msg_size;
        bool recipient;
        int process_id;
        char *msg_buf;
        std::vector<int> time_v;
        std::vector<int> fail_v;
    };

    class State
    {
    private:
        int id;
        std::vector<int> time_v;
        std::vector<int> fail_v;
        msg_log_t *msg_log;
        int msg_cnt;
        int last_checkpoint;
        std::ofstream msg_out;
        void serialize_ctrl(struct ctrl_msg_t *msg, char *data);
        void deserialize_ctrl(char *data, struct ctrl_msg_t *msg);
        void serialize(struct msg_t *msg, char *data);
        void deserialize(char *data, struct msg_t *msg);

        void recv_ctrl(char *data);
        int store_msg(struct msg_t *msg, bool recipient);

    public:
        State(int process_nr, int process_cnt, bool restart);
        State(int process_nr, int process_cnt, int fildes[][2]);
        ~State();
        int checkpoint();
        int send_void(char *input, int fildes[2], int size);
        int send_msg(char *input, int fildes[2], int size);
        int recv_msg(int fildes[2], char *output, int size);
        int recovery();
        void send_ctrl(int fildes[][2]);
    };
}

#endif