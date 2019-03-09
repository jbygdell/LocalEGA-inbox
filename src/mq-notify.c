#include <stdarg.h>

#include "log.h"
#include "mq-config.h"
#include "mq-notify.h"

#define MQ_OP_UPLOAD "up"
#define MQ_OP_REMOVE "rm"
#define MQ_OP_RENAME "mv"

int
mq_send_upload(const char* user, const char* filepath)
{ 
  logit("[MQ] %s uploaded %s", user, filepath);
#if DEBUG
  verbose("[MQ] sending '%s' to %s", MQ_OP_UPLOAD, mq_options->connection);
#endif
  return 0;
}

int
mq_send_remove(const char* user, const char* filepath)
{ 
  logit("[MQ] %s removed %s", user, filepath);
#if DEBUG
  verbose("[MQ] sending '%s' to %s", MQ_OP_REMOVE, mq_options->connection);
#endif
  return 0;
}

int
mq_send_rename(const char* user, const char* oldpath, const char* newpath)
{ 
  logit("[MQ] %s renamed %s into %s", user, oldpath, newpath);
#if DEBUG
  verbose("[MQ] sending '%s' to %s", MQ_OP_RENAME, mq_options->connection);
#endif
  return 0;
}

