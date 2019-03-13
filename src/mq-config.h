#ifndef __MQ_CONFIG_H_INCLUDED__
#define __MQ_CONFIG_H_INCLUDED__

#include <stdbool.h>
#include <sys/types.h> 

#include "mq-notify.h" 

/* Default config file, if not passed at command-line */
#define MQ_CFGFILE "/etc/ega/mq.conf"

struct mq_options_s {
  char* cfgfile;
  char* buffer;
  int buflen;
  
  mq_conn conn;        /* the connection pointer */

  bool  ssl;
  char* host;
  int   port;
  char* vhost;
  char* username;
  char* password;

  char* exchange;      /* Name of the MQ exchange */
  char* routing_key;   /* Routing key to send to */

  int attempts;        /* number of connection attempts. (int is enough) */
  int retry_delay;     /* in seconds */
  int heartbeat;       /* in seconds */
};

typedef struct mq_options_s mq_options_t;

extern mq_options_t* mq_options;

bool load_mq_config(char* cfgfile);
void clean_mq_config(void);

#endif /* !__MQ_CONFIG_H_INCLUDED__ */



