#include "server.hpp"

int get_random_num(int leftBorder, int rightBorder) {
    return rand() % (rightBorder - leftBorder + 1) + leftBorder;
}

void queue_ctor(EventQueue* queue, int* code_error) {

    MY_ASSERT(queue != NULL, PTR_ERROR);

    queue->events = (Event**)calloc(MAX_QUEUE_SIZE, sizeof(Event*));
    MY_ASSERT(queue->events != NULL, PTR_ERROR);

    queue->head = 0;
    queue->tail = -1;
    queue->size = 0;

    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init (&queue->not_empty, NULL);
    pthread_cond_init (&queue->not_full, NULL);
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

    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy (&queue->not_empty);
    pthread_cond_destroy (&queue->not_full);
}

void tracker_ctor(EventTracker* tracker, int* code_error) {

    MY_ASSERT(tracker != NULL, PTR_ERROR);

    tracker->hash_table = NULL;
    pthread_mutex_init(&tracker->mutex, NULL);
}

void tracker_dtor(EventTracker* tracker, int* code_error) {

    MY_ASSERT(tracker != NULL, PTR_ERROR);
    
    ProcessedEvents *entry, *tmp;

    HASH_ITER(hh, tracker->hash_table, entry, tmp) {
        HASH_DEL(tracker->hash_table, entry);
        free(entry);
    }
    tracker->hash_table = NULL; 

    pthread_mutex_destroy(&tracker->mutex);
}

