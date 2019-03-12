#ifndef __MQ_LISTENER_H_INCLUDED__
#define __MQ_LISTENER_H_INCLUDED__

extern int msg_queue_id;
extern char* username;

int mq_listener_spawn(const char* username);

int ipc_send_upload(const char* filepath);
int ipc_send_remove(const char* filepath);
int ipc_send_rename(const char* oldpath, const char* newpath);
int ipc_send_exit();

#endif /* !__MQ_LISTENER_H_INCLUDED__ */
