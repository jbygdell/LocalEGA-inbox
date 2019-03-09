#ifndef __MQ_NOTIFY_H_INCLUDED__
#define __MQ_NOTIFY_H_INCLUDED__

int mq_send_upload(const char* user, const char* filepath);
int mq_send_remove(const char* user, const char* filepath);
int mq_send_rename(const char* user, const char* oldpath, const char* newpath);

#endif /* !__MQ_NOTIFY_H_INCLUDED__ */
