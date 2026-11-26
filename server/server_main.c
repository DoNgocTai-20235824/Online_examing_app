#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "protocol.h"
#include "status.h"
#include <message.h>
#include "session.h"
#include "account.h"
#include "db/db.h"
#include "room.h"

static void handle_create_room(ClientSession *session, const char *payload);
static int parse_decimal_field(const char *text, int *out);
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
        int perm = check_permission(session, hdr.opcode);
        if(perm == 204){
            printf("[SECURITY] Client fd = %d bị từ chối Opcode %d: Chưa đăng nhập (204)\n", session->sockfd, hdr.opcode);
            char err_msg[] = "204|Not logged in";
            reply(session, hdr.opcode, err_msg, sizeof(err_msg) - 1);
            continue; // Bỏ qua switch, quay lại chờ request tiếp theo
        }
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

/**
 * Xử lý Opcode OP_CREATE_ROOM (Opcode 3)
 * Payload nhận vào từ Client có dạng: "room_name|num_questions|duration"
 */
static void handle_create_room(ClientSession *session, const char *payload){
    char input[MAX_PAYLOAD_SIZE + 1];
    size_t payload_len = payload ? strnlen(payload, sizeof(input)) : 0;
    if(payload_len == 0 || payload_len >= sizeof(input)){
        reply(session, OP_CREATE_ROOM, "200|Expected name|question_count|duration_seconds",
              sizeof("200|Expected name|question_count|duration_seconds") - 1);
        return;
    }
    memcpy(input, payload, payload_len + 1);

    char *name = input;
    char *questions_text = strchr(name, '|');
    if(!questions_text){
        reply(session, OP_CREATE_ROOM, "200|Expected name|question_count|duration_seconds",
              sizeof("200|Expected name|question_count|duration_seconds") - 1);
        return;
    }
    *questions_text++ = '\0';
    char *duration_text = strchr(questions_text, '|');
    if(!duration_text){
        reply(session, OP_CREATE_ROOM, "200|Expected name|question_count|duration_seconds",
              sizeof("200|Expected name|question_count|duration_seconds") - 1);
        return;
    }
    *duration_text++ = '\0';

    int num_questions, duration;
    if(strchr(duration_text, '|') ||
       !parse_decimal_field(questions_text, &num_questions) ||
       !parse_decimal_field(duration_text, &duration)){
        reply(session, OP_CREATE_ROOM, "200|Expected name|question_count|duration_seconds",
              sizeof("200|Expected name|question_count|duration_seconds") - 1);
        return;
    }

    int room_id = 0;
    int status = room_create(session, name, num_questions, duration, &room_id);
    char response[128];
    const char *message;
    switch(status){
        case ST_OK:             message = "Room created"; break;
        case ST_BAD_REQUEST:    message = "Invalid room settings"; break;
        case ST_NOT_LOGGED_IN:  message = "Not logged in"; break;
        case ST_ROOM_BAD_STATE: message = "Already in a room"; break;
        default:                message = "Could not create room"; break;
    }

    int response_len;
    if(status == ST_OK)
        response_len = snprintf(response, sizeof(response), "%d|%s|%d",
                                status, message, room_id);
    else
        response_len = snprintf(response, sizeof(response), "%d|%s",
                                status, message);
    if(response_len < 0 || (size_t)response_len >= sizeof(response)){
        reply(session, OP_ERROR, "999|Response formatting failed",
              sizeof("999|Response formatting failed") - 1);
        return;
    }
    reply(session, OP_CREATE_ROOM, response, (uint32_t)response_len);
    if(status == ST_OK)
        printf("[ROOM] %s created room id=%d name=%s\n",
               session->username, room_id, name);
}

static int parse_decimal_field(const char *text, int *out){
    if(!text || !text[0] || !out) return 0;
    for(const unsigned char *p = (const unsigned char *)text; *p; p++)
        if(*p < '0' || *p > '9') return 0;

    errno = 0;
    char *end;
    long value = strtol(text, &end, 10);
    if(errno == ERANGE || *end != '\0' || value > INT_MAX) return 0;
    *out = (int)value;
    return 1;
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
