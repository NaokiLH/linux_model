#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define DEV_MAJOR 0
#define DEV_MINOR 0
#define DEV_NR_DEVS 1 //device number

int pipe_major = DEV_MAJOR;
int pipe_minor = DEV_MINOR;

dev_t devt;

//register parameters
module_param(pipe_major, int, S_IRUGO);
module_param(pipe_minor, int, S_IRUGO);

//device struct
struct pipe_dev
{
    struct cdev cdev;
    char *data;
    int n;    //data length
    int size; //memory size
    struct spinlock lock;
};

struct pipe_dev *pipe_devp; //device struct pointer
struct class *pipe_class;   //class

//open
int pipe_open(struct inode *inode, struct file *filp)
{
    struct pipe_dev *dev;
    printk(KERN_INFO "***********************pipe_open**********************\n");
    dev = container_of(inode->i_cdev, struct pipe_dev, cdev);
    filp->private_data = dev;
    filp->f_pos = 0; //set file position to 0
    return 0;
}
//read
ssize_t pipe_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret = 0;
    printk(KERN_INFO "-----------------------pipe_read----------------------\n");
    struct pipe_dev *dev = filp->private_data;
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    printk("get spinlock\n");
    if (dev->n > 0) {
        if (*ppos + count > dev->n) // if read size is bigger than data length
        {
            count = dev->n;
        }
        printk("read count : %ld,length : %d,size : %d\n", count, dev->n, dev->size);
        if (copy_to_user(buf, dev->data, count)) {
            spin_unlock_irqrestore(&dev->lock, flags);
            printk(KERN_ERR "copy_to_user error\n");
            ret = -EFAULT;
        } else {
            ret = count;
            dev->n -= count;
            memmove(dev->data, dev->data + count, dev->n);
        }
    } else
        printk("read count : %ld,length : %d,size : %d\n", count, dev->n, dev->size);
    spin_unlock_irqrestore(&dev->lock, flags);
    printk("release spinlock\n");
    return ret;
}
//write
ssize_t pipe_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret = 0;
    printk(KERN_INFO "-----------------------pipe_write----------------------\n");
    struct pipe_dev *dev = filp->private_data;
    unsigned long flags;
    char *tmp;
    spin_lock_irqsave(&dev->lock, flags);
    printk("get spinlock\n");
    if (count + dev->n > dev->size) {
        tmp = krealloc(dev->data, count + dev->n, GFP_KERNEL);
        if (!tmp) {
            printk(KERN_ERR "krealloc failed\n");
            ret = -ENOMEM;
            goto fail_write;
        }
        dev->data = tmp;
        dev->size = count + dev->n;
        printk("after realloc the size is %d\n", dev->size);
    }
    if (copy_from_user(dev->data + dev->n, buf, count)) {
        printk(KERN_ERR "copy_from_user failed\n");
        ret = -EFAULT;
        goto fail_write;
    }
    ret = count;
    dev->n += count;
    spin_unlock_irqrestore(&dev->lock, flags);
    printk("release spinlock\n");
    return ret;
fail_write:
    spin_unlock_irqrestore(&dev->lock, flags);
    printk("release spinlock\n");
    return ret;
}
//release
int pipe_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "***********************pipe_close*********************\n");
    return 0;
}
//file operations
struct file_operations pipe_fops = {
    .owner = THIS_MODULE,
    .open = pipe_open,
    .read = pipe_read,
    .write = pipe_write,
    .release = pipe_release,
};

//init
static int __init pipe_init(void)
{
    int ret = 0;
    printk(KERN_INFO "-----------------------pipe_init----------------------\n");
    devt = MKDEV(pipe_major, pipe_minor);
    if (pipe_major) {
        ret = register_chrdev_region(devt, DEV_NR_DEVS, "pipe_dev");
    } else {
        ret = alloc_chrdev_region(&devt, pipe_minor, DEV_NR_DEVS, "pipe_dev");
        pipe_major = MAJOR(devt);
    }
    if (ret < 0) {
        printk(KERN_ERR "register char device error\n");
        return ret;
    }
    //init device struct
    pipe_devp = kmalloc(sizeof(struct pipe_dev), GFP_KERNEL);
    if (!pipe_devp) {
        printk(KERN_ERR "kmalloc error\n");
        goto fail_malloc;
    }
    //init memory
    memset(pipe_devp, 0, sizeof(struct pipe_dev));
    //init cdev
    cdev_init(&pipe_devp->cdev, &pipe_fops);
    pipe_devp->cdev.owner = THIS_MODULE;
    //init spinlock
    spin_lock_init(&pipe_devp->lock);
    //add cdev
    ret = cdev_add(&pipe_devp->cdev, devt, DEV_NR_DEVS);
    if (ret) {
        printk(KERN_ERR "cdev_add error\n");
        goto fail_cdev_add;
    }
    //create class
    pipe_class = class_create(THIS_MODULE, "pipe_class");
    if (IS_ERR(pipe_class)) {
        printk(KERN_ERR "class_create error\n");
        goto fail_class_create;
    }
    //create device
    device_create(
        pipe_class,
        NULL,
        devt,
        NULL,
        "pipe_dev");
    return 0;

fail_class_create:
    cdev_del(&pipe_devp->cdev);
fail_cdev_add:
    kfree(pipe_devp);
fail_malloc:
    unregister_chrdev_region(devt, DEV_NR_DEVS);
    return ret;
}

//exit
static void __exit pipe_exit(void)
{
    device_destroy(pipe_class, devt);
    class_destroy(pipe_class);
    cdev_del(&pipe_devp->cdev);
    kfree(pipe_devp);
    unregister_chrdev_region(devt, DEV_NR_DEVS);
    printk(KERN_INFO "-----------------------pipe_exit----------------------\n");
}

module_init(pipe_init);
module_exit(pipe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NaokiLH");
MODULE_DESCRIPTION("PIPE_DEV");
MODULE_VERSION("V1.0");