#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "signal_dev_TP5"
#define CLASS_NAME  "signal_class_TP5"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tu Nombre");
MODULE_DESCRIPTION("Char driver con GPIO y overlay");

static struct gpio_desc *gpio1;
static struct gpio_desc *gpio2;
static int current_signal = 0;
static char buffer[2];

static dev_t dev_num;
static struct class *signal_class;
static struct cdev signal_cdev;

static ssize_t driver_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    int value = current_signal == 0 ? gpiod_get_value(gpio1) : gpiod_get_value(gpio2);
    buffer[0] = value ? '1' : '0';
    buffer[1] = '\n';

    if (*off >= 2) return 0;
    if (copy_to_user(buf, buffer, 2)) return -EFAULT;
    *off += 2;
    return 2;
}

static ssize_t driver_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    char kbuf;
    if (copy_from_user(&kbuf, buf, 1)) return -EFAULT;

    if (kbuf == '0') current_signal = 0;
    else if (kbuf == '1') current_signal = 1;
    else return -EINVAL;

    return len;
}

static int driver_open(struct inode *inode, struct file *file) { return 0; }
static int driver_release(struct inode *inode, struct file *file) { return 0; }

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = driver_read,
    .write = driver_write,
    .open = driver_open,
    .release = driver_release,
};

static int signal_probe(struct platform_device *pdev)
{
    int ret;

    gpio1 = devm_gpiod_get_index(&pdev->dev, "signal", 0, GPIOD_IN);
    if (IS_ERR(gpio1)) return PTR_ERR(gpio1);

    gpio2 = devm_gpiod_get_index(&pdev->dev, "signal", 1, GPIOD_IN);
    if (IS_ERR(gpio2)) return PTR_ERR(gpio2);

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) return ret;

    cdev_init(&signal_cdev, &fops);
    ret = cdev_add(&signal_cdev, dev_num, 1);
    if (ret < 0) return ret;

    signal_class = class_create(CLASS_NAME);
    if (IS_ERR(signal_class)) return PTR_ERR(signal_class);

    device_create(signal_class, NULL, dev_num, NULL, DEVICE_NAME);

    dev_info(&pdev->dev, "Driver inicializado\n");
    return 0;
}

static int signal_remove(struct platform_device *pdev)
{
    device_destroy(signal_class, dev_num);
    class_destroy(signal_class);
    cdev_del(&signal_cdev);
    unregister_chrdev_region(dev_num, 1);
    return 0;
}

static const struct of_device_id signal_of_match[] = {
    { .compatible = "my,signal-driver" },
    { }
};
MODULE_DEVICE_TABLE(of, signal_of_match);

static struct platform_driver signal_driver = {
    .driver = {
        .name = "signal_driver",
        .of_match_table = signal_of_match,
    },
    .probe = signal_probe,
    .remove = signal_remove,
};
module_platform_driver(signal_driver);
