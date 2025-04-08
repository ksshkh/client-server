#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "errors.hpp"

const int UUID_LEN        = 37;
const int DATE_LEN        = 30;
const int BUFFER_SIZE     = 99;
const int HIT_PROBABILITY = 40;

#define PORT 8080

struct Event {
    char id[UUID_LEN]; 
    char date[DATE_LEN]; 
    int status;
};

struct BufferEvents {
    Event** buffer;
    size_t ip;
    bool full_buffer;
};

void buffer_events_ctor(BufferEvents* buff_events, int* code_error);
void buffer_events_dtor(BufferEvents* buff_events, int* code_error);
void get_current_time  (char* time_str, int* code_error);
void generate_uuid     (char *uuid_str, int* code_error);
Event generate_event   (int* code_error);
int get_random_num     (int leftBorder, int rightBorder);
void add_event         (BufferEvents* buff_events, Event* events, int* code_error);
void add_in_buffer     (BufferEvents* buff_events, Event* events, int* code_error);
int connect_to_server  (const char *server_ip, int port, int* code_error);
void send_json_data    (int sock, const char *json_data, int* code_error);
char* events_to_json   (const Event* events, int count, int* code_error);
size_t get_events_len  (const Event* events, int num_of_events, int* code_error);

#endif // CLIENT_HPP