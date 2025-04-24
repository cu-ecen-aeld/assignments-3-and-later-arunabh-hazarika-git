#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "systemcalls.h"



/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd) {
/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
  int status = system(cmd);
  return (status != -1);
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/


// Redirect stdout to outputfile; outputfile must not be NULL
bool redirect_stdout(const char* outputfile) {
  if (outputfile == NULL) return false;

  int fd = open(outputfile, O_CREAT|O_WRONLY|O_TRUNC, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
  if (fd == -1) {
    perror("Output file open error");
    return false;
  }
  if (dup2(fd, STDOUT_FILENO) == -1) {
    perror("Output file stdout bind error");
    return false;
  }
  if (close(fd) != 0) {
    perror("Output file close error");
    return false;    
  }
  return true;
}

// Abstract out the fork, execv, wait steps
bool run_child(char* cmd, char** arguments, int count, const char* outputfile) {
  fflush(stdout); // Flush stdout before forking
  pid_t fpid = fork();
  if (fpid == -1) {
    perror("Fork error");
    return false;
  }
  if (fpid != 0) {  // Parent process
    //printf("Parent is %d, child is %d\n", getpid(), fpid);
    int wstatus = 0;
    pid_t wpid = wait(&wstatus);
    if (wpid == -1) {
      perror("Wait error");
      return false;
    } else if (!WIFEXITED(wstatus)) {
      printf("Child exited abnormally with status %d\n", WEXITSTATUS(wstatus)); 
      return false;
    } else if (WEXITSTATUS(wstatus) != 0) {
      printf("Child completed with status %d\n", WEXITSTATUS(wstatus)); 
      return false;
    } else {
      return true;
    }
  } else { // Child process
    if (outputfile != NULL) {
      bool r = redirect_stdout(outputfile);
      if (r == false) {
	fprintf(stderr, "stdout redirection to %s failed\n", outputfile); 
	exit(1);
      }
    }
    int retval = execv(cmd, arguments);
    if (retval == -1) { // Will not reach if execv is successfull
      perror("Exec error");
      exit(127); // Exit, not return
    }
    // Always unreachable
    fprintf(stderr, "\nShould not reach\n");
    return true;
  }
}

bool do_exec(int count, ...)
{
  va_list args;
  va_start(args, count);
  char * command[count+1];
  int i;
  for(i=0; i<count; i++)
  {
    command[i] = va_arg(args, char *);
  }
  command[count] = NULL;
  // this line is to avoid a compile warning before your implementation is complete
  // and may be removed
  //command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
  bool r = run_child(command[0], command, count, NULL);
  va_end(args);
  return r;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
  va_list args;
  va_start(args, count);
  char * command[count+1];
  int i;
  for(i=0; i<count; i++) {
    command[i] = va_arg(args, char *);
  }
  command[count] = NULL;
  // this line is to avoid a compile warning before your implementation is complete
  // and may be removed
  // command[count] = command[count];
/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
  bool r = run_child(command[0], command, count, outputfile);
  va_end(args);

  return r;
}
