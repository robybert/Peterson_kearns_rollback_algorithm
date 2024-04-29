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

void get_comm_filename(int process_nr, char commit_filename[32])
{
    sprintf(commit_filename, "commit_process%d.dat", process_nr);
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
    auto [it, inserted] = arrived_msgs.insert(merged_time_fail_v);
    if (msg->fail_v[id] == fail_v[id]) // TODO: make this work if another failure occurs before it is removed
    {
        // remove the entry from the set
        arrived_msgs.erase(it);
    }

    return !(inserted);
}

bool Pet_kea::State::check_duplicate_ctrl(struct fail_log_t log)
{
    vector<int> fail_log_vector{log.id, log.fail_nr, log.res_time};
    auto [it, inserted] = arrived_ctrl.insert(fail_log_vector);
    return !(inserted);
}

bool Pet_kea::State::check_duplicate_commit(struct msg_log_t *l_msg)
{
    vector<int> merged_time_fail_v;
    merged_time_fail_v = l_msg->time_v_sender;
    merged_time_fail_v.insert(merged_time_fail_v.end(), l_msg->fail_v_sender.begin(), l_msg->fail_v_sender.end());
    auto [it, inserted] = committed_msg_set.insert(merged_time_fail_v);
    return !(inserted);
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

    // for (int i = 0; i < (int)time_v.size(); i++)
    // {
    //     *new_ptr = *old_ptr;
    //     new_ptr++;
    //     old_ptr++;
    // }

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

        // *new_ptr = *old_ptr;
        // new_ptr++;
        // old_ptr++;
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
        // *new_ptr = *old_ptr;
        // new_ptr++;
        // old_ptr++;
        // *new_ptr = *old_ptr;
        // checkpoint_msg_cnt = *old_ptr;
        // new_ptr++;
        // old_ptr++;
        // *new_ptr = *old_ptr;
        // checkpoint_last_ckpnt = *old_ptr;
        // new_ptr++;
        // old_ptr++;

        // for (int j = 0; j < (int)time_v.size(); j++)
        // {
        //     *new_ptr = *old_ptr;
        //     new_ptr++;
        //     old_ptr++;
        // }

        // for (int k = 0; k < (checkpoint_msg_cnt - checkpoint_last_ckpnt); k++)
        // {
        //     delta_ptr = copy_log(old_ptr, new_ptr);
        //     old_ptr += delta_ptr;
        //     new_ptr += delta_ptr;
        // }
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
    cout << id << " old msg_cnt:" << final_index << " removing:" << to_remove.size() << endl;
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
            vector<int>().swap(msg_log[i].time_v_sender); // TODO: double free
            vector<int>().swap(msg_log[i].fail_v_sender);
            continue;
        }
        new_log[new_final_index] = msg_log[i];
        new_final_index++;
        vector<int>().swap(msg_log[i].time_v_reciever);
        vector<int>().swap(msg_log[i].time_v_sender); // TODO: double free
        vector<int>().swap(msg_log[i].fail_v_sender);
    }
    free(msg_log);
    msg_log = new_log;
    cout << id << " new msg_cnt:" << new_final_index << endl;
    return new_final_index;
}

// int Pet_kea::State::rem_log_entries(vector<int> to_remove, int final_index)
// {
//     cout << id << " old msg_cnt:" << final_index << " removing:" << to_remove.size() << endl;
//     msg_log_t *src = 0, *dest = 0;
//     int move_cnt = 0;
//     for (int i = to_remove.size(), j = final_index; i > 0;)
//     {
//         if (j == to_remove.back())
//         {
//             // advance dest
//             to_remove.pop_back();
//             i--;
//             final_index--;
//             dest = &msg_log[j];
//             free(msg_log[j].msg_buf);
//             vector<int>().swap(msg_log[j].time_v_reciever);
//             vector<int>().swap(msg_log[j].time_v_sender); // TODO: double free
//             vector<int>().swap(msg_log[j].fail_v_sender);
//         }
//         else
//         {
//             if (src != dest && src != 0)
//             {
//                 memmove(dest, src, sizeof(msg_log_t) * move_cnt);
//             }

//             // set src and dest to j
//             src = &msg_log[j];
//             dest = src;
//             move_cnt++;
//         }
//         j--;
//     }
//     if (src != dest && src != 0)
//     {
//         memmove(dest, src, sizeof(msg_log_t) * move_cnt);
//     }
//     cout << id << " new msg_cnt:" << final_index << endl;
//     return final_index;
// }

