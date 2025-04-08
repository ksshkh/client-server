#ifndef ERRORS_HPP
#define ERRORS_HPP

#define ERR(str) "\x1b[31m" str "\x1b[0m\n"

#include <stdio.h>

enum Errors {
    NO_ERROR   = 0,

    PTR_ERROR     = 1 << 1,
    SIZE_ERROR    = 1 << 2,
    SOCKET_ERROR  = 1 << 3,
    IP_ERROR      = 1 << 4,
    CONNECT_ERROR = 1 << 5,
    SEND_ERROR    = 1 << 6,
    PARSE_ERROR   = 1 << 7,
    
    N_ERROR    = 8
};

#define MY_ASSERT(expression, err) if(!(expression)) {                                                                      \
    fprintf(stderr, ERR("%s: %d (%s) My assertion failed: \"" #expression "\""), __FILE__, __LINE__, __func__);             \
    *code_error |= err;                                                                                                     \
}

void ErrorsPrint(FILE* stream, int* code_error);

#endif // ERRORS_HPP