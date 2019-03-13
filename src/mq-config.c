#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <netdb.h>  //hostent
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "mq-utils.h"
#include "mq-config.h"
#include "mq-notify.h"

/* Default values */
#define MQ_ATTEMPTS   10
#define MQ_DELAY      10
#define MQ_HEARTBEAT  0
#define MQ_ENABLE_SSL false

/* global variable for the MQ connection settings */
mq_options_t* mq_options = NULL;

static int convert_host_to_ip(char* buffer, size_t buflen);

void
clean_mq_config(void)
{
  if(!mq_options) return;

  mq_clean(); /* Cleaning MQ connection */

  D2("Cleaning configuration [%p]", mq_options);
  if(mq_options->buffer){ free((char*)mq_options->buffer); }
  free(mq_options);
  return;
}


#ifdef DEBUG
static bool
valid_options(void)
{
  bool valid = true;
  if(!mq_options) { D3("No config struct"); return false; }

  D2("Checking the config struct");
  if(mq_options->heartbeat < 0    ) { D3("Invalid heartbeat");           valid = false; }
  if(mq_options->attempts < 0     ) { D3("Invalid connection attempts"); valid = false; }
  if(mq_options->retry_delay < 0  ) { D3("Invalid retry_delay");         valid = false; }
  if(mq_options->port < 0         ) { D3("Invalid port");                valid = false; }

  if(!mq_options->host            ) { D3("Missing host");                valid = false; }
  if(!mq_options->vhost           ) { D3("Missing vhost");               valid = false; }
  if(!mq_options->username        ) { D3("Missing username");            valid = false; }
  if(!mq_options->password        ) { D3("Missing password");            valid = false; }

  if(!mq_options->exchange        ) { D3("Missing exchange");            valid = false; }
  if(!mq_options->routing_key     ) { D3("Missing routing_key");         valid = false; }

  if(!valid){ D3("Invalid config struct from %s", mq_options->cfgfile); }

  int i;
  D3("BUFFER ------");
  for (i = 0; i < mq_options->buflen; i++){
    char c = mq_options->buffer[i];
    if (c == '\0')
      fprintf(stderr, ":");
    else
      fprintf(stderr, "%c", c);
  }
  fprintf(stderr, "\n");
  D3("------");


  return valid;
}
#endif

#define INJECT_OPTION(key,ckey,val,loc) do { if(!strcmp(key, ckey) && copy2buffer(val, &(loc), &buffer, &buflen) < 0 ){ return -1; } } while(0)
#define COPYVAL(val,dest) do { if( copy2buffer(val, &(dest), &buffer, &buflen) < 0 ){ return -1; } } while(0)

