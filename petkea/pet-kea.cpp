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

    for (int i = 0; i < msg->recieved_cnt; i++)
    {
        *q = msg->recieved[i].Tj;
        q++;
        for (int j = 0; j < (int)fail_v.size(); j++)
        {
            *q = msg->recieved[i].fail_v[j];
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

    struct int_vec_pair *recieved = (struct int_vec_pair *)malloc(msg->recieved_cnt * sizeof(struct int_vec_pair));
    vector<int> temp_v(fail_v.size(), 0);
    for (int i = 0; i < msg->recieved_cnt; i++)
    {
        recieved[i].Tj = *q;
        q++;
        for (int j = 0; j < (int)fail_v.size(); j++)
        {
            temp_v[j] = *q;
            q++;
        }
        recieved[i].fail_v = temp_v;
    }
    msg->recieved = recieved;
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

    // char *p = (char *)q;
    // for (int i = 0; i < msg->msg_size; i++)
    // {
    //     *p = msg->msg_buf[i];
    //     p++;
    // }
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
            msg->time_v[i] = *q; //////
            q++;
        }

        for (int i = 0; i < (int)fail_v.size(); i++)
        {
            msg->fail_v[i] = *q;
            q++;
        }
    }

    // char *p = (char *)q;
    msg->msg_buf = (char *)malloc(msg->msg_size);
    memcpy(msg->msg_buf, q, msg->msg_size);
    // for (int i = 0; i < msg->msg_size; i++)
    // {
    //     msg->msg_buf[i] = *p;
    //     p++;
    // }
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
        *q = log->time_v[i];
        q++;
    }
    for (int i = 0; i < (int)fail_v.size(); i++)
    {
        *q = log->fail_v[i];
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
    log->time_v = vector<int>(time_v.size(), 0);
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        log->time_v[i] = *q;
        q++;
    }
    log->fail_v = vector<int>(fail_v.size(), 0);
    for (int i = 0; i < (int)fail_v.size(); i++)
    {
        log->fail_v[i] = *q;
        q++;
    }
    log->msg_buf = (char *)malloc(log->msg_size);
    memcpy(log->msg_buf, q, log->msg_size);

    return (q - (int *)data) * sizeof(int) + log->msg_size;
}

void Pet_kea::State::send_ctrl(int fildes[][2])
{
    struct int_vec_pair **recvd_pair = (struct int_vec_pair **)malloc(time_v.size() * sizeof(struct int_vec_pair *));
    // pair<int, vector<int>> *recvd_pair[time_v.size()]; // TODO: malloc
    for (int i = 0; i < (int)time_v.size(); i++)
    {
        recvd_pair[i] = (struct int_vec_pair *)malloc(INT_VEC_PAIR_DELTA * sizeof(struct int_vec_pair));
    }
    vector<int> cnt(time_v.size(), 0);
    struct fail_log_t fail_log = {id, fail_v[id], time_v[id]};

    for (int i = 0; i < msg_cnt; i++)
    {
        // TODO: maybe a check here if processid in msg_t is changed
        recvd_pair[msg_log[i].process_id][cnt[msg_log[i].process_id]].Tj = msg_log[i].time_v[msg_log[i].process_id];
        // recvd_pair[msg_log[i].process_id][cnt[msg_log[i].process_id]].fail_v = vector<int>(2, 0);
        recvd_pair[msg_log[i].process_id][cnt[msg_log[i].process_id]].fail_v = msg_log[i].fail_v;
        cnt[msg_log[i].process_id]++;
        if (cnt[msg_log[i].process_id] % INT_VEC_PAIR_DELTA == 0)
        {
            struct int_vec_pair *new_pair = (struct int_vec_pair *)malloc((cnt[msg_log[i].process_id] + INT_VEC_PAIR_DELTA) * sizeof(struct int_vec_pair));
            for (int j = 0; j < cnt[msg_log[i].process_id]; i++)
            {
                new_pair[j].Tj = recvd_pair[msg_log[i].process_id][j].Tj;
                new_pair[j].fail_v = recvd_pair[msg_log[i].process_id][j].fail_v;
            }
            free(recvd_pair[msg_log[i].process_id]);
            recvd_pair[msg_log[i].process_id] = new_pair;
        }
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

        cout << id << " sending " << msg.log_entry.id << " " << msg.log_entry.fail_nr << " " << msg.log_entry.res_time << " messages recieved: " << msg.recieved_cnt << endl;
        for (int i = 0; i < msg.recieved_cnt; i++)
        {
            cout << "T^j:" << msg.recieved[i].Tj << " fail_v: ";
            for (int j = 0; j < (int)fail_v.size(); j++)
            {
                cout << msg.recieved[i].fail_v[j] << " ";
            }
            cout << endl;
        }

        // send the control message (serialize)
        size_t size = SER_SIZE_CTRL_MSG_T(msg.recieved_cnt, fail_v.size());
        char *data = (char *)malloc(size);
        serialize_ctrl(&msg, data);

        int ret = write(fildes[i][1], data, size);
        if (ret < 0)
        {
            // TODO: do error checking
        }
    }
}

