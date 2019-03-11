#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <errno.h>

#include "mq-utils.h"
#include "mq-config.h"
#include "mq-msg.h"
#include "mq-notify.h"
#include "mq-listener.h"

/* Queue ID for message between this process and its spawn listener */
int msg_queue_id = -1;
char* username = NULL;

extern void sftp_server_cleanup_exit(int i);

static int mq_listener_loop(void);

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

    D1("Forking for user %s", username);
    rc = mq_listener_loop();
    D1("Listener terminated");

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


static int
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
       D3("[%d] # messages in queue %d: %d\n", count, msg_queue_id, (int)buf.msg_qnum);
       count++;
#endif
       /* Pick any message. Msg is reused (no need to clean first) */
       msgrcv(msg_queue_id, &msg, sizeof(msg), 0, 0); /* Blocking call */

       switch(msg.type){
       case MSG_EXIT:
	 go_on = 0; /* Exit the loop */
	 if(username) free(username);
	 username = NULL;
	 break;
       case MSG_UPLOAD:
	 mq_send_upload(msg.filepath);
	 break;
       case MSG_REMOVE:
	 mq_send_remove(msg.filepath);
	 break;
       case MSG_RENAME:
	 mq_send_rename(msg.filepath_old, msg.filepath);
	 break;
       default:
	 D1("Unknown message type: %ld", msg.type);
	 break;
       }
     }

     D2("Listener loop exited");

     /* Clean the queue.
	On success, IPC_RMID returns 0
     */
     return msgctl(msg_queue_id, IPC_RMID, NULL); 
}


/*
 *
 * Sending messages to the IPC queue
 *
 */
static int _send_msg_to_queue(const mq_msg_t* msg);

int
mq_send_upload_to_queue(const char* filepath)
{ 
  D2("%s uploaded %s", username, filepath);

  mq_msg_t msg;
  msg.type = MSG_UPLOAD;
  msg.filepath = (char*)filepath;
  msg.filepath_len = strlen(filepath);
  msg.filepath_old = NULL;
  msg.filepath_len = 0;

  return _send_msg_to_queue(&msg);
}

int
mq_send_remove_to_queue(const char* filepath)
{ 
  D2("%s removed %s", username, filepath);

  mq_msg_t msg;
  msg.type = MSG_REMOVE;
  msg.filepath = (char*)filepath;
  msg.filepath_len = strlen(filepath);
  msg.filepath_old = NULL;
  msg.filepath_len = 0;

  return _send_msg_to_queue(&msg);
}


int
mq_send_rename_to_queue(const char* oldpath, const char* newpath)
{ 
  D2("%s renamed %s into %s", username, oldpath, newpath);

  mq_msg_t msg;
  msg.type = MSG_RENAME;
  msg.filepath = (char*)newpath;
  msg.filepath_len = strlen(newpath);
  msg.filepath_old = (char*)oldpath;
  msg.filepath_len = strlen(oldpath);

  return _send_msg_to_queue(&msg);
}

int
mq_send_exit_to_queue()
{
  D2("Sending exit to queue");

  mq_msg_t msg;
  msg.type = MSG_EXIT;
  msg.filepath = NULL;
  msg.filepath_len = 0;
  msg.filepath_old = NULL;
  msg.filepath_len = 0;

  return _send_msg_to_queue(&msg);
}

static int
_send_msg_to_queue(const mq_msg_t* msg)
{ 
  D3("Sending message of type %ld", msg->type);
  if(!msgsnd(msg_queue_id, (const void *)msg, sizeof(*msg), 0) /* no flags */ ){
    D1("Oh oh, we could not send the message.");
    return -1;
  }
  D3("Message sent");

  /* TODO: Improve the log output above */
  return 0;
}


