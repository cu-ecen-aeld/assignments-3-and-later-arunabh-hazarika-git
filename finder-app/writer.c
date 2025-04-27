#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>



int main(int argc, char** argv) {
  openlog(NULL, LOG_CONS, LOG_USER);
  if (argc != 3) {
    syslog(LOG_ERR,
	   "Insufficient arguments; Usage: writer <outfile> <text-to-write>");
    return EXIT_FAILURE;
  }

  const char* filename = argv[1];
  const char* txt = argv[2];

  FILE* f = fopen(filename, "wb");
  if (f == NULL) {
    const char* err = strerror(errno);
    syslog(LOG_ERR, "Unable to open file %s: %s", filename, err);
    return EXIT_FAILURE;
  }
  syslog(LOG_DEBUG, "Writing %s to %s", txt, filename);
  fprintf(f, "%s", txt);
  if (fclose(f) != 0) {
    const char* err = strerror(errno);
    syslog(LOG_ERR, "Error closing file %s", err);
  }
  closelog();
  return EXIT_SUCCESS;
}
