/* Server-side code to listen at port 9000, accept and read
 * messages from a client - each message terminated by '\n' -
 * and appends the output to a file specified by the macro
 * MESSAGES_FILE. The client is expected to terminate its message
 * with a '\n' and then wait for a response. Returns the entire
 * contents of MESSAGES_FILE in response to the client. Runs as
 * a daemon if the command line argument '-d' is specified. In that
 * mode, no output is made to stdout or stderr. All messages are
 * syslog'ed to /var/log/syslog, tagged by the program executable
 * name.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PORT 9000
#define ACCEPT_BACK_LOG 10
#define MESSAGES_FILE "/var/tmp/aesdsocketdata"
#define BUFF_SZ 64


typedef struct out_file {
  int fd;
  pthread_mutex_t* lock;
} out_file_t;


typedef struct worker_params {
  int sockfd;
  out_file_t* file;
} worker_params_t;

typedef struct workers {
  pthread_t* thread;
  struct workers* next;
} workers_t;

bool is_daemon = false;
bool terminate = false;


// This function prints out the msg to stderr (only if not
// running in daemon mode) and to syslog. If errnum is not
// 0 (errnum is expected to be equal to errno), prints out
// the system error together with the msg
void handle_error(char* msg, int errnum) {
  if (errnum != 0) {
    char* syserr = strerror(errnum);
    if (is_daemon == false) perror(msg);
    syslog(LOG_ERR, "%s: %s\n", msg, syserr);
  } else {
    if (is_daemon == false) fprintf(stderr, "%s\n", msg);
    syslog(LOG_ERR, "%s\n", msg);
  }
}


// Closes a socket, given the fd. Prints error
// to stderr and logs to syslog, if error occurs
void sock_close(int sfd) {
  if (shutdown(sfd, SHUT_RDWR) !=0) {
    handle_error("Socket shutdown failed", errno);
  }
  if (close(sfd) != 0) {
    handle_error("Socket close failed", errno);
  }
}

// Closes the file, given the fd. Prints error
// to stderr and logs to syslog, if error occurs
void file_close(int fd) {
  if (close(fd) != 0) {
    handle_error("File close failed", errno);
  }
}

// Terminates the program with EXIT_FAILURE status.
// Handles the error message and errnum error code,
// which is expected to be errno, simply. Finally
// closes the socket represented by sfd, if sfd is
// not -1, before terminating the program
void exit_on_failure(char* msg, int errnum, int sfd, int ffd) {
  handle_error(msg, errnum);
  if (sfd != -1) sock_close(sfd);
  if (ffd != -1) file_close(ffd);
  exit(EXIT_FAILURE);
}


// Signal handler for that terminates the server
void shutdown_server(int signum) {
  terminate = true;
}


// Sets the peer ip address into the char array passed in.
// Assumes that the array is of the appropriate size len.
// Returns true on success, false on failure
bool get_client_ip(int sockfd, char* client_ip, size_t len) {
  struct sockaddr_in sa;
  socklen_t sz = sizeof(sa);
  if (getpeername(sockfd, (struct sockaddr*)&sa, &sz) != 0) {
    handle_error("Failed to get peer info", errno);
    return false;
  }
  inet_ntop(AF_INET, &(sa.sin_addr), client_ip, len);
  return true;
}


// Reads data from sockfd until a newline is seen, appends the
// data to file->fd. The client is expected to send a message
// terminated by '\n'. Returns true on success, false on failure
bool receive(int sockfd, out_file_t* file) {
  char* ptr_eom = NULL;
  char* message = NULL;
  ssize_t msg_offset = 0;
  while (ptr_eom == NULL) { // Loop till '\n' indicates end of data
    // The read buffer; ensure null-terminated to make string
    // manipulation easier, so BUFF_SZ+1 and init to '\0'
    char rbuffer[BUFF_SZ+1] = {'\0'};
    ssize_t r_count = recv(sockfd, rbuffer, BUFF_SZ, 0); 
    if (r_count == -1) {
      handle_error("Failed to read data from client", errno);
      free(message);
      return false;
    } else if (r_count == 0) {
      handle_error("No data received from client", 0);
      free(message);
      return false;
    }
    // Check for \n; If count != position('\n') + 1,
    // extra data sent. Error out
    ptr_eom = strchr(rbuffer, '\n');
    if (ptr_eom != NULL && ptr_eom != rbuffer+r_count-1) {
      handle_error("Unexpected data after new-line", 0);
      free(message);
      return false;
    }
    // Append contents to message, realloc'ing along the way
    message = realloc(message, (msg_offset+r_count+1)*sizeof(*message));
    strcpy(message+msg_offset, rbuffer); // rbuffer is null-terminated
    msg_offset += r_count; // Next write should over-write \0
  }

  // Write msg_offset bytes, discarding the trailing '\0'
  // The lock is acquired here to maximize concurrency (ie
  // socket reads are concurrent); Caller is responsible for
  // releasing the lock
  int lock_status = pthread_mutex_lock(file->lock);
  if (lock_status == 0) {
    ssize_t cc = msg_offset; // Total character count
    // Assumes file->fd opened with O_SYNC flag;
    // msg_offset+1 at this point is the length of message
    ssize_t w_count = write(file->fd, message, cc);
    if (w_count != cc) {
      handle_error("Error writing to outfile", errno);
    }
    free(message);
    return (w_count != cc ? false : true);
  } else {
    handle_error("Failed to acquire outfile lock", 0);
    free(message);
    return false;
  }
}


// Reads all the messages and sends back to client.
// Assumes outfile lock already acquired. Releases
// the lock after reading file to socket send is
// concurrent
void dispatch(int sockfd, out_file_t* file) {
  char rbuffer[BUFF_SZ];
  char* message = NULL;
  ssize_t msg_sz = 0;
  // Reset file to beginning
  if (lseek(file->fd, 0, SEEK_SET) == -1) {
    handle_error("Could not reset out file position", errno);
    return;
  }
  for (ssize_t rc = read(file->fd, rbuffer, BUFF_SZ); rc != 0;
               rc = read(file->fd, rbuffer, BUFF_SZ)) {
    if (rc == -1) {
      handle_error("Failed to read data from file", errno);
      free(message);
      msg_sz = -1;
      break;
    }
    message = realloc(message, (msg_sz+rc)*sizeof(*message));
    strncpy(message+msg_sz, rbuffer, rc);
    msg_sz += rc;
  }
  int unlock_status = pthread_mutex_unlock(file->lock);
  if (unlock_status != 0) { // Considered non-fatal
    handle_error("Failed to release outfile lock", 0);
  }
  if (msg_sz != -1) {
    ssize_t w_count = send(sockfd, message, msg_sz, 0); 
    if (w_count != msg_sz) {
      if (w_count == -1) handle_error("Failed to send data", errno);
      else handle_error("Failed to send complete data", 0);
    }
  }
  free(message);
}



// Handles communication with a client - reading the data
// sent from the client and sending the response. The
// response is sent only if the data from the client is
// read successfully
void* converse(void* args) {
  worker_params_t* params = args;
  char client_ip[INET_ADDRSTRLEN+1];
  client_ip[INET_ADDRSTRLEN] = '\0'; // Ensure NULL-terminated
  if (get_client_ip(params->sockfd, client_ip, INET_ADDRSTRLEN)) {
    // Log connection accept
    if (is_daemon == false) {
      fprintf(stdout, "Accepted connection from %s\n", client_ip);
    }
    syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);
  }

  if (receive(params->sockfd, params->file) == true) {
    dispatch(params->sockfd, params->file); // This releases the file lock
  } else { // Release the file lock
    printf("Receive returned false\n");
    int unlock_status = pthread_mutex_unlock(params->file->lock);
    if (unlock_status != 0) { // Considered non-fatal?
      handle_error("Failed to release outfile lock", 0);
    }
  }
  
  sock_close(params->sockfd);
  if (is_daemon == false) {
    fprintf(stdout, "Closed connection from %s\n", client_ip);
  }
  syslog(LOG_INFO, "Closed connection from %s\n", client_ip);
  free(params); 
  return NULL;
} 

// Writes out the timestamp every q0 seconds, into the outfile
void* write_timestamp(void* args) {
  out_file_t* params = args;
  while (terminate == false) {
    sleep(10);
    char timestamp[200];
    time_t t = time(NULL);
    struct tm* lt = localtime(&t);
    if (lt == NULL) {
      handle_error("Failed to generate local time", errno);
      continue;
    }
    // RFC 2822 timestamp format
    size_t cc = strftime(timestamp, sizeof(timestamp), "%a, %d %b %Y %T %z", lt);
    if (cc == 0) {
      handle_error("Failed to format timestamp", -1);
      continue;
    }
    timestamp[cc] = '\n'; // Append new-line, replacing '\0'
    if (pthread_mutex_lock(params->lock) == 0) {
      // Assumes file->fd opened with O_SYNC flag
      if (write(params->fd, "timestamp:", 10) == -1 || 
          write(params->fd, timestamp, cc+1) == -1) {
        handle_error("Error writing timestamp to outfile", errno);
      }
      if (pthread_mutex_unlock(params->lock) != 0) {
        handle_error("Failed to release outfile lock", 0);
      }
    } else {
      handle_error("Failed to acquire outfile lock", 0);
    }
  }
}



void init_server(char* appname) {
  openlog(appname, LOG_CONS, LOG_USER);

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    exit_on_failure("Server socket create failed", errno, -1, -1);
  }

  // Set socket options (at socket level, hence SOL_SOCKET, not AF_INET)
  int enable=1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
		 &enable, sizeof(enable)) != 0) {
    exit_on_failure("Socket option setting failed", errno, sockfd, -1);
  }

  struct sockaddr_in sa = {
      .sin_family=AF_INET, .sin_port=htons(PORT), .sin_addr=INADDR_ANY 
  };
  if (bind(sockfd, (struct sockaddr*)&sa, sizeof(sa)) != 0) {
    exit_on_failure("Server socket bind failed", errno, sockfd, -1);
  }

  // Listen
  if (is_daemon == false) {
    fprintf(stdout, "Listening at localhost:%d\n", PORT);
  }
  syslog(LOG_INFO, "Listening at localhost:%d\n", PORT);
  if (listen(sockfd, ACCEPT_BACK_LOG) != 0) {
    exit_on_failure("Server socket listen failed", errno, sockfd, -1);
  }

  // Set up signal handler
  struct sigaction siga = { .sa_handler=shutdown_server };
  if (sigaction(SIGINT, &siga, NULL) != 0 ||
      sigaction(SIGTERM, &siga, NULL) != 0) {
    exit_on_failure("Failed to set up signal handler", errno, sockfd, -1);
  }

  // Open outfile
  int ofd = open(MESSAGES_FILE, O_CREAT|O_RDWR|O_SYNC, S_IRWXU|S_IRGRP|S_IROTH);
  if (ofd == -1) {
    exit_on_failure("Could not open outfile", errno, sockfd, -1);
  }

  // Create mutex and file struct
  pthread_mutex_t lock;
  if (pthread_mutex_init(&lock, NULL) != 0) {
    exit_on_failure("Could not create mutex", errno, sockfd, ofd);
  }
  out_file_t file = { .fd=ofd, .lock=&lock };

  // Start time-stamper thread
  pthread_t time_stamper;
  if (pthread_create(&time_stamper, NULL, write_timestamp, &file) != 0) {
    handle_error("Failed to create time-stamper thread", errno);
  }
 
  // Workers list
  workers_t* head = NULL;
  workers_t* tail = NULL;
  
  // Wait for connection
  while (terminate == false) {
    struct sockaddr ca;
    socklen_t sz = sizeof(ca);
    int psockfd = accept(sockfd, &ca, &sz);
    if (psockfd == -1) {
      if (terminate == false) {
	handle_error("Failed to accept client connection", errno);
      }
    } else {
      // Function converse() must free params
      worker_params_t* params = malloc(sizeof(*params));
      *params = (worker_params_t){ .sockfd=psockfd, .file=&file };
      workers_t* w = malloc(sizeof(*w));
      w->thread = malloc(sizeof(*w->thread)); // -> has higher precedence than * (indirection)
      w->next = NULL;
      if (head == NULL) head = w;
      if (tail != NULL) tail->next = w;
      tail = w;
      if (pthread_create(w->thread, NULL, converse, params) != 0) {
	handle_error("Failed to create worker thread", errno);
	free(params);
      }
    }
  }
  if (is_daemon == false) fprintf(stdout, "Caught signal, exiting\n");
  syslog(LOG_INFO, "Caught signal, exiting\n");
  // Join all threads and free workers node
  for (workers_t* w=head; w != NULL;) {
    if (pthread_join(*(w->thread), NULL) != 0) {
      handle_error("Failed to join worker thread", errno);
    }
    free(w->thread);
    workers_t* curr = w;
    w = w->next;
    free(curr);
  }
  // Join the time_stamper thread
  if (pthread_join(time_stamper, NULL) != 0) {
    handle_error("Failed to join time-stamper thread", errno);
  }
  // Close server socket
  sock_close(sockfd);
  // Close file
  file_close(ofd);
  // Delete file
  if (remove(MESSAGES_FILE) != 0) {
    handle_error("Could not delete out file", errno);
  }
  closelog();
}


int main(int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "-d") == 0) is_daemon = true;      

  if (is_daemon == true) {
    pid_t cpid = fork();
    if (cpid == -1) {
      handle_error("Failed to create child process", errno);
      return EXIT_FAILURE;
    }
    if (cpid == 0) {
      init_server(argv[0]);
    } else {
      printf("Created server process with pid %d\n", cpid);
    }
  } else {
    init_server(argv[0]);
  }
  return EXIT_SUCCESS;
}
