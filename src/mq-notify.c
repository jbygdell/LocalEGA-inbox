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
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#include <json-c/json.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Winsock2.h>
#else
#include <sys/time.h>
#endif

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
#define MQ_OP_UPLOAD "up"
#define MQ_OP_REMOVE "rm"
#define MQ_OP_RENAME "mv"

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

int build_and_send_message(amqp_connection_state_t conn, char const *exchange,
                           char const *routing_key, char const *user_info,
                           char const *file_path, char const *file_operation,
                           char const *new_file_path) {
  logit("build and send message");
  struct stat st;

  // TODO check if file exists if op != remove, if rename check to see which is
  // the old one
  off_t file_size = 0;
  char file_hash[65] = "";

  if (strcmp(file_operation, "remove") != 0 &&
      strcmp(file_operation, "rename") != 0) {
    logit("obtainig data");
    // obtain filesize
    stat(file_path, &st);
    file_size = st.st_size;
    logit("size is %u", (int)file_size);
    // Obtain file_hash
    sha256_file(file_path, file_hash);
    logit("hash is %s", file_hash);
  }

  // Create json message
  json_object *jsonobj = json_object_new_object();
  json_object *juser = json_object_new_string(user_info);
  json_object_object_add(jsonobj, "user", juser);
  json_object *jfile_operation = json_object_new_string(file_operation);
  json_object_object_add(jsonobj, "file_operation", jfile_operation);
  json_object *jfile_path = json_object_new_string(file_path);
  json_object_object_add(jsonobj, "file_path", jfile_path);
  json_object *jnew_file_path = json_object_new_string(new_file_path);
  json_object_object_add(jsonobj, "new_file_path", jnew_file_path);
  json_object *jfile_size = json_object_new_int64(file_size);
  json_object_object_add(jsonobj, "filesize", jfile_size);

  json_object *jchecksum = json_object_new_object();
  json_object *jc_value = json_object_new_string("sha256");
  json_object_object_add(jchecksum, "value", jc_value);
  json_object *jc_hash = json_object_new_string(file_hash);
  json_object_object_add(jchecksum, "hash", jc_hash);

  json_object_object_add(jsonobj, "encrypted_checksum", jchecksum);

  printf("exchange:%s, routing_key:%s\n", exchange, routing_key);
  logit("exchange:%s, routing_key:%s\n", exchange, routing_key);

  // create amqp message
  amqp_basic_properties_t props;
  props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
  props.content_type = amqp_cstring_bytes("application/json");
  props.delivery_mode = 2; // persistent delivery mode
  int ret= amqp_basic_publish(
      conn, 1, amqp_cstring_bytes(exchange), amqp_cstring_bytes(routing_key), 0,
      0, &props, amqp_cstring_bytes(json_object_to_json_string(jsonobj)));
  json_object_put(jsonobj);//free json object>
  return ret;
}

