#include "dns_thread.h"

static int (*has_finished)(void);
static void (*set_finished)(void);

void* dns_thread_main(void* args) {
    int listen_socket;

    // Fetch the necessary functions from arguments
    thread_args_t* arguments = args;
    has_finished = arguments->has_finished;
    set_finished = arguments->set_finished;

    printf("\nStarting listener...\n\n");
    if(!initialize_listener(&listen_socket)) {
        exit(1);
    }

    printf("\nStarted listener.\n\n");
    if(!start_listener(&listen_socket)) {
        exit(1);
    }

    return NULL;
}

int initialize_listener(int* listen_socket) {
    struct sockaddr_in listen_addr;
    int sockoptval = 1;

    // Create a listen socket over IP for UDP.
    if ((*listen_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("cannot create socket");
        return 0;
    }

    // Allow immediate reuse of the port.
    setsockopt(*listen_socket, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));

    // Define socket listening location.
    memset((char*) &listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET; // IPv4
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Host address
    listen_addr.sin_port = htons(PORT); // Port to listen to

    // Bind the listen socket to a port on an interface.
    if (bind(*listen_socket, (struct sockaddr*) &listen_addr, sizeof(listen_addr)) < 0) {
        perror("bind failed");
        return 0;
    }

    return 1;
}

void handle_datagram(unsigned char* buffer_in, ssize_t buffer_size_in, unsigned char** buffer_out,
        ssize_t* buffer_size_out) {
    dnsmsg_t question, reply;
    int error_flag = 0;
    printf("Hey, you there! I received a packet!\n");
    question = interpret_question(buffer_in, buffer_size_in, &error_flag);
    if(!error_flag) {
        reply = make_reply(question);

        serialize_message(reply, buffer_out, buffer_size_out);

        free_message(question);
        free_message(reply);
    }
}

int start_listener(int* listen_socket) {
    unsigned char buffer_in[BUFFER_SIZE];
    unsigned char* buffer_out;
    ssize_t buffer_size_in, buffer_size_out;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_length;
    client_addr_length = sizeof(client_addr);

    while (!has_finished()) {

        // TODO: the following line is blocking, allow some kind of mechanism to allow has_finished() to unblock the recv
        /*
         * Try reading an udp packet in our buffer. The client (who sent the datagram) details are
         * also saved.
         * Fourth argument is zero, we don't need any special behavior for this function.
         */
        while ((buffer_size_in = recvfrom(*listen_socket, buffer_in, sizeof(buffer_in), 0,
            (struct sockaddr*) &client_addr, &client_addr_length)) < 0) {
            if ((errno != ECHILD) && (errno != EINTR)) {
                perror("accept failed");
                return 0;
            }
        }

        // Read the buffer and convert it to a reply buffer, then send it if
        // the buffer_size_out is larger than 0. Would be 0 if an error occured.
        // This is sent over the existing socket.
        buffer_size_out = 0;
        handle_datagram(buffer_in, buffer_size_in, &buffer_out, &buffer_size_out);
        if(buffer_size_out > 0) {
            if (sendto(*listen_socket, buffer_out, buffer_size_out, 0,
                    (struct sockaddr*) &client_addr, client_addr_length) < 0) {
                perror("send failed");
                return 0;
            }
            free(buffer_out);
        }

        printf("Finished handling a datagram, listening for new ones.\n");
    }

    return 1;
}
