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
#define MQ_HEARTBEAT       0
#define MQ_VERIFY_PEER     0
#define MQ_VERIFY_HOSTNAME 0

/* global variable for the MQ connection settings */
mq_options_t* mq_options = NULL;

static int convert_host_to_ip(char** buffer, size_t* buflen);
static int dsn_parse(char** buffer, size_t* buflen);
static inline int copy2buffer(const char* data, char** dest, char **bufptr, size_t *buflen);
static inline void set_yes_no_option(char* key, char* val, char* name, int* loc);

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
  if(mq_options->port < 0         ) { D3("Invalid port");                valid = false; }

  if(!mq_options->dsn             ) { D3("Missing dsn connection");      valid = false; }

  if(!mq_options->host            ) { D3("Missing host");                valid = false; }
  if(!mq_options->vhost           ) { D3("Missing vhost");               valid = false; }
  if(!mq_options->username        ) { D3("Missing username");            valid = false; }
  if(!mq_options->password        ) { D3("Missing password");            valid = false; }

  if(!mq_options->exchange        ) { D3("Missing exchange");            valid = false; }
  if(!mq_options->routing_key     ) { D3("Missing routing_key");         valid = false; }

  if(mq_options->verify_peer &&
     !mq_options->cacert){ D3("Missing cacert, when using verify_peer"); valid = false; }

  if(!valid){ D3("Invalid configuration from %s", mq_options->cfgfile); }

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

#define INJECT_OPTION(key,ckey,val,loc) do { if(!strcmp(key, ckey) && copy2buffer(val, loc, &buffer, &buflen) < 0 ){ return -1; } } while(0)
#define COPYVAL(val,dest,b,blen) do { if( copy2buffer(val, dest, b, blen) < 0 ){ return -1; } } while(0)

static inline int
readconfig(FILE* fp, const char* cfgfile, char* buffer, size_t buflen)
{
  D3("Reading configuration file");
  _cleanup_str_ char* line = NULL;
  size_t len = 0;
  char *key,*eq,*val,*end;

  /* Default config values */
  mq_options->heartbeat = MQ_HEARTBEAT;
  mq_options->connection_opened = 0; /* not opened yet */
  mq_options->ssl = 0;
  mq_options->verify_peer = MQ_VERIFY_PEER;
  mq_options->verify_hostname = MQ_VERIFY_HOSTNAME;
  mq_options->dsn = NULL;
  mq_options->cacert = NULL;
  mq_options->host = NULL;
  mq_options->vhost = NULL;
  mq_options->username = NULL;
  mq_options->password = NULL;
  mq_options->ip = NULL;
  COPYVAL(cfgfile, &(mq_options->cfgfile), &buffer, &buflen);

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

    INJECT_OPTION(key, "exchange"      , val, &(mq_options->exchange)    );
    INJECT_OPTION(key, "routing_key"   , val, &(mq_options->routing_key) );
    INJECT_OPTION(key, "connection"    , val, &(mq_options->dsn)         );
    INJECT_OPTION(key, "cacert"        , val, &(mq_options->cacert)      );

    /* strtol ok even when val contains a comment #... */
    if(!strcmp(key, "heartbeat")) { mq_options->heartbeat   = strtol(val, NULL, 10); }

    /* Yes/No options */
    set_yes_no_option(key, val, "verify_peer", &(mq_options->verify_peer));
    set_yes_no_option(key, val, "verify_hostname", &(mq_options->verify_hostname));
  }

  D3("Initializing MQ connection/socket early");
  
  int rc = 0;
  if( (rc = dsn_parse(&buffer, &buflen)) != 0){
    D3("Error dsn parsing: %d", rc);
    return rc;
  }
  if( (rc = convert_host_to_ip(&buffer, &buflen)) != 0){
    D3("Error convert host to ip: %d", rc);
    return rc;
  }

  if( (rc = mq_init()) != 0){
    D3("Error mq_init: %d", rc);
    return rc;
  }

  return rc;
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
  mq_options->socket = NULL;

