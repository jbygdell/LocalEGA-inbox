#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <amqp.h>
#include <amqp_ssl_socket.h>
#include <amqp_tcp_socket.h>

#include <errno.h>
#include <openssl/sha.h>
#include <stddef.h>

#include <assert.h>

// Only linux:
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <json-c/json.h>
#include <json-c/json_object.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Winsock2.h>
#else
#include <sys/time.h>
#endif

#include "mq-config.h"
#include "mq-notify.h"
#include "mq-utils.h"

extern char *username;

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
 *    'file_last_modified': <num>, // a UNIX timestamp
 *   'encrypted_checksums': [{ 'type': <str>, 'value': <checksum as HEX> },
 *                           { 'type': <str>, 'value': <checksum as HEX> },
 *                           ...
 *                          ]
 * }
 *
 * The checksum algorithm type is 'md5', or 'sha256'.
 * 'sha256' is preferred.
 */
#define MQ_OP_UPLOAD "upload"
#define MQ_OP_REMOVE "remove"
#define MQ_OP_RENAME "rename"

void sha256_hash_string(unsigned char hash[SHA256_DIGEST_LENGTH],
                        char outputBuffer[65]) {
  int i = 0;

  for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
    sprintf(outputBuffer + (i * 2), "%02x", (unsigned char)hash[i]);
  }

  outputBuffer[64] = 0;
}

int sha256_file(char const *path, char outputBuffer[65]) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return -534;

  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  const int bufSize = 32768;
  unsigned char *buffer = malloc(bufSize);
  int bytesRead = 0;
  if (!buffer)
    return ENOMEM;
  while ((bytesRead = fread(buffer, 1, bufSize, file))) {
    SHA256_Update(&sha256, buffer, bytesRead);
  }
  SHA256_Final(hash, &sha256);

  sha256_hash_string(hash, outputBuffer);
  fclose(file);
  free(buffer);
  return 0;
}

char *build_message(char const *user_info, char const *file_path,
                    char const *file_operation, char const *old_file_path) {

  D3("build and send message");
  struct stat st;

  off_t file_size = 0;
  char file_hash[65] = "";
  time_t file_modified = 0;

  if (strcmp(file_operation, MQ_OP_REMOVE) != 0 &&
      strcmp(file_operation, MQ_OP_RENAME) != 0) {
    // obtain filesize
    stat(file_path, &st);
    file_size = st.st_size;
    file_modified = st.st_mtime;
    D3("Size is %u", (int)file_size);
    // Obtain file_hash
    sha256_file(file_path, file_hash);
    D3("Hash is %s", file_hash);
  }

  // Create json message
  json_object *jsonobj = json_object_new_object();
  json_object *juser = json_object_new_string(user_info);
  json_object_object_add(jsonobj, "user", juser);
  json_object *jfile_operation = json_object_new_string(file_operation);
  json_object_object_add(jsonobj, "operation", jfile_operation);
  json_object *jfile_path = json_object_new_string(file_path);
  json_object_object_add(jsonobj, "filepath", jfile_path);
  json_object *jold_file_path = json_object_new_string(old_file_path);
  json_object_object_add(jsonobj, "oldpath", jold_file_path);
  json_object *jfile_size = json_object_new_int64(file_size);
  json_object_object_add(jsonobj, "filesize", jfile_size);
  json_object *jfile_creation = json_object_new_int64(0);
  json_object_object_add(jsonobj, "file_created", jfile_creation);
  json_object *jfile_modification = json_object_new_int64(file_modified);
  json_object_object_add(jsonobj, "file_last_modified", jfile_modification);

  json_object *jchecksum = json_object_new_object();
  json_object *jc_type = json_object_new_string("sha256");
  json_object_object_add(jchecksum, "type", jc_type);
  json_object *jc_value = json_object_new_string(file_hash);
  json_object_object_add(jchecksum, "value", jc_value);

  json_object *jarray = json_object_new_array();
  json_object_array_add(jarray, jchecksum);

  json_object_object_add(jsonobj, "encrypted_checksums", jarray);

  const char *json_string =
      json_object_to_json_string_ext(jsonobj, JSON_C_TO_STRING_NOSLASHESCAPE);
  char *res = malloc(strlen(json_string) + 1);
  strcpy(res, json_string);

  json_object_put(jsonobj); // free json object
  return res;
}

