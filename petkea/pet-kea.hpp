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
#include <bits/stdc++.h>

// using namespace std;

const int MAX_LOG = 15000;

// const int STATE_SIZE =

// Hash function
struct vector_hash
{
    size_t operator()(const std::vector<int>
                          &myVector) const
    {
        std::hash<int> hasher;
        size_t answer = 0;

        for (int i : myVector)
        {
            answer ^= hasher(i) + 0x9e3779b9 +
                      (answer << 6) + (answer >> 2);
        }
        return answer;
    }
};

namespace Pet_kea
{

    const int SER_VOID_SIZE = 2 * sizeof(int);

    const int SAVE_CNT = 10;
    typedef enum message_type
    {
        MSG,
        CTRL,
        VOID,
        COMM1,
        COMM2,
        COMM3,
        COMM4
    } msg_type;

    struct fail_log_t
    {
        int id;
        int fail_nr;
        int res_time;
        fail_log_t() {}
        fail_log_t(int id, int fail_nr, int res_time) : id(id),
                                                        fail_nr(fail_nr),
                                                        res_time(res_time) {}
    };

    struct ctrl_msg_t
    {
        message_type msg_type;
        int sending_process_nr;
        struct fail_log_t log_entry;
        int recieved_cnt;
        std::set<std::pair<int, std::vector<int>>> recieved_msgs;
    };

    struct msg_t
    {
        message_type msg_type;
        int sending_process_nr;
        std::vector<int> time_v;
        std::vector<int> fail_v;
        int msg_size;
        char *msg_buf;
        ~msg_t()
        {
            free(msg_buf);
        }
    };

    struct msg_log_t
    {
        int msg_size;
        bool recipient;
        int process_id;
        char *msg_buf;
        std::vector<int> time_v_sender;
        std::vector<int> time_v_reciever;
        std::vector<int> fail_v_sender;
        ~msg_log_t()
        {
            free(msg_buf);
        }
        msg_log_t &operator=(const msg_log_t &other)
        {
            if (this != &other)
            {
                msg_size = other.msg_size;
                recipient = other.recipient;
                process_id = other.process_id;
                msg_buf = other.msg_buf;
                time_v_sender = other.time_v_sender;
                time_v_reciever = other.time_v_reciever;
                fail_v_sender = other.fail_v_sender;
            }
            return *this;
        }
    };

    /**
     * @brief Prints the contents of a message.
     * This function prints the contents of a message struct, including message type, sending process number, time vector, failure vector, message size.
     * @param msg Pointer to the message struct to be printed.
     */
    void print_msg(struct msg_t *msg);

    /**
     * @brief Prints the contents of a control message.
     * This function prints the contents of a control message struct, including message type, sending process number, log entry, received count, and received messages.
     * @param msg Pointer to the control message struct to be printed.
     */
    void print_ctrl_msg(struct ctrl_msg_t *msg);

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

        bool automatic_checkpointing;

        std::vector<struct fail_log_t> fail_log;
        std::unordered_set<std::vector<int>, vector_hash> arrived_msgs;
        std::unordered_set<std::vector<int>, vector_hash> arrived_ctrl;
        std::ofstream msg_out;
        std::ofstream commit_out;

        /**
         * @brief Checks if a message is a duplicate message.
         * @param msg Pointer to a msg_t structure representing the message to be checked.
         * @return true if if the message is a duplicate, false otherwise.
         */
        bool check_duplicate(struct msg_t *msg);

        bool check_duplicate_ctrl(struct fail_log_t log);

        /**
         * @brief Checks if a message is orphaned.
         * @param msg Pointer to a msg_t structure representing the message to be checked.
         * @return true if the message is orphaned, false otherwise.
         */
        bool check_orphaned(struct msg_t *msg);

        int next_checkpoint_after_rem(std::vector<int> removed_checkpoints, int curr_next_checkpoint);

        /**
         * @brief Removes log entries based on provided indices.
         * @param to_remove Vector containing indices of log entries to be removed.
         * @param final_index Index of the final log entry in the log structure.
         * @return New final index
         */
        int rem_log_entries(std::vector<int> to_remove, int final_index);

        void rem_checkpoints(std::vector<int> to_remove);
        int *next_checkpoint(int *ptr);

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

        void serialize_state(char *data);
        void deserialize_state(char *data);

        /**
         * @brief Stores a general message.
         * @param msg Pointer to the message structure to be stored.
         * @param recipient -1 if the stored msg is a recieve event, any positive interger for the recipient process_id.
         * @return 0 on success, -1 on failure.
         */
        int store_msg(struct msg_t *msg, int recipient);

        /**
         * @brief performs a rollback
         * @param msg Pointer to the  control message structure that activated the rollback
         * @return 0 on success, -1 on failure.
         */
        int rollback(struct ctrl_msg_t *msg);

    public:
        const int SER_LOG_SIZE = 3 * sizeof(int) + time_v.size() * 3 * sizeof(int);
        inline size_t SER_SIZE_CTRL_MSG_T(int recvd_cnt) { return (6 * sizeof(int) + recvd_cnt * (sizeof(int) + time_v.size() * sizeof(int))); };
        inline int SER_STATE_SIZE(int arrived_ctrl_size)
        {
            return (3 + (time_v.size()) + (3 * arrived_ctrl_size)) * sizeof(int);
        };

        const int SER_MSG_SIZE = 3 * sizeof(int) + time_v.size() * 2 * sizeof(int);
        const int CONST_CHECKPOINT_BYTESIZE = (4 + 2 * time_v.size()) * sizeof(int);

        // const int MSG_CHECKPOINT_BYTESYZE = SAVE_CNT * (AVERAGE_MSG_BYTESIZE + (3 + 3 * time_v.size()) * sizeof(int));
        /**
         * @brief Constructor for State class.
         * @param process_nr The process number.
         * @param process_cnt The total number of processes.
         * @param fd The file descriptors from all processes.
         * @param restart Flag indicating whether the process is being restarted.
         */
        State(int process_nr, int process_cnt, int (*fd)[2], bool restart);

        /**
         * @brief Destructor for State class.
         */
        ~State();

        int get_msg_cnt();

        /**
         * @brief Retrieves a message buffer.
         * This function retrieves the message buffer at the specified index `i`.
         * @param i Index of the message buffer to retrieve.
         * @return Pointer to the message buffer.
         */
        char *get_msg(int i);

        /**
         * @brief Retrieves the message log.
         * This function retrieves the message log, which is an array of message buffers.
         * @return Pointer to an array of message buffers.
         */
        char **get_msg_log();

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
         * @return 0 on success, 1 after recieving a VOID msg, 2 after recieving a CTRL msg, 3 after recieving a duplicate or orphaned message that was discarded, -1 on failure.
         */
        int recv_msg(int fildes[2], char *output, int size);

        /**
         * @brief Sends control message to other processes to indicate that a failure occured.
         * @param fildes Array of arrays containing file descriptors of the pipes.
         */
        void send_ctrl();

        /**
         * @brief updates file descriptors
         * @param process_id The process id of the file descriptors to be changed.
         * @param fd The new file descriptors.
         * @return 0 on success, -1 on failure.
         */
        int update_fd(int process_id, int fd[2]);
    };
}

#endif