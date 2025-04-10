#include "client.hpp"

void buffer_events_ctor(BufferEvents* buff_events, int* code_error) {

    MY_ASSERT(buff_events != NULL, PTR_ERROR);

    buff_events->buffer      = (Event*)calloc(BUFFER_SIZE, sizeof(Event));
    buff_events->ip          = 0;
    buff_events->full_buffer = false;
}

void buffer_events_dtor(BufferEvents* buff_events, int* code_error) {

    MY_ASSERT(buff_events != NULL, PTR_ERROR);

    free(buff_events->buffer);
    buff_events->buffer = NULL;

    buff_events->ip = 0;
}

void get_current_time(char* time_str, int* code_error) {

    MY_ASSERT(time_str != NULL, PTR_ERROR);

    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    strftime(time_str, 30, "%Y-%m-%d %H:%M:%S", tm_now);
}

void generate_uuid(char *uuid_str, int* code_error) {

    MY_ASSERT(uuid_str != NULL, PTR_ERROR);

    uuid_t uuid;
    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);
}

Event generate_event(int* code_error) {
    Event event;

    generate_uuid(event.id, code_error);
    get_current_time(event.date, code_error);
    event.id[UUID_LEN-1] = '\0';
    event.date[DATE_LEN-1] = '\0';

    event.status = get_random_num(0, 1); 

    return event;
}

int get_random_num(int leftBorder, int rightBorder) {
    return rand() % (rightBorder - leftBorder + 1) + leftBorder;
}

void add_event(BufferEvents* buff_events, Event event, int* code_error) {

    MY_ASSERT(buff_events            != NULL, PTR_ERROR);
    MY_ASSERT(buff_events->ip <= BUFFER_SIZE, SIZE_ERROR);

    if(buff_events->ip == BUFFER_SIZE) {
        if(!buff_events->full_buffer) {
            buff_events->full_buffer = true;
        }
        buff_events->ip = 0;
    }

    buff_events->buffer[buff_events->ip] = event;
    buff_events->ip++;
}

void add_in_buffer(BufferEvents* buff_events, Event event, int* code_error) {

    MY_ASSERT(buff_events != NULL, PTR_ERROR);

    /* если буффер заполнен (сгенерировано уже более 99 событий), то с вероятностью,
       равной HIT_PROBABILITY % событие в него попадёт (чтобы 100-ое событие было максимально случайным, при этом экономилась память)
       если буффер не заполнен, то событие точно в него попадает, чтобы не допустить ситуации, когда на 100-ом событии буфер пуст */

    if(buff_events->full_buffer) {  
        int hit_prob = get_random_num(1, 100);
        if(hit_prob <= HIT_PROBABILITY) {
            add_event(buff_events, event, code_error);
        }
    }
    else {
        add_event(buff_events, event, code_error);
    }
}

int connect_to_server(const char *server_ip, int port, int* code_error) {

    MY_ASSERT(server_ip != NULL, IP_ERROR); // сделать ассерт на порт
    // MY_ASSERT(port > 0 && port <= 65535, PORT_ERROR);????

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    MY_ASSERT(sock != -1, SOCKET_ERROR);

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        MY_ASSERT(true, IP_ERROR);
        close(sock);
        return -1;
    }

    if(connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        MY_ASSERT(true, CONNECT_ERROR);
        close(sock);
        return -1;
    }

    return sock;  
}

void send_json_data(int sock, const char *json_data, int* code_error) {

    MY_ASSERT(json_data != NULL, PTR_ERROR);

    uint32_t data_len = strlen(json_data);
    uint32_t net_len  = htonl(data_len);
  
    MY_ASSERT(send(sock, &net_len, sizeof(net_len), 0) == sizeof(net_len), SEND_ERROR);
    MY_ASSERT(send(sock, json_data, data_len, 0)       == data_len,        SEND_ERROR);
}

size_t get_events_len(const Event* events, int num_of_events, int* code_error) {

    MY_ASSERT(events != NULL, PTR_ERROR);

    size_t total_size = 14; 
    
    for(int i = 0; i < num_of_events; i++) {
        size_t event_size = 29 + strlen(events[i].id) + strlen(events[i].date) + 1;
        total_size += event_size;

        if(i > 0) {
            total_size += 1;
        }
    }

    return total_size;
}

char* events_to_json(const Event* events, int num_of_events, int* code_error) {

    MY_ASSERT(events != NULL, PTR_ERROR);

    size_t buf_size = get_events_len(events, num_of_events, code_error);

    char* json = (char*)calloc(buf_size, sizeof(char));
    MY_ASSERT(json != NULL, PTR_ERROR);

    char* ptr = json;
    size_t remaining = buf_size;

    ptr += snprintf(ptr, remaining, "{\"events\":[");
    remaining -= (ptr - json);

    for(int i = 0; i < num_of_events; i++) {
        int written = snprintf(
            ptr, remaining, 
            "%s{\"id\":\"%s\",\"date\":\"%s\",\"status\":%d}", 
            (i > 0) ? "," : "", 
            events[i].id, 
            events[i].date, 
            events[i].status
        );
        if (written >= remaining) break;
        ptr += written;
        remaining -= written;
    }
    snprintf(ptr, remaining, "]}");

    // printf("%s\n", json);

    return json;
}