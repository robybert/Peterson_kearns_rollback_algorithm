#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <unistd.h>
#include <string>

using namespace std;

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

const int BSIZE = sizeof(msg_t);
const int SER_ERR_MSG_SIZE = sizeof(int) * 5;
const int SEED = 555432;
const int CHILDREN = 3;

void serialize(struct msg_t *msg, char *data);

void deserialize(char *data, struct msg_t *msg);

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
 * @brief: Sends the ERR message to re-establish the FD
 * @param process_nr: The process number of the calling process
 * @param buffer: The message buffer for sending the ERR message
 * @param fildes: The FD of the pipes
 * @return: Returns 0 for succes, -1 for failure
 */
// int send_err_msg(int process_nr, msg_t *buffer, int fildes[CHILDREN][2]);

/**
 * @brief: Recieves the ERR message to close and re-establish the FD
 * @param buffer: The message buffer for sending the ERR message
 * @param fildes: The FD of the pipes
 * @return: Returns 0 for succes, -1 for failure
 */
// int recv_err_msg(struct msg_t *buffer, int fildes[CHILDREN][2]);

/**
 * @brief: Sends a message
 * @param process_nr: The process number of the calling process
 * @param dest_process_nr: The process number of the destination process
 * @param buffer: Message buffer, the msg_buf needs to be populated before calling this function
 * @param fildes: The FD of the pipes
 * @param restart: Enable if the process needs to send their FD to the other processes
 * @return: Returns the pid of the created process, for failure -1
 */
int send_msg(int process_nr, int dest_process_nr, struct msg_t buffer, int fildes[CHILDREN][2]);

/**
 * @brief: Lets processes send messages to eachother
 * @param process_nr: The process number of the calling process
 * @param fildes: The FD of the pipes
 * @param restart: Enable if the process needs to send its FD to the other processes
 */
void msg_process(int process_nr, int fildes[CHILDREN][2], int sv[CHILDREN][2], bool restart);

#endif