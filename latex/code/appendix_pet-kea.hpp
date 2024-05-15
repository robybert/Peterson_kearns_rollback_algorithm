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

const int MAX_LOG = 500;

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

void get_msg_filename(int process_nr, char msg_filename[32]);

namespace Pet_kea
{
    inline size_t SER_SIZE_CTRL_MSG_T(int recvd_cnt, int v_size)
    {
        return (6 * sizeof(int) + recvd_cnt * (sizeof(int) + v_size * sizeof(int)));
    };
    const int SER_VOID_SIZE = 2 * sizeof(int);

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

    struct comm_msg_t
    {
        message_type msg_type;
        int sending_process_nr;

        int time_v_j;
        std::vector<int> time_v_min;
        int committed_cnt;
        std::unordered_set<std::vector<int>, vector_hash> committed_msgs;
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
        int next_checkpoint;
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
                time_v_sender = other.time_v_sender;
                time_v_reciever = other.time_v_reciever;
                fail_v_sender = other.fail_v_sender;
                next_checkpoint = other.next_checkpoint;
                char *temp_buf = (char *)malloc(msg_size);
                memcpy(temp_buf, other.msg_buf, msg_size);
                free(msg_buf);
                msg_buf = temp_buf;
            }
            return *this;
        }
    };

    /**
     * @brief Prints the contents of a message.
     * This function prints the contents of a message struct, including message type,
     * sending process number, time vector, failure vector, message size.
     * @param msg Pointer to the message struct to be printed.
     */
    void print_msg(struct msg_t *msg);

    /**
     * @brief Prints the contents of a control message.
     * This function prints the contents of a control message struct, including message type,
     * sending process number, log entry, received count, and received messages.
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
        std::vector<int> time_v_min;
        std::vector<int> fail_v;
        int **fildes;
        msg_log_t *msg_log;
        int msg_cnt;
        std::vector<bool> commit_v;
        std::vector<bool> remove_v;
        std::vector<int> checkpoints;
        std::vector<std::vector<int>> ck_time_v;

        std::vector<struct fail_log_t> fail_log;
        std::unordered_set<std::vector<int>, vector_hash> arrived_msgs;
        std::unordered_set<std::vector<int>, vector_hash> arrived_ctrl;
        std::unordered_set<std::vector<int>, vector_hash> committed_msg_set;
        std::unordered_set<std::vector<int>, vector_hash> committed_recieve_events;
        std::ofstream msg_out;
        std::ofstream commit_out;

        /**
         * @brief Checks if a message is a duplicate message.
         * @param msg Pointer to a msg_t structure representing the message to be checked.
         * @return true if if the message is a duplicate, false otherwise.
         */
        bool check_duplicate(struct msg_t *msg);

        /**
         * @brief Checks for duplicate control messages in the fail log.
         * @param log The fail log structure containing control messages to be checked.
         * @return True if duplicate control messages are found, false otherwise.
         */
        bool check_duplicate_ctrl(struct fail_log_t log);

        /**
         * @brief Checks for duplicate commit messages in the message log.
         * @param l_msg Pointer to the message log structure containing commit messages to be checked.
         * @return True if duplicate commit messages are found, false otherwise.
         */
        bool check_duplicate_commit(struct msg_log_t *l_msg);

        /**
         * @brief Checks if a message is orphaned.
         * @param msg Pointer to a msg_t structure representing the message to be checked.
         * @return true if the message is orphaned, false otherwise.
         */
        bool check_orphaned(struct msg_t *msg);

        /**
         * @brief Finds the next_checkpoint variable after removing certain checkpoints.
         * @param removed_checkpoints A vector containing the IDs of checkpoints that have been removed.
         * @param curr_next_checkpoint The ID of the current next_checkpoint variable before removal.
         * @return The ID of the next checkpoint after considering the removed checkpoints.
         */
        int next_checkpoint_after_rem(std::vector<int> removed_checkpoints, int curr_next_checkpoint);

        /**
         * @brief Removes log entries based on provided indices.
         * @param to_remove Vector containing indices of log entries to be removed.
         * @param final_index Index of the final log entry in the log structure.
         * @return New final index
         */
        int rem_log_entries(std::vector<int> to_remove, int final_index);

        /**
         * @brief Removes checkpoints.
         * @param to_remove A vector containing the IDs of checkpoints to be removed.
         */
        void rem_checkpoints(std::vector<int> to_remove);

        /**
         * @brief Retrieves the next checkpoint.
         * @param ptr A pointer to the current checkpoint.
         * @return A pointer to the next checkpoint.
         */
        int *next_checkpoint(int *ptr);

        /**
         * @brief Serializes a commit message.
         * @param msg Pointer to the commit message structure to be serialized.
         * @param data Pointer to the character array where the serialized data will be stored.
         */
        void serialize_commit(struct comm_msg_t *msg, char *data);

        /**
         * @brief Deserializes a commit message.
         * @param data Pointer to the character array containing the serialized commit message.
         * @param msg Pointer to the commit message structure where the deserialized data will be stored.
         */
        void deserialize_commit(char *data, struct comm_msg_t *msg);
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
         * @brief Serializes the state data.
         * @param data A pointer to the character array where the serialized state data will be stored.
         */
        void serialize_state(char *data);

        /**
         * @brief Deserializes the state data.
         * @param data A pointer to the character array containing the serialized state data.
         */
        void deserialize_state(char *data);

        /**
         * @brief Stores a general message.
         * @param msg Pointer to the message structure to be stored.
         * @param recipient     -1 if the stored msg is a recieve event,
         *                      any positive interger for the recipient process_id.
         * @return 0 on success, -1 on failure.
         */
        int store_msg(struct msg_t *msg, int recipient);

        /**
         * @brief Commits a list of messages.
         * @param committed_msgs A vector containing the IDs of messages to be committed.
         * @return Returns 0 if the messages are successfully committed.
         *         Returns a non-zero value if an error occurs.
         */
        int commit_msgs(std::vector<int> committed_msgs);

        /**
         * @brief performs a rollback
         * @param msg Pointer to the  control message structure that activated the rollback
         * @return 0 on success, -1 on failure.
         */
        int rollback(struct ctrl_msg_t *msg);

        /**
         * @brief Sends a COMM2 message to the target with the specified ID.
         * @param target_id The ID of the target to which the commit message will be sent.
         * @return  Returns 0 if the commit message is successfully sent.
         *          Returns a non-zero value if an error occurs.
         */
        int send_commit(int target_id);

        /**
         * @brief Commits all messages that can be safely committed
         * @param is_instigator A boolean value indicating whether the calling function
         *                      is the initiator of the commit procedure (true) or not (false).
         * @return Returns 0 if the action is successfully committed.
         *         Returns a non-zero value if an error occurs.
         */
        int commit(bool is_instigator);

        /**
         * @brief Removes all messages and checkpoints that can safely be removed
         * @return Returns 0 if the data is successfully removed.
         *         Returns a non-zero value if an error occurs.
         */
        int remove_data();

    public:
        const int SER_LOG_SIZE = 4 * sizeof(int) + time_v.size() * 3 * sizeof(int);
        const int SER_MSG_SIZE = 3 * sizeof(int) + time_v.size() * 2 * sizeof(int);
        const int SER_COMM1_SIZE = 3 * sizeof(int);
        const int SER_COMM2_SIZE = 3 * sizeof(int);
        inline int SER_COMM3_SIZE(int committed_cnt)
        {
            return (committed_cnt * time_v.size() * 2) * sizeof(int) + (3 + time_v.size()) * sizeof(int);
        };
        inline int SER_COMM4_SIZE(int committed_cnt)
        {
            return (committed_cnt * time_v.size() * 2) * sizeof(int) + 3 * sizeof(int);
        };
        inline int SER_STATE_SIZE(int arr_ctrl_size, int comm_msg_size, int comm_recv_events_size)
        {

            return (5 + (time_v.size() * 4) + ((comm_msg_size + comm_recv_events_size) * time_v.size() * 2) + (3 * arr_ctrl_size)) * sizeof(int);
        };

        /**
         * @brief   Initalizes to 0, when 0 no automatic checkpointing will be executed,
         *          otherwise if (NUM_stored_events % SAVE_CNT == 0) is true a checkpoint is made
         */
        int SAVE_CNT;

        // Copy assignment operator
        State &operator=(const State &other)
        {
            if (this != &other)
            {
                id = other.id;
                time_v = other.time_v;
                fail_v = other.fail_v;

                if (fildes)
                {
                    for (int i = 0; i < (int)time_v.size(); i++)
                    {
                        free(fildes[i]);
                    }
                    free(fildes);
                }
                fildes = (int **)malloc((int)time_v.size() * sizeof(int *));
                for (int i = 0; i < (int)time_v.size(); i++)
                {
                    fildes[i] = (int *)malloc(2 * sizeof(int));
                    fildes[i][0] = other.fildes[i][0];
                    fildes[i][1] = other.fildes[i][1];
                }

                msg_cnt = other.msg_cnt;
                msg_log_t *temp_log = (msg_log_t *)calloc(MAX_LOG, sizeof(msg_log_t));

                for (int i = 0; i < other.msg_cnt; i++)
                {
                    temp_log[i] = other.msg_log[i];
                }
                for (int i = other.msg_cnt - 1; i >= 0; i--)
                {
                    std::vector<int>().swap(msg_log[i].time_v_reciever);
                    std::vector<int>().swap(msg_log[i].time_v_sender);
                    std::vector<int>().swap(msg_log[i].fail_v_sender);

                    free(msg_log[i].msg_buf);
                }
                free(msg_log);
                msg_log = temp_log;

                checkpoints = other.checkpoints;

                ck_time_v = other.ck_time_v;

                fail_log = other.fail_log;

                arrived_msgs = other.arrived_msgs;

                arrived_ctrl = other.arrived_ctrl;

                msg_out.close();
                char filename[32];
                get_msg_filename(id, filename);
                msg_out.open(filename,
                             std::ofstream::out | std::ofstream::binary | std::ofstream::ate | std::ofstream::in);
            }
            return *this;
        }

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
         * @brief Signals a commit procedure to take place. the process that calls this function is the instigator
         *        of the commit procedure.
         * @return Returns 0 upon successful signaling of the commit.
         *         Returns a non-zero value if an error occurs during signaling.
         */
        int signal_commit();

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
         * @return 0 on success, 1 after recieving a VOID msg, 2 after recieving a CTRL msg,
         *         3 after recieving a duplicate or orphaned message that was discarded, -1 on failure.
         */
        int recv_msg(int fildes[2], char *output, int size);

        /**
         * @brief Sends control message to other processes to indicate that a failure occured.
         * @param fildes Array of arrays containing file descriptors of the pipes.
         */
        int send_ctrl();

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