void Pet_kea::State::serialize_commit(struct comm_msg_t *msg, char *data)
{
    int *q = (int *)data;
    *q = (int)msg->msg_type;
    q++;

    *q = msg->sending_process_nr;
    q++;

    switch (msg->msg_type)
    {
    case COMM1:
        *q = 0; // padding
        break;

    case COMM2:
        *q = msg->time_v_j;
        break;

    case COMM3:
        *q = msg->committed_cnt;
        q++;
        for (int i = 0; i < (int)time_v.size(); i++)
        {
            *q = msg->time_v_min[i];
            q++;
        }
        for (unordered_set<vector<int>, vector_hash>::iterator ptr = msg->committed_msgs.begin(); ptr != msg->committed_msgs.end(); ptr++)
        {
            for (int i = 0; i < (int)time_v.size() * 2; i++)
            {
                *q = ptr->at(i);
                q++;
            }
        }
        break;
    case COMM4:

        *q = msg->committed_cnt;
        q++;
        for (unordered_set<vector<int>, vector_hash>::iterator ptr = msg->committed_msgs.begin(); ptr != msg->committed_msgs.end(); ptr++)
        {
            for (int i = 0; i < (int)time_v.size() * 2; i++)
            {
                *q = ptr->at(i);
                q++;
            }
        }

        break;

    default:
        break;
    }
}

void Pet_kea::State::deserialize_commit(char *data, struct comm_msg_t *msg)
{
    int *q = (int *)data;
    msg->msg_type = (msg_type)*q;
    q++;

    msg->sending_process_nr = *q;
    q++;
    vector<int> temp_vec;
    switch (msg->msg_type)
    {
    case COMM1:
        break;

    case COMM2:
        msg->time_v_j = *q;
        break;

    case COMM3:

        msg->committed_cnt = *q;
        q++;
        for (int i = 0; i < (int)time_v.size(); i++)
        {
            msg->time_v_min.push_back(*q);
            q++;
        }

        for (int i = 0; i < msg->committed_cnt; i++)
        {
            for (int j = 0; j < (int)time_v.size() * 2; j++)
            {
                temp_vec.push_back(*q);
                q++;
            }

            msg->committed_msgs.insert(temp_vec);
            temp_vec.clear();
        }
        break;
    case COMM4:

        msg->committed_cnt = *q;
        q++;
        for (int i = 0; i < msg->committed_cnt; i++)
        {
            for (int j = 0; j < (int)time_v.size() * 2; j++)
            {
                temp_vec.push_back(*q);
                q++;
            }

            msg->committed_msgs.insert(temp_vec);
            temp_vec.clear();
        }

        break;

    default:
        break;
    }
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

    for (set<pair<int, vector<int>>>::iterator ptr = msg->recieved_msgs.begin(); ptr != msg->recieved_msgs.end(); ptr++) // TODO: is this correct?
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
    *q = log->next_checkpoint;
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
    log->next_checkpoint = *q;
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

    *q = arrived_msgs.size();
    q++;

    *q = arrived_ctrl.size();
    q++;

    *q = committed_msg_set.size();
    q++;

    *q = committed_recieve_events.size();
    q++;

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        *q = time_v[i];
        q++;
    }
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        *q = time_v_min[i];
        q++;
    }
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        *q = commit_v[i];
        q++;
    }

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        *q = remove_v[i];
        q++;
    }

    for (unordered_set<vector<int>, vector_hash>::iterator ptr = arrived_msgs.begin(); ptr != arrived_msgs.end(); ptr++)
    {
        for (int i = 0; i < (int)time_v.size() * 2; i++)
        {
            *q = ptr->at(i);
            q++;
        }
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
    for (unordered_set<vector<int>, vector_hash>::iterator ptr = committed_msg_set.begin(); ptr != committed_msg_set.end(); ptr++)
    {
        for (int i = 0; i < (int)time_v.size() * 2; i++)
        {
            *q = ptr->at(i);
            q++;
        }
    }
    for (unordered_set<vector<int>, vector_hash>::iterator ptr = committed_recieve_events.begin(); ptr != committed_recieve_events.end(); ptr++)
    {
        for (int i = 0; i < (int)time_v.size() * 2; i++)
        {
            *q = ptr->at(i);
            q++;
        }
    }
}

