#include "pet-kea.hpp"

using namespace std;

void get_fail_filename(int process_nr, char fail_filename[32])
{
    sprintf(fail_filename, "fail_v_process_%d.dat", process_nr);
}

void get_msg_filename(int process_nr, char msg_filename[32])
{
    sprintf(msg_filename, "msg_process_%d.dat", process_nr);
}

void get_state_filename(int process_nr, char state_filename[32])
{
    sprintf(state_filename, "state_process%d.dat", process_nr);
}

void Pet_kea::print_msg(struct msg_t *msg)
{
    cout << "MSG  time vector(";
    for (int i = 0; i < (int)msg->time_v.size(); i++)
    {
        cout << msg->time_v[i];
        if (i < (int)msg->time_v.size())
        {
            cout << ", ";
        }
    }
    cout << ") fail vector(";
    for (int i = 0; i < (int)msg->fail_v.size(); i++)
    {
        cout << msg->fail_v[i];
        if (i < (int)msg->fail_v.size())
        {
            cout << ", ";
        }
    }
    cout << ")" << endl;
}

void Pet_kea::print_ctrl_msg(struct ctrl_msg_t *msg)
{
    cout << "CTRL with log(" << msg->log_entry.id << ", " << msg->log_entry.fail_nr << ", " << msg->log_entry.res_time << ") messages recieved: " << msg->recieved_cnt << endl;
    for (set<pair<int, vector<int>>>::iterator ptr = msg->recieved_msgs.begin(); ptr != msg->recieved_msgs.end(); ptr++)
    {
        cout << "       Tj: " << ptr->first << " fail_v: ";
        for (int j = 0; j < (int)ptr->second.size(); j++)
        {
            cout << ptr->second[j] << ":";
        }
        cout << " lenght: " << ptr->second.size() << endl;
    }
}

int *Pet_kea::State::next_checkpoint(int *ptr)
{
    int to_skip = *ptr - *(ptr + 1);
    ptr += 2 + time_v.size();

    for (int i = 0; i < to_skip; i++)
    {
        ptr += ((SER_LOG_SIZE + *ptr) / sizeof(int));
    }
    return ptr;
}

char *Pet_kea::State::get_msg(int i)
{
    return msg_log[i].msg_buf;
}

char **Pet_kea::State::get_msg_log()
{
    char **output_log = (char **)malloc(msg_cnt * sizeof(char *));
    for (int i = 0; i < msg_cnt; i++)
    {
        output_log[i] = msg_log[i].msg_buf;
    }

    return output_log;
}

bool Pet_kea::State::check_duplicate(struct msg_t *msg)
{

    vector<int> merged_time_fail_v;
    merged_time_fail_v = msg->time_v;
    merged_time_fail_v.insert(merged_time_fail_v.end(), msg->fail_v.begin(), msg->fail_v.end());
    if (arrived_msgs.contains(merged_time_fail_v))
        return true;

    arrived_msgs.insert(merged_time_fail_v);
    return false;
}

bool Pet_kea::State::check_duplicate_ctrl(struct fail_log_t log)
{
    vector<int> fail_log_vector{log.id, log.fail_nr, log.res_time};
    if (arrived_ctrl.contains(fail_log_vector))
        return true;

    arrived_ctrl.insert(fail_log_vector);
    return false;
}

bool Pet_kea::State::check_orphaned(struct msg_t *msg)
{
    for (int i = 0; i < (int)fail_log.size(); i++)
    {
        if (msg->fail_v[fail_log[i].id] < fail_log[i].fail_nr && msg->time_v[fail_log[i].id] > fail_log[i].res_time)
            return true;
        else
            continue;
    }

    return false;
}

