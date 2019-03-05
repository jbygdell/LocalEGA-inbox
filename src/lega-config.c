#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <strings.h>
#include <stdio.h>

#include "lega-utils.h"
#include "lega-config.h"

/* Default config file, if not passed at command-line */
#define CFGFILE "/etc/ega/mq.conf"


options_t* lega_options = NULL;
char* syslog_name = "LEGA-MQ";

void
lega_config_clean(void)
{
  if(!lega_options) return;
  D2("Cleaning configuration [%p]", lega_options);

  if(lega_options->buffer){ free((char*)lega_options->buffer); }
  free(lega_options);
  return;
}


#ifdef DEBUG
static bool
valid_options(void)
{
  bool valid = true;
  if(!lega_options) { D3("No config struct"); return false; }

  D2("Checking the config struct");
  if(lega_options->heartbeat < 0    ) { D3("Invalid heartbeat");           valid = false; }
  if(lega_options->attempts < 0     ) { D3("Invalid connection attempts"); valid = false; }
  if(lega_options->retry_delay < 0  ) { D3("Invalid retry_delay");         valid = false; }

  if(!lega_options->connection      ) { D3("Missing connection");          valid = false; }
  if(!lega_options->exchange        ) { D3("Missing exchange");            valid = false; }
  if(!lega_options->routing_key     ) { D3("Missing routing_key");         valid = false; }

  if(!valid){ D3("Invalid config struct from %s", lega_options->cfgfile); }
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
  lega_options->attempts = 10;
  lega_options->retry_delay = 10;
  lega_options->heartbeat = 0;
  COPYVAL(cfgfile, lega_options->cfgfile);

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

    INJECT_OPTION(key, "connection"         , val, lega_options->connection        );
    INJECT_OPTION(key, "exchange"           , val, lega_options->exchange          );
    INJECT_OPTION(key, "routing_key"        , val, lega_options->routing_key       );

    if(!strcmp(key, "connection_attempts" )) { lega_options->attempts    = strtol(val, NULL, 8); } /* ok when val contains a comment #... */
    if(!strcmp(key, "retry_delay"         )) { lega_options->retry_delay = strtol(val, NULL, 8); }
    if(!strcmp(key, "heartbeat"           )) { lega_options->heartbeat   = strtol(val, NULL, 8); }
	
  }

  return 0;
}

bool
lega_config_load(char* cfgfile)
{
  D1("Loading configuration %s", cfgfile);
  if(lega_options){ D2("Already loaded [@ %p]", lega_options); return true; }

  _cleanup_file_ FILE* fp = NULL;
  size_t size = 1024;

  /* If no config file in passed */
  if(!cfgfile) cfgfile = CFGFILE;
  
  /* read or re-read */
  fp = fopen(cfgfile, "r");
  if (fp == NULL || errno == EACCES) { D2("Error accessing the config file: %s", strerror(errno)); return false; }

  lega_options = (options_t*)malloc(sizeof(options_t));
  if(!lega_options){ D3("Could not allocate options data structure"); return false; };
  lega_options->buffer = NULL;

REALLOC:
  D3("Allocating buffer of size %zd", size);
  if(lega_options->buffer)free(lega_options->buffer);
  lega_options->buffer = malloc(sizeof(char) * size);
  /* memset(lega_options->buffer, '\0', size); */
  *(lega_options->buffer) = '\0';
  if(!lega_options->buffer){ D3("Could not allocate buffer of size %zd", size); return false; };

  if( readconfig(fp, cfgfile, lega_options->buffer, size) < 0 ){
    size = size << 1; // double it
    goto REALLOC;
  }

  D2("Conf loaded [@ %p]", lega_options);

#ifdef DEBUG
  return valid_options();
#else
  return true;
#endif
}