void Pet_kea::State::deserialize_state(char *data)
{
    int *q = (int *)data;
    id = *q;
    q++;

    msg_cnt = *q;
    q++;

    int arrived_msgs_size = *q;
    q++;

    int arrived_ctrl_size = *q;
    q++;

    int committed_msg_set_size = *q;
    q++;

    int committed_recieve_events_size = *q;
    q++;

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        time_v.push_back(*q);
        q++;
    }

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        time_v_min.push_back(*q);
        q++;
    }

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        commit_v.push_back(*q);
        q++;
    }

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        remove_v.push_back(*q);
        q++;
    }
    vector<int> temp_vec;

    for (int i = 0; i < arrived_msgs_size; i++)
    {
        for (int j = 0; j < (int)time_v.size() * 2; j++)
        {
            temp_vec.push_back(*q);
            q++;
        }
        arrived_msgs.insert(temp_vec);
        temp_vec.clear();
    }

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

    for (int i = 0; i < committed_msg_set_size; i++)
    {
        for (int j = 0; j < (int)time_v.size() * 2; j++)
        {
            temp_vec.push_back(*q);
            q++;
        }
        committed_msg_set.insert(temp_vec);
        temp_vec.clear();
    }

    for (int i = 0; i < committed_recieve_events_size; i++)
    {
        for (int j = 0; j < (int)time_v.size() * 2; j++)
        {
            temp_vec.push_back(*q);
            q++;
        }
        committed_recieve_events.insert(temp_vec);
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

        print_ctrl_msg(&msg);

        // send the control message (serialize)
        size_t size = SER_SIZE_CTRL_MSG_T(msg.recieved_cnt, fail_v.size());
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
    msg_log[msg_cnt].next_checkpoint = checkpoints.size();
    msg_log[msg_cnt].msg_buf = (char *)malloc(msg->msg_size);
    memcpy(msg_log[msg_cnt].msg_buf, msg->msg_buf, msg->msg_size);
    msg_log[msg_cnt].time_v_reciever = time_v;
    if (recipient == -1)
    {
        msg_log[msg_cnt].time_v_sender = msg->time_v;
        msg_log[msg_cnt].fail_v_sender = msg->fail_v;
        // msg_log[msg_cnt].time_v_reciever = time_v;
        msg_log[msg_cnt].process_id = msg->sending_process_nr;
        msg_log[msg_cnt].recipient = true;
    }
    else
    {
        msg_log[msg_cnt].time_v_sender = time_v;
        // msg_log[msg_cnt].time_v_reciever = time_v;
        msg_log[msg_cnt].fail_v_sender = fail_v;
        msg_log[msg_cnt].process_id = recipient;
        msg_log[msg_cnt].recipient = false;
    }

    msg_cnt++;
    if (msg_cnt % SAVE_CNT == 0)
        checkpoint();

    return 0;
}

int Pet_kea::State::commit_msgs(vector<int> msgs)
{
    // TODO: think of how to commit the message maybe another file?
    char filename[32];
    get_comm_filename(id, filename);

    for (int iterator : msgs)
    {
        char data[msg_log[iterator].msg_size + SER_LOG_SIZE];
        serialize_log(&msg_log[iterator], data);
        commit_out.write(data, msg_log[iterator].msg_size + SER_LOG_SIZE);
    }
    commit_out.flush();

    return 0;
}