void Pet_kea::State::rem_checkpoints(vector<int> to_remove)
{
    // read file and reconstruct it

    vector<int> reverse_to_remove = to_remove;
    reverse(reverse_to_remove.begin(), reverse_to_remove.end());
    char filename[32];
    get_msg_filename(id, filename);
    msg_out.close();
    ifstream msg_in(filename, ifstream::in | ifstream::binary);
    msg_in.seekg(0, msg_in.end);
    size_t file_size = msg_in.tellg();
    msg_in.seekg(0, ifstream::beg);
    char msg_file[file_size];
    msg_in.read(msg_file, file_size);
    msg_in.close();

    char new_msg_file[file_size];

    int *old_ptr = (int *)msg_file;
    int *new_ptr = (int *)new_msg_file;

    old_ptr++;
    new_ptr++;

    int checkpoint_msg_cnt = 0, checkpoint_last_ckpnt = 0;
    int checkpoint_cnt = (int)checkpoints.size();
    vector<int> new_checkpoints;
    vector<std::vector<int>> new_ck_time_v;
    new_checkpoints.push_back(0);
    new_ck_time_v.push_back(ck_time_v[0]);
    int *temp_ptr;

    for (int i = 1; i < checkpoint_cnt; i++)
    {
        if (!reverse_to_remove.empty() && reverse_to_remove.back() == i)
        {
            reverse_to_remove.pop_back();
            old_ptr = next_checkpoint(old_ptr);
            continue;
        }

        temp_ptr = old_ptr;

        checkpoint_last_ckpnt = checkpoint_msg_cnt;
        checkpoint_msg_cnt += *old_ptr - *(old_ptr + 1);
        *new_ptr = checkpoint_msg_cnt;
        new_ptr++;
        old_ptr++;
        *new_ptr = checkpoint_last_ckpnt;
        new_ptr++;
        old_ptr++;

        temp_ptr = next_checkpoint(temp_ptr);

        memcpy(new_ptr, old_ptr, (temp_ptr - old_ptr) * sizeof(int));

        new_checkpoints.push_back(checkpoint_msg_cnt);
        new_ck_time_v.push_back(ck_time_v[i]);

        new_ptr += (temp_ptr - old_ptr);
        old_ptr = temp_ptr;
    }

    checkpoints.swap(new_checkpoints);
    ck_time_v.swap(new_ck_time_v);

    size_t test = (size_t)new_ptr;
    size_t test2 = (size_t)(int *)&new_msg_file;

    file_size = new_ptr - (int *)&new_msg_file;
    file_size = test - test2;

    new_ptr = (int *)new_msg_file;

    *new_ptr = checkpoints.size() - 1;

    msg_out.open(filename, ofstream::out | ofstream::binary | ofstream::trunc);
    msg_out.write(new_msg_file, file_size);
    return;
}

int Pet_kea::State::next_checkpoint_after_rem(vector<int> removed_checkpoints, int curr_next_checkpoint)
{
    int result = curr_next_checkpoint;
    for (const int &i : removed_checkpoints)
    {
        if (i < curr_next_checkpoint)
            result--;
        else
            break;
    }
    return result;
}

int Pet_kea::State::rem_log_entries(vector<int> to_remove, int final_index)
{
    // cout << id << " old msg_cnt:" << final_index << " removing:" << to_remove.size() << endl;
    msg_log_t *new_log = (msg_log_t *)calloc(MAX_LOG, sizeof(msg_log_t));

    vector<int>::iterator curr = to_remove.begin();
    int new_final_index = 0;
    for (int i = 0; i < msg_cnt; i++)
    {
        if (curr != to_remove.end() && i == *curr)
        {
            curr++;
            free(msg_log[i].msg_buf);
            vector<int>().swap(msg_log[i].time_v_reciever);
            vector<int>().swap(msg_log[i].time_v_sender);
            vector<int>().swap(msg_log[i].fail_v_sender);
            continue;
        }
        new_log[new_final_index] = msg_log[i];
        new_final_index++;
        vector<int>().swap(msg_log[i].time_v_reciever);
        vector<int>().swap(msg_log[i].time_v_sender);
        vector<int>().swap(msg_log[i].fail_v_sender);
    }
    free(msg_log);
    msg_log = new_log;
    // cout << id << " new msg_cnt:" << new_final_index << endl;
    return new_final_index;
}

void Pet_kea::State::serialize_ctrl(struct ctrl_msg_t *msg, char *data)
{
    int *q = (int *)data;
    *q = (int)msg->msg_type;
    q++;

    *q = msg->recieved_cnt;
    q++;

    *q = msg->sending_process_nr;
    q++;

    *q = msg->log_entry.id;
    q++;
    *q = msg->log_entry.fail_nr;
    q++;
    *q = msg->log_entry.res_time;
    q++;

    for (set<pair<int, vector<int>>>::iterator ptr = msg->recieved_msgs.begin(); ptr != msg->recieved_msgs.end(); ptr++)
    {
        *q = ptr->first;
        q++;
        for (int j = 0; j < (int)fail_v.size(); j++)
        {
            *q = ptr->second[j];
            q++;
        }
    }
}