void queue_push(EventQueue* queue, Event* event, int* code_error) {
                 
    MY_ASSERT(queue != NULL,                  PTR_ERROR);
    MY_ASSERT(event != NULL,                  PTR_ERROR);
    MY_ASSERT(queue->size != MAX_QUEUE_SIZE, SIZE_ERROR);

    pthread_mutex_lock(&queue->lock);

    while(queue->size == MAX_QUEUE_SIZE) {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }

    queue->tail = (queue->tail + 1) % MAX_QUEUE_SIZE;
    queue->events[queue->tail] = event;
    queue->size++;

    pthread_cond_signal (&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}

Event* queue_pop(EventQueue* queue, int* code_error) {

    MY_ASSERT(queue != NULL,     PTR_ERROR);

    pthread_mutex_lock(&queue->lock);

    while(queue->size == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }

    Event* event = queue->events[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;
    queue->size--;

    pthread_cond_signal (&queue->not_full);
    pthread_mutex_unlock(&queue->lock);

    return event;
}

bool queue_is_full(EventQueue *queue, int* code_error) {

    MY_ASSERT(queue != NULL,     PTR_ERROR);

    pthread_mutex_lock(&queue->lock);

    bool is_full = (queue->size == MAX_QUEUE_SIZE);

    pthread_mutex_unlock(&queue->lock);

    return is_full;
}

bool is_duplicate(EventTracker* event_tracker, const char* id, int* code_error) {

    MY_ASSERT(event_tracker != NULL, PTR_ERROR);
    MY_ASSERT(id            != NULL, PTR_ERROR);

    pthread_mutex_lock(&event_tracker->mutex);

    ProcessedEvents* found = NULL;
    HASH_FIND_STR(event_tracker->hash_table, id, found);

    if(!found) {
        ProcessedEvents* new_entry = (ProcessedEvents*)calloc(1, sizeof(ProcessedEvents));
        strcpy(new_entry->id, id);
        HASH_ADD_STR(event_tracker->hash_table, id, new_entry);
    }

    pthread_mutex_unlock(&event_tracker->mutex);

    return (found != NULL);
}

void* worker_thread(void* arg) {
    ThreadArgs* args            = (ThreadArgs*)arg;
    EventQueue* queue           = args->queue;
    EventTracker* event_tracker = args->event_tracker;
    Metrics* metrics            = args->metrics;
    int* code_error             = args->code_error;

    while(true) {
        Event* event = queue_pop(queue, code_error);

        while(queue->size == 0) {
            pthread_cond_wait(&queue->not_empty, &queue->lock);
        }

        int processed_time_ms = get_random_num(10, 500);

        struct timespec ts = {
            .tv_sec = processed_time_ms / 1000,
            .tv_nsec = (processed_time_ms % 1000) * 1000000
        };
        nanosleep(&ts, NULL);

        bool duplicate = is_duplicate(event_tracker, event->id, code_error);

        pthread_mutex_lock(&metrics->lock);
        metrics->total_processed++;
        metrics->total_time_ms += processed_time_ms;
        metrics->total_duplicates += (int)duplicate;
        pthread_mutex_unlock(&metrics->lock);

        free(event);
    }
    return NULL;
}

Event* parse_event(const char* json_str, int* code_error) {

    MY_ASSERT(json_str != NULL, PTR_ERROR);

    json_error_t json_error;
    json_t* root = json_loads(json_str, 0, &json_error);
    MY_ASSERT(root != NULL, PARSE_ERROR);

    Event* event = (Event*)calloc(1, sizeof(Event));
    MY_ASSERT(event != NULL, PTR_ERROR);

    json_t* j_id     = json_object_get(root, "id");
    json_t* j_date   = json_object_get(root, "date");
    json_t* j_status = json_object_get(root, "status");

    MY_ASSERT(j_id != NULL && j_date != NULL && j_status != NULL, PARSE_ERROR);

    printf("KKKKKKKKK\n");

    const char* id   = json_string_value(j_id);
    const char* date = json_string_value(j_date);
    const int status = json_integer_value(j_status);

    strncpy(event->id, id, UUID_LEN - 1);
    strncpy(event->date, date, DATE_LEN - 1);
    event->id[UUID_LEN - 1]   = '\0';
    event->date[DATE_LEN - 1] = '\0';
    event->status = status;

    json_decref(root);
    return event;

    //сделать проверку на длину, тип правильной ли длины id data
}

EventArray parse_events_array(const char* json_str, int* code_error) {
    EventArray result = {NULL, 0};
    
    MY_ASSERT(json_str != NULL, PTR_ERROR);

    json_error_t json_error;
    json_t* root = json_loads(json_str, 0, &json_error);
    if (!root) {
        fprintf(stderr, "JSON error: %s (line %d, column %d)\n",
                json_error.text, json_error.line, json_error.column);
        *code_error = PARSE_ERROR;
        return result;
    }

    /* Проверяем основной формат */
    json_t* j_events = json_object_get(root, "events");
    if (!j_events || !json_is_array(j_events)) {
        fprintf(stderr, "Expected 'events' array in JSON\n");
        json_decref(root);
        *code_error = PARSE_ERROR;
        return result;
    }

    /* Выделяем память под результат */
    size_t array_size = json_array_size(j_events);
    result.events = (Event**)malloc(array_size * sizeof(Event*));
    if (!result.events) {
        *code_error = PTR_ERROR;
        json_decref(root);
        return result;
    }

    /* Парсим каждый элемент массива */
    size_t i;
    json_t* value;
    json_array_foreach(j_events, i, value) {
        if (!json_is_object(value)) {
            fprintf(stderr, "Event %zu is not an object\n", i);
            continue;
        }

        /* Конвертируем JSON объект обратно в строку для вашей функции */
        char* event_json = json_dumps(value, JSON_COMPACT);
        if (!event_json) {
            *code_error = PARSE_ERROR;
            continue;
        }

        /* Используем вашу существующую функцию для парсинга */
        Event* event = parse_event(event_json, code_error);
        free(event_json);
        
        if (!event) {
            continue;
        }

        result.events[result.count++] = event;
    }

    json_decref(root);
    return result;
}

void print_metrics(Metrics* metrics) {
    pthread_mutex_lock(&metrics->lock);
    printf("Processed: %d, Duplicates: %d, AvgTime: %.2fms\n",
           metrics->total_processed,
           metrics->total_duplicates,
           metrics->total_time_ms / metrics->total_processed);
    pthread_mutex_unlock(&metrics->lock);
}