REALLOC:
  D3("Allocating buffer of size %zd", size);
  if(mq_options->buffer)free(mq_options->buffer);
  mq_options->buflen = sizeof(char) * size;
  mq_options->buffer = malloc(mq_options->buflen);
  memset(mq_options->buffer, '\0', size);
  /* *(mq_options->buffer) = '\0'; */
  if(!mq_options->buffer){ D3("Could not allocate buffer of size %zd", size); return false; };
  
  if( readconfig(fp, cfgfile, mq_options->buffer, size) < 0 ){

    /* Rewind first */
    if(fseek(fp, 0, SEEK_SET)){ D3("Could not rewind config file to start"); return false; }

    /* Double it */
    size = size << 1;
    goto REALLOC;
  }

  D3("Conf loaded [@ %p]", mq_options);

#ifdef DEBUG
  return valid_options();
#else
  return true;
#endif
}

/* Must be called after dsn_parse() */
static int
convert_host_to_ip(char** buffer, size_t* buflen)
{
  D3("Convert hostname to IP");
  struct hostent *he;
  struct in_addr **addr_list;
  int i;

  /* get the host info */
  if ( !(he = gethostbyname(mq_options->host)) ) { D1("gethostbyname error"); return 1; }

  addr_list = (struct in_addr **) he->h_addr_list;
  
  /* The first entry will be good */
  for(i = 0; addr_list[i] != NULL; i++) 
    {
      COPYVAL(inet_ntoa(*addr_list[i]), &(mq_options->ip), buffer, buflen);
      D2("%s converted to %s", mq_options->host, mq_options->ip);
      return 0;
    }

  D2("Error converting to ip: %s", mq_options->host);
  return 1;
}

/*
 * Moves a string value to a buffer (including a \0 at the end).
 * Adjusts the pointer to pointer right after the \0.
 *
 * Returns -size in case the buffer is <size> too small.
 * Otherwise, returns the <size> of the string.
 */
static inline int
copy2buffer(const char* data, char** dest, char **bufptr, size_t *buflen)
{
  size_t slen = strlen(data) + 1;

  if(*buflen < slen) {
    D3("buffer too small [currently: %zd bytes left] to copy \"%s\" [%zd bytes]", *buflen, data, slen);
    return -slen;
  }

  strncpy(*bufptr, data, slen-1);
  (*bufptr)[slen-1] = '\0';
  
  if(dest) *dest = *bufptr; /* record location */
  *bufptr += slen;
  *buflen -= slen;
  
  return slen;
}

static inline void
set_yes_no_option(char* key, char* val, char* name, int* loc)
{
  if(!strcmp(key, name)) {
    if(!strcasecmp(val, "yes") || !strcasecmp(val, "true") || !strcmp(val, "1") || !strcasecmp(val, "on")){
      *loc = 1;
    } else if(!strcasecmp(val, "no") || !strcasecmp(val, "false") || !strcmp(val, "0") || !strcasecmp(val, "off")){
      *loc = 0;
    } else {
      D2("Could not parse the %s option: Using %s instead.", name, ((*loc)?"yes":"no"));
    }
  }
}

static int
dsn_parse(char** buffer, size_t* buflen)
{
  D3("Parsing DSN");
  if(!mq_options->dsn) return 2;

  struct amqp_connection_info ci;
  char *url = strdup(mq_options->dsn);
  amqp_default_connection_info(&ci);
  int rc;
  if ( (rc = amqp_parse_url(url, &ci)) ) {
    D1("Unable to parse connection URL: %s [Error %s]", url, amqp_error_string2(rc));
    return 1;
  }

  COPYVAL(ci.host    , &(mq_options->host)    , buffer, buflen);
  COPYVAL(ci.vhost   , &(mq_options->vhost)   , buffer, buflen);
  COPYVAL(ci.user    , &(mq_options->username), buffer, buflen);
  COPYVAL(ci.password, &(mq_options->password), buffer, buflen);

  mq_options->port = ci.port;
  mq_options->ssl = ci.ssl;

  D1("Host: %s", mq_options->host);

  return 0;
}
