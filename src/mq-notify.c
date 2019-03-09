#include <stdarg.h>

#include "log.h"
/* #include "mq-utils.h" */

/* #define verbose D1 */

#include "mq-notify.h"

#define MQ_OP_UPLOAD "up"
#define MQ_OP_REMOVE "rm"
#define MQ_OP_RENAME "mv"

int
mq_send_upload(const char* user, const char* filepath)
{ 
  verbose("[MQ] %s uploaded %s", user, filepath);
  return 0;
}

int
mq_send_remove(const char* user, const char* filepath)
{ 
  verbose("[MQ] %s removed %s", user, filepath);
  return 0;
}

int
mq_send_rename(const char* user, const char* oldpath, const char* newpath)
{ 
  verbose("[MQ] %s renamed %s into %s", user, oldpath, newpath);
  return 0;
}

