// Inclusión de cabeceras necesarias para módulos del kernel y manejo de GPIOs, dispositivos, etc.
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/uaccess.h>

// Definición de nombres para el dispositivo y la clase
#define DEVICE_NAME "signal_dev_TP5"
#define CLASS_NAME  "signal_class_TP5"

// Información del módulo
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grupo Multitaskers");
MODULE_DESCRIPTION("Char driver con GPIO y overlay");

// Variables globales
static struct gpio_desc *gpio1;      // Primer GPIO de entrada
static struct gpio_desc *gpio2;      // Segundo GPIO de entrada
static int current_signal = 0;       // Señal actualmente seleccionada (0 o 1)
static char buffer[2];               // Buffer para enviar datos al espacio de usuario

static dev_t dev_num;                // Número mayor/menor del dispositivo
static struct class *signal_class;  // Clase del dispositivo
static struct cdev signal_cdev;     // Estructura del char device

// Función de lectura del dispositivo
static ssize_t driver_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
   // Lee el valor del GPIO seleccionado (0 o 1)
   int value = current_signal == 0 ? gpiod_get_value(gpio1) : gpiod_get_value(gpio2);
   buffer[0] = value ? '1' : '0';  // Convierte el valor a carácter
   buffer[1] = '\n';               // Agrega salto de línea

   if (*off >= 2) return 0;  // Si ya se leyó todo el contenido, retorna 0 (EOF)
   if (copy_to_user(buf, buffer, 2)) return -EFAULT;  // Copia a espacio de usuario
   *off += 2;  // Avanza el offset
   return 2;   // Retorna el número de bytes leídos
}

// Función de escritura del dispositivo
static ssize_t driver_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
   char kbuf;
   if (copy_from_user(&kbuf, buf, 1)) return -EFAULT;  // Copia primer byte desde espacio de usuario

   // Cambia la señal seleccionada según el valor recibido ('0' o '1')
   if (kbuf == '0') current_signal = 0;
   else if (kbuf == '1') current_signal = 1;
   else return -EINVAL;  // Si no es válido, retorna error

   return len;  // Devuelve la cantidad de bytes escritos
}

// Funciones abiertas/cerradas básicas que no hacen nada en este caso
static int driver_open(struct inode *inode, struct file *file) { return 0; }
static int driver_release(struct inode *inode, struct file *file) { return 0; }

// Tabla de operaciones del dispositivo
static struct file_operations fops = {
   .owner = THIS_MODULE,
   .read = driver_read,
   .write = driver_write,
   .open = driver_open,
   .release = driver_release,
};

// Función que se llama cuando el dispositivo es detectado (probe)
static int signal_probe(struct platform_device *pdev)
{
   int ret;

   // Obtiene los GPIOs definidos en el Device Tree como "signal", índices 0 y 1
   gpio1 = devm_gpiod_get_index(&pdev->dev, "signal", 0, GPIOD_IN);
   if (IS_ERR(gpio1)) return PTR_ERR(gpio1);

   gpio2 = devm_gpiod_get_index(&pdev->dev, "signal", 1, GPIOD_IN);
   if (IS_ERR(gpio2)) return PTR_ERR(gpio2);

   // Asigna número mayor/menor dinámicamente
   ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
   if (ret < 0) return ret;

   // Inicializa y registra el char device
   cdev_init(&signal_cdev, &fops);
   ret = cdev_add(&signal_cdev, dev_num, 1);
   if (ret < 0) return ret;

   // Crea la clase del dispositivo en sysfs (/sys/class)
   signal_class = class_create(CLASS_NAME);
   if (IS_ERR(signal_class)) return PTR_ERR(signal_class);

   // Crea el archivo de dispositivo en /dev
   device_create(signal_class, NULL, dev_num, NULL, DEVICE_NAME);

   dev_info(&pdev->dev, "Driver inicializado\n");
   return 0;
}

// Función que se llama cuando el dispositivo se elimina (remove)
static int signal_remove(struct platform_device *pdev)
{
   device_destroy(signal_class, dev_num);  // Elimina /dev/...
   class_destroy(signal_class);            // Elimina la clase
   cdev_del(&signal_cdev);                 // Elimina char device
   unregister_chrdev_region(dev_num, 1);   // Libera número mayor/menor
   return 0;
}

// Tabla de dispositivos compatibles (para Device Tree)
static const struct of_device_id signal_of_match[] = {
   { .compatible = "my,signal-driver" },  // Debe coincidir con el Device Tree
   { }
};
MODULE_DEVICE_TABLE(of, signal_of_match);

// Estructura principal del platform driver
static struct platform_driver signal_driver = {
   .driver = {
       .name = "signal_driver",
       .of_match_table = signal_of_match,
   },
   .probe = signal_probe,
   .remove = signal_remove,
};

// Macro que registra el platform driver automáticamente al cargar el módulo
module_platform_driver(signal_driver);