void Pet_kea::State::deserialize_ctrl(char *data, struct ctrl_msg_t *msg)
{
    int *q = (int *)data;
    msg->msg_type = (msg_type)*q;
    q++;

    msg->recieved_cnt = *q;
    q++;

    msg->sending_process_nr = *q;
    q++;

    msg->log_entry.id = *q;
    q++;
    msg->log_entry.fail_nr = *q;
    q++;
    msg->log_entry.res_time = *q;
    q++;

    pair<int, vector<int>> temp_pair;
    for (int i = 0; i < msg->recieved_cnt; i++)
    {

        temp_pair.first = *q;
        q++;
        for (int j = 0; j < (int)fail_v.size(); j++)
        {
            temp_pair.second.push_back(*q);
            q++;
        }

        msg->recieved_msgs.insert(temp_pair);
        temp_pair.second.clear();
    }
}

void Pet_kea::State::serialize(struct msg_t *msg, char *data)
{
    int *q = (int *)data;
    *q = msg->msg_type;
    q++;

    *q = msg->msg_size;
    q++;

    *q = msg->sending_process_nr;
    q++;
    if (msg->msg_type == MSG)
    {
        for (int i = 0; i < (int)time_v.size(); i++)
        {
            *q = msg->time_v[i];
            q++;
        }

        for (int i = 0; i < (int)fail_v.size(); i++)
        {
            *q = msg->fail_v[i];
            q++;
        }
    }

    memcpy(q, msg->msg_buf, msg->msg_size);
}
void Pet_kea::State::deserialize(char *data, struct msg_t *msg)
{
    int *q = (int *)data;
    msg->msg_type = (msg_type)*q;
    q++;

    msg->msg_size = *q;
    q++;

    msg->sending_process_nr = *q;
    q++;
    if (msg->msg_type == MSG)
    {
        for (int i = 0; i < (int)time_v.size(); i++)
        {
            msg->time_v.push_back(*q);
            q++;
        }

        for (int i = 0; i < (int)fail_v.size(); i++)
        {
            msg->fail_v.push_back(*q);
            q++;
        }
    }

    msg->msg_buf = (char *)malloc(msg->msg_size);
    memcpy(msg->msg_buf, q, msg->msg_size);
}

void Pet_kea::State::serialize_log(struct msg_log_t *log, char *data)
{
    int *q = (int *)data;
    *q = log->msg_size;
    q++;
    *q = log->recipient;
    q++;
    *q = log->process_id;
    q++;

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        *q = log->time_v_sender[i];
        q++;
    }

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        *q = log->time_v_reciever[i];
        q++;
    }

    for (int i = 0; i < (int)fail_v.size(); i++)
    {
        *q = log->fail_v_sender[i];
        q++;
    }

    memcpy(q, log->msg_buf, log->msg_size);
}
int Pet_kea::State::deserialize_log(char *data, struct msg_log_t *log)
{
    int *q = (int *)data;
    log->msg_size = *q;
    q++;
    log->recipient = *q;
    q++;
    log->process_id = *q;
    q++;

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        log->time_v_sender.push_back(*q);
        q++;
    }

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        log->time_v_reciever.push_back(*q);
        q++;
    }

    for (int i = 0; i < (int)fail_v.size(); i++)
    {
        log->fail_v_sender.push_back(*q);
        q++;
    }

    log->msg_buf = (char *)malloc(log->msg_size);
    memcpy(log->msg_buf, q, log->msg_size);

    return (q - (int *)data) * sizeof(int) + log->msg_size;
}

void Pet_kea::State::serialize_state(char *data)
{
    int *q = (int *)data;
    *q = id;
    q++;

    *q = msg_cnt;
    q++;

    *q = arrived_ctrl.size();
    q++;

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        *q = time_v[i];
        q++;
    }

    for (unordered_set<vector<int>, vector_hash>::iterator ptr = arrived_ctrl.begin(); ptr != arrived_ctrl.end(); ptr++)
    {
        *q = ptr->at(0);
        q++;
        *q = ptr->at(1);
        q++;
        *q = ptr->at(2);
        q++;
    }
}

void Pet_kea::State::deserialize_state(char *data)
{
    int *q = (int *)data;
    id = *q;
    q++;

    msg_cnt = *q;
    q++;

    int arrived_ctrl_size = *q;
    q++;

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        time_v[i] = *q;
        q++;
    }

    vector<int> temp_vec;
    for (int i = 0; i < arrived_ctrl_size; i++)
    {
        temp_vec.push_back(*q);
        q++;
        temp_vec.push_back(*q);
        q++;
        temp_vec.push_back(*q);
        q++;
        arrived_ctrl.insert(temp_vec);
        temp_vec.clear();
    }
}

