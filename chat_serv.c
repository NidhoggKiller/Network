#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUF_SIZE 100
#define MAX_CLNT 256
#define NAME_SIZE 20

typedef struct {
    int sock;
    char name[NAME_SIZE];
    int in_private_chat;
    int partner_sock;
    int invited_by;
} Client;

void *handle_clnt(void *arg);
void send_msg(char *msg, int len, int exclude_sock);
void error_handling(const char *msg);
int find_client_by_name(const char *name);
int find_client_by_sock(int sock);
void remove_client(int sock);
void print_clients();

int clnt_cnt = 0;
Client clnt_socks[MAX_CLNT];
pthread_mutex_t mutx;

int main(int argc, char *argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);

        pthread_mutex_lock(&mutx);
        if (clnt_cnt >= MAX_CLNT) {
            close(clnt_sock);
            pthread_mutex_unlock(&mutx);
            continue;
        }

        Client *new_client = (Client *)malloc(sizeof(Client));
        new_client->sock = clnt_sock;
        snprintf(new_client->name, NAME_SIZE, "Client%d", clnt_cnt);
        new_client->in_private_chat = 0;
        new_client->partner_sock = -1;
        new_client->invited_by = -1;
        clnt_socks[clnt_cnt++] = *new_client;
        print_clients();
        pthread_mutex_unlock(&mutx);

        pthread_create(&t_id, NULL, handle_clnt, (void *)new_client);
        pthread_detach(t_id);
        printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
    }
    close(serv_sock);
    pthread_mutex_destroy(&mutx);
    return 0;
}

