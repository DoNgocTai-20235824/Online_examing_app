#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "protocol.h"
#include <message.h>
#include "session.h"
#include "account.h"
#include "db/db.h"

static void handle_create_room(ClientSession *session, const char *payload);
static void handle_list_rooms(ClientSession *session);
static void handle_join_room(ClientSession *session, const char *payload);
static void handle_submit_exam(ClientSession *session, const char *payload);
static void handle_practice_req(ClientSession *session, const char *payload);

static void* client_handler(void* arg){
    int connfd = *((int*)arg);
    free(arg);
    pthread_detach(pthread_self());

    ClientSession *session = session_alloc(connfd);
    if(!session){
        close(connfd);
        return NULL;
    }

    printf("[SERVER Thread %lu] Bat dau phuc vu client socket_fd = %d\n",
           (unsigned long)pthread_self(), connfd);
    Header hdr;
    char payload_buf[MAX_PAYLOAD_SIZE + 1];
    for(;;){
        memset(&hdr, 0, sizeof(hdr));
        memset(payload_buf, 0, sizeof(payload_buf));
        ssize_t bytes_read = recv_msg(connfd, &hdr, payload_buf, MAX_PAYLOAD_SIZE);
        if(bytes_read < 0){
            printf("[SERVER Thread %lu] Client socket_fd = %d ngat ket noi.\n",
                   (unsigned long)pthread_self(), connfd);
            break;
        }

        printf("[SERVER Thread %lu] Recv Opcode: %d, Length: %u\n",
               (unsigned long)pthread_self(), hdr.opcode, hdr.payload_len);
        switch(hdr.opcode){
            case OP_PING:
                reply(session, OP_PONG, "PONG", 5);
                break;
            case OP_LOGIN:
                handle_login(session, payload_buf);
                break;
            case OP_REGISTER:
                handle_register(session, payload_buf);
                break;
            case OP_CREATE_ROOM:
                handle_create_room(session, payload_buf);
                break;
            case OP_LIST_ROOMS:
                handle_list_rooms(session);
                break;
            case OP_JOIN_ROOM:
                handle_join_room(session, payload_buf);
                break;
            case OP_PRACTICE_REQ:
                handle_practice_req(session, payload_buf);
                break;
            case OP_SUBMIT_EXAM:
                handle_submit_exam(session, payload_buf);
                break;
            case OP_LOGOUT:
                handle_logout(session);
                break;
            default:
                fprintf(stderr, "[SERVER] Unknown Opcode: %d\n", hdr.opcode);
                reply(session, OP_ERROR, "UNKNOWN_OPCODE", 15);
                break;
        }
    }

    close(session->sockfd);
    session_free(session);
    printf("[SERVER Thread %lu] Hoan tat va giai phong dung luong.\n",
           (unsigned long)pthread_self());
    return NULL;
}

static void handle_create_room(ClientSession *session, const char *payload){
    printf("[AUTH] Handling CREATE_ROOM: %s\n", payload);
    reply(session, OP_CREATE_ROOM, "ROOM_CREATED_ID_101", 20);
}

static void handle_list_rooms(ClientSession *session){
    printf("[ROOM] Handling LIST_ROOMS\n");
    reply(session, OP_LIST_ROOMS, "ROOM_101: NOT_STARTED", 21);
}

static void handle_join_room(ClientSession *session, const char *payload){
    printf("[ROOM] Handling JOIN_ROOM: %s\n", payload);
    reply(session, OP_JOIN_ROOM, "JOIN_SUCCESS", 13);
}

static void handle_practice_req(ClientSession *session, const char *payload){
    printf("[PRACTICE] Handling PRACTICE_REQ: %s\n", payload);
    reply(session, OP_PRACTICE_REQ, "QUESTION_DATA_HERE", 19);
}

static void handle_submit_exam(ClientSession *session, const char *payload){
    printf("[EXAM] Handling SUBMIT EXAM: %s\n", payload);
    reply(session, OP_SUBMIT_EXAM, "SCORE: 8/10", 11);
}

static int setup_server_socket(int port){
    int listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listenfd < 0){
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
        perror("bind() failed");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    if(listen(listenfd, 10) < 0){
        perror("listen() failed");
        close(listenfd);
        exit(EXIT_FAILURE);
    }
    printf("[SERVER] Listening on port %d...\n", port);
    return listenfd;
}

int main(void){
    if(db_connect(DB_DEFAULT_PATH) != 0 || db_exec_file(DB_SCHEMA_PATH) != 0){
        fprintf(stderr, "[SERVER] Khong mo duoc CSDL. Hay chay server tu thu muc goc cua project.\n");
        return EXIT_FAILURE;
    }
    printf("[SERVER] Da mo CSDL %s\n", DB_DEFAULT_PATH);

    int listenfd = setup_server_socket(DEFAULT_PORT);
    for(;;){
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        int connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
        if(connfd < 0) continue;

        int *pconnfd = malloc(sizeof(*pconnfd));
        if(!pconnfd){
            perror("[SERVER] malloc failed");
            close(connfd);
            continue;
        }
        *pconnfd = connfd;

        pthread_t tid;
        int rc = pthread_create(&tid, NULL, client_handler, pconnfd);
        if(rc != 0){
            fprintf(stderr, "[SERVER] pthread_create failed: %s\n", strerror(rc));
            close(connfd);
            free(pconnfd);
        }
    }
}