void Pet_kea::State::send_ctrl()
{
    set<pair<int, std::vector<int>>> recvd_msgs[time_v.size()];

    vector<int> cnt(time_v.size(), 0);
    struct fail_log_t fail_log = {id, fail_v[id], time_v[id]};

    pair<int, vector<int>> temp_pair;
    for (int i = 0; i < msg_cnt; i++)
    {
        if (!msg_log[i].recipient)
        {
            continue;
        }

        temp_pair.first = msg_log[i].time_v_sender[msg_log[i].process_id];
        temp_pair.second = msg_log[i].fail_v_sender;
        recvd_msgs[msg_log[i].process_id].insert(temp_pair);
        cnt[msg_log[i].process_id]++;
    }

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        // prepare and send the ctrl messages
        if (i == id)
            continue;
        struct ctrl_msg_t msg;
        msg.msg_type = CTRL;
        msg.sending_process_nr = id;
        msg.log_entry = fail_log;
        msg.recieved_cnt = cnt[i];
        msg.recieved_msgs = recvd_msgs[i];

        // print_ctrl_msg(&msg);

        // send the control message (serialize)
        size_t size = SER_SIZE_CTRL_MSG_T(msg.recieved_cnt);
        char *data = (char *)malloc(size);
        serialize_ctrl(&msg, data);

        int ret = write(fildes[i][1], data, size);
        if (ret < 0)
        {
            // TODO: do error checking
        }

        free(data);
    }
}

int Pet_kea::State::store_msg(struct msg_t *msg, int recipient)
{
    // store the message
    if (msg_cnt >= MAX_LOG)
    {
        // increase max size
        cout << "max log size reached of process " << id << endl;
        // TODO: handle reached max log size
        return -1;
    }

    msg_log[msg_cnt].msg_size = msg->msg_size;
    msg_log[msg_cnt].msg_buf = (char *)malloc(msg->msg_size);
    memcpy(msg_log[msg_cnt].msg_buf, msg->msg_buf, msg->msg_size);
    msg_log[msg_cnt].time_v_reciever = time_v;
    if (recipient == -1)
    {
        msg_log[msg_cnt].time_v_sender = msg->time_v;
        msg_log[msg_cnt].fail_v_sender = msg->fail_v;
        msg_log[msg_cnt].process_id = msg->sending_process_nr;
        msg_log[msg_cnt].recipient = true;
    }
    else
    {
        msg_log[msg_cnt].time_v_sender = time_v;
        msg_log[msg_cnt].fail_v_sender = fail_v;
        msg_log[msg_cnt].process_id = recipient;
        msg_log[msg_cnt].recipient = false;
    }

    msg_cnt++;
    if (automatic_checkpointing && msg_cnt % SAVE_CNT == 0)
        checkpoint();

    return 0;
}

