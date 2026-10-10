#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

// Static minimal heap if ever called
static char s_heap_buf[512];
static char *s_heap_ptr = s_heap_buf;

int _write(int file, char *ptr, int len) {
    (void)file; (void)ptr;
    return len;
}

int _read(int file, char *ptr, int len) {
    (void)file; (void)ptr; (void)len;
    errno = ENOSYS;
    return -1;
}

int _sbrk(int incr) {
    char *prev = s_heap_ptr;
    if ((s_heap_ptr + incr) > (s_heap_buf + sizeof(s_heap_buf))) {
        errno = ENOMEM;
        return -1;
    }
    s_heap_ptr += incr;
    return (int)prev;
}

int _close(int file) { (void)file; return -1; }
int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
int _fstat(int file, struct stat *st) { (void)file; st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { (void)file; return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }
int _getpid(void) { return 1; }
void _exit(int status) { (void)status; while (1); }
