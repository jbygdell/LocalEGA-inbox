
#ifndef __LEGA_CONFIG_H_INCLUDED__
#define __LEGA_CONFIG_H_INCLUDED__

#include <stdbool.h>
#include <sys/types.h> 

struct options_s {
  char* cfgfile;
  char* buffer;
  
  char* connection;    /* DSN URL */
  char* exchange;      /* Name of the MQ exchange */
  char* routing_key;   /* Routing key to send to */

  int attempts;        /* number of connection attempts. (int is enough) */
  int retry_delay;     /* in seconds */
  int heartbeat;       /* in seconds */
};

typedef struct options_s options_t;

extern options_t* lega_options;

bool lega_loadconfig(char* cfgfile);
void lega_cleanconfig(void);

#endif /* !__LEGA_CONFIG_H_INCLUDED__ */



