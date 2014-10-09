
#include <errno.h>
#include <string.h>
#include <platform-abstraction/criticalsection.h>
#include <platform-abstraction/panic.h>
#include <libopencm3/stm32/usart.h>
#include <os.h>

typedef struct {
    void *(*open) (void *file, const char *path, int flags, int mode);
    int (*close)  (void *file);
    int (*write)  (void *file, const char *buf, int len);
    int (*read)   (void *file, char *buf, int len);
} file_ops_t;

extern const file_ops_t uart_ops; // defined below

#define FILE_DESC_TABLE_SIZE 10
struct {
    const file_ops_t *ops;
    void *file;
} fd_list[FILE_DESC_TABLE_SIZE] = {
    {.ops = &uart_ops, .file = (void *)USART1}, // stdin
    {.ops = &uart_ops, .file = (void *)USART1}, // stdout
    {.ops = &uart_ops, .file = (void *)USART1}  // stderr
};




static void *uart_op_open (void *file, const char *path, int flags, int mode)
{
    return file;
}

static int uart_op_close (void *file)
{
    return 0;
}

static int uart_op_write (void *file, const char *buf, int len)
{
    int32_t  i;
    for (i = 0; i < len; i++) {
        usart_send_blocking((uint32_t)file, buf[i]);
    }
    return len;
}

static int uart_op_read (void *file, char *buf, int len)
{
    return 0; // TODO!
}

const file_ops_t uart_ops = {
    uart_op_open,
    uart_op_close,
    uart_op_write,
    uart_op_read
};


// /dev/ devices
typedef struct {
    const char *devicename;
    const file_ops_t *ops;
    void *dev;
} dev_file_ops_t;

const dev_file_ops_t dev_fd_tab[] = {
    {.devicename = "uart1", .ops = &uart_ops, .dev = (void *)USART1},
    {.devicename = "uart2", .ops = &uart_ops, .dev = (void *)UART4},
    {.devicename = "uart3", .ops = &uart_ops, .dev = (void *)USART2},
    {.devicename = "uart4", .ops = &uart_ops, .dev = (void *)USART6}
};
#define NB_DEV_FD (sizeof(dev_fd_tab)/sizeof(dev_file_ops_t))




static int find_unused_filedescr(void)
{
    CRITICAL_SECTION_ALLOC();
    CRITICAL_SECTION_ENTER();
    int i;
    for (i = 3; i < FILE_DESC_TABLE_SIZE; i++) {
        if (fd_list[i].ops == NULL) {
            fd_list[i].ops = (file_ops_t *)1; // mark file descr. as used
            CRITICAL_SECTION_EXIT();
            return i;
        }
    }
    CRITICAL_SECTION_EXIT();
    return -1;
}

static void delete_filedescr(int fd)
{
    CRITICAL_SECTION_ALLOC();
    CRITICAL_SECTION_ENTER();
    fd_list[fd].ops = NULL;
    CRITICAL_SECTION_EXIT();
}

#define DEV_PATH "/dev/"
#define SD_PATH  "/sd/"
int _open(const char *path, int flags, int mode)
{
    int fd = find_unused_filedescr();
    if (fd < 0) {
        return -1; // not enough file descriptors
    }

    // open a device
    if (strncmp(path, DEV_PATH, strlen(DEV_PATH)) == 0) {
        int i = 0;
        while (strcmp(path + strlen(DEV_PATH), dev_fd_tab[i].devicename) != 0) {
            if (++i == NB_DEV_FD) {
                goto fail; // device doesn't exist
            }
        }
        void *f = dev_fd_tab[i].ops->open(dev_fd_tab[i].dev, path + strlen(DEV_PATH), flags, mode);
        if (f != NULL) {
            fd_list[fd].file = f;
            fd_list[fd].ops = dev_fd_tab[i].ops;
            return fd;
        } else {
            goto fail; // open failed
        }

    // open file on sd card
    } else if (strncmp(path, SD_PATH, strlen(SD_PATH)) == 0) {
        goto fail; // TODO
    }

fail:
    delete_filedescr(fd);
    return -1;
}