void *handle_clnt(void *arg) {
    Client *clnt = (Client *)arg;
    int clnt_sock = clnt->sock;
    int str_len = 0;
    char msg[BUF_SIZE];
    char name_msg[NAME_SIZE + BUF_SIZE];

    while ((str_len = read(clnt_sock, msg, sizeof(msg) - 1)) != 0) {
        if (str_len == -1) {
            break;
        }
        msg[str_len] = '\0';

        pthread_mutex_lock(&mutx);
        int clnt_index = find_client_by_sock(clnt_sock);
        pthread_mutex_unlock(&mutx);

        printf("Received message from %s: %s\n", clnt_socks[clnt_index].name, msg);

        if (strcmp(msg, "/id\n") == 0) {
            snprintf(name_msg, sizeof(name_msg), "Your ID: %s\n", clnt_socks[clnt_index].name);
            write(clnt_sock, name_msg, strlen(name_msg));
        } else if (strncmp(msg, "/invite ", 8) == 0) {
            char invited_name[NAME_SIZE];
            sscanf(msg + 8, "%s", invited_name);
            pthread_mutex_lock(&mutx);
            int invited_index = find_client_by_name(invited_name);
            if (invited_index == -1) {
                pthread_mutex_unlock(&mutx);
                snprintf(name_msg, sizeof(name_msg), "用户不存在\n");
                write(clnt_sock, name_msg, strlen(name_msg));
            } else if (clnt_socks[invited_index].in_private_chat == 1 || clnt_socks[invited_index].invited_by != -1) {
                pthread_mutex_unlock(&mutx);
                snprintf(name_msg, sizeof(name_msg), "用户正在私聊中或已被邀请\n");
                write(clnt_sock, name_msg, strlen(name_msg));
            } else {
                snprintf(name_msg, sizeof(name_msg), "%s 邀请你进入一对一聊天室。输入/yes接受，/no拒绝\n", clnt_socks[clnt_index].name);
                write(clnt_socks[invited_index].sock, name_msg, strlen(name_msg));
                clnt_socks[clnt_index].in_private_chat = 1;
                clnt_socks[clnt_index].partner_sock = clnt_socks[invited_index].sock;
                clnt_socks[invited_index].invited_by = clnt_socks[clnt_index].sock;
                pthread_mutex_unlock(&mutx);
            }
        } else if (strncmp(msg, "/yes", 4) == 0) {
            pthread_mutex_lock(&mutx);
            int partner_sock = clnt_socks[clnt_index].invited_by;
            int partner_index = find_client_by_sock(partner_sock);
            if (partner_sock != -1 && clnt_socks[partner_index].in_private_chat == 1) {
                snprintf(name_msg, sizeof(name_msg), "对方已接受邀请，进入一对一聊天室\n");
                write(partner_sock, name_msg, strlen(name_msg));

                clnt_socks[clnt_index].in_private_chat = 1;
                clnt_socks[clnt_index].partner_sock = partner_sock;
                clnt_socks[partner_index].partner_sock = clnt_sock;

                snprintf(name_msg, sizeof(name_msg), "已进入一对一聊天模式\n");
                write(clnt_sock, name_msg, strlen(name_msg));
                write(partner_sock, name_msg, strlen(name_msg));
                pthread_mutex_unlock(&mutx);

                while (1) {
                    str_len = read(clnt_sock, msg, sizeof(msg) - 1);
                    if (str_len == 0 || strcmp(msg, "/out\n") == 0) {
                        snprintf(name_msg, sizeof(name_msg), "结束一对一聊天\n");
                        write(clnt_sock, name_msg, strlen(name_msg));
                        write(partner_sock, name_msg, strlen(name_msg));
                        pthread_mutex_lock(&mutx);
                        clnt_socks[clnt_index].in_private_chat = 0;
                        clnt_socks[clnt_index].partner_sock = -1;
                        clnt_socks[clnt_index].invited_by = -1;
                        clnt_socks[partner_index].in_private_chat = 0;
                        clnt_socks[partner_index].partner_sock = -1;
                        pthread_mutex_unlock(&mutx);
                        break;
                    }
                    msg[str_len] = '\0';
                    write(partner_sock, msg, str_len);
                }
            } else {
                pthread_mutex_unlock(&mutx);
            }
        } else if (strncmp(msg, "/no", 3) == 0) {
            pthread_mutex_lock(&mutx);
            int inviter_sock = clnt_socks[clnt_index].invited_by;
            int inviter_index = find_client_by_sock(inviter_sock);
            if (inviter_sock != -1) {
                snprintf(name_msg, sizeof(name_msg), "对方拒绝了邀请\n");
                write(inviter_sock, name_msg, strlen(name_msg));
                clnt_socks[clnt_index].in_private_chat = 0;
                clnt_socks[clnt_index].partner_sock = -1;
                clnt_socks[clnt_index].invited_by = -1;
                clnt_socks[inviter_index].in_private_chat = 0;
                clnt_socks[inviter_index].partner_sock = -1;
            }
            pthread_mutex_unlock(&mutx);
        } else if (clnt_socks[clnt_index].in_private_chat == 1) {
            pthread_mutex_lock(&mutx);
            int partner_sock = clnt_socks[clnt_index].partner_sock;
            pthread_mutex_unlock(&mutx);
            write(partner_sock, msg, str_len);
        } else {
            send_msg(msg, str_len, clnt_sock);
        }
    }

    remove_client(clnt_sock);
    close(clnt_sock);
    free(clnt);  // Free allocated memory
    return NULL;
}

void send_msg(char *msg, int len, int exclude_sock) {
    pthread_mutex_lock(&mutx);
    for (int i = 0; i < clnt_cnt; i++) {
        if (clnt_socks[i].sock != exclude_sock && clnt_socks[i].in_private_chat == 0)
            write(clnt_socks[i].sock, msg, len);
    }
    pthread_mutex_unlock(&mutx);
}

void error_handling(const char *msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

int find_client_by_name(const char *name) {
    for (int i = 0; i < clnt_cnt; i++) {
        if (strcmp(clnt_socks[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int find_client_by_sock(int sock) {
    for (int i = 0; i < clnt_cnt; i++) {
        if (clnt_socks[i].sock == sock) {
            return i;
        }
    }
    return -1;
}

void remove_client(int sock) {
    pthread_mutex_lock(&mutx);
    for (int i = 0; i < clnt_cnt; i++) {
        if (clnt_socks[i].sock == sock) {
            while (i < clnt_cnt - 1) {
                clnt_socks[i] = clnt_socks[i + 1];
                i++;
            }
            clnt_cnt--;
            break;
        }
    }
    print_clients();
    pthread_mutex_unlock(&mutx);
}

void print_clients() {
    printf("Current clients:\n");
    for (int i = 0; i < clnt_cnt; i++) {
        printf("Client %d: %s\n", i, clnt_socks[i].name);
    }
}
