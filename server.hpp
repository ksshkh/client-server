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

const int MAX_QUEUE_SIZE = 100;
const int UUID_LEN       = 37;
const int DATE_LEN       = 30;
const int NUM_OF_THREADS = 4;

#define PORT 8080

struct Event {
    char id[UUID_LEN]; 
    char date[DATE_LEN]; 
    int status;
};

struct EventQueue {
    Event** events;  
    int head;                        
    int tail;                        
    int size;

    pthread_mutex_t lock;            
    pthread_cond_t  not_empty;         
    pthread_cond_t  not_full;          
};

struct ProcessedEvents{
    char id[UUID_LEN];
    UT_hash_handle hh;
};

struct EventTracker{
    ProcessedEvents* hash_table;  
    pthread_mutex_t mutex;       
};

struct Metrics {
    int  total_processed;    
    int total_duplicates;   
    double total_time_ms;     
    pthread_mutex_t lock;    
};

struct ThreadArgs {
    EventQueue* queue;
    EventTracker* event_tracker;
    Metrics* metrics;
    int* code_error;
};

typedef struct {
    Event** events;    // Массив событий
    size_t count;      // Количество событий
} EventArray;

int get_random_num(int leftBorder, int rightBorder);
void queue_ctor(EventQueue* queue, int* code_error);
void queue_dtor(EventQueue* queue, int* code_error);
void tracker_ctor(EventTracker* tracker, int* code_error);
void tracker_dtor(EventTracker* tracker, int* code_error);
void queue_push(EventQueue* queue, Event* event, int* code_error);
Event* queue_pop(EventQueue* queue, int* code_error);
bool queue_is_full(EventQueue *queue, int* code_error);
bool is_duplicate(EventTracker* event_tracker, const char* id, int* code_error);
void* worker_thread(void* arg);
Event* parse_event(const char* json_str, int* code_error);
EventArray parse_events_array(const char* json_str, int* code_error);
void print_metrics(Metrics* metrics);

#endif // SERVER_HPP