#ifndef ROOM_H
#define ROOM_H

#include <pthread.h>
#include <stddef.h>
#include <time.h>
#include "session.h"

#define MAX_ROOMS 100
#define MAX_ROOM_QUESTIONS 50
#define MAX_ROOM_MEMBERS 100
#define ROOM_NAME_MAX 63
#define ROOM_USERNAME_MAX 63

// Trạng thái phòng thi
typedef enum {
    ROOM_WAITING,
    ROOM_RUNNING,
    ROOM_FINISHED
} RoomState;

// Thông tin người trong phòng
typedef struct {
    char username[ROOM_USERNAME_MAX + 1];
    int fd; // Socket
    int submitted;
    int correct;
    time_t submit_time;
} Participant;

// Thông tin phòng
typedef struct {
    int id;
    char name[ROOM_NAME_MAX + 1];
    char owner[ROOM_USERNAME_MAX + 1];
    int num_questions;
    int duration;
    RoomState state;
    time_t start_time;
    time_t end_time;
    int qids[MAX_ROOM_QUESTIONS];
    Participant members[MAX_ROOM_MEMBERS];
    int member_count;
} Room;

extern pthread_mutex_t g_lock;

int room_create(ClientSession *owner, const char *name, int num_questions,
                int duration, int *out_room_id);

int room_get_snapshot(int room_id, Room *out);

int room_list_snapshot(Room *out, size_t capacity, size_t *out_count);

int room_add_member(Room* r, const char* username, int fd);

int room_remove_member(Room* r, const char* username);

Participant* room_find_member(Room* r, const char* username);

#endif
