#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <strings.h>
#include <stdio.h>

#include "mq-utils.h"
#include "mq-config.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Default values */
#define MQ_ATTEMPTS  10
#define MQ_DELAY     10
#define MQ_HEARTBEAT 0

/* global variable for the MQ connection settings */
mq_options_t* mq_options = NULL;

void
clean_mq_config(void)
{
  if(!mq_options) return;
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

  if(!mq_options->connection      ) { D3("Missing connection");          valid = false; }
  if(!mq_options->exchange        ) { D3("Missing exchange");            valid = false; }
  if(!mq_options->routing_key     ) { D3("Missing routing_key");         valid = false; }

  if(!valid){ D3("Invalid config struct from %s", mq_options->cfgfile); }
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

    INJECT_OPTION(key, "connection"         , val, mq_options->connection        );
    INJECT_OPTION(key, "exchange"           , val, mq_options->exchange          );
    INJECT_OPTION(key, "routing_key"        , val, mq_options->routing_key       );

    if(!strcmp(key, "connection_attempts" )) { mq_options->attempts    = strtol(val, NULL, 10); } /* ok when val contains a comment #... */
    if(!strcmp(key, "retry_delay"         )) { mq_options->retry_delay = strtol(val, NULL, 10); }
    if(!strcmp(key, "heartbeat"           )) { mq_options->heartbeat   = strtol(val, NULL, 10); }

  }

  return 0;
}

bool
load_mq_config(char* cfgfile)
{
  printf("Loading configuration %s", cfgfile);
  if(mq_options){ D2("Already loaded [@ %p]", mq_options); return true; }

  _cleanup_file_ FILE* fp = NULL;
  size_t size = 1024;

  /* If no config file in passed */
  if(!cfgfile) cfgfile = MQ_CFGFILE;
  
  /* read or re-read */
  fp = fopen(cfgfile, "r");
  if (fp == NULL || errno == EACCES) { D2("Error accessing the config file: %s", strerror(errno)); return false; }

  mq_options = (mq_options_t*)malloc(sizeof(mq_options_t));
  if(!mq_options){ D3("Could not allocate options data structure"); return false; };
  mq_options->buffer = NULL;

REALLOC:
  D3("Allocating buffer of size %zd", size);
  if(mq_options->buffer)free(mq_options->buffer);
  mq_options->buffer = malloc(sizeof(char) * size);
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
