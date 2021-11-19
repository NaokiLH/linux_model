#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define DEV_MAJOR 0
#define DEV_MINOR 0
#define DEV_NR_DEVS 1 //设备数量

int char_major = DEV_MAJOR;
int char_minor = DEV_MINOR;

dev_t devt;

//注册为参数
module_param(char_major, int, S_IRUGO);
module_param(char_minor, int, S_IRUGO);

//定义设备结构体
struct char_dev
{
    struct cdev cdev;
    char *c;
    int n;
    struct mutex mtx;
};

struct char_dev *char_devp; //定义设备结构体指针
struct class *char_cls;     //定义一个类

//open方法
int char_open(struct inode *inode, struct file *filp)
{
    struct char_dev *dev;
    dev = container_of(inode->i_cdev, struct char_dev, cdev);
    filp->private_data = dev;
    return 0;
}
//read方法
ssize_t char_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    int ret = 0;
    struct char_dev *dev = filp->private_data;
    if (mutex_lock_interruptible(&dev->mtx)) {
        return -ERESTARTSYS;
    }
    printk("get mutex\n");
    printk("dev->n : %d ,file_pos : %lld\n", dev->n, filp->f_pos);

    if (*ppos >= dev->n) //判断读取的位置是否超出范围
    {
        mutex_unlock(&dev->mtx);
        return 0;
    }
    if (*ppos + size > dev->n) //判断读取的长度是否超出范围
        size = dev->n - *ppos;
    if (copy_to_user(buf, dev->c, size)) {
        mutex_unlock(&dev->mtx);
        ret = -EFAULT;
    } else {
        *ppos += size;
        ret = size;
    }
    mutex_unlock(&dev->mtx);
    printk("release mutex\n");
    return ret;
}
//write方法
ssize_t char_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    int ret = 0;
    char *new_devc = NULL;
    struct char_dev *dev = filp->private_data;
    if (mutex_lock_interruptible(&dev->mtx)) {
        return -ERESTARTSYS;
    }
    printk("get mutex\n");
    if (filp->f_flags & O_APPEND) {
        *ppos = dev->n;
        printk("read O_APPEND\n");
    }
    printk("write start at %lld in address %p\n", *ppos, dev->c);
    if (*ppos + size > dev->n) //判断写入的长度是否超出范围
    {
        new_devc = krealloc(dev->c, *ppos + size, GFP_KERNEL);
        if (!new_devc) {
            mutex_unlock(&dev->mtx);
            return -ENOMEM;
        }
        dev->c = new_devc;
    }
    if (copy_from_user(dev->c + *ppos, buf, size)) {
        mutex_unlock(&dev->mtx);
        ret = -EFAULT;
    } else {
        dev->n = *ppos + size;
        *ppos += size;
        printk("write end at %lld\n", *ppos);
        printk("n:%d\n", dev->n);
        ret = size;
    }
    mutex_unlock(&dev->mtx);
    printk("release mutex\n");
    return ret;
}
loff_t char_llseek(struct file *filp, loff_t offset, int whence)
{
    loff_t newpos;
    struct char_dev *dev = filp->private_data;
    printk("llseek");
    switch (whence) {
        case 0: /* SEEK_SET */
            newpos = offset;
            break;
        case 1: /* SEEK_CUR */
            newpos = filp->f_pos + offset;
            break;
        case 2: /* SEEK_END */
            newpos = dev->n + offset;
            break;
        default: /* can't happen */
            return -EINVAL;
    }
    if (newpos < 0)
        return -EINVAL;
    filp->f_pos = newpos;
    return newpos;
}
int char_release(struct inode *inode, struct file *filp)
{
    printk("release\n");
    return 0;
}

//定义文件操作结构体
struct file_operations char_fops = {
    .owner = THIS_MODULE,
    .open = char_open,
    .read = char_read,
    .write = char_write,
    .release = char_release,
    .llseek = char_llseek,
};

static int char_init(void)
{
    int ret = 0;
    printk("char_dev_init\n");
    if (char_major) {
        devt = MKDEV(char_major, char_minor);                        //获取设备号
        ret = register_chrdev_region(devt, DEV_NR_DEVS, "char_dev"); //给定参数注册设备号
    } else
        ret = alloc_chrdev_region(&devt, char_minor, DEV_NR_DEVS, "char_dev"); //动态分配设备号
    if (ret < 0)                                                               //分配失败
    {
        printk(KERN_ERR "register_chrdev_region failed\n");
        return ret;
    }
    char_devp = kmalloc(sizeof(struct char_dev), GFP_KERNEL); //分配内存
    if (!char_devp) {
        ret = -ENOMEM;
        goto fail_malloc;
    }
    memset(char_devp, 0, sizeof(struct char_dev));       //初始化内存
    cdev_init(&char_devp->cdev, &char_fops);             //初始化cdev结构体
    char_devp->cdev.owner = THIS_MODULE;                 //设置所有者
    mutex_init(&char_devp->mtx);                         //初始化互斥锁
    ret = cdev_add(&char_devp->cdev, devt, DEV_NR_DEVS); //添加设备
    if (ret) {
        printk(KERN_ERR "cdev_add failed\n");
        goto fail_add;
    }
    char_cls = class_create(THIS_MODULE, "char_dev"); //创建类
    if (IS_ERR(char_cls)) {
        printk(KERN_ERR "class_create failed\n");
        goto fail_class;
    }
    device_create(char_cls, NULL, devt, NULL, "char_dev"); //创建设备
    return 0;

fail_class:
    cdev_del(&char_devp->cdev);
fail_add:
    kfree(char_devp);
fail_malloc:
    unregister_chrdev_region(devt, DEV_NR_DEVS);
    return ret;
}

static void char_exit(void)
{
    device_destroy(char_cls, devt);
    class_destroy(char_cls);
    cdev_del(&char_devp->cdev);
    kfree(char_devp);
    unregister_chrdev_region(devt, DEV_NR_DEVS);
    printk(KERN_INFO "char_dev_exit\n");
}

module_init(char_init);
module_exit(char_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NaokiLH");
MODULE_DESCRIPTION("char_dev");
MODULE_VERSION("V1.0");