int init_connection(amqp_connection_state_t conn,
                    struct amqp_connection_info *ci, int heartbeat, int argc,
                    char const *const *argv) {
  // amqp
  amqp_socket_t *socket = NULL;
  // struct
  struct timeval tval;
  struct timeval *tv;

  // timeout
  tv = &tval;
  tv->tv_sec = 30;
  tv->tv_usec = 0;

  if (ci->ssl) { // check if ssl
    socket = amqp_ssl_socket_new(conn);
    if (!socket) {
      fprintf(stderr, "Error: creating SSL/TLS socket");
      logit("Error: creating SSL/TLS socket");
      return -1;
    }
    amqp_ssl_socket_set_verify_peer(socket, 0);
    amqp_ssl_socket_set_verify_hostname(socket, 0);
    if (argc > 6) {
      int nextarg = 6;
      int ret = amqp_ssl_socket_set_cacert(socket, argv[5]);
      if (ret < 0) {
        fprintf(stderr, "Error: could not set cacert");
        logit("Error: creating SSL/TLS socket");
        return -1;
      }
      if (argc > nextarg && !strcmp("verifypeer", argv[nextarg])) {
        amqp_ssl_socket_set_verify_peer(socket, 1);
        nextarg++;
      }
      if (argc > nextarg && !strcmp("verifyhostname", argv[nextarg])) {
        amqp_ssl_socket_set_verify_hostname(socket, 1);
        nextarg++;
      }
      if (argc > nextarg + 1) {
        ret = amqp_ssl_socket_set_key(socket, argv[nextarg + 1], argv[nextarg]);
        if (ret < 0) {
          fprintf(stderr, "Error: could not set key");
          logit("Error: creating SSL/TLS socket");
          return -1;
        }
      }
    }
  } else { // non ssl
    socket = amqp_tcp_socket_new(conn);
    if (!socket) {
      fprintf(stderr, "Error: creating TCP socket (out of memory)");
      logit("Error: creating TCP socket (out of memory)");
      return -1;
    }
  }

  logit("Host is %s, port is %u", ci->host, ci->port);
  int ret = amqp_socket_open_noblock(socket, ci->host, ci->port, tv);
  if (ret < 0) {
    fprintf(stderr, "Error: opening connection");
    logit("Error: opening connection: %s", amqp_error_string2(ret));
    return -1;
  }

  amqp_rpc_reply_t amqp_ret =
      amqp_login(conn, ci->vhost, 0, 131072, heartbeat, AMQP_SASL_METHOD_PLAIN,
                 ci->user, ci->password);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    fprintf(stderr, "Error: Logging in");
    logit("Error: Logging in");
    return -1;
  }

  amqp_channel_open(conn, 1);
  amqp_ret = amqp_get_rpc_reply(conn);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    fprintf(stderr, "Error: Opening channel");
    logit("Error: Opening channel");
    return -1;
  }
  return 0;
}

int close_connection(amqp_connection_state_t conn,
                     struct amqp_connection_info *ci) {

  amqp_rpc_reply_t amqp_ret = amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    fprintf(stderr, "Error: Closing channel");
    return -1;
  }

  amqp_ret = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    fprintf(stderr, "Error: Closing connection");
    return -1;
  }
  int ret = amqp_destroy_connection(conn);
  if (ret < 0) {
    fprintf(stderr, "Error: Ending connection");
    return -1;
  }
  if (ci->ssl) { // check if ssl
    ret = amqp_uninitialize_ssl_library();
    if (ret < 0) {
      fprintf(stderr, "Error: Uninitializing SSL library");
      return -1;
    }
  }
  return 0;
}

