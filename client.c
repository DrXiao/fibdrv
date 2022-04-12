#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main() {
    long long sz;

    char buf[16];
    unsigned long long *ptr = buf;
    char write_buf[] = "testing writing";
    int offset = 150; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        read(fd, buf, 16);
        sz = write(fd, write_buf, strlen(write_buf));
        // printf("Reading from " FIB_DEV
        //       " at offset %d, returned the sequence "
        //       "%lld.\n",
        //       i, sz);
        printf("Reading from " FIB_DEV " at offset %d, returned the sequence ",
               i);
        printf("%llx%016llx.\n", *ptr, *(ptr + 1));
        printf("time: %lld\n", sz);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        read(fd, buf, 16);
        sz = write(fd, NULL, 0);
        // printf("Reading from " FIB_DEV
        //       " at offset %d, returned the sequence "
        //       "%lld.\n",
        //       i, sz);
        printf("Reading from " FIB_DEV " at offset %d, returned the sequence ",
               i);
        printf("%llx%016llx.\n", *ptr, *(ptr + 1));
        printf("time: %lld\n", sz);
    }

    close(fd);
    return 0;
}
