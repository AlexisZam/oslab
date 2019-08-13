/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * Alexis Zamanis
 *
 */

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "lunix-chrdev.h"
#include "lunix-lookup.h"
#include "lunix.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;

/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state) {
    struct lunix_sensor_struct *sensor;

    WARN_ON(!(sensor = state->sensor));
    return sensor->msr_data[state->type]->last_update != state->buf_timestamp;
}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state) {
    struct lunix_sensor_struct *sensor;
    unsigned long flags;
    uint32_t last_update, value;
    long res;

    debug("entering\n");

    /*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */
    sensor = state->sensor;
    spin_lock_irqsave(&sensor->lock, flags);

    /*
	 * Any new data available?
	 */
    if (!lunix_chrdev_state_needs_refresh(state)) {
        spin_unlock_irqrestore(&sensor->lock, flags);
        return -EAGAIN;
    }
    last_update = sensor->msr_data[state->type]->last_update;
    value = sensor->msr_data[state->type]->values[0];
    spin_unlock_irqrestore(&sensor->lock, flags);

    /*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */
    switch (state->type) {
    case BATT:
        res = lookup_voltage[value];
        break;
    case TEMP:
        res = lookup_temperature[value];
        break;
    case LIGHT:
        res = lookup_light[value];
        break;
    }

    state->buf_lim = snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%c%d.%d\n", res >= 0 ? ' ' : '-', res / 1000, res % 1000);
    state->buf_timestamp = last_update;

    debug("leaving\n");
    return 0;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open(struct inode *inode, struct file *filp) {
    /* Declarations */
    unsigned int minor;
    enum lunix_msr_enum type;
    struct lunix_sensor_struct sensor;
    struct lunix_chrdev_state_struct *state;
    int ret;

    debug("entering\n");
    ret = -ENODEV;
    if ((ret = nonseekable_open(inode, filp)) < 0)
        goto out;

    /*
	 * Associate this open file with the relevant sensor based on
	 * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
	 */
    minor = iminor(inode);
    type = minor & 7;
    sensor = lunix_sensors[minor >> 3];

    /* Allocate a new Lunix character device private state structure */
    state = kzalloc(sizeof(struct lunix_chrdev_state_struct), GFP_KERNEL);
    if (!state) {
        printk(KERN_ERR "Failed to allocate memory for state\n");
        ret = -ENOMEM;
        goto out;
    }

    state->type = type;
    state->sensor = &sensor;

    state->buf_lim = 0;
    // state->buf_data;
    state->buf_timestamp = 0;

    sema_init(&state->lock, 1);
    // init_MUTEX(&state->lock);

    filp->private_data = state;
out:
    debug("leaving, with ret = %d\n", ret);
    return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp) {
    kfree(filp->private_data);
    return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    /* Why? */
    return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos) {
    ssize_t ret;

    struct lunix_sensor_struct *sensor;
    struct lunix_chrdev_state_struct *state;

    state = filp->private_data;
    WARN_ON(!state);

    sensor = state->sensor;
    WARN_ON(!sensor);

    if (down_interruptible(&state->lock))
        return -ERESTARTSYS;
    /*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */
    if (*f_pos == 0) {
        while (lunix_chrdev_state_update(state) == -EAGAIN) {
            up(&state->lock);
            if (filp->f_flags & O_NONBLOCK)
                return -EAGAIN;
            /* The process needs to sleep */
            if (wait_event_interruptible(sensor->wq, lunix_chrdev_state_needs_refresh(state)))
                return -ERESTARTSYS;
            if (down_interruptible(&state->lock))
                return -ERESTARTSYS;
        }
    }

    /* End of file */
    if (*f_pos >= state->buf_lim) {
        ret = 0;
        goto out;
    }

    /* Determine the number of cached bytes to copy to userspace */
    if (cnt > state->buf_lim - *f_pos)
        cnt = state->buf_lim - *f_pos;

    if (copy_to_user(usrbuf, state->buf_data + *f_pos, cnt)) {
        ret = -EFAULT;
        goto out;
    }

    *f_pos += cnt;
    ret = cnt;

    /* Auto-rewind on EOF mode? */
    if (*f_pos >= state->buf_lim)
        *f_pos = 0;
out:
    up(&state->lock);
    return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma) {
    return -EINVAL;
}

static struct file_operations lunix_chrdev_fops =
    {
        .owner = THIS_MODULE,
        .open = lunix_chrdev_open,
        .release = lunix_chrdev_release,
        .read = lunix_chrdev_read,
        .unlocked_ioctl = lunix_chrdev_ioctl,
        .mmap = lunix_chrdev_mmap};

int lunix_chrdev_init(void) {
    /*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
    int ret;
    dev_t dev_no;
    unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;

    debug("initializing character device\n");
    cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
    lunix_chrdev_cdev.owner = THIS_MODULE;

    dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
    ret = register_chrdev_region(dev_no, lunix_minor_cnt, "lunix");
    if (ret < 0) {
        debug("failed to register region, ret = %d\n", ret);
        goto out;
    }
    ret = cdev_add(&lunix_chrdev_cdev, dev_no, lunix_minor_cnt);
    if (ret < 0) {
        debug("failed to add character device\n");
        goto out_with_chrdev_region;
    }
    debug("completed successfully\n");
    return 0;

out_with_chrdev_region:
    unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
    return ret;
}

void lunix_chrdev_destroy(void) {
    dev_t dev_no;
    unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;

    debug("entering\n");
    dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
    cdev_del(&lunix_chrdev_cdev);
    unregister_chrdev_region(dev_no, lunix_minor_cnt);
    debug("leaving\n");
}
