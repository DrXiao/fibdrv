#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#define LL_MSB_MASK 0x8000000000000000
#define LL_LSB_MASK 1

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

struct BigN {
    unsigned long long upper, lower;
};

// Addition
static inline struct BigN addBigN(struct BigN x, struct BigN y) {
    struct BigN output = {0};
    output.upper = x.upper + y.upper;
    if (y.lower > ~x.lower) output.upper++;
    output.lower = x.lower + y.lower;
    return output;
}

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

// Multiplication
static struct BigN productBigN(struct BigN x, struct BigN y) {
    struct BigN output = {0};
    while (x.upper || x.lower) {
        if (x.lower & 1) output = addBigN(output, y);
        x = rshiftBigN(x);
        y = lshiftBigN(y);
    }
    return output;
}

static struct BigN fib_sequence_BigN(long long k) {
    struct BigN a = {.upper = 0, .lower = 0};
    struct BigN b = {.upper = 0, .lower = 1};
    if (k == 0) goto RETVAL;
    for (int i = ((sizeof(long long) << 3) - __builtin_clzll(k)); i >= 1; i--) {
        struct BigN t1 = productBigN(a, subBigN(lshiftBigN(b), a));
        struct BigN t2 = addBigN(productBigN(b, b), productBigN(a, a));
        a = t1;
        b = t2;
        if (k & (1 << (i - 1))) {
            t1 = addBigN(a, b);
            a = b;
            b = t1;
        }
    }
RETVAL:
    return a;
}

/*
static long long fib_sequence(long long k) {
    long long a = 0, b = 1;
    for (int i = ((sizeof(long long) << 3) - __builtin_clzll(k)); i >= 1; i--) {
        long long t1 = a * (2 * b - a);
        long long t2 = b * b + a * a;
        a = t1;
        b = t2;
        if (k & (1 << (i - 1))) {
            t1 = a + b;
            a = b;
            b = t1;
        }
    }

    return a;
}
*/

static ktime_t fib_kt;

static struct BigN fib_time_proxy_BigN(long long k) {
    fib_kt = ktime_get();
    struct BigN result = fib_sequence_BigN(k);
    fib_kt = ktime_sub(ktime_get(), fib_kt);
    return result;
}

static int fib_open(struct inode *inode, struct file *file) {
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file) {
    mutex_unlock(&fib_mutex);
    return 0;
}

static ktime_t cpy_to_user_kt;
/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file, char *buf, size_t size,
                        loff_t *offset) {
    struct BigN output = fib_time_proxy_BigN(*offset);
    cpy_to_user_kt = ktime_get();
    copy_to_user(buf, &output, sizeof(struct BigN));
    cpy_to_user_kt = ktime_sub(ktime_get(), cpy_to_user_kt);
    return (ssize_t)sizeof(struct BigN);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file, const char *buf, size_t size,
                         loff_t *offset) {
    switch (size) {
        case 0:
            return ktime_to_ns(fib_kt);
        case 1:
            return ktime_to_ns(cpy_to_user_kt);
    }
    return 0;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig) {
    loff_t new_pos = 0;
    switch (orig) {
        case 0: /* SEEK_SET: */
            new_pos = offset;
            break;
        case 1: /* SEEK_CUR: */
            new_pos = file->f_pos + offset;
            break;
        case 2: /* SEEK_END: */
            new_pos = MAX_LENGTH - offset;
            break;
    }

    // if (new_pos > MAX_LENGTH)
    //     new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0) new_pos = 0;  // min case
    file->f_pos = new_pos;         // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void) {
    int rc = 0;
    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void) {
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
