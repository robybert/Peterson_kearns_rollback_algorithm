/**
 * @file pet-kea.hpp
 * @brief Header file containing the state class and its member functions
 */

#ifndef _PETKEA_HPP_
#define _PETKEA_HPP_

#include <unistd.h>
#include <vector>
#include <set>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>

// using namespace std;

const int MAX_LOG = 500;

// const int STATE_SIZE =

namespace Pet_kea
{
    // int SER_SIZE_CTRL_MSG_T(int i) = 6 * sizeof(int);
    //  const SER_SIZE_MSG_T = ;
    inline size_t SER_SIZE_CTRL_MSG_T(int recvd_cnt, int v_size) { return (6 * sizeof(int) + recvd_cnt * (sizeof(int) + v_size * sizeof(int))); }; // TODO: check this zise

    const int SAVE_CNT = 10;
    // int AVERAGE_MSG_BYTESIZE = 72;
    const int INT_VEC_PAIR_DELTA = 100;
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

    struct int_vec_pair
    {
        int Tj;
        std::vector<int> fail_v;

        int_vec_pair(int v_size) : fail_v(v_size, 0) {}
    };

    struct ctrl_msg_t
    {
        message_type msg_type;
        int sending_process_nr;
        struct fail_log_t log_entry;
        int recieved_cnt;
        // struct int_vec_pair *recieved; // TODO:make this a std::set
        std::set<std::pair<int, std::vector<int>>> recvd_msgs;
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
    };

    struct msg_log_t
    {
        int msg_size;
        bool recipient;
        int process_id;
        char *msg_buf;
        std::vector<int> time_v_s;
        std::vector<int> time_v_r;
        std::vector<int> fail_v_s;
    };

    /**
     * @brief Sends data to another process without it being recorded in the state.
     * @param input Pointer to the data to be sent.
     * @param fildes Array containing file descriptors of the pipe.
     * @param size Size of the data to be sent.
     * @return Number of bytes sent on success, -1 on failure.
     */
    int send_void(char *input, int fildes[2], int size);

    class State
    {
    private:
        int id;
        std::vector<int> time_v;
        std::vector<int> fail_v;
        int **fildes;
        msg_log_t *msg_log;
        int msg_cnt;
        std::vector<int> checkpoints;
        std::vector<std::vector<int>> ck_time_v;
        std::ofstream msg_out;

        bool check_duplicate(struct msg_t *msg);

        bool check_orphaned(struct msg_t *msg);

        int rem_log_entries(std::vector<int> to_remove, int final_index);

        /**
         * @brief Serializes a control message structure into a character array.
         * @param msg Pointer to the control message structure to be serialized.
         * @param data Pointer to the character array where the serialized data will be stored.
         */
        void serialize_ctrl(struct ctrl_msg_t *msg, char *data);

        /**
         * @brief Deserializes a control message from a character array.
         * @param data Pointer to the character array containing serialized data.
         * @param msg Pointer to the control message structure where deserialized data will be stored.
         */
        void deserialize_ctrl(char *data, struct ctrl_msg_t *msg);

        /**
         * @brief Serializes a general message structure into a character array.
         * @param msg Pointer to the message structure to be serialized.
         * @param data Pointer to the character array where the serialized data will be stored.
         */
        void serialize(struct msg_t *msg, char *data);

        /**
         * @brief Deserializes a general message from a character array.
         * @param data Pointer to the character array containing serialized data.
         * @param msg Pointer to the message structure where deserialized data will be stored.
         */
        void deserialize(char *data, struct msg_t *msg);

        /**
         * @brief Serializes a log message structure into a character array.
         * @param log Pointer to the log message structure to be serialized.
         * @param data Pointer to the character array where the serialized data will be stored.
         */
        void serialize_log(struct msg_log_t *log, char *data);

        /**
         * @brief Deserializes a log message from a character array.
         * @param data Pointer to the character array containing serialized data.
         * @param log Pointer to the log message structure where deserialized data will be stored.
         * @return returns the size of the deserialized log message
         */
        int deserialize_log(char *data, struct msg_log_t *log);

        /**
         * @brief Receives a control message.
         * @param data Pointer to the character array containing the received control message.
         */
        void recv_ctrl(struct ctrl_msg_t *c_msg);

        /**
         * @brief Stores a general message.
         * @param msg Pointer to the message structure to be stored.
         * @param recipient -1 if the stored msg is a recieve event, any positive interger for the recipient process_id.
         * @return 0 on success, -1 on failure.
         */
        int store_msg(struct msg_t *msg, int recipient);

        int rollback(struct ctrl_msg_t *msg);

    public:
        const int SER_LOG_SIZE = 3 * sizeof(int) + time_v.size() * 3 * sizeof(int);
        const int CONST_CHECKPOINT_BYTESIZE = (4 + 2 * time_v.size()) * sizeof(int);
        // const int MSG_CHECKPOINT_BYTESYZE = SAVE_CNT * (AVERAGE_MSG_BYTESIZE + (3 + 3 * time_v.size()) * sizeof(int));
        /**
         * @brief Constructor for State class.
         * @param process_nr The process number.
         * @param process_cnt The total number of processes.
         * @param restart Flag indicating whether the process is being restarted.
         */
        State(int process_nr, int process_cnt, int (*fd)[2], bool restart);

        /**
         * @brief Destructor for State class.
         */
        ~State();

        /**
         * @brief Creates a checkpoint of the process state and the recieved messages.
         * @return 0 on success, -1 on failure.
         */
        int checkpoint();

        /**
         * @brief Sends a message to another process that will be recorded in the state.
         *
         * @param input Pointer to the message to be sent.
         * @param fildes Array containing file descriptors of the pipe.
         * @param size Size of the message to be sent.
         * @return Number of bytes sent on success, -1 on failure.
         */
        int send_msg(char *input, int process_id, int size);

        /**
         * @brief Receives a message from another process
         * @param fildes Array containing file descriptors of the pipe.
         * @param output Pointer to the buffer where the received message will be stored.
         * @param size Size of the buffer.
         * @return 0 on success, 1 after recieving a duplicate or orphaned message that was discarded, -1 on failure.
         */
        int recv_msg(int fildes[2], char *output, int size);

        /**
         * @brief Sends control message to other processes to indicate that a failure occured.
         * @param fildes Array of arrays containing file descriptors of the pipes.
         */
        void send_ctrl();

        int update_fd(int process_id, int fd[2]);
    };
}

#endif