#include<linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

//original µC driver given by St
#include "ilps28qsw-pid-master/ilps28qsw_reg.h"

#define DEVICE_NAME 	("ilps28qsw"	)
#define ILPS28QSW_ADDR 	(	0x5C	)



struct ilps28qsw_device{
	stmdev_ctx_t i2c_handles;
	struct list_head list_entry;
  dev_t i_dev;
  struct cdev c_dev;
};

/*Global variable for driver:*/ 
static uint8_t nb_devices_probed = 0;//Number of devices probed by the driver
                                     //to avoid getting the same minor nb
static dev_t dev_first_nb;//First dev number to add from (MINOR/MAJOR on /dev)

//This structure is only needed because this driver manualy add the device on
//the i2c bus (adapter).
static struct i2c_adapter *i2c_adapt_rpi = NULL;
static struct class *pressure_class = NULL;


//Creat List for keeping tracks of devices
LIST_HEAD(device_list);

//plateform read (used by µC driver)
int ilps28qsw_plateform_read( void *handle, 
                              uint8_t reg, 
                              uint8_t *bufp, 
                              uint16_t len){
	int ret;
	//Retrieve the i2c client from the device
  struct i2c_client *c = (struct i2c_client *)handle;
	
  //Message to be sent
  struct i2c_msg msg[2];
	msg[0].addr = c->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = bufp;

	ret = i2c_transfer(c->adapter, msg, 2);
  if(ret<0)
	  dev_err(&c->adapter->dev, "ilps28qsw: I2C read failed error code: %d\n", 
            -ret);
	
	return ret;
}

//plateform write (used by µC driver)
int ilps28qsw_plateform_write(void *handle,
                              uint8_t reg,
                              const uint8_t *bufp,
                            uint16_t len){
  int ret;
	//Retrieve the i2c client from the device
  struct i2c_client *c = (struct i2c_client *) handle;
	
  //Creating message to send
	uint8_t *buf = kzalloc(len+1, GFP_KERNEL);
	if(!buf)
		return -ENOMEM;
	
	buf[0] = reg;
	memcpy(&(buf[1]), bufp,len);

	
	ret = i2c_master_send(c, buf, len+1); 
	if (ret < 0)
		dev_err(&c->adapter->dev, "I2C write failed error code: %d\n", -ret);
	
  kfree(buf);
	return ret;
}


int ilps28qsw_open(struct inode *in, struct file *f){
  
  int ret = -ENXIO; 
  struct ilps28qsw_device *ilps;
  list_for_each_entry(ilps, &device_list, list_entry){
    if(ilps->i_dev == in->i_cdev->dev)
    {
      ret = 0;
      break;
    }
  }
  if(ret){
    pr_warn("ilps28qsw: No device matched %d\n", MAJOR(in->i_cdev->dev));
    goto _not_found;
  }
  
  //Store a pointer to device struct for other methods:
  f->private_data = ilps;

  pr_info("ilps28qsw: Driver file was opened\n");

_not_found:
  return ret;


}
ssize_t ilps28qsw_write(struct file *f, const char __user *u, 
                        size_t len, loff_t *){

  pr_info("Driver file was written\n");
  return len;

}

//Opperation structure to talk with de device with files
static struct file_operations ilps28qsw_fops = {
  .owner =  THIS_MODULE,
  .open = ilps28qsw_open,
  .write = ilps28qsw_write,
};