void Pet_kea::State::recv_ctrl(char *data)
{
    struct ctrl_msg_t c_msg;
    deserialize_ctrl(data, &c_msg);
    cout << id << " recieving " << c_msg.log_entry.id << " " << c_msg.log_entry.fail_nr << " " << c_msg.log_entry.res_time << " messages recieved: " << c_msg.recieved_cnt << endl;
    for (int i = 0; i < c_msg.recieved_cnt; i++)
    {
        cout << "T^j:" << c_msg.recieved[i].Tj << " fail_v: ";
        for (int j = 0; j < (int)fail_v.size(); j++)
        {
            cout << c_msg.recieved[i].fail_v[j] << " ";
        }
        cout << endl;
    }
    // TODO:handle the ctrl messages
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

Pet_kea::State::State(int process_nr, int process_cnt, bool restart) : id(process_nr),
                                                                       time_v(process_cnt, 0),
                                                                       fail_v(process_cnt, 0),
                                                                       msg_cnt(0),
                                                                       last_checkpoint(0)
{
    if (restart)
    {
        char filename[32];
        print_msg(id, filename);
        ifstream msg_in(filename, ifstream::in | ifstream::binary);

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

        // TODO: change this to accomodate for changeable size
        int begin_log = msg_in.tellg();
        msg_in.seekg(0, msg_in.end);
        int end_log = msg_in.tellg();
        size = end_log - begin_log;

        msg_in.seekg(begin_log, ifstream::beg);

        msg_log = (msg_log_t *)malloc(MAX_LOG * sizeof(msg_log_t));
        // msg_in.read((char *)msg_log, size); // maybe excessive
        char log_buffer[end_log];
        char *curr_pos = log_buffer;
        msg_in.read(log_buffer, end_log);
        int read_msg_cnt = 0;

        for (int j = 0, k = 0; k < (int)size; read_msg_cnt++)
        {
            j = deserialize_log(curr_pos, &msg_log[read_msg_cnt]);
            curr_pos = curr_pos + j;
            k = k + j;

            // msg_in.read((char *)&msg_log[i], sizeof(msg_log->msg_buf) + sizeof(msg_log->recipient) + sizeof(msg_log->process_id));
            // msg_log[i].time_v = vector<int>(process_cnt, 0);
            // msg_in.read((char *)&msg_log[i].time_v[0], sizeof(msg_log->time_v));
            // msg_log[i].fail_v = vector<int>(process_cnt, 0);
            // msg_in.read((char *)&msg_log[i].fail_v[0], sizeof(msg_log->fail_v));
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
                time_v = msg_log[msg_cnt - 1].time_v;
            }
        }

        // for (int i = 0; i < to_read; i++)
        // {
        //     cout << msg_log[i].process_id << " " << msg_log[i].recipient << " " << msg_log[i].msg_buf << " time_v: " << msg_log[i].time_v[0] << "-" << msg_log[i].time_v[1] << " fail_v: " << msg_log[i].fail_v[0] << "-" << msg_log[i].fail_v[1] << endl;
        // }

        // close in fd and open out fd
        msg_in.close();
        msg_out.open(filename, ofstream::out | ofstream::binary | ofstream::app); // TODO: this is where .dat files get cleared
        print_fail(id, filename);
        ifstream fail_in(filename, ifstream::in | ifstream::binary);
        fail_in.read((char *)&id, sizeof(id));
        fail_in.read((char *)&fail_v[0], sizeof(fail_v)); // TODO: write failurelog recovery
        fail_in.close();

        ofstream fail_out(filename, ofstream::out | ofstream::binary | ofstream::app);
        fail_out.seekp(0, ofstream::end);
        fail_v[id]++;
        struct fail_log_t entry = {id, fail_v[id], time_v[id]};
        fail_out.write((char *)&entry, sizeof(fail_log_t)); // TODO: check if i want to append the fail file
        fail_out.close();

        // send_ctrl(fildes);
    }
    else
    {
        char filename[32];
        print_fail(id, filename);
        ofstream fail_out(filename, ofstream::out | ofstream::binary | ofstream::trunc);
        print_msg(id, filename);
        msg_out.open(filename, ofstream::out | ofstream::binary | ofstream::trunc);

        fail_out.write(reinterpret_cast<const char *>(&id), sizeof(id));
        fail_out.write((char *)&fail_v[0], sizeof(fail_v));
        fail_out.close();

        msg_log = (msg_log_t *)malloc(MAX_LOG * sizeof(msg_log_t));
    }
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

    msg_out.seekp(0, ofstream::beg);
    msg_out.write((char *)update, sizeof(int) * 3);
    free(update);
    // msg_out.write(reinterpret_cast<const char *>(&id), sizeof(id));
    // msg_out.write(reinterpret_cast<const char *>(&msg_cnt), sizeof(msg_cnt));
    // msg_out.write(reinterpret_cast<const char *>(&last_checkpoint), sizeof(last_checkpoint));

    msg_out.write((char *)&time_v[0], sizeof(time_v));

    // append last messages
    msg_out.seekp(0, ofstream::end);
    size_t log_size = SER_LOG_SIZE(time_v.size());
    for (; last_checkpoint < msg_cnt; last_checkpoint++)
    {
        char data[msg_log[last_checkpoint].msg_size + log_size];
        serialize_log(&msg_log[last_checkpoint], data);
        msg_out.write(data, msg_log[last_checkpoint].msg_size + log_size);

        // msg_out.write((char *)&msg_log[last_checkpoint], sizeof(msg_log->recipient) + sizeof(msg_log->process_id));
        // msg_out.write((char *)&msg_log[last_checkpoint].time_v[0], sizeof(msg_log->time_v));
        // msg_out.write((char *)&msg_log[last_checkpoint].fail_v[0], sizeof(msg_log->fail_v));
    }
    // msg_out.write((char *)&msg_log[last_checkpoint], (msg_cnt - last_checkpoint) * sizeof(msg_log_t));
    // last_checkpoint = msg_cnt;

    return 0;
}

