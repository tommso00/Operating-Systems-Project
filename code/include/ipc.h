#ifndef IPC_H
#define IPC_H

#include "common.h"
#include "error_codes.h"

typedef enum {
    MSG_INVALID = 0,
    MSG_REQUEST,
    MSG_RESPONSE,
    MSG_EVENT
} msg_kind ;

typedef enum {
    CMD_NONE = 0,
    CMD_PING,
    CMD_INFO,
    CMD_SWITCH,
    CMD_SET_PARAM,
    CMD_GET_STATE,
    CMD_LINK_PARENT,
    CMD_ADD_CHILD,
    CMD_REMOVE_CHILD,
    CMD_TERMINATE,
    CMD_NOTIFY_OVERRIDE,
    CMD_NOTIFY_CRASH
} cmd ;

typedef struct {
    msg_kind kind;
    cmd cmd;
    device_id src_id;
    device_id dst_id;
    pid_t src_pid;
    int request_id;
    int status;
    char arg1[LABEL_MAX];
    char arg2[VALUE_MAX];
    char payload[PAYLOAD_MAX];
} message ;

int make_device_fifo_path(device_id id, char *buffer, size_t buffer_len);
int make_reply_fifo_path(pid_t pid, int request_id, char *buffer, size_t buffer_len);

int send_message(const char *fifo_path, const message *msg);
int recv_message(int fd, message *msg);
int request_reply(const char *request_fifo,
                       const char *reply_fifo,
                       const message *request,
                       message *response);

#endif