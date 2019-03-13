#ifndef __MQ_NOTIFY_H_INCLUDED__
#define __MQ_NOTIFY_H_INCLUDED__

#include "amqp.h"

typedef amqp_connection_state_t mq_conn;

int mq_init(void);
int mq_clean(void);

int mq_send_upload(const char* username, const char* filepath);
int mq_send_remove(const char* username, const char* filepath);
int mq_send_rename(const char* username, const char* oldpath, const char* newpath);

#endif /* !__MQ_NOTIFY_H_INCLUDED__ */
