/* No POSIX IO: application logs use drv_uart TX DMA. */
#include <errno.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdint.h>
int _close(int fd) { (void)fd; errno=EBADF; return -1; }
int _fstat(int fd,struct stat *s) { (void)fd; (void)s; errno=EBADF; return -1; }
int _isatty(int fd) { (void)fd; return 0; }
int _lseek(int fd,int off,int whence) { (void)fd;(void)off;(void)whence;errno=EBADF;return -1; }
int _read(int fd,char *p,int n) { (void)fd;(void)p;(void)n;errno=EBADF;return -1; }
int _write(int fd,const char *p,int n) { (void)fd;(void)p;(void)n;errno=EBADF;return -1; }
int _getpid(void) { return 1; }
int _kill(int pid,int sig) { (void)pid;(void)sig;errno=EINVAL;return -1; }
void *_sbrk(ptrdiff_t increment) {
    extern char _end, _heap_end;
    static uintptr_t current;
    if(!current) current=(uintptr_t)&_end;
    uintptr_t lo=(uintptr_t)&_end, hi=(uintptr_t)&_heap_end;
    if(increment<0 || (uintptr_t)increment>hi-current || current<lo) {
        errno=ENOMEM; return (void *)-1;
    }
    void *old=(void *)current;
    current+=(uintptr_t)increment;
    return old;
}
void _exit(int code) { (void)code; for(;;) {} }