// Probe function, add a device to the driver if compatible
static int ilps28qsw_probe(struct i2c_client *client){
	
	int ret = 0;
  dev_t curr_dev;
  struct ilps28qsw_device *new_ilps;
  pr_info("ilps28qsw: Driver probed a new client\n");


	/*Variables for register modification*/
    ilps28qsw_bus_mode_t bus_mode;
    ilps28qsw_stat_t status;
    ilps28qsw_md_t md;
  

    //Check functionnality of the adaptor
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;
  dev_info(&(client->dev), "The adapteur support the right functions \n" );
  
	//Read family id; if this fail, it's not the rigtht device
	ret = i2c_smbus_read_byte_data(client, ILPS28QSW_WHO_AM_I);
	if (ret != ILPS28QSW_ID)
		return (ret < 0)? ret : -ENODEV;
  dev_info(&(client->dev), "Device appears to be supported --> probing \n" );

  //Allocate data for the driver:
  new_ilps = kzalloc(sizeof(*new_ilps), GFP_KERNEL);
  if(IS_ERR(new_ilps)){
      pr_err("ilps28qsw: Can't allocate data for device");
      goto _ilps_device_alloc;
    }

  //registering the device as a char driver:
  if(nb_devices_probed >= 255){
    dev_err(&(client->dev), "To many device probed, try load/unload driver\n");
    goto _cdev_add;
  }

  cdev_init(&new_ilps->c_dev, &ilps28qsw_fops);
  curr_dev = MKDEV(MAJOR(dev_first_nb), MINOR(dev_first_nb)+nb_devices_probed);
  ret = cdev_add(&new_ilps->c_dev, curr_dev, 1);
  if (ret < 0){
    dev_err(&client->dev, "Can't add char device to system, errno: %d\n", ret);
    goto _cdev_add;
  }
  if(IS_ERR(device_create(pressure_class, 
                      NULL, 
                      curr_dev,
                      NULL,
                      "ilps28qsw%d", nb_devices_probed))){
    dev_err(&client->dev,"Can't creat char device");
    goto _device_create;
  }
  nb_devices_probed++;
  
  dev_info( &(client->dev), "Device added as char device, Major:%d Minor:%d\n", 
            MAJOR(curr_dev),  
            MINOR(curr_dev));


  //Populate the rest of the device structure:
  new_ilps->i2c_handles.write_reg = ilps28qsw_plateform_write;
  new_ilps->i2c_handles.read_reg = ilps28qsw_plateform_read;
  new_ilps->i2c_handles.mdelay= msleep;
  new_ilps->i2c_handles.handle =  client;
  new_ilps->i_dev=  curr_dev;//dev number for device identification
  
  //link list for multiple devices support:
  INIT_LIST_HEAD(&new_ilps->list_entry);//Initialise the list 
  list_add_tail(&new_ilps->list_entry, &device_list);// Add device to list

  //Pass driver data to the client
  i2c_set_clientdata(client, new_ilps);

  //Init new device
  /* Restore default configuration */
  ilps28qsw_init_set(&new_ilps->i2c_handles, ILPS28QSW_RESET);
  do {///TODO This can block forever, add count down
    msleep(100);
    ilps28qsw_status_get(&new_ilps->i2c_handles, &status);
  } while (status.sw_reset);
  pr_info("ilps28qsw: Sensor RESET -> OK\n");

  /* Disable AH/QVAR to save power consumption */
  ret = ilps28qsw_ah_qvar_en_set(&new_ilps->i2c_handles, 0);
  if (ret< 0){
    //printf("QVAR NOT DEACTIVATED: %d - %s\n",ret, strerror(errno));
    goto _init_fail;
  }
  pr_info("ilps28qsw: Qvar Deactivated\n");

	/* Set bdu and if_inc recommended for driver usage */
	ret = ilps28qsw_init_set(&new_ilps->i2c_handles, ILPS28QSW_DRV_RDY);
  if (ret<0){
		goto _init_fail;
  }

	/* Select bus interface */
	bus_mode.filter = ILPS28QSW_AUTO;
	ret = ilps28qsw_bus_mode_set(&new_ilps->i2c_handles, &bus_mode);
	if (ret<0)
		goto _init_fail;

	/* Set Output Data Rate */
	md.odr = ILPS28QSW_ONE_SHOT;
	md.avg = ILPS28QSW_128_AVG;
	md.lpf = ILPS28QSW_LPF_ODR_DIV_4;
	md.fs = ILPS28QSW_4060hPa;
	ret = ilps28qsw_mode_set(&new_ilps->i2c_handles, &md);
	if (ret<0)
		goto _init_fail;
  

  pr_info("ilps28qsw: Sensor probed and initialised\n");
  return 0;

  //Error handling 
_init_fail:
  pr_err("ilps28qsw: device init failed\n");
  device_destroy(pressure_class, new_ilps->i_dev);
_device_create:
  cdev_del(&new_ilps->c_dev);
_cdev_add: 
_ilps_device_alloc:
  kfree(new_ilps);
  return ret;
}

