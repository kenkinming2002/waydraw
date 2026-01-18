#include "hibernate.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <fcntl.h>
#include <unistd.h>

#include <string.h>

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>


// This should really be in the standard but it is not.
static char *aprintf(const char *restrict format, ...)
{
  va_list ap;

  va_start(ap, format);
  int n = vsnprintf(NULL, 0, format, ap);
  assert(n >= 0);
  va_end(ap);

  size_t size = n + 1;
  char *buf = malloc(size);
  assert(buf);

  va_start(ap, format);
  vsnprintf(buf, size, format, ap);
  va_end(ap);

  return buf;
}

static char *control_file_path(void)
{
  char *wayland_display = getenv("WAYLAND_DISPLAY");
  if(!wayland_display)
  {
    fprintf(stderr, "error: hibernation: WAYLAND_DISPLAY not set - not running under a wayland compositor\n");
    exit(EXIT_FAILURE);
  }

  char *waydraw_display = getenv("WAYDRAW_DISPLAY");
  if(!waydraw_display)
    wayland_display = "waydraw";

  char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  if(!xdg_runtime_dir)
    xdg_runtime_dir = "/tmp";

  return aprintf("%s/%s-%s", xdg_runtime_dir, waydraw_display, wayland_display);
}

static char *path;
static int fd;

void try_resume(void)
{
  path = control_file_path();
  if(mkfifo(path, 0600) < 0 && errno != EEXIST)
  {
    fprintf(stderr, "error: hibernation: failed to create named pipe at %s:%s\n", path, strerror(errno));
    exit(EXIT_FAILURE);
  }

  fd = open(path, O_RDWR);
  if(fd < 0)
  {
    fprintf(stderr, "error: hibernation: failed to open control file at %s:%s\n", path, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if(flock(fd, LOCK_EX | LOCK_NB) < 0)
  {
    if(errno != EAGAIN && errno != EWOULDBLOCK)
    {
      fprintf(stderr, "error: hibernation: failed to acquire lock on %s:%s\n", path, strerror(errno));
      exit(EXIT_FAILURE);
    }

    char byte = 69;
    if(write(fd, &byte, sizeof byte) < 0)
    {
      fprintf(stderr, "error: hibernation: failed to write to control file at %s:%s\n", path, strerror(errno));
      exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
  }
}

void suspend(void)
{
  ssize_t n;
  char byte;

  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

  for(;;)
  {
    n = read(fd, &byte, sizeof byte);

    if(n == 0)
    {
      fprintf(stderr, "error: hibernation: bailed out due to control file at %s missing\n", path);
      exit(EXIT_FAILURE);
    }

    if(n < 0)
    {
      if(errno == EWOULDBLOCK || errno == EAGAIN)
        break;

      fprintf(stderr, "error: hibernation: failed to read from control file at %s: %s\n", path, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

  n = read(fd, &byte, sizeof byte);

  if(n == 0)
  {
    fprintf(stderr, "error: hibernation: bailed out due to control file at %s missing\n", path);
    exit(EXIT_FAILURE);
  }

  if(n < 0)
  {
    fprintf(stderr, "error: hibernation: failed to read from control file at %s: %s\n", path, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

