#include "pet-kea.hpp"

using namespace std;

void print_fail(int process_nr, char fail_v_filename[32])
{
    sprintf(fail_v_filename, "fail_v_process_%d.dat", process_nr);
}

void print_msg(int process_nr, char fail_v_filename[32])
{
    sprintf(fail_v_filename, "msg_process_%d.dat", process_nr);
}

void Pet_kea::deserialize_ctrl(char *data, struct ctrl_msg_t *msg)
{
    // TODO: deserialize ctrl msg
}

void Pet_kea::serialize(struct msg_t *msg, char *data)
{
    int *q = (int *)data;
    *q = msg->msg_type;
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

        char *p = (char *)q;
        for (int i = 0; i < 64; i++)
        {
            *p = msg->contents.ptp_msg.msg_buf[i];
            p++;
        }
    }
    else if (msg->msg_type == ERR)
    {
        // TODO: write serialization for errmsg
        *q = msg->contents.ptp_err.pid;
        q++;
        *q = msg->contents.ptp_err.fildes[0];
        q++;
        *q = msg->contents.ptp_err.fildes[1];
        q++;

        // serialize ctrl_msg_t
        *q = msg->contents.ptp_err.msg->log_entry.id;
        q++;
        *q = msg->contents.ptp_err.msg->log_entry.fail_nr;
        q++;
        *q = msg->contents.ptp_err.msg->log_entry.res_time;
        q++;

        *q = msg->contents.ptp_err.msg->recieved_cnt;
        q++;

        for (int i = 0; i < msg->contents.ptp_err.msg->recieved_cnt; i++)
        {
            *q = msg->contents.ptp_err.msg->recieved->first;
            q++;
            for (int j = 0; j < (int)fail_v.size(); j++)
            {
                *q = msg->contents.ptp_err.msg->recieved->second[i];
                q++;
            }
        }
    }
}
void Pet_kea::deserialize(char *data, struct msg_t *msg)
{
    int *q = (int *)data;
    msg->msg_type = (msg_type)*q;
    q++;
    msg->sending_process_nr = *q;
    q++;

    if (msg->msg_type == MSG)
    {
        for (int i = 0; i < (int)time_v.size(); i++)
        {
            msg->time_v[i] = *q; //////
            q++;
        }

        for (int i = 0; i < (int)fail_v.size(); i++)
        {
            msg->fail_v[i] = *q;
            q++;
        }
        char *p = (char *)q;
        for (int i = 0; i < 64; i++)
        {
            msg->contents.ptp_msg.msg_buf[i] = *p;
            p++;
        }
    }
    else
    {
        // TODO: write deserialization for errmsg
        msg->contents.ptp_err.pid = *q;
        q++;
        msg->contents.ptp_err.fildes[0] = *q;
        q++;
        msg->contents.ptp_err.fildes[1] = *q;
        q++;

        // serialize ctrl_msg_t
        msg->contents.ptp_err.msg->log_entry.id = *q;
        q++;
        msg->contents.ptp_err.msg->log_entry.fail_nr = *q;
        q++;
        msg->contents.ptp_err.msg->log_entry.res_time = *q;
        q++;

        msg->contents.ptp_err.msg->recieved_cnt = *q;
        q++;

        for (int i = 0; i < msg->contents.ptp_err.msg->recieved_cnt; i++)
        {
            msg->contents.ptp_err.msg->recieved->first = *q;
            q++;
            for (int j = 0; j < (int)fail_v.size(); j++)
            {
                msg->contents.ptp_err.msg->recieved->second[i] = *q;
                q++;
            }
        }
    }
}

