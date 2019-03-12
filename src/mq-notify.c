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
 * They are JSON formatted as
 *
 * {
 *                  'user': <str>,
 *              'filepath': <str>,
 *             'operation': ( "upload" | "remove" | "rename" ),
 *              'filesize': <num>,
 *               'oldpath': <str>, // Ignored if not "rename"
 *          'file_created': <num>, // a UNIX timestamp
 *    'file_last_modified': <num>, // a UNIX timestamp
 *   'encrypted_checksums': [{ 'type': <str>, 'value': <checksum as HEX> },
 *                           { 'type': <str>, 'value': <checksum as HEX> },
 *                           ...
 *                          ]
 * }
 *
 * The checksum algorithm type is 'md5', or 'sha256'.
 * 'sha256' is prefered.
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