int send_rabbit_message(amqp_connection_state_t conn, char const *exchange,
                        char const *routing_key, char const *message) {

  D3("Exchange:%s, Routing_key:%s", exchange, routing_key);

  // create amqp message
  amqp_basic_properties_t props;
  props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
  props.content_type = amqp_cstring_bytes("application/json");
  props.delivery_mode = 2; // persistent delivery mode
  int ret = amqp_basic_publish(conn, 1, amqp_cstring_bytes(exchange),
                               amqp_cstring_bytes(routing_key), 0, 0, &props,
                               amqp_cstring_bytes(message));
  D3("return code is %u", ret);
  return ret;
}

int init_connection(amqp_connection_state_t conn,
                    struct amqp_connection_info *ci, int heartbeat) {
  // amqp
  amqp_socket_t *socket = NULL;
  // struct
  struct timeval tval;
  struct timeval *tv;

  // timeout
  tv = &tval;
  tv->tv_sec = 30;
  tv->tv_usec = 0;

  if (ci->ssl) { // ssl
    socket = amqp_ssl_socket_new(conn);
    if (!socket) {
      D2("Error: creating SSL/TLS socket");
      return -1;
    }
    amqp_ssl_socket_set_verify_peer(socket, 0);
    amqp_ssl_socket_set_verify_hostname(socket, 0);
    /* TODO read certificate options. Some of these options expects paths
    //that are not accessible while in chroot
    if (mq_options->set_ca_cert) {
      int ret = amqp_ssl_socket_set_cacert(socket, mq_options->ca_cert);
      if (ret < 0) {
        D2("Error: creating SSL/TLS socket");
        return -1;
      }
      if (mq_options->verify_peer) {
        amqp_ssl_socket_set_verify_peer(socket, 1);
        nextarg++;
      }
      if (mq_options->verifyhostname) {
        amqp_ssl_socket_set_verify_hostname(socket, 1);
        nextarg++;
      }
      if (mq_options->set_key) {
        ret = amqp_ssl_socket_set_key(socket, mq_options->client_cert,
    mq_options->client_key); if (ret < 0) { D2("Error: creating SSL/TLS
    socket"); return -1;
        }
      }
    }*/
  } else { // non ssl
    socket = amqp_tcp_socket_new(conn);
    if (!socket) {
      D2("Error: creating TCP socket (out of memory)");
      return -1;
    }
  }

  D3("Host is %s, port is %u", ci->host, ci->port);
  int ret = amqp_socket_open_noblock(socket, ci->host, ci->port, tv);
  if (ret < 0) {
    D2("Error: opening connection: %s", amqp_error_string2(ret));
    return -1;
  }

  amqp_rpc_reply_t amqp_ret =
      amqp_login(conn, ci->vhost, 0, 131072, heartbeat, AMQP_SASL_METHOD_PLAIN,
                 ci->user, ci->password);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    D2("Error: Logging in");
    return -1;
  }

  amqp_channel_open(conn, 1);
  amqp_ret = amqp_get_rpc_reply(conn);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    D2("Error: Opening channel");
    return -1;
  }
  return 0;
}

