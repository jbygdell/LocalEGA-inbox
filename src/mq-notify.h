#ifndef __MQ_NOTIFY_H_INCLUDED__
#define __MQ_NOTIFY_H_INCLUDED__

int mq_send_upload(const char* filepath);
int mq_send_remove(const char* filepath);
int mq_send_rename(const char* oldpath, const char* newpath);

#endif /* !__MQ_NOTIFY_H_INCLUDED__ */
