#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define GET_FIB_CALC_TIME 0
#define GET_CPY_TO_USER_TIME 1
#define TIME_CALC(t1, t2) t2.tv_nsec - t1.tv_nsec
#define FIB_DEV "/dev/fibonacci"
#define LL_MSB_MASK 0x8000000000000000
#define LL_LSB_MASK 1

struct BigN {
    unsigned long long upper, lower;
};

// Subtraction
static inline struct BigN subBigN(struct BigN x, struct BigN y) {
    struct BigN output = {0};
    output.upper = x.upper - y.upper;
    if (y.lower > x.lower) output.upper--;
    output.lower = x.lower - y.lower;
    return output;
}

// Left Shift
static inline struct BigN lshiftBigN(struct BigN x) {
    x.upper <<= 1;
    x.upper |=
        (x.lower & LL_MSB_MASK) >> ((sizeof(unsigned long long) << 3) - 1);
    x.lower <<= 1;
    return x;
}

// Right Shift
static inline struct BigN rshiftBigN(struct BigN x) {
    x.lower >>= 1;
    x.lower |= (x.upper & LL_LSB_MASK)
               << ((sizeof(unsigned long long) << 3) - 1);
    x.upper >>= 1;
    return x;
}

static inline bool leBigN(struct BigN x, struct BigN y) {
    if (x.lower < y.lower) {
        if (x.upper)
            x.upper--;
        else
            return false;
    }
    return x.upper >= y.upper;
}

char *bign_to_str(char buf[16]) {
    static char str[1024] = "";
    static char num_map[] = "0123456789";
    int idx = sizeof(str);
    struct BigN bign = {.upper = *(unsigned long long *)buf,
                        .lower = *(((unsigned long long *)buf) + 1)};
    if (!bign.upper && !bign.lower) str[--idx] = num_map[bign.lower];

    while (bign.upper || bign.lower) {
        struct BigN divisor = {.upper = 0xA000000000000000, .lower = 0};
        struct BigN setbit = {.upper = 0x1000000000000000, .lower = 0};
        struct BigN quotient = {0, 0};
        while ((bign.upper || (bign.lower >= 10))) {
            bool le = leBigN(bign, divisor);
            if (le) {
                bign = subBigN(bign, divisor);
                quotient.upper |= setbit.upper;
                quotient.lower |= setbit.lower;
            }
            divisor = rshiftBigN(divisor);
            setbit = rshiftBigN(setbit);
        }
        str[--idx] = num_map[bign.lower];
        bign = quotient;
    }


    return str + idx;
}

int main() {

    char buf[16];
    // char write_buf[] = "testing writing";
    int offset = 150; /* TODO: try test something bigger than the limit */

    struct timespec t1, t2;
    long long times[3][151];

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        long long fib_calc_time, cpy_to_user_time;
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_REALTIME, &t1);
        read(fd, buf, 16);
        clock_gettime(CLOCK_REALTIME, &t2);
        printf("Reading from " FIB_DEV " at offset %d, returned the sequence ",
               i);
        printf("%s.\n", bign_to_str(buf));
        fib_calc_time = write(fd, NULL, GET_FIB_CALC_TIME);
        cpy_to_user_time = write(fd, NULL, GET_CPY_TO_USER_TIME);
        times[0][i] = TIME_CALC(t1, t2);
        times[1][i] = fib_calc_time;
        times[2][i] = cpy_to_user_time;
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        read(fd, buf, 16);
        printf("Reading from " FIB_DEV " at offset %d, returned the sequence ",
               i);
        printf("%s.\n", bign_to_str(buf));
        // fib_calc_time = write(fd, NULL, GET_FIB_CALC_TIME);
        // cpy_to_user_time = write(fd, NULL, GET_CPY_TO_USER_TIME);
    }

    FILE *out = fopen("time.csv", "w");
    // fprintf(out, "user, kernel, kernel_to_user\n");
    for (int i = 0; i <= offset; i++) {
        fprintf(out, "%d\t%lld\t%lld\t%lld\n", i, times[0][i], times[1][i],
                times[2][i]);
    }
    fclose(out);

    close(fd);
    return 0;
}
