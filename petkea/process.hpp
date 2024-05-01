/**
 * @file process.hpp
 * @brief Header file containing the structs and function for msg_process
 */

#ifndef _PROCESS_HPP_
#define _PROCESS_HPP_

#include <unistd.h>
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
#include <csignal>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <thread>

const int CHILDREN = 2;
#include "pet-kea.hpp"

const int SER_ERR_MSG_SIZE = sizeof(int) * 5;
const int SEED = 555432;

typedef enum message_type
{
    MSG,
    ERR
} message_type;

struct ptp_msg
{
    char msg[64];
};

struct ptp_err
{
    pid_t pid;
    int fildes[2];
};

struct msg_t
{
    message_type type;
    int sending_process_nr;
    union
    {
        struct ptp_msg ptp_msg;
        struct ptp_err ptp_err;
    };
};

/**
 * @brief Makes pipe and handles errors
 * @param fildes The file descriptor of the pipes
 * @return Returns 0 for success and -1 for failure
 */
int make_pipe(int fildes[2], int sv[2]);

/**
 * @brief Forks the current process and lets it execute msg_process()
 * @param process_nr The process number of the calling process
 * @param fildes The FD of the pipes
 * @param restart Enable if the process needs to send their FD to the other processes
 * @return Returns the pid of the created process, for failure -1
 */
pid_t fork_process(int process_nr, int fildes[CHILDREN][2], int sv[CHILDREN][2], bool restart);

/**
 * @brief Kills and Restarts the specified process
 * @param process_nr The process number of the process to restart
 * @param pid The process id of the process to restart
 * @param fildes The FD of the pipes
 * @return Returns the pid of the newly created process
 */
pid_t restart_process(int process_nr, pid_t pid, int fildes[CHILDREN][2], int sv[CHILDREN][2]);

/**
 * @brief Serializes a message structure into a character array.
 * @param msg Pointer to the message structure to be serialized.
 * @param data Pointer to the character array where the serialized data will be stored.
 */
void serialize(struct msg_t *msg, char *data);

/**
 * @brief Deserializes a message from a character array.
 * @param data Pointer to the character array containing serialized data.
 * @param msg Pointer to the message structure where deserialized data will be stored.
 */
void deserialize(char *data, struct msg_t *msg);

/**
 * @brief Sends the ERR message to re-establish the FD
 * @param process_nr The process number of the calling process
 * @param state the state of the peterson-kearns class
 * @param fildes The file descriptors of the pipes
 * @return Returns 0 for succes, -1 for failure
 */
int send_err_msg(int process_nr, int fildes[CHILDREN][2]);

/**
 * @brief Recieves the CTRL message to close and re-establish the FD
 * @param buffer The message buffer for sending the CTRL message
 * @param fildes The FD of the pipes
 * @return Returns 0 for succes, -1 for failure
 */
int recv_err_msg(struct msg_t *buffer, int fildes[CHILDREN][2]);

/**
 * @brief Sends a message
 * @param msg Message buffer, the msg_buf needs to be populated before calling this function
 * @param fildes The file descriptor of the pipes
 * @param state The state of the peterson-kearns class
 * @return Returns the pid of the created process, for failure -1
 */
int send_msg(struct msg_t *msg, int process_id, Pet_kea::State *state);

/**
 * @brief recieves a message
 * @param msg Message buffer, the function will poulate the struct
 * @param fildes The file descriptor of the pipes
 * @param state The state of the peterson-kearns class
 * @return Returns the pid of the created process, for failure -1
 */
int recv_msg(struct msg_t *msg, int fildes[2], Pet_kea::State *state, int64_t *rollback_duration_arr);

/**
 * @brief Lets processes send messages to eachother
 * @param process_nr The process number of the calling process
 * @param fildes The file descriptor of the pipes
 * @param restart Enable if the process needs to send its file descriptors to the other processes
 */
void msg_process(int process_nr, int fildes[CHILDREN][2], int sv[CHILDREN][2], bool restart);

#endif