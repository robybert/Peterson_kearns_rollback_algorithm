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

void Pet_kea::State::serialize_ctrl(struct ctrl_msg_t *msg, char *data)
{
    int *q = (int *)data;
    *q = msg->msg_type;
    q++;
    *q = msg->sending_process_nr;
    q++;

    *q = msg->log_entry.id;
    q++;
    *q = msg->log_entry.fail_nr;
    q++;
    *q = msg->log_entry.res_time;
    q++;

    *q = msg->recieved_cnt;
    q++;

    for (int i = 0; i < msg->recieved_cnt; i++)
    {
        *q = msg->recieved->first;
        q++;
        for (int j = 0; j < (int)fail_v.size(); j++)
        {
            *q = msg->recieved->second[i];
            q++;
        }
    }
}

void Pet_kea::State::deserialize_ctrl(char *data, struct ctrl_msg_t *msg)
{
    int *q = (int *)data;
    msg->msg_type = (msg_type)*q;
    q++;
    msg->sending_process_nr = *q;
    q++;

    msg->log_entry.id = *q;
    q++;
    msg->log_entry.fail_nr = *q;
    q++;
    msg->log_entry.res_time = *q;
    q++;

    msg->recieved_cnt = *q;
    q++;

    msg->recieved = (pair<int, vector<int>> *)malloc(sizeof(pair<int, vector<int>>) * msg->recieved_cnt);

    for (int i = 0; i < msg->recieved_cnt; i++)
    {
        msg->recieved->first = *q;
        q++;
        for (int j = 0; j < (int)fail_v.size(); j++)
        {
            msg->recieved->second[j] = *q;
            q++;
        }
    }
}

void Pet_kea::State::serialize(struct msg_t *msg, char *data)
{
    int *q = (int *)data;
    *q = msg->msg_type;
    q++;
    *q = msg->sending_process_nr;
    q++;

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

    *q = msg->msg_size;
    q++;

    char *p = (char *)q;
    for (int i = 0; i < msg->msg_size; i++)
    {
        *p = msg->msg_buf[i];
        p++;
    }

    // else if (msg->msg_type == CTRL)
    // {
    //     // TODO: write serialization for errmsg
    //     *q = msg->contents.ptp_err.pid;
    //     q++;
    //     *q = msg->contents.ptp_err.fildes[0];
    //     q++;
    //     *q = msg->contents.ptp_err.fildes[1];
    //     q++;

    //     // serialize ctrl_msg_t
    //     *q = msg->contents.ptp_err.msg->log_entry.id;
    //     q++;
    //     *q = msg->contents.ptp_err.msg->log_entry.fail_nr;
    //     q++;
    //     *q = msg->contents.ptp_err.msg->log_entry.res_time;
    //     q++;

    //     *q = msg->contents.ptp_err.msg->recieved_cnt;
    //     q++;

    //     for (int i = 0; i < msg->contents.ptp_err.msg->recieved_cnt; i++)
    //     {
    //
    //     }
    // }
}
void Pet_kea::State::deserialize(char *data, struct msg_t *msg)
{
    int *q = (int *)data;
    msg->msg_type = (msg_type)*q;
    q++;
    msg->sending_process_nr = *q;
    q++;

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

    msg->msg_size = *q;
    q++;

    char *p = (char *)q;
    msg->msg_buf = (char *)malloc(msg->msg_size);
    for (int i = 0; i < msg->msg_size; i++)
    {
        msg->msg_buf[i] = *p;
        p++;
    }
    // else
    // {
    //     // TODO: write deserialization for errmsg
    //     msg->contents.ptp_err.pid = *q;
    //     q++;
    //     msg->contents.ptp_err.fildes[0] = *q;
    //     q++;
    //     msg->contents.ptp_err.fildes[1] = *q;
    //     q++;

    //     // serialize ctrl_msg_t
    //     msg->contents.ptp_err.msg->log_entry.id = *q;
    //     q++;
    //     msg->contents.ptp_err.msg->log_entry.fail_nr = *q;
    //     q++;
    //     msg->contents.ptp_err.msg->log_entry.res_time = *q;
    //     q++;

    //     msg->contents.ptp_err.msg->recieved_cnt = *q;
    //     q++;

    //
    // }
}

void Pet_kea::State::send_ctrl(int fildes[][2])
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
        // prepare and send the ctrl messages
        if (i == id)
            continue;
        struct ctrl_msg_t msg; // = (ctrl_msg_t *)malloc(sizeof(ctrl_msg_t));
        msg.msg_type = CTRL;
        msg.sending_process_nr = id;
        msg.log_entry = fail_log;
        msg.recieved_cnt = cnt[i];
        msg.recieved = recvd_pair[i];
        // send the control message (serialize)
        size_t size = sizeof(ctrl_msg_t) + (sizeof(pair<int, vector<int>>) * msg.recieved_cnt);
        char *data = (char *)malloc(size); // TODO: fix size???
        serialize_ctrl(&msg, data);

        int ret = write(fildes[i][1], data, size);
        if (ret < 0)
        {
            // TODO: do error checking
        }
    }
}

void Pet_kea::State::recv_ctrl()
{
}

int Pet_kea::State::store_msg(struct msg_t *msg, bool recipient)
{
    // store the message
    if (msg_cnt >= MAX_LOG)
    {
        // increase max size
        cout << "max log size reached of process " << id << endl;
        return -1;
    }

    msg_log[msg_cnt].msg_size = msg->msg_size;
    msg_log[msg_cnt].recipient = recipient;
    msg_log[msg_cnt].process_id = msg->sending_process_nr; // TODO: look at the sending and recving process nr in thesis
    msg_log[msg_cnt].msg_buf = (char *)malloc(msg->msg_size);
    memcpy(msg_log[msg_cnt].msg_buf, msg->msg_buf, msg->msg_size);
    msg_log[msg_cnt].time_v = time_v;
    msg_log[msg_cnt].fail_v = fail_v;
    msg_cnt++;
    if (msg_cnt % SAVE_CNT == 0)
        checkpoint();

    // cout << id << "     " << recipient << "      " << msg->contents.ptp_msg.msg_buf << "     time_v: " << time_v[0] << "-" << time_v[1] << endl;
    return 0;
}