int _close(int file) {
    if (file == -1) {
        return -1;
    }
    int ret = fd_list[file].ops->close(fd_list[file].file);
    if (ret == 0) {
        delete_filedescr(file);
    }
    return ret;
}

int _read(int file, char *ptr, int len) {
    if (file == -1) {
        return -1;
    }
    return fd_list[file].ops->read(fd_list[file].file, ptr, len);
}

int _write(int file, char *ptr, int len) {
    if (file == -1) {
        return -1;
    }
    return fd_list[file].ops->write(fd_list[file].file, ptr, len);
}


/*
parts of this code are from http://stm32discovery.nano-age.co.uk/open-source-development-with-the-stm32-discovery/getting-newlib-to-work-with-stm32-and-code-sourcery-lite-eabi
*/
// /*
//  environ
//  A pointer to a list of environment variables and their values.
//  For a minimal environment, this empty list is adequate:
//  */
// char *__env[1] = { 0 };
// char **environ = __env;

// void _exit(int status) {
//     OS_ERR err;
//     /* NULL kills running task */
//     OSTaskDel((OS_TCB *)NULL, &err);

//     if (err != OS_ERR_NONE) {
//         PANIC("exit from non-thread context");
//     }

//     /* should not return */
//     while (1);
// }

// /*
//  execve
//  Transfer control to a new process. Minimal implementation (for a system without processes):
//  */
// int _execve(char *name, char **argv, char **env) {
//     errno = ENOMEM;
//     return -1;
// }
// /*
//  fork
//  Create a new process. Minimal implementation (for a system without processes):
//  */

// int _fork(void) {
//     errno = EAGAIN;
//     return -1;
// }


#include <sys/stat.h>
/*
 fstat
 Status of an open file. For consistency with other minimal implementations in these examples,
 all files are regarded as character special devices.
 The `sys/stat.h' header file required is distributed in the `include' subdirectory for this C library.
 */
int _fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

// /*
//  getpid
//  Process-ID; this is sometimes used to generate strings unlikely to conflict with other processes. Minimal implementation, for a system without processes:
//  */

int _getpid(void) {
    return 1;
}

/*
 isatty
 Query whether output stream is a terminal. For consistency with the other minimal implementations,
 */
int _isatty(int file) {
    switch (file) {
        case 0:
        case 1:
        case 2:
            return 1;
        default:
            errno = ENOTTY;
            return 0;
    }
}

/*
 kill
 Send a signal. Minimal implementation:
 */
int _kill(int pid, int sig) {
    errno = EINVAL;
    return (-1);
}

// /*
//  link
//  Establish a new name for an existing file. Minimal implementation:
//  */
// int _link(char *old, char *_new) {
//     errno = EMLINK;
//     return -1;
// }

/*
 lseek
 Set position in a file. Minimal implementation:
 */
int _lseek(int file, int ptr, int dir) {
    return 0;
}

/*
 sbrk
 Increase program data space.
 Malloc and related functions depend on this
 */
void *_sbrk(int incr) {
    extern char _sheap;
    extern char _eheap;
    static char *heap_end = 0;
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = &_sheap;
    }
    prev_heap_end = heap_end;

    if (heap_end + incr > &_eheap)
    {
        errno = ENOMEM;
        return (void *) -1;
    }

    heap_end += incr;
    return (void *) prev_heap_end;
}

// /*
//  stat
//  Status of a file (by name). Minimal implementation:
//  int    _EXFUN(stat,( const char *__path, struct stat *__sbuf ));
//  */
// int _stat(const char *filepath, struct stat *st) {
//     st->st_mode = S_IFCHR;
//     return 0;
// }

// /*
//  times
//  Timing information for current process. Minimal implementation:
//  */
// clock_t _times(struct tms *buf) {
//     return -1;
// }

// /*
//  unlink
//  Remove a file's directory entry. Minimal implementation:
//  */
// int _unlink(char *name) {
//     errno = ENOENT;
//     return -1;
// }

// /*
//  wait
//  Wait for a child process. Minimal implementation:
//  */
// int _wait(int *status) {
//     errno = ECHILD;
//     return -1;
// }
