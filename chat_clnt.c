#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 100
#define NAME_SIZE 20

void * send_msg(void * arg);
void * recv_msg(void * arg);
void error_handling(char * msg);

char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void * thread_return;

    if (argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    sprintf(name, "[%s]", argv[3]); // Set username
    sock = socket(PF_INET, SOCK_STREAM, 0); // Create a socket

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]); // Set server IP address
    serv_addr.sin_port = htons(atoi(argv[2])); // Set port number

    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("connect() error"); // Connect to the server
    }

    // Create threads for sending and receiving messages
    pthread_create(&snd_thread, NULL, send_msg, (void *) &sock);
    pthread_create(&rcv_thread, NULL, recv_msg, (void *) &sock);
    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);
    close(sock); // Close the socket
    return 0;
}

void * send_msg(void * arg) {
    int sock = *((int *) arg);
    char name_msg[NAME_SIZE + BUF_SIZE];
    while (1) {
        fgets(msg, BUF_SIZE, stdin); // Read user input
        if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")) {
            close(sock);
            exit(0); // Exit program if user inputs "q" or "Q"
        } else if (!strncmp(msg, "/invite ", 8) || !strcmp(msg, "/yes\n") || !strcmp(msg, "/no\n") || !strcmp(msg, "/out\n") || !strcmp(msg, "/id\n")) {
            write(sock, msg, strlen(msg)); // Send special commands directly
        } else {
            sprintf(name_msg, "%s %s", name, msg); // Prepend username to the message
            write(sock, name_msg, strlen(name_msg)); // Send the message
        }
    }
    return NULL;
}

void * recv_msg(void * arg) {
    int sock = *((int *) arg);
    char name_msg[NAME_SIZE + BUF_SIZE];
    int str_len;
    while (1) {
        str_len = read(sock, name_msg, NAME_SIZE + BUF_SIZE - 1); // Read message from server
        if (str_len == -1)
            return (void *) -1;
        name_msg[str_len] = 0; // Null-terminate the string
        fputs(name_msg, stdout); // Print the message to the terminal
    }
    return NULL;
}

void error_handling(char * msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1); // Print error message and exit the program
}
