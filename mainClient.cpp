#include "client.hpp"

const char* SERVER_IP = "127.0.0.1";

int main(void) {

    srand(time(NULL));

    int* code_error = (int*)calloc(1, sizeof(int));
    *code_error     = NO_ERROR;

    BufferEvents buff_events = {};
    size_t events_counter    = 0;

    buffer_events_ctor(&buff_events, code_error);

    int sock = connect_to_server(SERVER_IP, PORT, code_error);
    MY_ASSERT(sock != -1, SOCKET_ERROR);

    while(true) {
        int t_interval_ms = get_random_num(1, 1000);
        int num_of_events = get_random_num(1, 1000);

        Event* events = (Event*)calloc(num_of_events, sizeof(Event));;
        MY_ASSERT(events != NULL, PTR_ERROR);

        for(int i = 0; i < num_of_events; i++) {
            ++events_counter;
            if(events_counter % 100 == 0) { // берётся рандомное событие из буффера рандомных событий
                int right_border    = buff_events.full_buffer ? BUFFER_SIZE - 1 : buff_events.ip - 1;
                int random_ip_event = get_random_num(0, right_border);

                events[i] = buff_events.buffer[random_ip_event];
            }
            else {
                events[i] = generate_event(code_error);
                add_in_buffer(&buff_events, events[i], code_error);
            }
        }

        printf("Generated %d events. Next batch in %d ms\n", num_of_events, t_interval_ms);

        char* json = events_to_json(events, num_of_events, code_error);
        if(json) {
            send_json_data(sock, json, code_error);
            printf("Data was sent.\n");
            free(json);
        }

        free(events);

        struct timespec ts = {
            .tv_sec = t_interval_ms / 1000,
            .tv_nsec = (t_interval_ms % 1000) * 1000000
        };
        nanosleep(&ts, NULL);
    }

    buffer_events_dtor(&buff_events, code_error);
    MY_ASSERT(close(sock) == 0, CLOSE_ERROR);

    return 0;
}