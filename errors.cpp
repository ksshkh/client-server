#include "errors.hpp"

static const char* errors_names[] = {"NO_ERROR",
                                     "PTR_ERROR",
                                     "SIZE_ERROR",
                                     "SOCKET_ERROR",
                                     "IP_ERROR",
                                     "CONNECT_ERROR",
                                     "SEND_ERROR",
                                     "PARSE_ERROR",
                                     "READ_ERROR",
                                     "PORT_ERROR",
                                     "CLOSE_ERROR"};

void ErrorsPrint(FILE* stream, int* code_error) {
    for (int i = 0; i < N_ERROR; i++) {
        if (*code_error & (1 << i)) {
            fprintf(stream, ERR("%s"), errors_names[i]);
        }
    }
}