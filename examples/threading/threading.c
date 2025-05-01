#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param) {
  // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
  // hint: use a cast like the one below to obtain thread arguments from your parameter
  // struct thread_data* thread_func_args = (struct thread_data *) thread_param;
  thread_data_t* ptr_td = (thread_data_t*)thread_param;
  ptr_td->call_count++;
  if (sleep(ptr_td->wait_to_obtain_ms/1000) != 0) {
    ERROR_LOG("Sleep interrupted before acquiring lock");
    ptr_td->thread_complete_success = false;
    return thread_param;
  }
  
  int status = pthread_mutex_lock(ptr_td->mutex);
  if (status != 0) {
    ERROR_LOG("Failed to obtain mutex; error code %d", status);
    ptr_td->thread_complete_success = false;
    return thread_param;
  }

  if (sleep(ptr_td->wait_to_release_ms/1000) != 0) {
    ERROR_LOG("Sleep interrupted before releasing lock");
    ptr_td->thread_complete_success = false;
    return thread_param;
  }

  status = pthread_mutex_unlock(ptr_td->mutex);
  if (status != 0) {
    ERROR_LOG("Failed to release mutex; error code %d", status);
    ptr_td->thread_complete_success = false;
    return thread_param;
  }
  
  ptr_td->thread_complete_success = true;
  return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t* thread, pthread_mutex_t* mutex,
				  int wait_to_obtain_ms, int wait_to_release_ms) {
  /**
   * TODO: allocate memory for thread_data, setup mutex and wait arguments,
   * pass thread_data to created thread
   * using threadfunc() as entry point.
   *
   * return true if successful.
   *
   * See implementation details in threading.h file comment block
   */
  thread_data_t* ptr_td = malloc(sizeof(*ptr_td));
  ptr_td->mutex = mutex;
  ptr_td->wait_to_obtain_ms = wait_to_obtain_ms;
  ptr_td->wait_to_release_ms = wait_to_release_ms;
  ptr_td->call_count = 0;
  ptr_td->thread_complete_success = false;

  int status = pthread_create(thread, NULL, threadfunc, ptr_td);
  if (status != 0) {
    ERROR_LOG("Failed to create thread; error code %d", status);
    return false;
  }
  return true;
}