void Pet_kea::send_ctrl(int fildes[][2], struct msg_t *msg)
{

    pair<int, vector<int>> *recvd_pair[time_v.size()]; // TODO: malloc
    vector<int> cnt(time_v.size(), 0);
    struct fail_log_t fail_log = {id, fail_v[id], time_v[id]};

    for (int i = 0; i < msg_cnt; i++)
    {
        // TODO: maybe a check here if processid in msg_t is changed
        recvd_pair[msg_log[i].process_id][cnt[msg_log[i].process_id]].first = msg_log[i].time_v[msg_log[i].process_id];
        recvd_pair[msg_log[i].process_id][cnt[msg_log[i].process_id]].second = msg_log[i].fail_v;
        cnt[msg_log[i].process_id]++;
    }

    for (int i = 0; i < (int)time_v.size(); i++)
    {
        // TODO: prepare and send the ctrl messages
        if (i == id)
            continue;
        struct ctrl_msg_t *ctrl_msg = (ctrl_msg_t *)malloc(sizeof(ctrl_msg_t));
        ctrl_msg->log_entry = fail_log;
        ctrl_msg->recieved_cnt = cnt[i];
        ctrl_msg->recieved = recvd_pair[i];
        msg->msg_type = ERR;
        msg->sending_process_nr = id;
        msg->contents.ptp_err.msg = ctrl_msg;
        // TODO: send the control message (serialize)
        char *data = (char *)malloc(sizeof(msg_t) + sizeof(recvd_pair[i])); // TODO: fix size
        serialize(msg, data);

        int ret = write(fildes[i][1], data, sizeof(data));
        if (ret < 0)
        {
            // TODO: do error checking
        }
    }
}

void Pet_kea::recv_ctrl()
{
}

int Pet_kea::store_msg(struct msg_t *msg, bool recipient)
{
    // store the message
    if (msg_cnt >= MAX_LOG)
    {
        // increase max size
        cout << "max log size reached of process " << id << endl;
        return -1;
    }

    sprintf(msg_log[msg_cnt].msg_buf, msg->contents.ptp_msg.msg_buf);
    msg_log[msg_cnt].recipient = recipient;
    msg_log[msg_cnt].process_id = msg->sending_process_nr; // TODO: look at the sending and recving process nr in thesis
    msg_log[msg_cnt].time_v = time_v;
    msg_log[msg_cnt].fail_v = fail_v;
    msg_cnt++;
    if (msg_cnt % SAVE_CNT == 0)
        checkpoint();

    cout << id << "     " << recipient << "      " << msg->contents.ptp_msg.msg_buf << "     time_v: " << time_v[0] << "-" << time_v[1] << endl;
    return 0;
}

Pet_kea::Pet_kea(int process_nr, int process_cnt) : id(process_nr),
                                                    time_v(process_cnt, 0),
                                                    fail_v(process_cnt, 0),
                                                    msg_cnt(0),
                                                    last_checkpoint(0)
{
    char filename[32];
    print_fail(id, filename);
    ofstream fail_out(filename, ios::out | ios::binary);
    print_msg(id, filename);
    msg_out.open(filename, ios::out | ios::binary);

    fail_out.write((char *)id, sizeof(id));
    fail_out.write((char *)&fail_v[0], sizeof(fail_v));
    fail_out.close();

    msg_log = (msg_log_t *)malloc(MAX_LOG * sizeof(msg_log_t));
}

Pet_kea::Pet_kea(int process_nr, int process_cnt, int fildes[][2], struct msg_t *msg) : id(process_nr),
                                                                                        time_v(process_cnt, 0),
                                                                                        fail_v(process_cnt, 0),
                                                                                        msg_cnt(0),
                                                                                        last_checkpoint(0)
{
    char filename[32];
    print_msg(id, filename);
    ifstream msg_in(filename, ios::in | ios::binary);
    msg_in.read((char *)&id, sizeof(id));
    msg_in.read((char *)&msg_cnt, sizeof(msg_cnt));
    msg_in.read((char *)&last_checkpoint, sizeof(last_checkpoint));
    // time_v.resize(process_cnt);
    size_t size = time_v.size();
    msg_in.read((char *)&time_v[0], sizeof(time_v));

    int begin_log = msg_in.tellg();
    msg_in.seekg(0, msg_in.end);
    int end_log = msg_in.tellg();
    size = end_log - begin_log;
    int to_read = size / (sizeof(msg_log->msg_buf) + sizeof(msg_log->recipient) + sizeof(msg_log->process_id) + sizeof(time_v) + sizeof(fail_v));

    msg_in.seekg(begin_log, ios::beg);

    msg_log = (msg_log_t *)malloc(MAX_LOG * sizeof(msg_log_t));
    // msg_in.read((char *)msg_log, size); // maybe excessive

    for (int i = 0; i < to_read; i++)
    {
        msg_in.read((char *)&msg_log[i], sizeof(msg_log->msg_buf) + sizeof(msg_log->recipient) + sizeof(msg_log->process_id));
        msg_log[i].time_v = vector<int>(process_cnt, 0);
        msg_in.read((char *)&msg_log[i].time_v[0], sizeof(msg_log->time_v));
        msg_log[i].fail_v = vector<int>(process_cnt, 0);
        msg_in.read((char *)&msg_log[i].fail_v[0], sizeof(msg_log->fail_v));
    }

    // TODO: detect lost messages if crash happened during checkpoint
    if (msg_cnt != to_read)
    {
        msg_cnt = to_read;
        time_v = msg_log[to_read - 1].time_v;
    }

    // for (int i = 0; i < to_read; i++)
    // {
    //     cout << msg_log[i].process_id << " " << msg_log[i].recipient << " " << msg_log[i].msg_buf << " time_v: " << msg_log[i].time_v[0] << "-" << msg_log[i].time_v[1] << " fail_v: " << msg_log[i].fail_v[0] << "-" << msg_log[i].fail_v[1] << endl;
    // }

    // close in fd and open out fd
    msg_in.close();
    msg_out.open(filename, ios::out | ios::binary);
    print_fail(id, filename);
    ifstream fail_in(filename, ios::in | ios::binary);
    fail_in.read((char *)id, sizeof(id));
    fail_in.read((char *)&fail_v[0], sizeof(fail_v)); // TODO: write failurelog recovery
    fail_in.close();

    ofstream fail_out(filename, ios::out | ios::binary);
    fail_out.seekp(0, ios::end);
    fail_v[id]++;
    struct fail_log_t entry = {id, fail_v[id], time_v[id]};
    fail_out.write((char *)&entry, sizeof(fail_log_t));
    fail_out.close();

    send_ctrl(fildes, msg);
}

