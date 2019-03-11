#ifndef __MQ_LISTENER_H_INCLUDED__
#define __MQ_LISTENER_H_INCLUDED__

extern int msg_queue_id;
extern char* username;

int mq_listener_spawn(const char* username);

int mq_send_upload_to_queue(const char* filepath);
int mq_send_remove_to_queue(const char* filepath);
int mq_send_rename_to_queue(const char* oldpath, const char* newpath);
int mq_send_exit_to_queue();

#endif /* !__MQ_LISTENER_H_INCLUDED__ */
