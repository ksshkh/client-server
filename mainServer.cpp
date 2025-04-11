#include "server.hpp"

int main(void) {

    int code_error = 0;
    
    int server_fd = setup_server_socket(PORT, &code_error);
    if(server_fd == -1) {
        fprintf(stderr, "Failed to setup server socket\n");
        return 1;
    }
    
    printf("Server started on port %d\n", PORT);
    printf("Waiting for connections...\n");

    while(true) {
        sockaddr_in client_address;
        socklen_t client_addrlen = sizeof(client_address);
        
        int* client_socket = (int*)malloc(sizeof(int));
        *client_socket = accept(server_fd, (struct sockaddr*)&client_address, &client_addrlen);

        ThreadArgs* args    = (ThreadArgs*)calloc(1, sizeof(ThreadArgs));
        args->client_socket = *client_socket;
        args->code_error    = &code_error;
        
        if (*client_socket < 0) {
            perror("accept failed");
            free(client_socket);
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("New connection from %s:%d\n", client_ip, ntohs(client_address.sin_port));

        // Создаем поток для обработки клиента
        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, run_server, args) != 0) {
            perror("pthread_create failed");
            close(*client_socket);
            free(client_socket);
            continue;
        }

        // Отсоединяем поток, чтобы он мог завершиться самостоятельно
        pthread_detach(thread_id);
    }

    close(server_fd);

    return 0;
}