Pet_kea::~Pet_kea()
{
    free(msg_log);
    msg_out.close();
}

int Pet_kea::checkpoint()
{
    // write state and time vector at the start of the file
    // TODO: write this in one go
    msg_out.seekp(0, ios::beg);
    msg_out.write(reinterpret_cast<const char *>(&id), sizeof(id));
    msg_out.write(reinterpret_cast<const char *>(&msg_cnt), sizeof(msg_cnt));
    msg_out.write(reinterpret_cast<const char *>(&last_checkpoint), sizeof(last_checkpoint));

    msg_out.write((char *)&time_v[0], sizeof(time_v));

    // append last messages
    msg_out.seekp(0, ios::end);
    for (; last_checkpoint < msg_cnt; last_checkpoint++)
    {
        msg_out.write((char *)&msg_log[last_checkpoint], sizeof(msg_log->msg_buf) + sizeof(msg_log->recipient) + sizeof(msg_log->process_id));
        msg_out.write((char *)&msg_log[last_checkpoint].time_v[0], sizeof(msg_log->time_v));
        msg_out.write((char *)&msg_log[last_checkpoint].fail_v[0], sizeof(msg_log->fail_v));
    }
    // msg_out.write((char *)&msg_log[last_checkpoint], (msg_cnt - last_checkpoint) * sizeof(msg_log_t));
    // last_checkpoint = msg_cnt;

    return 0;
}

int Pet_kea::send_msg(char *input, int fildes[2])
{
    // inc T^i
    time_v[id]++;

    struct msg_t msg(time_v.size());

    msg.msg_type = MSG;
    sprintf(msg.contents.ptp_msg.msg_buf, input);
    msg.time_v = time_v;
    msg.sending_process_nr = id;
    msg.fail_v = fail_v;

    char data[sizeof(msg_t)];
    serialize(&msg, data);

    // send the message
    // cout << id << " vector sent = " << msg.time_v[0] << "-" << msg.time_v[1] << endl;

    if (write(fildes[1], data, sizeof(msg_t)) < 0)
    {
        // handle error
    }

    store_msg(&msg, false);

    return 0;
}

int Pet_kea::recv_msg(int fildes[2], struct msg_t *ret_buf)
{
    // read message
    int ret;
    char data[sizeof(msg_t)];
    ret = read(fildes[0], data, sizeof(msg_t));
    // do err checking
    if (ret < 0)
    {
    }

    deserialize(data, ret_buf);
    // cout << id << " vector recieved =" << ret_buf->time_v[0] << "-" << ret_buf->time_v[1] << endl;

    // check if it is an errmsg

    // inc T^i and inc T^/j to max(T^j of send event, prev event T^j)
    time_v[id]++;
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        if (i == id)
            continue;
        time_v.at(i) = max(ret_buf->time_v[i], time_v[i]);
    }

    store_msg(ret_buf, true);

    return 0;
}

int Pet_kea::recovery()
{
    return 0;
}