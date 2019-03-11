#ifndef __MQ_MSG_H_INCLUDED__
#define __MQ_MSG_H_INCLUDED__

#include <sys/types.h>

/* The operation is encoded in the message type */
#define MSG_UPLOAD 1L
#define MSG_REMOVE 2L
#define MSG_RENAME 3L
#define MSG_EXIT   4L

/* The filename is malloc-ed and we keep track of its length.
   Hopefully, copying the msg into the queue will not only copy the pointer,
   and it will know about to copy the bytes from the filepaths.

   Use NULL and 0 if there is no filepath_old (ie, if not a rename operation).
*/
struct mq_msg_s {
  long type;
  char* filepath;
  size_t filepath_len;
  char* filepath_old;
  size_t filepath_old_len;
};

typedef struct mq_msg_s mq_msg_t;


#endif /* !__MQ_MSG_H_INCLUDED__ */
