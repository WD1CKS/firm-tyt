#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>
#undef errno
extern int errno;

/* This file is all about the stubs, suppress unused parameter warnings */
#pragma GCC diagnostic ignored "-Wunused-parameter"

void
_exit(int status)
{
	for(;;);
}

int _fstat(int file, struct stat *st) {
  st->st_mode = S_IFCHR;
  return 0;
}

int _open(const char *name, int flags, int mode) {
  return -1;
}

int _write(int file, char *ptr, int len) {
  errno = EINVAL;
  return -1;
}

caddr_t _sbrk(int incr) {
  extern char _end;		/* Defined by the linker */
  static char *heap_end;
  char *prev_heap_end;
 
  if (heap_end == 0) {
    heap_end = &_end;
  }
  prev_heap_end = heap_end;
#ifdef notyet	// For now just let the stack and heap intermix freely... what could go wrong?
  if (heap_end + incr > stack_ptr) {
    write (1, "Heap and stack collision\n", 25);
    for (;;);
  }
#endif

  heap_end += incr;
  return (caddr_t) prev_heap_end;
}

int _kill(int pid, int sig) {
  errno = EINVAL;
  return -1;
}

int _getpid(void) {
  return 1;
}

int _times(struct tms *buf) {
  return -1;
}

int _unlink(char *name) {
  errno = ENOENT;
  return -1; 
}

int _close(int file) {
  return -1;
}

int _isatty(int file) {
  return 1;
}

int _link(char *old, char *new) {
  errno = EMLINK;
  return -1;
}

int _lseek(int file, int ptr, int dir) {
  return 0;
}

int _read(int file, char *ptr, int len) {
  return 0;
}

int _gettimeofday(struct timeval *tp, struct timezone *tzp) {
  errno = EPERM;
  return -1;
}