int send_message(int argc, char const *const *argv) {
  // params
  char const *conn_string;
  char const *exchange;
  char const *routing_key;
  int connection_attempts;
  int retry_delay;
  int heartbeat;
  char const *file_operation;
  char const *user_info;
  char const *file_path;
  char const *new_file_path;
  // amqp
  amqp_connection_state_t conn = NULL;
  // structs
  struct amqp_connection_info ci;

  if (argc < 5) {
    fprintf(stderr, "Error: Incorrect number of parameters");
    return -1;
  }

  // conn params
  conn_string = mq_options->connection;
  exchange = mq_options->exchange;
  routing_key = mq_options->routing_key;
  connection_attempts = mq_options->attempts;
  retry_delay = mq_options->retry_delay;
  heartbeat = mq_options->heartbeat;

  //info params
  file_operation = argv[1];
  user_info = argv[2];
  file_path = argv[3];
  new_file_path = argv[4];

  logit("conn_string is %s", conn_string);
  logit("exchange is %s", exchange);
  logit("routing_key is %s", routing_key);
  logit("file_path is %s", file_path);
  logit("connection_attempts is %u", connection_attempts);


  // Parse url
  int res;

  char *url = strdup(conn_string);
  amqp_default_connection_info(&ci);
  res = amqp_parse_url(url, &ci);
  if (res) {
    fprintf(stderr, "Expected to successfully parse URL, but didn't: %s (%s)\n",
            url, amqp_error_string2(-res));
    logit("Expected to successfully parse URL, but didn't: %s (%s)\n",
            url, amqp_error_string2(-res));
    abort();
  }

  int current_attempt = 0;
  int has_failed = 1;
  while (has_failed && current_attempt < connection_attempts) {
    has_failed = 0;
    fprintf(stderr, "Current_attempt:%u/%u\n", current_attempt,
            connection_attempts);
    logit("Current_attempt:%u/%u\n", current_attempt, connection_attempts);
    if (current_attempt > 0) { // wait
      fprintf(stderr, "Sleeping\n");
      sleep(retry_delay);
    }
    conn = amqp_new_connection();
    logit("init conn");
    int init_result = init_connection(conn, &ci, heartbeat, argc, argv);
    if (init_result < 0) {
      has_failed = 1;
      logit("init connection has failed");
      goto end;
    }

    logit("send message");
    int rabbit_status =
        build_and_send_message(conn, exchange, routing_key, user_info,
                               file_path, file_operation, new_file_path);
    if (rabbit_status < 0) {
      logit("rabbit send has failed");
      has_failed = 1;
      goto end;
    }

    int close_result;
  end:
    logit("close message");
    close_result = close_connection(conn, &ci);
    if (close_result < 0) {
      has_failed = 1;
      logit("close connection has failed");
    }
    current_attempt++;
  }

  free(url);
  logit("---Done");
  printf("Done\n");
  return 0;
}

int lega_send_message(int argc, char const *const *argv) {
  int retval, child;
  struct sigaction newsa, oldsa;

  /*
   * This code arranges that the demise of the child does not cause
   * the application to receive a signal it is not expecting - which
   * may kill the application or worse.
   */
  memset(&newsa, '\0', sizeof(newsa));
  newsa.sa_handler = SIG_DFL;
  sigaction(SIGCHLD, &newsa, &oldsa);

  /* fork */
  child = fork();
  if (child == 0) {
    retval = send_message(argc, argv);

    exit(retval);
  } else if (child > 0) {
    printf("the parent continues");
    // We do not wait for the child so we hope that it goes ok
  } else {
    printf("fork failed");
    retval = -1;
  }

  sigaction(SIGCHLD, &oldsa, NULL); /* restore old signal handler */
  return retval;
}

int
mq_send_upload(const char* filepath)
{ 
  D2("%s uploaded %s", username, filepath);
  D3("sending '%s' to %s", MQ_OP_UPLOAD, mq_options->connection);

  const char *args[] = { NULL, NULL, NULL, NULL, NULL };
  args[0] = "send_message";
  args[1] = "upload";
  args[2] = user;
  args[3] = filepath;
  args[4] = "";

  int result = send_message(5,(char *const *) args);

  logit("return code is %u", result);

  return 0;
}

int
mq_send_remove(const char* filepath)
{ 
  D2("%s removed %s", username, filepath);
  D3("sending '%s' to %s", MQ_OP_REMOVE, mq_options->connection);

  const char *args[] = { NULL, NULL, NULL, NULL, NULL };
  args[0] = "send_message";
  args[1] = "remove";
  args[2] = username;
  args[3] = filepath;
  args[4] = "";

  int result = send_message(5,(char *const *) args);

  logit("return code is %u", result);

  return 0;
}

int
mq_send_rename(const char* oldpath, const char* newpath)
{ 

  D2("%s renamed %s into %s", username, oldpath, newpath);
  D3("sending '%s' to %s", MQ_OP_RENAME, mq_options->connection);

  const char *args[] = { NULL, NULL, NULL, NULL, NULL };
  args[0] = "send_message";
  args[1] = "rename";
  args[2] = user;
  args[3] = oldpath;
  args[4] = newpath;

  int result = send_message(5,(char *const *) args);

  logit("return code is %u", result);

  return 0;
}
