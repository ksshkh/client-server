#ifndef SERVER_HPP
#define SERVER_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <pthread.h>
#include <uuid/uuid.h>
#include <uthash.h>
#include <jansson.h> 

#include "errors.hpp"

const int PORT           = 8068;
const int MAX_QUEUE_SIZE = 1000;
const int UUID_LEN       = 37;
const int DATE_LEN       = 30;
const int NUM_OF_THREADS = 4;

struct Event {
    char id[UUID_LEN]; 
    char date[DATE_LEN]; 
    int status;
};

struct Metrics {
    int  total_processed;    
    int total_duplicates;   
    double total_time_ms;       
};

struct EventQueue { // кольцевой буффер для очереди событий
    Event** events;  
    int head;                        
    int tail;                        
    int size;
    
    pthread_mutex_t mutex;           
    pthread_cond_t  cond; 
};

struct ProcessedEvents{ // хэш-таблица для отслеживания дубликатов
    char id[UUID_LEN];
    UT_hash_handle hh;
};

struct ThreadArgs {
    int client_socket;
    int* code_error;
};

struct EventArray {
    Event** events;   
    size_t num_of_events;  
};

struct ServerContext {
    EventQueue queue;
    Metrics metrics;
    ProcessedEvents* processed;      
    bool running;                    
    pthread_mutex_t metrics_mutex;
    int* code_error;   
};

int get_random_num     (int leftBorder, int rightBorder);
int setup_server_socket(int port, int* code_error);
void parse_events_array(const char* json_str, EventArray* event_array, int* code_error);
Event* parse_event     (json_t* event_obj, int* code_error);
void print_metrics     (Metrics* metrics, int* code_error);
void* run_server       (void* arg);
void* event_processor  (void* arg);
void queue_ctor        (EventQueue* queue, int* code_error);
void queue_dtor        (EventQueue* queue, int* code_error);
int queue_push         (EventQueue* queue, Event* event, int* code_error);
Event* queue_pop       (EventQueue* queue, int* code_error);

#endif // SERVER_HPP