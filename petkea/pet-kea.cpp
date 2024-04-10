#include "pet-kea.hpp"

using namespace std;

void get_fail_filename(int process_nr, char fail_v_filename[32])
{
    sprintf(fail_v_filename, "fail_v_process_%d.dat", process_nr);
}

void get_msg_filename(int process_nr, char fail_v_filename[32])
{
    sprintf(fail_v_filename, "msg_process_%d.dat", process_nr);
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
    vector<int> merged_time_fail_v(msg->time_v.size() + msg->fail_v.size());
    merge(msg->time_v.begin(), msg->time_v.end(), msg->fail_v.begin(), msg->fail_v.end(), merged_time_fail_v.begin());
    auto [it, inserted] = arrived_msgs.insert(merged_time_fail_v);
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

int Pet_kea::State::rem_log_entries(vector<int> to_remove, int final_index)
{
    msg_log_t *src = 0, *dest = 0;
    int move_cnt = 0;
    for (int i = to_remove.size(), j = final_index; i > 0;)
    {
        if (j == to_remove.back())
        {
            // advance dest
            to_remove.pop_back();
            i--;
            final_index--;
            dest = &msg_log[j];
            free(msg_log[j].msg_buf);
            vector<int>().swap(msg_log[j].time_v_reciever);
            vector<int>().swap(msg_log[j].time_v_sender);
            vector<int>().swap(msg_log[j].fail_v_sender);
        }
        else
        {
            if (src != dest && src != 0)
            {
                memmove(dest, src, sizeof(msg_log_t) * move_cnt);
            }

            // set src and dest to j
            src = &msg_log[j];
            dest = src;
            move_cnt++;
        }
        j--;
    }
    if (src != dest && src != 0)
    {
        memmove(dest, src, sizeof(msg_log_t) * move_cnt);
    }

    return final_index;
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
    msg_log[msg_cnt].msg_buf = (char *)malloc(msg->msg_size);
    memcpy(msg_log[msg_cnt].msg_buf, msg->msg_buf, msg->msg_size);

    if (recipient == -1)
    {
        msg_log[msg_cnt].time_v_sender = msg->time_v;
        msg_log[msg_cnt].fail_v_sender = msg->fail_v;
        msg_log[msg_cnt].time_v_reciever = time_v;
        msg_log[msg_cnt].process_id = msg->sending_process_nr;
        msg_log[msg_cnt].recipient = true;
    }
    else
    {
        msg_log[msg_cnt].time_v_sender = time_v;
        msg_log[msg_cnt].time_v_reciever = std::vector<int>(time_v.size(), 0);
        msg_log[msg_cnt].fail_v_sender = fail_v;
        msg_log[msg_cnt].process_id = recipient;
        msg_log[msg_cnt].recipient = false;
    }

    msg_cnt++;
    if (msg_cnt % SAVE_CNT == 0)
        checkpoint();

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
        get_msg_filename(id, filename);
        ifstream msg_in(filename, ifstream::in | ifstream::binary);
        msg_in.seekg(0, msg_in.end);
        size_t file_size = msg_in.tellg();
        msg_in.seekg(0, ifstream::beg);
        char msg_file[file_size];
        msg_in.read(msg_file, file_size);
        msg_in.close();

        int *curr_pos = (int *)msg_file;

        // msg_in.read((char *)update, sizeof(int) * 4);

        id = *curr_pos;
        curr_pos++;
        msg_cnt = *curr_pos;
        curr_pos++;
        curr_pos++;
        int num_checkpoints = *curr_pos;
        curr_pos++;
        // free(update);
        // update = (int *)malloc(time_v.size() * sizeof(int));

        // msg_in.read((char *)update, time_v.size() * sizeof(int));
        for (int i = 0; i < (int)time_v.size(); i++)
        {
            time_v[i] = *curr_pos;
            curr_pos++;
        }
        // free(update - time_v.size());

        // TODO: change this to accomodate for changeable size
        // int begin_log = msg_in.tellg();
        // msg_in.seekg(0, msg_in.end);
        // int end_log = msg_in.tellg();

        // msg_in.seekg(begin_log, ifstream::beg);

        msg_log = (msg_log_t *)calloc(MAX_LOG, sizeof(msg_log_t));
        // char log_buffer[end_log];
        // msg_in.read(log_buffer, end_log);
        int read_msg_cnt = 0;
        for (int i = 0; i < num_checkpoints; i++)
        {
            curr_pos++;
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

        msg_out.open(filename, ofstream::out | ofstream::binary | ofstream::app); // TODO: this is where .dat files get cleared
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

int Pet_kea::State::checkpoint()
{
    // write state and time vector at the start of the file

    int *update = (int *)malloc(sizeof(int) * 4);
    *update = id;
    update++;
    *update = msg_cnt;
    update++;
    *update = checkpoints.back();
    update++;
    *update = checkpoints.size();
    update--;
    update--;
    update--;

    msg_out.seekp(0, ofstream::beg);
    msg_out.write((char *)update, sizeof(int) * 4);

    int *time_v_buffer = (int *)malloc(sizeof(int) * time_v.size());
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        time_v_buffer[i] = time_v[i];
    }
    msg_out.write((char *)time_v_buffer, sizeof(int) * time_v.size());

    // append last messages
    msg_out.seekp(0, ofstream::end);
    size_t log_size = SER_LOG_SIZE;

    msg_out.write((char *)update, sizeof(int) * 3);
    free(update);

    // for (int i = 0; i < (int)time_v.size(); i++)
    // {
    //     time_v_buffer[i] = time_v[i];
    // }
    msg_out.write((char *)time_v_buffer, sizeof(int) * time_v.size());

    free(time_v_buffer);

    for (int i = checkpoints.back(); i < msg_cnt; i++)
    {
        char data[msg_log[i].msg_size + log_size];
        serialize_log(&msg_log[i], data);
        msg_out.write(data, msg_log[i].msg_size + log_size);
    }
    checkpoints.push_back(msg_cnt);
    ck_time_v.push_back(time_v);
    msg_out.flush();

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
    size_t init_read_size = SER_SIZE_CTRL_MSG_T(0, 0);

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

        if (CTRL == (msg_type)*q)
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