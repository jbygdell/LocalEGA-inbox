#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "mq-utils.h"
#include "mq-config.h"
#include "mq-notify.h"

extern char* username;

/*
 *
 * Send the messages to the broker 
 *
 *
 */
#define MQ_OP_UPLOAD "up"
#define MQ_OP_REMOVE "rm"
#define MQ_OP_RENAME "mv"

int
mq_send_upload(const char* filepath)
{ 
  D2("%s uploaded %s", username, filepath);
  D3("sending '%s' to %s", MQ_OP_UPLOAD, mq_options->connection);
  return 0;
}

int
mq_send_remove(const char* filepath)
{ 
  D2("%s removed %s", username, filepath);
  D3("sending '%s' to %s", MQ_OP_REMOVE, mq_options->connection);
  return 0;
}

int
mq_send_rename(const char* oldpath, const char* newpath)
{ 
  D2("%s renamed %s into %s", username, oldpath, newpath);
  D3("sending '%s' to %s", MQ_OP_RENAME, mq_options->connection);
  return 0;
}
