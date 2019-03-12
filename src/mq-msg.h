#ifndef __MQ_MSG_H_INCLUDED__
#define __MQ_MSG_H_INCLUDED__

/* #include <sys/types.h> */
#include <limits.h>

/* On Linux, it's 4096 */
#define MQ_PATH_MAX PATH_MAX

/* The operation is encoded in the message type */
#define MSG_UPLOAD 1L
#define MSG_REMOVE 2L
#define MSG_RENAME 3L
#define MSG_EXIT   4L

/*
  From: https://linux.die.net/man/3/msgsnd

  We need to define a struct that contains a field of type long,
  and then a data portion.

  We cannot pass char pointers, since it would point to unknown memory
  for the listener.

  We use char arrays of fixed sizes for the data portion.
  Increase the size at compile time if not enough.
*/
struct mq_msg_s {
  long type;
  char path[MQ_PATH_MAX];
  char oldpath[MQ_PATH_MAX];
};

typedef struct mq_msg_s mq_msg_t;


#endif /* !__MQ_MSG_H_INCLUDED__ */
