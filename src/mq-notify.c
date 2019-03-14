#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

/* For JSON */
#include <json-c/json.h>

/* For uuid in the MQ message */
#include <uuid/uuid.h>
#define UUID_STR_LEN	37

/* For RabbitMQ */
#include "amqp.h"
#include "amqp_ssl_socket.h"
#include "amqp_tcp_socket.h"

#include "mq-utils.h"
#include "mq-config.h"
#include "mq-notify.h"

static char* build_message(int operation, const char* username, const char* filepath, const char* oldpath);
static int send_message(const char* message);

/* ================================================
 *
 *              Broker connection
 *
 * ================================================ */

int
mq_init(void)
{
  D2("Initializing connection to message broker");

  amqp_socket_t *socket = NULL;
  amqp_rpc_reply_t amqp_ret;

  mq_options->conn = amqp_new_connection();

  socket = amqp_tcp_socket_new(mq_options->conn);
  if (!socket) { D3("Error creating TCP socket"); return 1; }

  if (amqp_socket_open(socket, mq_options->host, mq_options->port)) {
    D1("Error opening TCP socket");
    return 2;
  }

  amqp_ret =
    amqp_login(mq_options->conn,
	       mq_options->vhost,
	       0, /* no limit for channel number */
	       AMQP_DEFAULT_FRAME_SIZE,
	       mq_options->heartbeat,
	       AMQP_SASL_METHOD_PLAIN,
	       mq_options->username,
	       mq_options->password);

  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    D2("Error: Logging in");
    return 3;
  }

  amqp_channel_open(mq_options->conn, 1);
  amqp_ret = amqp_get_rpc_reply(mq_options->conn);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    D2("Error opening channel");
    return 4;
  }

  D3("Connection is at [%p]", mq_options->conn);
  return 0;
}

int
mq_clean(void)
{
  if(!mq_options->conn) return 0; /* Not initialized */

  D2("Cleaning connection to message broker");
  amqp_rpc_reply_t amqp_ret;
  int rc;

  amqp_ret = amqp_channel_close(mq_options->conn, 1, AMQP_REPLY_SUCCESS);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    D2("Error: Closing channel");
    return 1;
  }

  amqp_ret = amqp_connection_close(mq_options->conn, AMQP_REPLY_SUCCESS);
  if (amqp_ret.reply_type != AMQP_RESPONSE_NORMAL) {
    D2("Error: Closing connection");
    return 2;
  }

  /* check if ssl */
  if (mq_options->ssl && (rc = amqp_uninitialize_ssl_library()) < 0) {
    D2("Error: Uninitializing SSL library");
    return 3;
  }

  if ((rc = amqp_destroy_connection(mq_options->conn)) < 0) {
    D2("Error: Ending connection");
    return 4;
  }

  mq_options->conn = NULL;
  return 0;
}

/* ================================================
 *
 *                For the messages
 *
 * ================================================ */
#define MQ_OP_UPLOAD 1
#define MQ_OP_REMOVE 2
#define MQ_OP_RENAME 3

int
mq_send_upload(const char* username, const char* filepath)
{ 
  D2("%s uploaded %s", username, filepath);

  _cleanup_str_ char* msg = build_message(MQ_OP_UPLOAD, username, filepath, NULL);
  
  D3("sending '%s' to %s", msg, mq_options->host);

  return send_message(msg);
}

int
mq_send_remove(const char* username, const char* filepath)
{ 
  D2("%s removed %s", username, filepath);

  _cleanup_str_ char* msg = build_message(MQ_OP_REMOVE, username, filepath, NULL);

  D3("sending '%s' to %s", msg, mq_options->host);

  return send_message(msg);
}

int
mq_send_rename(const char* username, const char* oldpath, const char* newpath)
{ 
  D2("%s renamed %s into %s", username, oldpath, newpath);

  _cleanup_str_ char* msg = build_message(MQ_OP_RENAME, username, newpath, oldpath);

  D3("sending '%s' to %s", msg, mq_options->host);

  return send_message(msg);
}

static char*
build_message(int operation,
	      const char* username,
	      const char* filepath,
	      const char* oldpath)
{
  char* res = NULL;
  json_object *obj = json_object_new_object();

  /* Common things */
  json_object_object_add(obj,
			 "user",
			 json_object_new_string(username));
  json_object_object_add(obj,
			 "filepath",
			 json_object_new_string(filepath));

  /* Convert operation */
  switch(operation){
  case MQ_OP_UPLOAD:
    json_object_object_add(obj,
			   "operation",
			   json_object_new_string("upload"));
    break;
  case MQ_OP_REMOVE:
    json_object_object_add(obj,
			   "operation",
			   json_object_new_string("remove"));
    break;
  case MQ_OP_RENAME:
    json_object_object_add(obj,
			   "operation",
			   json_object_new_string("rename"));
    /* Add the oldpath for rename */
    json_object_object_add(obj,
			   "oldpath",
			   json_object_new_string(oldpath)); /* Not NULL */
    break;
  default:
    D1("Unknown operation: %d", operation);
    goto final;
  }

  res = strdup(json_object_to_json_string_ext(obj, JSON_C_TO_STRING_NOSLASHESCAPE));

final:
  json_object_put(obj); // free json object, and the other ones inside
  return res;
}


static int
do_send_message(const char* message)
{
  amqp_basic_properties_t props;
  props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG | AMQP_BASIC_CORRELATION_ID_FLAG;
  props.content_type = amqp_cstring_bytes("application/json");
  props.delivery_mode = 2; /* persistent delivery mode */

  /* Generate Correlation id */
  char correlation_id[UUID_STR_LEN];
  uuid_t uu;
  uuid_generate(uu);
  uuid_unparse(uu, correlation_id);
  D3("Correlation ID: %s", correlation_id);
  props.correlation_id = amqp_cstring_bytes(correlation_id);

  return amqp_basic_publish(mq_options->conn,
			    1, /* channel */
			    amqp_cstring_bytes(mq_options->exchange),
			    amqp_cstring_bytes(mq_options->routing_key),
			    0, 0, &props,
			    amqp_cstring_bytes(message)); /* body */
}

static int
send_message(const char* message){

  int attempt = 0;
  int rc = 1;

  while(attempt++ < mq_options->attempts){
    D3("[%2d/%d] Sending message", attempt, mq_options->attempts);

    if(!mq_options->conn && (rc = mq_init()) != 0) /* Error initializing */
      goto final;

    if(do_send_message(message) == AMQP_STATUS_OK){
      D2("Message sent to amqp%s://%s:%d/%s", ((mq_options->ssl)?"s":""),
	                                      mq_options->host,
	                                      mq_options->port,
	                                      mq_options->vhost);
      rc = 0;
      goto final;
    }

    /* Depending on exit status, clean the connection and retry immediately
     * otherwise sleep for mq_options->retry_delay seconds
     */
    /* if( do_clean_and_retry ){ */
    /*   if( (rc = mq_clean()) != 0 ) /\* Error cleaning *\/ */
    /* 	goto final; */
    /* } else { */
    /*   sleep(mq_options->retry_delay); */
    /* } */
  }
  D2("Unable to send Message after %d attempts", mq_options->attempts);

final:
  return rc;
}