int Pet_kea::State::rollback(struct ctrl_msg_t *msg)
{
    // cout << "entered the rollback section" << endl;
    // print_ctrl_msg(msg);

    // RB.2
    char filename[32];
    get_fail_filename(id, filename);
    ofstream fail_out(filename, ofstream::out | ofstream::binary | ofstream::app);
    // fail_out.seekp(0, ofstream::end);

    struct fail_log_t entry = {msg->sending_process_nr, msg->log_entry.fail_nr, msg->log_entry.res_time};
    fail_out.write((char *)&entry, sizeof(fail_log_t));
    fail_out.close();

    fail_log.push_back(entry);

    //  RB.2.3
    fail_v[msg->sending_process_nr] = msg->log_entry.fail_nr;
    if (time_v[msg->sending_process_nr] > msg->log_entry.res_time)
    {
        //  RB.2.1      remove ckeckpoints T^i > crT^i, set state and T to latest ckeckpoint, replays
        vector<int> checkpoints_to_remove;

        for (int i = 0; i < (int)checkpoints.size(); i++)
        {
            if (ck_time_v[i].at(msg->sending_process_nr) > msg->log_entry.res_time)
                checkpoints_to_remove.push_back(i);
        }

        rem_checkpoints(checkpoints_to_remove);

        // set state and time_v to latestck
        int prev_cnt = msg_cnt;
        msg_cnt = checkpoints.back();
        int temp_msg_cnt = msg_cnt;
        time_v = ck_time_v.back();

        // replay messages
        std::vector<int> indices_to_rem;

        for (int i = msg_cnt; i < prev_cnt; i++)
        {
            if (msg_log[i].time_v_sender[msg->sending_process_nr] <= msg->log_entry.res_time && msg_log[i].time_v_sender[id] >= ck_time_v.back()[id])
            {
                // replay messages only inc time_v and msg_cnt
                // cout << "replayed msg" << endl;
                msg_cnt++;
                if (msg_log[i].recipient)
                {
                    time_v[id]++;
                    for (int j = 0; j < (int)time_v.size(); j++)
                    {
                        if (j == id)
                            continue;
                        time_v.at(j) = max(msg_log[i].time_v_sender[j], time_v[j]);
                    }
                }
                else
                {
                    time_v[id]++;
                }
            }
            else if (!msg_log[i].recipient && msg_log[i].time_v_sender[msg->sending_process_nr] > msg->log_entry.res_time)
            {
                // add to remove vector
                indices_to_rem.push_back(i);
            }
            else if (msg_log[i].recipient && msg_log[i].time_v_reciever[msg->sending_process_nr] > msg->log_entry.res_time && msg_log[i].time_v_sender[msg->sending_process_nr] > msg->log_entry.res_time && msg_log[i].fail_v_sender[msg->sending_process_nr] < msg->log_entry.fail_nr)
            {
                // add to remove vector
                indices_to_rem.push_back(i);
            }
        }
        if (!indices_to_rem.empty())
        {
            msg_cnt = rem_log_entries(indices_to_rem, prev_cnt);
            indices_to_rem.clear();
        }

        //  RB.2.2

        // RB.3

        for (int i = temp_msg_cnt; i < msg_cnt; i++) // TODO: check if you have to go from the beginning
        {
            // move recv event to the back??? TODO: ask if this is what is meant with RB.3.2
            if (msg_log[i].recipient && msg_log[i].time_v_reciever[msg->sending_process_nr] > msg->log_entry.res_time)
            {
                // cout << id << " moved RECV event to the back" << endl;
                if (msg_cnt >= MAX_LOG)
                {
                    // increase max size
                    cout << "max log size reached of process " << id << endl;
                    return -1;
                }

                msg_log[msg_cnt].msg_size = msg_log[i].msg_size;
                msg_log[msg_cnt].recipient = true;
                msg_log[msg_cnt].process_id = msg_log[i].process_id;
                msg_log[msg_cnt].msg_buf = (char *)malloc(msg_log[i].msg_size);
                memcpy(msg_log[msg_cnt].msg_buf, msg_log[i].msg_buf, msg_log[i].msg_size);

                msg_log[msg_cnt].time_v_sender = msg_log[i].time_v_sender;
                msg_log[msg_cnt].fail_v_sender = msg_log[i].fail_v_sender;
                msg_log[msg_cnt].time_v_reciever = msg_log[i].time_v_reciever;

                msg_cnt++;

                time_v[id]++;
                for (int j = 0; j < (int)time_v.size(); j++)
                {
                    if (j == id)
                        continue;
                    time_v.at(j) = max(msg_log[i].time_v_sender[i], time_v[i]);
                }

                // remove the duplicate msg from the log
                indices_to_rem.push_back(i);
            }

            // retransmit send events that have not arrived RB.3.3
            if (!msg_log[i].recipient && msg_log[i].process_id == msg->sending_process_nr && !(msg->recieved_msgs.contains(pair<int, vector<int>>(msg_log[i].time_v_sender[id], msg_log[i].fail_v_sender))))
            {
                // cout << id << " retransmitted msg Tj: " << msg_log[i].time_v_sender[id] << "fail_v: ";
                // for (int j = 0; j < (int)fail_v.size(); j++)
                // {
                //     cout << msg_log[i].fail_v_sender[j] << ":";
                // }

                // cout << " res_time: " << msg_log[i].time_v_sender[msg->sending_process_nr] << endl;
                struct msg_t retransmit_msg;
                retransmit_msg.msg_type = MSG;
                retransmit_msg.sending_process_nr = id;
                retransmit_msg.time_v = msg_log[i].time_v_sender;
                retransmit_msg.fail_v = msg_log[i].fail_v_sender;
                retransmit_msg.msg_size = msg_log[i].msg_size;
                retransmit_msg.msg_buf = (char *)malloc(retransmit_msg.msg_size * sizeof(char));
                memcpy(retransmit_msg.msg_buf, msg_log[i].msg_buf, retransmit_msg.msg_size);

                char data[SER_MSG_SIZE + msg_log[i].msg_size];
                serialize(&retransmit_msg, data);

                // send the message
                if (write(fildes[msg_log[i].process_id][1], data, SER_MSG_SIZE + msg_log[i].msg_size) < 0)
                {
                    // TODO:handle error
                }
            }
        }
        if (!indices_to_rem.empty())
        {

            msg_cnt = rem_log_entries(indices_to_rem, msg_cnt);
        }

        checkpoint();
    }
    return 0;
}

