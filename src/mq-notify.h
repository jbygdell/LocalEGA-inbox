#ifndef __MQ_NOTIFY_H_INCLUDED__
#define __MQ_NOTIFY_H_INCLUDED__

int mq_init(void);
int mq_clean(void);

int mq_send_upload(const char* username, const char* filepath, const char* hexdigest, const off_t filesize, const time_t modified);
int mq_send_remove(const char* username, const char* filepath);
int mq_send_rename(const char* username, const char* oldpath, const char* newpath);

#endif /* !__MQ_NOTIFY_H_INCLUDED__ */
