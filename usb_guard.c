#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/mutex.h>

#define PROC_NAME "usb_guard"

static unsigned short vendor = 0;
static unsigned short product = 0;
module_param(vendor, ushort, 0444);
MODULE_PARM_DESC(vendor, "Target USB vendor ID (hex), e.g. 0xabcd");

module_param(product, ushort, 0444);
MODULE_PARM_DESC(product, "Target USB product ID (hex), e.g. 0x1234");

static char serial[128] = "";
module_param_string(serial, serial, sizeof(serial), 0444);
MODULE_PARM_DESC(serial, "Optional target USB serial string. Leave empty to ignore.");

static struct proc_dir_entry *proc_entry;

/* Мы “отслеживаем” конкретное устройство */
static struct usb_device *tracked_udev;
static DEFINE_MUTEX(tracked_lock);
static atomic_t present = ATOMIC_INIT(0);

static bool match_on_add(struct usb_device *udev)
{
    char buf[128];
    int len;

    if (!udev || vendor == 0 || product == 0)
        return false;

    if (le16_to_cpu(udev->descriptor.idVendor) != vendor)
        return false;

    if (le16_to_cpu(udev->descriptor.idProduct) != product)
        return false;

    /* Если serial не задан — VID/PID достаточно */
    if (serial[0] == '\0')
        return true;

    /* Иначе проверяем серийник (только на ADD — там он читается нормально) */
    if (udev->descriptor.iSerialNumber == 0)
        return false;

    memset(buf, 0, sizeof(buf));
    len = usb_string(udev, udev->descriptor.iSerialNumber, buf, sizeof(buf) - 1);
    if (len < 0)
        return false;

    return (strncmp(buf, serial, sizeof(buf)) == 0);
}

static void track_device(struct usb_device *udev)
{
    mutex_lock(&tracked_lock);
    if (!tracked_udev) {
        tracked_udev = usb_get_dev(udev); /* держим ссылку */
        atomic_set(&present, 1);
        pr_info("usb_guard: target PRESENT (VID=%04x PID=%04x)\n", vendor, product);
    }
    mutex_unlock(&tracked_lock);
}

static void untrack_if_same(struct usb_device *udev)
{
    mutex_lock(&tracked_lock);
    if (tracked_udev == udev) {
        atomic_set(&present, 0);
        usb_put_dev(tracked_udev);
        tracked_udev = NULL;
        pr_info("usb_guard: target REMOVED (VID=%04x PID=%04x)\n", vendor, product);
    }
    mutex_unlock(&tracked_lock);
}

static int usb_guard_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
    struct usb_device *udev = data;

    switch (action) {
    case USB_DEVICE_ADD:
        if (match_on_add(udev))
            track_device(udev);
        break;
    case USB_DEVICE_REMOVE:
        /* На REMOVE не читаем serial — просто смотрим, то ли это устройство */
        untrack_if_same(udev);
        break;
    default:
        break;
    }

    return NOTIFY_OK;
}

static struct notifier_block usb_nb = {
    .notifier_call = usb_guard_notifier,
};

/* Найти устройство, если оно было вставлено ДО загрузки модуля */
static int find_existing_cb(struct usb_device *udev, void *data)
{
    if (match_on_add(udev)) {
        track_device(udev);
        /* даже если продолжим — track_device второй раз не возьмёт */
    }
    return 0;
}

static ssize_t proc_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    char msg[4];
    int val = atomic_read(&present) ? 1 : 0;
    int len = scnprintf(msg, sizeof(msg), "%d\n", val);
    return simple_read_from_buffer(ubuf, count, ppos, msg, len);
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
};

static int __init usb_guard_init(void)
{
    proc_entry = proc_create(PROC_NAME, 0444, NULL, &proc_fops);
    if (!proc_entry) {
        pr_err("usb_guard: failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }

    usb_register_notify(&usb_nb);

    /* Скан текущих USB-девайсов: чтобы “сразу 1”, если ключ уже вставлен */
    usb_for_each_dev(NULL, find_existing_cb);

    pr_info("usb_guard: loaded. Read /proc/%s. vendor=0x%04x product=0x%04x serial='%s'\n",
            PROC_NAME, vendor, product, serial);
    return 0;
}

static void __exit usb_guard_exit(void)
{
    usb_unregister_notify(&usb_nb);

    mutex_lock(&tracked_lock);
    if (tracked_udev) {
        usb_put_dev(tracked_udev);
        tracked_udev = NULL;
    }
    atomic_set(&present, 0);
    mutex_unlock(&tracked_lock);

    if (proc_entry)
        proc_remove(proc_entry);

    pr_info("usb_guard: unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Denis Rodionov");
MODULE_DESCRIPTION("Safe USB key presence checker by VID/PID (+ optional serial) via notifier + /proc");

module_init(usb_guard_init);
module_exit(usb_guard_exit);