void Pet_kea::State::swap_log(State &first, const State &second)
{
    for (int i = 0; i < first.msg_cnt; i++)
    {
        first.msg_log[i].time_v_reciever = second.msg_log[i].time_v_reciever;
        first.msg_log[i].time_v_sender = second.msg_log[i].time_v_sender;
        first.msg_log[i].fail_v_sender = second.msg_log[i].fail_v_sender;

        first.msg_log[i].msg_buf = (char *)malloc(second.msg_log[i].msg_size);
        memcpy(first.msg_log[i].msg_buf, second.msg_log[i].msg_buf, second.msg_log[i].msg_size);
    }
}

Pet_kea::State::State(int process_nr, int process_cnt, int (*fd)[2], bool restart) : id(process_nr),
                                                                                     time_v(process_cnt, 0),
                                                                                     fail_v(process_cnt, 0),
                                                                                     msg_cnt(0),
                                                                                     automatic_checkpointing(false),
                                                                                     arrived_msgs()

{
    fildes = (int **)malloc(process_cnt * sizeof(int *));
    for (int i = 0; i < process_cnt; i++)
    {
        fildes[i] = (int *)malloc(2 * sizeof(int));
        fildes[i][0] = fd[i][0];
        fildes[i][1] = fd[i][1];
    }
    if (restart)
    {
        char filename[32];
        get_state_filename(id, filename);
        ifstream state_in(filename, ifstream::in | ifstream::binary);
        state_in.seekg(0, ifstream::end);
        size_t file_size = state_in.tellg();
        state_in.seekg(0, ifstream::beg);
        char state_file[file_size];
        state_in.read(state_file, file_size);
        state_in.close();
        deserialize_state(state_file);

        get_msg_filename(id, filename);
        ifstream msg_in(filename, ifstream::in | ifstream::binary);
        msg_in.seekg(0, msg_in.end);
        file_size = msg_in.tellg();
        msg_in.seekg(0, ifstream::beg);
        char msg_file[file_size];
        msg_in.read(msg_file, file_size);
        msg_in.close();

        int *curr_pos = (int *)msg_file;

        int num_checkpoints = *curr_pos;
        curr_pos++;

        msg_log = (msg_log_t *)calloc(MAX_LOG, sizeof(msg_log_t));

        checkpoints.push_back(0);
        ck_time_v.push_back(vector<int>(process_cnt, 0));

        int read_msg_cnt = 0;
        for (int i = 0; i < num_checkpoints; i++)
        {
            // curr_pos++;
            int ck_msg_cnt = *curr_pos;
            checkpoints.push_back(*curr_pos);
            curr_pos++;
            int to_read = ck_msg_cnt - *curr_pos;
            curr_pos++;
            std::vector<int> temp_ck_time_v;

            for (int j = 0; j < (int)time_v.size(); j++)
            {
                temp_ck_time_v.push_back(*curr_pos);
                curr_pos++;
            }
            ck_time_v.push_back(temp_ck_time_v);

            for (int k = 0, ret = 0; k < to_read; k++, read_msg_cnt++)
            {
                ret = deserialize_log((char *)curr_pos, &msg_log[read_msg_cnt]);
                curr_pos += ret / sizeof(int);
            }
        }

        // detect lost messages if crash happened during checkpoint
        if (msg_cnt != read_msg_cnt)
        {
            msg_cnt = read_msg_cnt;
            if (msg_cnt == 0)
            {
                time_v = vector<int>(process_cnt, 0);
            }
            else
            {
                time_v = msg_log[msg_cnt - 1].time_v_sender;
            }
        }

        // insert all recieved msgs in arrived_msgs
        vector<int> merged_time_fail_v;
        for (int i = 0; i < msg_cnt; i++)
        {
            if (msg_log[i].recipient)
            {

                merged_time_fail_v = msg_log[i].time_v_sender;
                merged_time_fail_v.insert(merged_time_fail_v.end(), msg_log[i].fail_v_sender.begin(), msg_log[i].fail_v_sender.end());
                arrived_msgs.insert(merged_time_fail_v);
            }
        }

        msg_out.open(filename, ofstream::out | ofstream::binary | ofstream::ate | ofstream::in);
        get_fail_filename(id, filename);

        ifstream fail_in(filename, ifstream::in | ifstream::binary);

        fail_in.seekg(0, fail_in.end);
        file_size = fail_in.tellg();
        fail_in.seekg(0, ifstream::beg);
        char fail_file[file_size];
        fail_in.read(fail_file, file_size);
        fail_in.close();
        // TODO: wrtie failure log recovery
        curr_pos = (int *)fail_file;
        fail_log_t temp_fail_log;
        while (curr_pos < (int *)(fail_file + file_size))
        {
            temp_fail_log.id = *curr_pos;
            curr_pos++;
            temp_fail_log.fail_nr = *curr_pos;
            curr_pos++;
            temp_fail_log.res_time = *curr_pos;
            curr_pos++;
            fail_log.push_back(temp_fail_log);
            fail_v[temp_fail_log.id] = max(fail_v[temp_fail_log.id], temp_fail_log.fail_nr);
        }

        ofstream fail_out(filename, ofstream::out | ofstream::binary | ofstream::app);
        fail_out.seekp(0, ofstream::end);
        fail_v[id]++;
        struct fail_log_t entry = {id, fail_v[id], time_v[id]};
        fail_out.write((char *)&entry, sizeof(fail_log_t));
        fail_out.close();

        fail_log.push_back(entry);

        send_ctrl();
        // exit(0);
    }
    else
    {
        char filename[32];
        get_fail_filename(id, filename);
        ofstream fail_out(filename, ofstream::out | ofstream::binary | ofstream::trunc);
        get_msg_filename(id, filename);
        msg_out.open(filename, ofstream::out | ofstream::binary | ofstream::trunc);

        struct fail_log_t entry = {id, 0, 0};
        fail_out.write((char *)&entry, sizeof(fail_log_t));
        fail_out.close();

        fail_log.push_back(fail_log_t(id, 0, 0));

        msg_log = (msg_log_t *)calloc(MAX_LOG, sizeof(msg_log_t));
        checkpoints.push_back(0);
        ck_time_v.push_back(vector<int>(process_cnt, 0));
    }
}