static void ilps28qsw_remove(struct i2c_client *client){
  struct ilps28qsw_device *ilps = i2c_get_clientdata(client);
  list_del(&ilps->list_entry);
  device_destroy(pressure_class, ilps->i_dev);
  cdev_del(&ilps->c_dev);
	kfree(ilps);
  pr_info("ilps28qsw: Driver removed a client\n");
}


static const struct i2c_device_id ilps28qsw_id[]={
	{DEVICE_NAME, ILPS28QSW_ADDR},
	{},
};
MODULE_DEVICE_TABLE(i2c, ilps28qsw_id);


static struct i2c_driver ilps28qsw_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name="ilps28qsw",
	},
	.probe = ilps28qsw_probe,	
	.remove = ilps28qsw_remove,	
	.id_table = ilps28qsw_id,	
};

static struct i2c_board_info pressure_i2c_board_info = {
        I2C_BOARD_INFO(DEVICE_NAME, ILPS28QSW_ADDR)
    };

static int __init ilps28qsw_init(void){

  int ret;
  static struct i2c_client *i2c_client_ilps28qsw = NULL;

  ret = alloc_chrdev_region(&dev_first_nb, 0, 256, DEVICE_NAME);
  if (ret < 0){
    pr_err("Can't allocat device id region\n");
    goto exit;
  }

  //Creating a class for the device:
  pressure_class = class_create("pressure_sensor");
  if(IS_ERR(pressure_class)){
    pr_err("Fail creating the class");
    ret = PTR_ERR(pressure_class);
    goto class_error;
  }

  ret = i2c_add_driver(&ilps28qsw_i2c_driver);
  if(ret<0){
    pr_err("Fail adding driver on system\n");
    goto _add_driver;
  }

  i2c_adapt_rpi = i2c_get_adapter(1);//Get the adapter named 1
  if (i2c_adapt_rpi == NULL){
    pr_err("Can't retrieve i2c adapter\n");
    ret = PTR_ERR(i2c_adapt_rpi);
    goto exit;
  }

  i2c_client_ilps28qsw = i2c_new_client_device(i2c_adapt_rpi, 
      &pressure_i2c_board_info);
  if (i2c_client_ilps28qsw == NULL){
    pr_err("Can't creat new i2c_client\n");
    ret = PTR_ERR(i2c_client_ilps28qsw);
    goto exit;
  }

	pr_info("Init of the driver is complete\n");
	goto exit;
	

_add_driver:
	class_destroy(pressure_class);
class_error:
	unregister_chrdev_region(dev_first_nb, 256);
exit:

	i2c_put_adapter(i2c_adapt_rpi);
	return ret;

}
module_init(ilps28qsw_init);

static void __exit ilps28qsw_exit(void){
  
	i2c_del_driver(&ilps28qsw_i2c_driver);
	class_destroy(pressure_class);
	unregister_chrdev_region(dev_first_nb, 256);
	pr_info("exit driver\n");
}
module_exit(ilps28qsw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin VERDIER <martin.verdier@umontpellier.fr>");
MODULE_DESCRIPTION("Tentative de driver I2C \
                    pour capteur de pression (ILPS28qsw)");
MODULE_VERSION("0.1");

