#include "server.hpp"

int main(void) {

    int code_error = 0;
    EventQueue queue;
    queue_ctor(&queue, &code_error);
    Metrics metrics            = {0, 0, 0.0, PTHREAD_MUTEX_INITIALIZER};
    EventTracker event_tracker = {NULL, PTHREAD_MUTEX_INITIALIZER};
    ThreadArgs args            = {&queue, &event_tracker, &metrics, &code_error};

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    pthread_t workers[NUM_OF_THREADS] = {};
    for (int i = 0; i < NUM_OF_THREADS; i++) {
        pthread_create(&workers[i], NULL, worker_thread, (void*)&args);
    }
    
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 10);
    printf("Server started on port %d\n", PORT);

    while(true) {
        int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept failed");
        continue;
    }

    char buffer[4096] = {0};
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer)-1);
    if (bytes_read <= 0) {
        close(client_fd);
        continue;
    }

    /* Парсим массив событий */
    EventArray events = parse_events_array(buffer, &code_error);
    
    pthread_mutex_lock(&queue.lock);
    for (size_t i = 0; i < events.count; i++) {
        if (queue.size < MAX_QUEUE_SIZE) {
            queue_push(&queue, events.events[i], &code_error);
        } else {
            printf("Queue full! Event dropped.\n");
            free(events.events[i]);
        }
    }
    pthread_mutex_unlock(&queue.lock);

    printf("Processed %zu events\n", events.count);
    free(events.events); // Освобождаем только массив, события уже в очереди
    close(client_fd);
    }

    close(server_fd);
    queue_dtor(&queue, &code_error);

    return 0;
}