static inline int
readconfig(FILE* fp, const char* cfgfile, char* buffer, size_t buflen)
{
  D3("Reading configuration file");
  _cleanup_str_ char* line = NULL;
  size_t len = 0;
  char *key,*eq,*val,*end;

  /* Default config values */
  mq_options->attempts = MQ_ATTEMPTS;
  mq_options->retry_delay = MQ_DELAY;
  mq_options->heartbeat = MQ_HEARTBEAT;
  mq_options->ssl = MQ_ENABLE_SSL;
  COPYVAL(cfgfile, mq_options->cfgfile);

  /* Parse line by line */
  while (getline(&line, &len, fp) > 0) {
	
    key=line;
    /* remove leading whitespace */
    while(isspace(*key)) key++;
      
    if((eq = strchr(line, '='))) {
      end = eq - 1; /* left of = */
      val = eq + 1; /* right of = */
	  
      /* find the end of the left operand */
      while(end > key && isspace(*end)) end--;
      *(end+1) = '\0';
	  
      /* find where the right operand starts */
      while(*val && isspace(*val)) val++;
	  
      /* find the end of the right operand */
      eq = val;
      while(*eq != '\0') eq++;
      eq--;
      if(*eq == '\n') { *eq = '\0'; } /* remove new line */
	  
    } else val = NULL; /* could not find the '=' sign */

    INJECT_OPTION(key, "exchange"      , val, mq_options->exchange    );
    INJECT_OPTION(key, "routing_key"   , val, mq_options->routing_key );
    INJECT_OPTION(key, "host"          , val, mq_options->host        );
    INJECT_OPTION(key, "vhost"         , val, mq_options->vhost       );
    INJECT_OPTION(key, "username"      , val, mq_options->username    );
    INJECT_OPTION(key, "password"      , val, mq_options->password    );

    /* strtool ok even when val contains a comment #... */
    if(!strcmp(key, "connection_attempts" )) { mq_options->attempts    = strtol(val, NULL, 10); }
    if(!strcmp(key, "retry_delay"         )) { mq_options->retry_delay = strtol(val, NULL, 10); }
    if(!strcmp(key, "heartbeat"           )) { mq_options->heartbeat   = strtol(val, NULL, 10); }
    if(!strcmp(key, "port"                )) { mq_options->port        = strtol(val, NULL, 10); }

    if(!strcmp(key, "enable_ssl")) {
      if(!strcasecmp(val, "yes") || !strcasecmp(val, "true") || !strcmp(val, "1") || !strcasecmp(val, "on")){
	mq_options->ssl = true;
      } else if(!strcasecmp(val, "no") || !strcasecmp(val, "false") || !strcmp(val, "0") || !strcasecmp(val, "off")){
	mq_options->ssl = false;
      } else {
	D2("Could not parse the enable_ssl option: Using %s instead.", ((mq_options->ssl)?"yes":"no"));
      }
    }	

  }

  D3("Convert MQ host to ip");
  return convert_host_to_ip(buffer, buflen);
}

bool
load_mq_config(char* cfgfile)
{
  D1("Loading configuration %s", cfgfile);
  if(mq_options){ D2("Already loaded [@ %p]", mq_options); return true; }

  _cleanup_file_ FILE* fp = NULL;
  size_t size = 100;

  /* If no config file in passed */
  if(!cfgfile) cfgfile = MQ_CFGFILE;
  
  /* read or re-read */
  fp = fopen(cfgfile, "r");
  if (fp == NULL || errno == EACCES) { D2("Error accessing the config file: %s", strerror(errno)); return false; }

  mq_options = (mq_options_t*)malloc(sizeof(mq_options_t));
  if(!mq_options){ D3("Could not allocate options data structure"); return false; };
  mq_options->buffer = NULL;
  mq_options->conn = NULL;

REALLOC:
  D3("Allocating buffer of size %zd", size);
  if(mq_options->buffer)free(mq_options->buffer);
  mq_options->buflen = sizeof(char) * size;
  mq_options->buffer = malloc(mq_options->buflen);
  /* memset(mq_options->buffer, '\0', size); */
  *(mq_options->buffer) = '\0';
  if(!mq_options->buffer){ D3("Could not allocate buffer of size %zd", size); return false; };

  if( readconfig(fp, cfgfile, mq_options->buffer, size) < 0 ){
    size = size << 1; // double it
    goto REALLOC;
  }

  D2("Conf loaded [@ %p]", mq_options);

#ifdef DEBUG
  return valid_options();
#else
  return true;
#endif
}

static int
convert_host_to_ip(char* buffer, size_t buflen)
{
  struct hostent *he;
  struct in_addr **addr_list;
  int i;
  
  char* hostname = mq_options->host;
  D3("Hostname so far: %s", hostname);

  if ( !(he = gethostbyname(hostname)) )
    {
      // get the host info
      D1("gethostbyname error");
      return 1;
    }
  
  addr_list = (struct in_addr **) he->h_addr_list;
  
  for(i = 0; addr_list[i] != NULL; i++) 
    {
      //Return the first one;
      COPYVAL(inet_ntoa(*addr_list[i]), mq_options->host);
      D2("%s converted to %s", hostname, mq_options->host);
      return 0;
    }

  D2("Error converting to ip: %s", mq_options->host);
  return 1;
}
