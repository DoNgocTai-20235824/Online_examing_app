#include "room.h"
#include "status.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static Room g_rooms[MAX_ROOMS];
static size_t g_room_count;
static int g_next_room_id = 1;

pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static int valid_room_name(const char *name){
    if(!name || name[0] == '\0') return 0;
    size_t len = strnlen(name, ROOM_NAME_MAX + 1);
    if(len == 0 || len > ROOM_NAME_MAX) return 0;
    for(size_t i = 0; i < len; i++){
        if(name[i] == '|' || name[i] == '\n' || name[i] == '\r' ||
           (unsigned char)name[i] < 32 || (unsigned char)name[i] == 127)
            return 0;
    }
    return 1;
}

int room_create(ClientSession *owner, const char *name, int num_questions,
                int duration, int *out_room_id){
    if(!owner || !owner->is_authencated || owner->username[0] == '\0')
        return ST_NOT_LOGGED_IN;
    if(!valid_room_name(name) ||
       num_questions < 1 || num_questions > MAX_ROOM_QUESTIONS ||
       duration < 30 || duration > 3600)
        return ST_BAD_REQUEST;
    pthread_mutex_lock(&g_lock);
    if(owner->current_room_id != -1){
        pthread_mutex_unlock(&g_lock);
        return ST_ROOM_BAD_STATE;
    }
    if(g_room_count == MAX_ROOMS || g_next_room_id == INT32_MAX){
        pthread_mutex_unlock(&g_lock);
        return ST_UNKNOWN_ERROR;
    }

    Room *room = &g_rooms[g_room_count];
    memset(room, 0, sizeof(*room));
    room->id = g_next_room_id++;
    snprintf(room->name, sizeof(room->name), "%s", name);
    snprintf(room->owner, sizeof(room->owner), "%s", owner->username);
    room->num_questions = num_questions;
    room->duration = duration;
    room->state = ROOM_WAITING;
    room->members[0].fd = owner->sockfd;
    snprintf(room->members[0].username, sizeof(room->members[0].username),
             "%s", owner->username);
    room->member_count = 1;
    owner->current_room_id = room->id;
    g_room_count++;
    if(out_room_id) *out_room_id = room->id;
    pthread_mutex_unlock(&g_lock);
    return ST_OK;
}

int room_get_snapshot(int room_id, Room *out){
    if(room_id <= 0 || !out) return -1;

    pthread_mutex_lock(&g_lock);
    for(size_t i = 0; i < g_room_count; i++){
        if(g_rooms[i].id == room_id){
            *out = g_rooms[i];
            pthread_mutex_unlock(&g_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int room_list_snapshot(Room *out, size_t capacity, size_t *out_count){
    if(!out_count || (capacity > 0 && !out)) return -1;

    pthread_mutex_lock(&g_lock);
    *out_count = g_room_count;
    if(capacity < g_room_count){
        pthread_mutex_unlock(&g_lock);
        return -1;
    }
    if(g_room_count > 0)
        memcpy(out, g_rooms, g_room_count * sizeof(*out));
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int room_add_member(Room* r, const char* username, int fd){
    if(!r || r->member_count >= MAX_ROOM_MEMBERS) return -1;
    for(int i = 0; i < r->member_count; i++){
        if(strcmp(username, r->members[i].username) == 0){
            r->members[i].fd = fd; // Cập nhật lại socket nếu reconnect
            return 0;
        }
    }
    Participant* p = &(r->members[r->member_count++]);
    memset(p, 0, sizeof(Participant));
    strncpy(p->username, username, sizeof(p->username));
    p->fd = fd;
    p->submitted = 0;
    p->correct = 0;
    p->submit_time = 0;
    return 0;
}

Participant* room_find_member(Room* r, const char* username){
    if(!r || !username) return NULL;
    for(int i = 0; i < r->member_count; i++){
        if(strcmp(r->members[i].username, username) == 0){
            return &(r->members[i]);
        }
    }
    return NULL;
}

int room_remove_member(Room* r, const char* username){
    if(!r || !username) return -1;
    for(int i = 0; i < r->member_count; i++){
        if(strcmp(r->members[i].username, username) == 0){
            for(int j = i; j < r->member_count - 1; j++){
                r->members[j] = r->members[j+1];
            }
            r->member_count--;
            return 0;
        }
    }
    return -1;
}
