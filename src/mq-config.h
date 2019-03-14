#ifndef __MQ_CONFIG_H_INCLUDED__
#define __MQ_CONFIG_H_INCLUDED__

#include <stdbool.h>
#include <sys/types.h> 

#include "amqp.h"

/* Default config file, if not passed at command-line */
#define MQ_CFGFILE "/etc/ega/mq.conf"

struct mq_options_s {
  char* cfgfile;
  char* buffer;
  int buflen;
  
  char* dsn;                      /* the connection definition as one string */
  amqp_connection_state_t conn;   /* the connection pointer */
  amqp_socket_t *socket;          /* socket prepared outside chroot */
  int connection_opened;          /* connection open called */

  int   ssl;
  int   verify_hostname;
  int   verify_peer;              /* For the SSL context */
  char* cacert;                   /* For TLS verification */

  char* host;                     /* Updated from the above DSN */
  char* ip;                       /* Converted before chroot */
  int   port;
  char* vhost;
  char* username;
  char* password;

  char* exchange;      /* Name of the MQ exchange */
  char* routing_key;   /* Routing key to send to */

  int heartbeat;       /* in seconds */
};

typedef struct mq_options_s mq_options_t;

extern mq_options_t* mq_options;

bool load_mq_config(char* cfgfile);
void clean_mq_config(void);

#endif /* !__MQ_CONFIG_H_INCLUDED__ */



