#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <errno.h>

#include "openbsd-compat/bsd-misc.h"
#include "log.h"

#include "mq-utils.h"
#include "mq-config.h"
#include "mq-msg.h"
#include "mq-notify.h"
#include "mq-listener.h"

/* Queue ID for message between this process and its spawn listener */
int msg_queue_id = -1;

char* username = NULL;

/* Whether we are a listener or still the internal-sftp */
int is_listener = 0;

extern void sftp_server_cleanup_exit(int i);

static void mq_listener_loop(void);

int
mq_listener_spawn(const char* user)
{
  int rc, child;
  struct sigaction newsa, oldsa;

  /* Save the user's name */
  username = strdup(user);

  pid_t pid = getpid();

  /* Prepare the message queue */
  D1("Get queue for a new listener [pid: %du]", pid);
  msg_queue_id = msgget(
			ftok(mq_options->ipc_key_prefix, (int)pid),
			0666 | IPC_CREAT
			);
  if( msg_queue_id < 0) {
    D1("Unable to get IPC queue: %s", strerror(errno));
    return 2;
  }
  D2("Queue ID: %d [parent: %du]", msg_queue_id, pid);

  /*
   * This code arranges that the demise of the child does not cause
   * the application to receive a signal it is not expecting - which
   * may kill the application or worse.
   *
   * From https://github.com/linux-pam/linux-pam/blob/master/modules/pam_mkhomedir/pam_mkhomedir.c
   */
  memset(&newsa, '\0', sizeof(newsa));
  newsa.sa_handler = SIG_DFL;
  sigaction(SIGCHLD, &newsa, &oldsa);
  
  /* fork */
  child = fork();
  if (child == 0) {

    D1("Flagging the listener for %s", username);
    is_listener = 1; /* We are a listener */

    D1("Forking for user %s", username);
    mq_listener_loop();
    D1("Listener terminated");

    /* Clean the queue. On success, IPC_RMID returns 0 */
    rc = msgctl(msg_queue_id, IPC_RMID, NULL); 
    /* And exit */
    if(username) free(username);
    username = NULL;
    sftp_server_cleanup_exit(rc);
     
  } else if (child > 0) {

     /* This is the parent, we just go on */
     D1("Listener created, parent moving on");
     /* restore old signal handler */
     sigaction(SIGCHLD, &oldsa, NULL);
     rc = 0;

  } else {
    D1("Listener not working");
    rc = 2;
    msgctl(msg_queue_id, IPC_RMID, NULL); /* Ignore return value */
  }

  return rc;
}


static void
mq_listener_loop(void)
{
     mq_msg_t msg;
     volatile unsigned int go_on = 1;
#if DEBUG > 2
     struct msqid_ds buf;
     int count = 0;
#endif

     while(go_on){

#if DEBUG > 2
       msgctl(msg_queue_id, IPC_STAT, &buf);
       D3("[%d] # messages in queue %d: %d", count, msg_queue_id, (int)buf.msg_qnum);
       count++;
#endif
       /* Pick any message. Msg is reused (no need to clean first) */
       msgrcv(msg_queue_id, &msg, sizeof(msg) - sizeof(long), 0, 0); /* Blocking call */

       D3("received message of type %ld", msg.type);

       switch(msg.type){
       case MSG_EXIT:
	 go_on = 0; /* Exit the loop */
	 break;
       case MSG_UPLOAD:
	 mq_send_upload(msg.path);
	 break;
       case MSG_REMOVE:
	 mq_send_remove(msg.path);
	 break;
       case MSG_RENAME:
	 mq_send_rename(msg.oldpath, msg.path);
	 break;
       default:
	 D1("Unknown message type: %ld", msg.type);
	 break;
       }
     }

     D2("Listener loop exited");
}


/*
 *
 * Sending messages to the IPC queue
 *
 */
static int _send_msg_to_queue(const mq_msg_t* msg);

int
ipc_send_upload(const char* filepath)
{ 
  D2("[IPC] %s uploaded %s", username, filepath);

  mq_msg_t msg;
  msg.type = MSG_UPLOAD;
  memset(msg.path, '\0', sizeof(msg.path));
  size_t path_len = strlen(filepath);
  strncpy(msg.path, filepath, MIN(sizeof(msg.path), path_len));
  /* memset(msg.oldpath, '\0', sizeof(msg.oldpath)); */

  return _send_msg_to_queue(&msg);
}

int
ipc_send_remove(const char* filepath)
{ 
  D2("[IPC] %s removed %s", username, filepath);

  mq_msg_t msg;
  msg.type = MSG_REMOVE;
  memset(msg.path, '\0', sizeof(msg.path));
  size_t path_len = strlen(filepath);
  strncpy(msg.path, filepath, MIN(sizeof(msg.path), path_len));
  /* memset(msg.path_old, '\0', sizeof(msg.path_old)); */

  return _send_msg_to_queue(&msg);
}

int
ipc_send_rename(const char* oldpath, const char* newpath)
{ 
  D2("[IPC] %s renamed %s into %s", username, oldpath, newpath);

  mq_msg_t msg;
  msg.type = MSG_RENAME;

  size_t path_len = strlen(newpath);
  memset(msg.path, '\0', sizeof(msg.path));
  strncpy(msg.path, newpath, MIN(sizeof(msg.path), path_len));

  path_len = strlen(oldpath);
  memset(msg.oldpath, '\0', sizeof(msg.oldpath));
  strncpy(msg.oldpath, oldpath, MIN(sizeof(msg.oldpath), path_len));

  return _send_msg_to_queue(&msg);
}

int
ipc_send_exit()
{
  D2("[IPC] Sending exit to queue");

  mq_msg_t msg;
  msg.type = MSG_EXIT;
  /* memset(msg.path, '\0', sizeof(msg.path)); */
  /* memset(msg.path_old, '\0', sizeof(msg.path_old)); */

  return _send_msg_to_queue(&msg);
}

static int
_send_msg_to_queue(const mq_msg_t* msg)
{ 
  D3("[IPC] Sending message of type %ld", msg->type);
  if(msgsnd(msg_queue_id, (const void *)msg, sizeof(*msg) - sizeof(long), 0) /* no flags */ ){
    D1("Oh oh, we could not send the message.");
    return -1;
  }
  D3("[IPC] Message sent");

  /* TODO: Improve the log output above */
  return 0;
}