int Pet_kea::State::rollback(struct ctrl_msg_t *msg)
{
    cout << "entered the rollback section" << endl;
    print_ctrl_msg(msg);

    // RB.2
    if (time_v[msg->sending_process_nr] > msg->log_entry.res_time)
    {
        //  RB.2.1      remove ckeckpoints T^i > crT^i, set state and T to latest ckeckpoint, replays
        int prev_cnt = msg_cnt, to_remove = 0, size = ck_time_v.size() - 1, checkpoint_T_i = ck_time_v.back().at(msg->sending_process_nr);

        while (checkpoint_T_i > msg->log_entry.res_time)
        {
            checkpoint_T_i = ck_time_v.at(size - to_remove).at(msg->sending_process_nr);
            to_remove++;
        }
        if (to_remove > 1)
        {
            to_remove--;

            // remove the to_remove checkpoints
            int bytes_to_remove = to_remove * (sizeof(int) * (3 + time_v.size())) + (checkpoints.back() - checkpoints[size - to_remove]) * SER_LOG_SIZE;

            for (int i = checkpoints[size - to_remove]; i < checkpoints.back(); i++)
            {
                bytes_to_remove += msg_log[i].msg_size * sizeof(char); // TODO: check the size
            }

            char filename[32];
            get_msg_filename(id, filename);
            std::filesystem::path p;
            int file_size = std::filesystem::file_size(std::filesystem::path(filename));
            file_size -= bytes_to_remove;
            truncate(filename, file_size);
            for (int i = 0; i < to_remove; i++)
            {
                checkpoints.pop_back();
                ck_time_v.pop_back();
            }
        }

        // set state and time_v to latestck
        msg_cnt = checkpoints.back();
        time_v = ck_time_v.back();

        // replay messages
        std::vector<int> indices_to_rem;

        for (int i = msg_cnt; i < prev_cnt; i++)
        {
            if (msg_log[i].time_v_sender[msg->sending_process_nr] <= msg->log_entry.res_time && msg_log[i].time_v_sender[id] >= ck_time_v.back()[id])
            {
                // replay messages only inc time_v and msg_cnt
                cout << "replayed msg" << endl;
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
            prev_cnt = rem_log_entries(indices_to_rem, prev_cnt);
            indices_to_rem.clear();
        }

        //  RB.2.2
        char filename[32];
        get_fail_filename(id, filename);
        ofstream fail_out(filename, ofstream::out | ofstream::binary | ofstream::app);
        fail_out.seekp(0, ofstream::end);

        struct fail_log_t entry = {msg->sending_process_nr, msg->log_entry.fail_nr, msg->log_entry.res_time};
        fail_out.write((char *)&entry, sizeof(fail_log_t));
        fail_out.close();

        fail_log.push_back(fail_log_t(msg->sending_process_nr, msg->log_entry.fail_nr, msg->log_entry.res_time));

        //  RB.2.3
        fail_v[msg->sending_process_nr] = msg->log_entry.fail_nr;

        // RB.3

        for (int i = 0; i < prev_cnt; i++) // TODO: check if you have to go from the beginning
        {
            // move recv event to the back??? TODO: ask if this is what is meant with RB.3.2
            if (msg_log[i].recipient && msg_log[i].time_v_reciever[msg->sending_process_nr] > msg->log_entry.res_time)
            {
                cout << id << " moved RECV event to the back" << endl;
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
            if (!msg_log[i].recipient && msg_log[i].process_id == msg->sending_process_nr && !(msg->recieved_msgs.contains(pair<int, vector<int>>(msg_log[i].time_v_sender[id], msg_log[i].fail_v_sender)))) // TODO: check if contains is working
            {
                cout << id << " retransmitted msg Tj: " << msg_log[i].time_v_sender[id] << "fail_v: ";
                for (int j = 0; j < (int)fail_v.size(); j++)
                {
                    cout << msg_log[i].fail_v_sender[j] << ":";
                }

                cout << " res_time: " << msg_log[i].time_v_sender[msg->sending_process_nr] << endl;
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
            prev_cnt = msg_cnt;
            msg_cnt -= indices_to_rem.size();
            prev_cnt = rem_log_entries(indices_to_rem, prev_cnt);
        }

        checkpoint();
    }
    return 0;
}

Pet_kea::State::State(int process_nr, int process_cnt, int (*fd)[2], bool restart) : id(process_nr),
                                                                                     time_v(process_cnt, 0),
                                                                                     fail_v(process_cnt, 0),
                                                                                     msg_cnt(0),
                                                                                     commit_v(process_cnt, false),
                                                                                     remove_v(process_cnt, false),
                                                                                     arrived_msgs()

{
    time_v_min = vector(process_cnt, 0);
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
        char state_file[file_size];
        state_in.read(state_file, file_size);
        state_in.close();
        deserialize_state(state_file);

        get_msg_filename(id, filename);
        ifstream msg_in(filename, ifstream::in | ifstream::binary);
        msg_in.seekg(0, msg_in.end);
        file_size = msg_in.tellg();
        char msg_file[file_size];
        msg_in.read(msg_file, file_size);
        msg_in.close();

        int *curr_pos = (int *)msg_file;

        // msg_cnt = *curr_pos;
        // curr_pos++;
        // curr_pos++;
        int num_checkpoints = *curr_pos;
        curr_pos++;

        // for (int i = 0; i < (int)time_v.size(); i++)
        // {
        //     time_v[i] = *curr_pos;
        //     curr_pos++;
        // }

        msg_log = (msg_log_t *)calloc(MAX_LOG, sizeof(msg_log_t));

        checkpoints.push_back(0);
        ck_time_v.push_back(vector<int>(process_cnt, 0));

        int read_msg_cnt = 0;
        for (int i = 0; i < num_checkpoints; i++)
        {
            // curr_pos++;
            int ck_msg_cnt = *curr_pos;
            curr_pos++;
            checkpoints.push_back(*curr_pos);
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

        msg_out.open(filename, ofstream::out | ofstream::binary | ofstream::app);
        get_comm_filename(id, filename);
        commit_out.open(filename, ofstream::out | ofstream::binary | ofstream::app);
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
        }

        ofstream fail_out(filename, ofstream::out | ofstream::binary | ofstream::app);
        fail_out.seekp(0, ofstream::end);
        fail_v[id]++;
        struct fail_log_t entry = {id, fail_v[id], time_v[id]};
        fail_out.write((char *)&entry, sizeof(fail_log_t));
        fail_out.close();

        fail_log.push_back(fail_log_t(id, fail_v[id], time_v[id]));

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
        get_comm_filename(id, filename);
        commit_out.open(filename, ofstream::out | ofstream::binary | ofstream::trunc);

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
        // if (msg_log[i].recipient)
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

int Pet_kea::State::checkpoint()
{
    // write state and time vector at the start of the file

    int *update = (int *)malloc(sizeof(int) * 2);
    // *update = id;
    // update++;
    *update = msg_cnt;
    update++;
    *update = checkpoints.back();
    // update++;
    // *update = checkpoints.size();
    // update--;
    // update--;
    update--;

    msg_out.seekp(0, ofstream::beg);
    int num_checkpoints = checkpoints.size();
    msg_out.write((char *)&num_checkpoints, sizeof(int));

    int *time_v_buffer = (int *)malloc(sizeof(int) * time_v.size());
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        time_v_buffer[i] = time_v[i];
    }
    // msg_out.write((char *)time_v_buffer, sizeof(int) * time_v.size());

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
    int state_size = SER_STATE_SIZE(arrived_msgs.size(), arrived_ctrl.size(), committed_msg_set.size(), committed_recieve_events.size());

    char data[state_size];
    serialize_state(data);

    state_out.write(data, state_size);
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

int Pet_kea::State::signal_commit()
{
    struct comm_msg_t msg;
    msg.msg_type = COMM1;
    msg.sending_process_nr = id;
    char data[SER_COMM1_SIZE];
    serialize_commit(&msg, data);

    // write to all other processes
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        if (i == id)
            continue;
        if (write(fildes[i][1], data, SER_COMM1_SIZE) < 0)
        {
            // TODO:handle error
        }
    }
    commit_v[id] = true;
    return 0;
}

int Pet_kea::State::send_commit(int target_id)
{
    struct comm_msg_t msg;
    msg.msg_type = COMM2;
    msg.sending_process_nr = id;
    msg.time_v_j = ck_time_v.back().at(id);

    char data[SER_COMM2_SIZE];
    serialize_commit(&msg, data);
    // write back to sender

    if (write(fildes[target_id][1], data, SER_COMM2_SIZE) < 0)
    {
        // TODO:handle error
    }
    return 0;
}

int Pet_kea::State::remove_data()
{
    vector<int> indices_to_remove, checkpoints_to_remove;
    vector<int> temp_vec;
    unordered_set<std::vector<int>, vector_hash> next_committed_recieve_events;

    // remove checkpoint
    for (int i = 1; i < (int)checkpoints.size() - 1; i++)
    {
        if (ck_time_v[i + 1] <= time_v_min)
        {
            checkpoints_to_remove.push_back(i);
        }
    }

    // remove messages
    for (int i = 0; i < msg_cnt; i++)
    {
        temp_vec.clear();
        temp_vec = msg_log[i].time_v_sender;
        temp_vec.insert(temp_vec.end(), msg_log[i].fail_v_sender.begin(), msg_log[i].fail_v_sender.end());
        if (msg_log[i].next_checkpoint >= (int)checkpoints.size())
        {
            msg_log[i].next_checkpoint = next_checkpoint_after_rem(checkpoints_to_remove, msg_log[i].next_checkpoint);
            if (!msg_log[i].recipient && committed_recieve_events.contains(temp_vec))
            {
                next_committed_recieve_events.insert(temp_vec);
            }
            continue;
        }
        if (msg_log[i].recipient && ck_time_v.at(msg_log[i].next_checkpoint) <= time_v_min) // tODO: fix removing Ts and Fs from arrived messages
        {
            // remove form msg_log
            indices_to_remove.push_back(i);
            committed_msg_set.erase(temp_vec);
            continue;
        }

        if (!msg_log[i].recipient && committed_recieve_events.contains(temp_vec)) // TODO: check comparing vectors
        {
            if (ck_time_v.at(msg_log[i].next_checkpoint) <= time_v_min)
            {

                // remove from msg_log
                indices_to_remove.push_back(i);
                committed_msg_set.erase(temp_vec);
                continue;
            }
            else
            {
                next_committed_recieve_events.insert(temp_vec);
            }
        }
        msg_log[i].next_checkpoint = next_checkpoint_after_rem(checkpoints_to_remove, msg_log[i].next_checkpoint);
    }
    msg_cnt = rem_log_entries(indices_to_remove, msg_cnt);

    rem_checkpoints(checkpoints_to_remove);
    committed_recieve_events.clear();
    committed_recieve_events = next_committed_recieve_events;

    // for (int i = 0; i < msg_cnt; i++)
    // {
    //     if (msg_log[i].recipient)
    //     {
    //         cout << id << " recv event with time_v:";
    //         for (int j = 0; j < (int)time_v.size(); j++)
    //         {
    //             cout << msg_log[i].time_v_reciever[j] << ":";
    //         }
    //     }
    //     else
    //     {
    //         cout << id << " send event with time_v:";
    //         for (int j = 0; j < (int)time_v.size(); j++)
    //         {
    //             cout << msg_log[i].time_v_sender[j] << ":";
    //         }
    //     }
    //     cout << " next_checkpoint:";
    //     if (msg_log[i].next_checkpoint < (int)ck_time_v.size())
    //     {
    //         for (int j = 0; j < (int)time_v.size(); j++)
    //         {
    //             cout << ck_time_v[msg_log[i].next_checkpoint].at(j) << ":";
    //         }
    //         cout << endl;
    //     }
    //     else
    //     {
    //         cout << " not yet checkpointed" << endl;
    //     }
    // }

    // remove fail_log not possible in asynchronos setting
    return 0;
}

int Pet_kea::State::commit(bool is_instigator)
{
    // commit the messagesTODO:fix this
    vector<int> committed_msgs;
    for (int i = 0; i < msg_cnt; i++)
    {
        if ((msg_log[i].recipient ? msg_log[i].time_v_reciever : msg_log[i].time_v_sender) <= time_v_min && !check_duplicate_commit(&msg_log[i]))
        {
            committed_msgs.push_back(i);
        }
    }
    commit_msgs(vector<int>(committed_msgs));

    // remove information

    unordered_set<vector<int>, vector_hash> committed_set[time_v.size()];
    struct comm_msg_t msg;
    if (is_instigator)
    {
        msg.msg_type = COMM3;
        msg.time_v_min = time_v_min;
    }
    else
    {
        msg.msg_type = COMM4;
    }
    msg.sending_process_nr = id;
    vector<int> merged_vector;

    // for (int i = committed_msgs.back(); !committed_msgs.empty(); i = committed_msgs.back())
    int it;
    while (!committed_msgs.empty())
    {
        it = committed_msgs.back();
        if (msg_log[it].recipient)
        {
            merged_vector = msg_log[it].time_v_sender;
            merged_vector.insert(merged_vector.end(), msg_log[it].fail_v_sender.begin(), msg_log[it].fail_v_sender.end());
            committed_set[msg_log[it].process_id].insert(merged_vector);
            merged_vector.clear();
        }

        committed_msgs.pop_back();
        // i = committed_msgs.back();
    }

    remove_v[id] = true;
    int max_cnt = 0;
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        if (max_cnt < (int)committed_set[i].size())
            max_cnt = committed_set[i].size();
    }
    char *data = (char *)malloc(msg.msg_type == COMM3 ? SER_COMM3_SIZE(max_cnt) : SER_COMM4_SIZE(max_cnt));
    // write to all other processes
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        if (i == id)
            continue;

        msg.committed_msgs = committed_set[i];
        msg.committed_cnt = committed_set[i].size();

        if (is_instigator)
        {

            cout << id << " sending comm3 to " << i << "commit_cnt: " << msg.committed_cnt << "time_v_min:";
            for (int j = 0; j < (int)time_v.size(); j++)
            {
                cout << msg.time_v_min[j] << ":";
            }
            cout << endl;
        }
        else
        {
            cout << id << " sending comm4 to " << i << "commit_cnt: " << msg.committed_cnt << endl;
        }

        serialize_commit(&msg, data);

        if (write(fildes[i][1], data, (msg.msg_type == COMM3 ? SER_COMM3_SIZE(msg.committed_cnt) : SER_COMM4_SIZE(msg.committed_cnt))) < 0)
        {
            // TODO:handle error
        }
        msg.committed_msgs.clear();
    }
    free(data);
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
    try
    {
        if (write(fildes[process_id][1], data, SER_MSG_SIZE + size) < 0)
            runtime_error("failed to write");
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
    size_t init_read_size = SER_COMM1_SIZE;

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

            char *c_data = (char *)malloc(SER_SIZE_CTRL_MSG_T(*q, fail_v.size()));
            memcpy(c_data, data, init_read_size);

            extra_data = c_data + init_read_size;

            ret = read(fildes[0], extra_data, SER_SIZE_CTRL_MSG_T(*q, fail_v.size()) - init_read_size);

            struct ctrl_msg_t c_msg;
            deserialize_ctrl(c_data, &c_msg);
            free(c_data);
            free(data);
            if (check_duplicate_ctrl(c_msg.log_entry))
                return 2;

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
        else if (COMM1 == (msg_type)*q)
        {
            cout << id << " entered COMM1" << endl;
            q++;
            send_commit(*q);
            free(data);
            return 4;
        }
        else if (COMM2 == (msg_type)*q)
        {
            cout << id << " entered COMM2" << endl;

            ret = read(fildes[0], extra_data, SER_COMM2_SIZE - init_read_size);
            struct comm_msg_t comm2_msg;
            deserialize_commit(data, &comm2_msg);
            free(data);
            time_v_min[comm2_msg.sending_process_nr] = comm2_msg.time_v_j;
            commit_v[comm2_msg.sending_process_nr] = true;
            if (commit_v == vector<bool>(time_v.size(), true))
            {
                commit_v.flip();
                time_v_min[id] = ck_time_v.back().at(id);
                commit(true);
            }
            return 4;
        }
        else if (COMM3 == (msg_type)*q)
        {
            cout << id << " entered COMM3" << endl;
            q++;
            q++;
            char *comm3_data = (char *)malloc(SER_COMM3_SIZE(*q));
            memcpy(comm3_data, data, init_read_size);

            extra_data = comm3_data + init_read_size;

            ret = read(fildes[0], extra_data, SER_COMM3_SIZE(*q) - init_read_size);

            struct comm_msg_t comm3_msg;
            deserialize_commit(comm3_data, &comm3_msg);
            free(comm3_data);
            free(data);
            time_v_min = comm3_msg.time_v_min;

            commit(false);
            committed_recieve_events.insert(comm3_msg.committed_msgs.begin(), comm3_msg.committed_msgs.end());
            // committed_msg_set.insert(comm3_msg.committed_msgs.begin(), comm3_msg.committed_msgs.end());
            remove_v[comm3_msg.sending_process_nr] = true;
            return 4;
        }
        else if (COMM4 == (msg_type)*q) // TODO: write this
        {
            cout << id << " entered COMM4" << endl;
            q++;
            q++;
            char *comm4_data = (char *)malloc(SER_COMM4_SIZE(*q));
            memcpy(comm4_data, data, init_read_size);

            extra_data = comm4_data + init_read_size;

            ret = read(fildes[0], extra_data, SER_COMM4_SIZE(*q) - init_read_size);
            struct comm_msg_t comm4_msg;
            deserialize_commit(comm4_data, &comm4_msg);
            free(comm4_data);
            free(data);
            committed_recieve_events.insert(comm4_msg.committed_msgs.begin(), comm4_msg.committed_msgs.end());
            remove_v[comm4_msg.sending_process_nr] = true;
            if (remove_v == vector<bool>(time_v.size(), true))
            {
                remove_v.flip();
                remove_data();
            }
            return 4;
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
