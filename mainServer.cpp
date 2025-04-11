#include "server.hpp"

int main(void) {

    int* code_error = (int*)calloc(1, sizeof(int));
    *code_error     = NO_ERROR;
    
    int server_fd = setup_server_socket(PORT, code_error);
    MY_ASSERT(server_fd != -1, SOCKET_ERROR);
    
    printf("Server started on port %d\n", PORT);
    printf("Waiting for connections...\n");

    while(true) {
        sockaddr_in client_address;
        socklen_t client_addrlen = sizeof(client_address);
        
        int client_socket = accept(server_fd, (struct sockaddr*)&client_address, &client_addrlen);
        MY_ASSERT(client_socket >= 0, SOCKET_ERROR);

        ThreadArgs* args = (ThreadArgs*)calloc(1, sizeof(ThreadArgs));
        MY_ASSERT(args != NULL, PTR_ERROR);

        args->client_socket = client_socket;
        args->code_error    = code_error;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("New connection from %s:%d\n", client_ip, ntohs(client_address.sin_port));

        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, run_server, args) != 0) {            
            perror("pthread_create failed");
            MY_ASSERT(close(client_socket) == 0, CLOSE_ERROR);
            continue;
        }

        pthread_detach(thread_id);
    }

    MY_ASSERT(close(server_fd) == 0, CLOSE_ERROR);

    return 0;
}