Pet_kea::State::~State()
{
    for (int i = msg_cnt - 1; i >= 0; i--)
    {
        std::vector<int>().swap(msg_log[i].time_v_reciever);
        std::vector<int>().swap(msg_log[i].time_v_sender);
        std::vector<int>().swap(msg_log[i].fail_v_sender);

        free(msg_log[i].msg_buf);
    }

    free(msg_log);
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        free(fildes[i]);
    }

    free(fildes);
    msg_out.close();
}
int Pet_kea::State::get_msg_cnt()
{
    return msg_cnt;
}

int Pet_kea::State::checkpoint()
{
    // write state and time vector at the start of the file

    int *update = (int *)malloc(sizeof(int) * 2);
    *update = msg_cnt;
    update++;
    *update = checkpoints.back();
    update--;

    msg_out.seekp(0, ofstream::beg);
    int *num_checkpoints = (int *)malloc(sizeof(int));
    *num_checkpoints = checkpoints.size();
    msg_out.write((char *)num_checkpoints, sizeof(int));
    free(num_checkpoints);

    int *time_v_buffer = (int *)malloc(sizeof(int) * time_v.size());
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        time_v_buffer[i] = time_v[i];
    }

    // append last messages
    msg_out.seekp(0, ofstream::end);

    msg_out.write((char *)update, sizeof(int) * 2);
    free(update);

    msg_out.write((char *)time_v_buffer, sizeof(int) * time_v.size());

    free(time_v_buffer);

    for (int i = checkpoints.back(); i < msg_cnt; i++)
    {
        char data[msg_log[i].msg_size + SER_LOG_SIZE];
        serialize_log(&msg_log[i], data);
        msg_out.write(data, msg_log[i].msg_size + SER_LOG_SIZE);
    }
    checkpoints.push_back(msg_cnt);
    ck_time_v.push_back(time_v);
    msg_out.flush();

    char filename[32];
    get_state_filename(id, filename);

    ofstream state_out(filename, ofstream::out | ofstream::binary | ofstream::trunc);
    int state_size = SER_STATE_SIZE(arrived_ctrl.size());

    char data[state_size];
    serialize_state(data);

    state_out.write(data, state_size);
    state_out.close();
    return 0;
}

int Pet_kea::send_void(char *input, int fildes[2], int size)
{
    char data[SER_VOID_SIZE + size];

    int *q = (int *)data;
    *q = VOID;
    q++;
    *q = size;
    q++;
    memcpy(q, input, size);

    if (write(fildes[1], data, SER_VOID_SIZE + size) < 0)
    {
        // TODO:handle error
    }

    return 0;
}

