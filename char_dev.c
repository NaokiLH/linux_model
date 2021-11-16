#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
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
};

struct char_dev *char_devp; //定义设备结构体指针
struct class *char_cls;     //定义一个类

// open方法
int char_open(struct inode *inode, struct file *filp)
{
    struct char_dev *dev;
    dev = container_of(inode->i_cdev, struct char_dev, cdev); //获取设备结构体指针
    filp->private_data = dev;                                 //将设备结构体指针放到文件描述符结构的私有数据中
    return 0;
}
// read方法
ssize_t char_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    struct char_dev *dev = filp->private_data;
    int ret = 0;
    if (*ppos >= dev->n) //判断读取的位置是否超出范围
    {
        return 0;
    }
    if (*ppos + size > dev->n) //判断读取的长度是否超出范围
        size = dev->n - *ppos;
    if (copy_to_user(buf, dev->c, size))
        ret = -EFAULT;
    else {
        *ppos += size;
        ret = size;
    }
    return ret;
}
// write方法
ssize_t char_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    struct char_dev *dev = filp->private_data;
    //初始化并申请内存
    int ret = -ENOMEM;
    kfree(dev->c);
    dev->c = NULL;
    dev->n = size;
    dev->c = kmalloc(size, GFP_KERNEL);

    if (!dev->c)
        return ret;
    if (copy_from_user(dev->c, buf, size)) {
        ret = -EFAULT;
        kfree(dev->c);
        return ret;
    }
    dev->n = size;
    return size;
}
//释放设备
int char_release(struct inode *inode, struct file *filp) { return 0; }

//定义文件操作结构体
struct file_operations char_fops = {
    .owner = THIS_MODULE,
    .open = char_open,
    .read = char_read,
    .write = char_write,
    .release = char_release,
};

static int char_init(void)
{
    printk("char_dev_init\n");
    int ret = 0;
    if (char_major) {
        devt = MKDEV(char_major, char_minor); //获取设备号
        ret = register_chrdev_region(devt, DEV_NR_DEVS,
                                     "char_dev"); //给定参数注册设备号
    } else
        ret = alloc_chrdev_region(&devt, char_minor, DEV_NR_DEVS,
                                  "char_dev"); //动态分配设备号
    if (ret < 0)                               //分配失败
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