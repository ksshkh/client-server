#include "server.hpp"

int get_random_num(int leftBorder, int rightBorder) {
    return rand() % (rightBorder - leftBorder + 1) + leftBorder;
}

int setup_server_socket(int port, int* code_error) {

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    MY_ASSERT(server_fd != -1, SOCKET_ERROR);
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in address;
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(port);
    
    MY_ASSERT(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == 0, SOCKET_ERROR);
    MY_ASSERT(listen(server_fd, 4) == 0, SOCKET_ERROR);
    
    return server_fd;
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

    strncpy(event->id, id, UUID_LEN - 1);
    strncpy(event->date, date, DATE_LEN - 1);
    event->id[UUID_LEN - 1]   = '\0';
    event->date[DATE_LEN - 1] = '\0';
    event->status = status;

    return event;

    //сделать проверку на длину, тип правильной ли длины id data
}

void parse_events_array(const char* json_str, EventArray* event_array, int* code_error) {

    MY_ASSERT(json_str != NULL, PTR_ERROR);

    json_error_t json_error;
    json_t* root = json_loads(json_str, 0, &json_error);
    if (!root) {
        fprintf(stderr, "JSON error: %s (line %d, column %d)\n",
                json_error.text, json_error.line, json_error.column);
        *code_error = PARSE_ERROR;
        return;
    }

    json_t* j_events = json_object_get(root, "events");
    if (!j_events || !json_is_array(j_events)) {
        fprintf(stderr, "Expected 'events' array in JSON\n");
        json_decref(root);
        *code_error = PARSE_ERROR;
        return;
    }

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

void print_metrics(Metrics* metrics) {
    printf("Processed: %d, Duplicates: %d, AvgTime: %.2fms\n",
           metrics->total_processed,
           metrics->total_duplicates,
           metrics->total_time_ms / metrics->total_processed);
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);
    int code_error = 0; // надо передавать его в функцию

    EventQueue queue = {};
    queue_ctor(&queue, &code_error);
    Metrics metrics = {0, 0, 0.0};
    
    while(1) {
        // 1. Читаем длину сообщения
        uint32_t data_len;
        int len_read = read(client_socket, &data_len, sizeof(data_len));
        
        if(len_read <= 0) break;  // Соединение разорвано
        
        data_len = ntohl(data_len);  // Конвертируем из сетевого порядка байт
        
        // 2. Читаем само сообщение
        char* buffer = (char*)malloc(data_len + 1);
        int total_read = 0;
        
        while(total_read < data_len) {
            int bytes_read = read(client_socket, buffer + total_read, data_len - total_read);
            if(bytes_read <= 0) break;
            total_read += bytes_read;
        }
        
        if(total_read < data_len) {
            free(buffer);
            break;  // Не удалось прочитать полное сообщение
        }
        
        buffer[data_len] = '\0';  // Добавляем нуль-терминатор
        
        // 3. Обрабатываем JSON
        EventArray event_array = {0};
        parse_events_array(buffer, &event_array, &code_error);

        for(size_t i = 0; i < event_array.num_of_events; i++) {
            if(!queue_push(&queue, event_array.events[i], &code_error)) {
                free(event_array.events[i]); // отбрасываем если очередь полна
                printf("KICKED!!!!!!!!");
            }
        }

        while (queue.size > 0) {
            Event* event = queue_pop(&queue, &code_error);
            
            int t_interval_ms = get_random_num(10, 500);
            struct timespec ts = {
                .tv_sec = t_interval_ms / 1000,
                .tv_nsec = (t_interval_ms % 1000) * 1000000
            };
            nanosleep(&ts, NULL);
            
            metrics.total_processed++;
            metrics.total_time_ms += t_interval_ms;
            print_metrics(&metrics);
            
            free(event);
        }
        
        free(event_array.events);
        free(buffer);
        
    }
    
    close(client_socket);
    return NULL;
}

void queue_ctor(EventQueue* queue, int* code_error) {

    MY_ASSERT(queue != NULL, PTR_ERROR);

    queue->events = (Event**)calloc(MAX_QUEUE_SIZE, sizeof(Event*));
    MY_ASSERT(queue->events != NULL, PTR_ERROR);

    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
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
}

int queue_push(EventQueue* queue, Event* event, int* code_error) {
                 
    MY_ASSERT(queue != NULL,                  PTR_ERROR);
    MY_ASSERT(event != NULL,                  PTR_ERROR);

    if(queue->size == MAX_QUEUE_SIZE) {
        return 0;
    }

    queue->events[queue->tail] = event;
    queue->tail = (queue->tail + 1) % MAX_QUEUE_SIZE;
    queue->size++;

    return 1;
}

Event* queue_pop(EventQueue* queue, int* code_error) {

    MY_ASSERT(queue != NULL, PTR_ERROR);

    while(queue->size <= 0) {
        return NULL;
    }

    Event* event = queue->events[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;
    queue->size--;

    return event;
}