int Pet_kea::State::send_msg(char *input, int process_id, int size)
{
    // inc T^i
    time_v[id]++;

    struct msg_t msg;

    msg.msg_type = MSG;
    msg.sending_process_nr = id;
    msg.time_v = time_v;
    msg.fail_v = fail_v;
    msg.msg_size = size;
    msg.msg_buf = (char *)malloc(size * sizeof(char));
    memcpy(msg.msg_buf, input, size * sizeof(char));

    char data[SER_MSG_SIZE + size];
    serialize(&msg, data);

    // send the message
    int ret;
    try
    {
        ret = write(fildes[process_id][1], data, SER_MSG_SIZE + size); // TODO: write to broken pipe handle error
        if (ret < 0)
        {
            throw runtime_error("failed to write");
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << endl;
        perror("write failed");
        return -1;
    }

    store_msg(&msg, process_id);

    return 0;
}

int Pet_kea::State::recv_msg(int fildes[2], char *output, int size)
{
    // read message
    int ret;
    size_t init_read_size = SER_VOID_SIZE;

    try
    {
        char *extra_data, *data = (char *)malloc(SER_MSG_SIZE + size * sizeof(char)); // TODO: check size
        extra_data = data;
        ret = read(fildes[0], data, init_read_size); // TODO: check size
        // do err checking
        if (ret < 0)
        {
        }

        extra_data += init_read_size;

        int *q = (int *)data;

        if (MSG == (msg_type)*q)
        {
            ret = read(fildes[0], extra_data, SER_MSG_SIZE + size - init_read_size);
            struct msg_t msg;
            deserialize(data, &msg);
            free(data);

            if (check_duplicate(&msg))
            {
                return 3;
            }

            if (check_orphaned(&msg))
            {
                return 3;
            }

            time_v[id]++;
            for (int i = 0; i < (int)time_v.size(); i++)
            {
                if (i == id)
                    continue;
                time_v.at(i) = max(msg.time_v[i], time_v[i]);
            }

            // inc T^i and inc T^/j to max(T^j of send event, prev event T^j)

            store_msg(&msg, -1);

            memcpy(output, msg.msg_buf, msg.msg_size);
        }
        else if (CTRL == (msg_type)*q)
        {

            q++;

            char *c_data = (char *)malloc(SER_SIZE_CTRL_MSG_T(*q));
            memcpy(c_data, data, init_read_size);

            extra_data = c_data + init_read_size;

            ret = read(fildes[0], extra_data, SER_SIZE_CTRL_MSG_T(*q) - init_read_size);

            struct ctrl_msg_t c_msg;
            deserialize_ctrl(c_data, &c_msg);
            free(c_data);
            free(data);
            if (check_duplicate_ctrl(c_msg.log_entry))
            {

                for (int i = 0; i < msg_cnt; i++)
                {
                    if (!msg_log[i].recipient && msg_log[i].process_id == c_msg.sending_process_nr && !(c_msg.recieved_msgs.contains(pair<int, vector<int>>(msg_log[i].time_v_sender[id], msg_log[i].fail_v_sender))))
                    {
                        struct msg_t retransmit_msg;
                        retransmit_msg.msg_type = MSG;
                        retransmit_msg.sending_process_nr = id;
                        retransmit_msg.time_v = msg_log[i].time_v_sender;
                        retransmit_msg.fail_v = msg_log[i].fail_v_sender;
                        retransmit_msg.msg_size = msg_log[i].msg_size;
                        retransmit_msg.msg_buf = (char *)malloc(retransmit_msg.msg_size * sizeof(char));
                        memcpy(retransmit_msg.msg_buf, msg_log[i].msg_buf, retransmit_msg.msg_size);

                        char data[SER_MSG_SIZE + msg_log[i].msg_size];
                        serialize(&retransmit_msg, data);

                        // send the message
                        if (write(this->fildes[msg_log[i].process_id][1], data, SER_MSG_SIZE + msg_log[i].msg_size) < 0)
                        {
                            // TODO:handle error
                        }
                    }
                }
                return 2;
            }

            rollback(&c_msg);
            return 2;
        }
        else if (VOID == (msg_type)*q)
        {
            q++;

            int v_size = *q;

            char *v_data = (char *)malloc(SER_MSG_SIZE + v_size);
            memcpy(v_data, data, init_read_size);
            free(data);
            extra_data = v_data + init_read_size;

            ret = read(fildes[0], extra_data, SER_VOID_SIZE + v_size - init_read_size);

            // process the void message aka give to method caller
            q = (int *)v_data;
            q++;
            q++;
            memcpy(output, q, v_size);
            free(v_data);
            return 1;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << endl;
        perror("failed in recv_msg");
    }

    return 0;
}

int Pet_kea::State::update_fd(int process_id, int fd[2])
{
    if (process_id < 0 || process_id >= (int)time_v.size())
        return -1;

    fildes[process_id][0] = fd[0];
    fildes[process_id][1] = fd[1];
    return 0;
}