int Pet_kea::State::send_void(char *input, int fildes[2], int size)
{
    struct msg_t msg(time_v.size());
    msg.msg_type = VOID;
    msg.sending_process_nr = id;
    msg.msg_size = size;
    msg.msg_buf = input;

    char data[sizeof(msg_t) + size];

    serialize(&msg, data);

    if (write(fildes[1], data, sizeof(msg_t) + size) < 0)
    {
        // handle error
    }

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
    size_t init_read_size = SER_SIZE_CTRL_MSG_T(0, 0);

    char *extra_data, *data = (char *)malloc(sizeof(msg_t) + size); // TODO: check size
    ret = read(fildes[0], data, init_read_size);                    // TODO: check size
    // do err checking
    if (ret < 0)
    {
    }

    extra_data = data + init_read_size;

    int *q = (int *)data;

    if (CTRL == (msg_type)*q)
    {
        q++;

        ret = read(fildes[0], extra_data, SER_SIZE_CTRL_MSG_T(*q, fail_v.size()) - init_read_size);

        recv_ctrl(data);
    }
    else if (VOID == (msg_type)*q)
    {
        cout << "entered void section" << endl;
        q++;

        ret = read(fildes[0], extra_data, sizeof(msg_t) + *q - init_read_size);
        struct msg_t v_msg(time_v.size());
        deserialize(data, &v_msg);

        // TODO: process the void message aka give to method caller
        memcpy(output, v_msg.msg_buf, v_msg.msg_size);
        return 1;
    }

    ret = read(fildes[0], extra_data, sizeof(msg_t) + size - init_read_size);
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