Pet_kea::State::State(int process_nr, int process_cnt) : id(process_nr),
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

    fail_out.write(reinterpret_cast<const char *>(&id), sizeof(id));
    fail_out.write((char *)&fail_v[0], sizeof(fail_v));
    fail_out.close();

    msg_log = (msg_log_t *)malloc(MAX_LOG * sizeof(msg_log_t));
}

Pet_kea::State::State(int process_nr, int process_cnt, int fildes[][2]) : id(process_nr),
                                                                          time_v(process_cnt, 0),
                                                                          fail_v(process_cnt, 0),
                                                                          msg_cnt(0),
                                                                          last_checkpoint(0)
{
    char filename[32];
    print_msg(id, filename);
    ifstream msg_in(filename, ios::in | ios::binary);

    int *update = (int *)malloc(sizeof(int) * 3);
    msg_in.read((char *)update, sizeof(int) * 3);

    id = *update;
    update++;
    msg_cnt = *update;
    update++;
    last_checkpoint = *update;
    update--;
    update--;
    free(update);

    // msg_in.read((char *)&id, sizeof(id));
    // msg_in.read((char *)&msg_cnt, sizeof(msg_cnt));
    // msg_in.read((char *)&last_checkpoint, sizeof(last_checkpoint));
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

    // detect lost messages if crash happened during checkpoint
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
    fail_in.read((char *)&id, sizeof(id));
    fail_in.read((char *)&fail_v[0], sizeof(fail_v)); // TODO: write failurelog recovery
    fail_in.close();

    ofstream fail_out(filename, ios::out | ios::binary);
    fail_out.seekp(0, ios::end);
    fail_v[id]++;
    struct fail_log_t entry = {id, fail_v[id], time_v[id]};
    fail_out.write((char *)&entry, sizeof(fail_log_t));
    fail_out.close();

    send_ctrl(fildes);
}

Pet_kea::State::~State()
{
    free(msg_log);
    msg_out.close();
}

int Pet_kea::State::checkpoint()
{
    // write state and time vector at the start of the file
    // TODO: write this in one go

    int *update = (int *)malloc(sizeof(int) * 3);
    *update = id;
    update++;
    *update = msg_cnt;
    update++;
    *update = last_checkpoint;
    update--;
    update--;

    msg_out.seekp(0, ios::beg);
    msg_out.write((char *)update, sizeof(int) * 3);
    free(update);
    // msg_out.write(reinterpret_cast<const char *>(&id), sizeof(id));
    // msg_out.write(reinterpret_cast<const char *>(&msg_cnt), sizeof(msg_cnt));
    // msg_out.write(reinterpret_cast<const char *>(&last_checkpoint), sizeof(last_checkpoint));

    msg_out.write((char *)&time_v[0], sizeof(time_v));

    // append last messages
    msg_out.seekp(0, ios::end);
    for (; last_checkpoint < msg_cnt; last_checkpoint++)
    {
        msg_out.write((char *)&msg_log[last_checkpoint], sizeof(msg_log->recipient) + sizeof(msg_log->process_id));
        msg_out.write((char *)&msg_log[last_checkpoint].time_v[0], sizeof(msg_log->time_v));
        msg_out.write((char *)&msg_log[last_checkpoint].fail_v[0], sizeof(msg_log->fail_v));
    }
    // msg_out.write((char *)&msg_log[last_checkpoint], (msg_cnt - last_checkpoint) * sizeof(msg_log_t));
    // last_checkpoint = msg_cnt;

    return 0;
}

int Pet_kea::State::send_msg(char *input, int fildes[2], int size)
{
    // inc T^i
    time_v[id]++;

    struct msg_t msg(time_v.size());

    msg.msg_type = MSG;
    msg.sending_process_nr = id;
    msg.time_v = time_v;
    msg.fail_v = fail_v;
    msg.msg_size = size;
    msg.msg_buf = input;

    char data[sizeof(msg_t) + size];
    serialize(&msg, data);

    // send the message
    // cout << id << " vector sent = " << msg.time_v[0] << "-" << msg.time_v[1] << endl;

    if (write(fildes[1], data, sizeof(msg_t) + size) < 0)
    {
        // handle error
    }

    store_msg(&msg, false);

    return 0;
}

int Pet_kea::State::recv_msg(int fildes[2], char *output, int size)
{
    // read message
    int ret;
    char *data = (char *)malloc(sizeof(msg_t) + size); // TODO: check size
    ret = read(fildes[0], data, sizeof(msg_t) + size); // TODO: check size
    // do err checking
    if (ret < 0)
    {
    }

    struct msg_t msg(time_v.size());
    deserialize(data, &msg);
    // cout << id << " vector recieved =" << ret_buf->time_v[0] << "-" << ret_buf->time_v[1] << endl;

    // check if it is an errmsg

    // inc T^i and inc T^/j to max(T^j of send event, prev event T^j)
    time_v[id]++;
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        if (i == id)
            continue;
        time_v.at(i) = max(msg.time_v[i], time_v[i]);
    }

    store_msg(&msg, true);

    // output = (char *)malloc(msg.msg_size);
    memcpy(output, msg.msg_buf, msg.msg_size);

    return 0;
}

int Pet_kea::State::recovery()
{
    return 0;
}