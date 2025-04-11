#include "server.hpp"

int get_random_num(int leftBorder, int rightBorder) {
    return rand() % (rightBorder - leftBorder + 1) + leftBorder;
}

int setup_server_socket(int port, int* code_error) {

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    MY_ASSERT(server_fd != -1, SOCKET_ERROR);
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    sockaddr_in address;
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(port);
    
    MY_ASSERT(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == 0, SOCKET_ERROR);
    MY_ASSERT(listen(server_fd, 4) == 0, SOCKET_ERROR);
    
    return server_fd;
}

void parse_events_array(const char* json_str, EventArray* event_array, int* code_error) {

    MY_ASSERT(json_str != NULL, PTR_ERROR);

    json_error_t json_error;    
    json_t* root = json_loads(json_str, 0, &json_error);
    MY_ASSERT(root != NULL, PARSE_ERROR);

    json_t* j_events = json_object_get(root, "events");
    MY_ASSERT(j_events != NULL && json_is_array(j_events), PARSE_ERROR);

    event_array->num_of_events = json_array_size(j_events);
    event_array->events = (Event**)calloc(event_array->num_of_events, sizeof(Event*));
    MY_ASSERT(event_array->events != NULL, PTR_ERROR);

    for(size_t i = 0; i < event_array->num_of_events; i++) {
        json_t* event_obj = json_array_get(j_events, i);
        if (!json_is_object(event_obj)) {
            continue;
        }
        event_array->events[i] = parse_event(event_obj, code_error);       
    }

    json_decref(root);
}

Event* parse_event(json_t* event_obj, int* code_error) {

    MY_ASSERT(event_obj != NULL, PTR_ERROR);

    json_t* j_id     = json_object_get(event_obj, "id");
    json_t* j_date   = json_object_get(event_obj, "date");
    json_t* j_status = json_object_get(event_obj, "status");

    const char* id   = json_string_value(j_id);
    const char* date = json_string_value(j_date);
    const int status = json_integer_value(j_status);

    Event* event = (Event*)calloc(1, sizeof(Event));
    MY_ASSERT(event != NULL, PTR_ERROR);

    strncpy(event->id, id, UUID_LEN - 1);
    strncpy(event->date, date, DATE_LEN - 1);
    event->id[UUID_LEN - 1]   = '\0';
    event->date[DATE_LEN - 1] = '\0';
    event->status = status;

    return event;
}

void print_metrics(Metrics* metrics, int* code_error) {

    MY_ASSERT(metrics != NULL, PTR_ERROR);

    printf("|Processed: %d| Duplicates: %d| AvgTime: %.2fms|\n",
           metrics->total_processed,
           metrics->total_duplicates,
           metrics->total_time_ms / metrics->total_processed);
}

void* run_server(void* arg) {
    
    ThreadArgs* args = (ThreadArgs*)arg;
    int client_socket = args->client_socket;
    int* code_error = args->code_error; 
    free(arg);

    ServerContext context = {};
    context.running = true;
    queue_ctor(&context.queue, code_error);
    pthread_mutex_init(&context.metrics_mutex, NULL);
    context.code_error = code_error;

    pthread_t processor_thread;
    pthread_create(&processor_thread, NULL, event_processor, &context);
    
    while(true) {
        uint32_t data_len;
        int len_read = read(client_socket, &data_len, sizeof(data_len));
        if(len_read <= 0) {
            break; 
        }
        
        data_len = ntohl(data_len);  
        char* buffer = (char*)calloc(data_len + 1, sizeof(char));
        MY_ASSERT(buffer != NULL, PTR_ERROR);
        int bytes_read = read(client_socket, buffer, data_len); 
        MY_ASSERT(bytes_read == data_len, READ_ERROR);      
        buffer[data_len] = '\0'; 

        EventArray event_array = {0};
        parse_events_array(buffer, &event_array, code_error);

        for(size_t i = 0; i < event_array.num_of_events; i++) {
            if(!queue_push(&context.queue, event_array.events[i], code_error)) {
                free(event_array.events[i]); 
            }
        }

        free(event_array.events);
        free(buffer);
    }

    context.running = false;
    pthread_cond_signal(&context.queue.cond); 
    pthread_join(processor_thread, NULL);

    ProcessedEvents *current, *tmp;
    HASH_ITER(hh, context.processed, current, tmp) {
        HASH_DEL(context.processed, current);
        free(current);
    }   
    queue_dtor(&context.queue, code_error);
    pthread_mutex_destroy(&context.metrics_mutex);
    MY_ASSERT(close(client_socket) == 0, CLOSE_ERROR);
    
    return NULL;
}

void* event_processor(void* arg) {

    ServerContext* context = (ServerContext*)arg;
    int* code_error = context->code_error;
    
    while(context->running) {
        Event* event = queue_pop(&context->queue, code_error);
        MY_ASSERT(event != NULL, PTR_ERROR);

        ProcessedEvents* found = NULL;
        HASH_FIND_STR(context->processed, event->id, found);
        
        int t_interval_ms = get_random_num(10, 500);
        struct timespec ts = {
            .tv_sec = t_interval_ms / 1000,
            .tv_nsec = (t_interval_ms % 1000) * 1000000
        };
        nanosleep(&ts, NULL);
        
        pthread_mutex_lock(&context->metrics_mutex);
        context->metrics.total_processed++;
        context->metrics.total_time_ms += t_interval_ms;
        
        if(found) {
            context->metrics.total_duplicates++;
        } 
        else {
            ProcessedEvents* item = (ProcessedEvents*)calloc(1, sizeof(ProcessedEvents));
            MY_ASSERT(item != NULL, PTR_ERROR);

            strcpy(item->id, event->id);
            HASH_ADD_STR(context->processed, id, item);
        }
        
        print_metrics(&context->metrics, context->code_error);
        pthread_mutex_unlock(&context->metrics_mutex);
        
        free(event);
    }
    
    return NULL;
}

void queue_ctor(EventQueue* queue, int* code_error) {

    MY_ASSERT(queue != NULL, PTR_ERROR);

    queue->events = (Event**)calloc(MAX_QUEUE_SIZE, sizeof(Event*));
    MY_ASSERT(queue->events != NULL, PTR_ERROR);

    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;

    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void queue_dtor(EventQueue* queue, int* code_error) {

    MY_ASSERT(queue != NULL, PTR_ERROR);

    for(int i = 0; i < queue->size; i++) {
        free(queue->events[(queue->head + i) % MAX_QUEUE_SIZE]);
    }

    free(queue->events);
    queue->events = NULL;

    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;

    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
}

int queue_push(EventQueue* queue, Event* event, int* code_error) {
                 
    MY_ASSERT(queue != NULL, PTR_ERROR);
    MY_ASSERT(event != NULL, PTR_ERROR);

    pthread_mutex_lock(&queue->mutex);

    if(queue->size == MAX_QUEUE_SIZE) {
        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }

    queue->events[queue->tail] = event;
    queue->tail = (queue->tail + 1) % MAX_QUEUE_SIZE;
    queue->size++;

    pthread_cond_signal(&queue->cond); 
    pthread_mutex_unlock(&queue->mutex);

    return 1;
}

Event* queue_pop(EventQueue* queue, int* code_error) {

    MY_ASSERT(queue != NULL, PTR_ERROR);

    pthread_mutex_lock(&queue->mutex);

    while(queue->size <= 0) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }

    Event* event = queue->events[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;
    queue->size--;

    pthread_mutex_unlock(&queue->mutex);

    return event;
}