int close_connection(amqp_connection_state_t conn,
                     struct amqp_connection_info *ci) {

  amqp_rpc_reply_t amqp_ret = amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    D2("Error: Closing channel");
    return -1;
  }

  amqp_ret = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    D2("Error: Closing connection");
    return -1;
  }
  int ret = amqp_destroy_connection(conn);
  if (ret < 0) {
    D2("Error: Ending connection");
    return -1;
  }
  if (ci->ssl) { // check if ssl
    ret = amqp_uninitialize_ssl_library();
    if (ret < 0) {
      D2("Error: Uninitializing SSL library");
      return -1;
    }
  }
  return 0;
}

int send_message(const char *file_operation, const char *user_info,
                 const char *file_path, const char *new_file_path) {
  // params
  char const *conn_string;
  char const *exchange;
  char const *routing_key;
  int connection_attempts;
  int retry_delay;
  int heartbeat;

  // amqp
  amqp_connection_state_t conn = NULL;
  // structs
  struct amqp_connection_info ci;

  // conn params
  conn_string = mq_options->connection;
  exchange = mq_options->exchange;
  routing_key = mq_options->routing_key;
  connection_attempts = mq_options->attempts;
  retry_delay = mq_options->retry_delay;
  heartbeat = mq_options->heartbeat;

  D3("conn_string is %s", conn_string);
  D3("exchange is %s", exchange);
  D3("routing_key is %s", routing_key);
  D3("file_path is %s", file_path);
  D3("connection_attempts is %u", connection_attempts);
  D3("file_operation is %s", file_operation);
  D3("file_path is %s", file_path);
  D3("user_info is %s", user_info);

  // Parse url
  int res;
  char *message = NULL;
  char *url = strdup(conn_string);
  amqp_default_connection_info(&ci);
  res = amqp_parse_url(url, &ci);
  if (res) {
    D2("Expected to successfully parse URL, but didn't: %s (%s)", url,
       amqp_error_string2(-res));
    abort();
  }
  // Build message
  message = build_message(user_info, file_path, file_operation, new_file_path);

  int current_attempt = 0;
  int has_failed = 1;

  while (has_failed && current_attempt < connection_attempts) {
    has_failed = 0;
    D2("Current_attempt:%u/%u", current_attempt, connection_attempts);
    if (current_attempt > 0) { // wait
      D3("Sleeping...");
      sleep(retry_delay);
    }
    if (!conn) {
      conn = amqp_new_connection();
      int init_conn = init_connection(conn, &ci, heartbeat);
      if (init_conn < 0) {
        has_failed = 1;
        goto end;
      }
    }

    D3("Send message");
    int rabbit_status =
        send_rabbit_message(conn, exchange, routing_key, message);
    D3("rabbit_status is %d", rabbit_status);
    if (rabbit_status != 0) {
      D2("Rabbit send has failed");
      has_failed = 1;
    }

  end:
    if (has_failed) {
      D3("Close message");
      int close_result = close_connection(conn, &ci);
      if (close_result < 0) {
        D2("Close connection has failed");
      }
      conn = NULL;
    }

    current_attempt++;
  }
  if (message) {
    free(message);
  }
  free(url);
  D3("Done");
  return 0;
}

int mq_send_upload(const char *filepath) {
  D2("%s uploaded %s", username, filepath);
  D3("sending '%s' to %s", MQ_OP_UPLOAD, mq_options->connection);

  int result = send_message(MQ_OP_UPLOAD, username, filepath, "");

  D2("return code is %u", result);
  return result;
}

int mq_send_remove(const char *filepath) {
  D2("%s removed %s", username, filepath);
  D3("sending '%s' to %s", MQ_OP_REMOVE, mq_options->connection);

  int result = send_message(MQ_OP_REMOVE, username, filepath, "");

  D2("return code is %u", result);
  return result;
}

int mq_send_rename(const char *oldpath, const char *newpath) {
  D2("%s renamed %s into %s", username, newpath, oldpath);
  D3("sending '%s' to %s", MQ_OP_RENAME, mq_options->connection);

  int result = send_message(MQ_OP_RENAME, username, newpath, oldpath);

  D2("return code is %u", result);
  return result;
}
