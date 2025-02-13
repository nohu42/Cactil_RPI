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
#define ILPS28QSW_NB_TRY 5

#define ILPS28QSW_SLEEP_TIMEOUT 10



stmdev_ctx_t i2c_handles;

static ssize_t temp_reading_show(struct device *dev, struct device_attribute *attr, char *buff){
	
		
	ssize_t ret;//Variable pour return
	int nb_try = ILPS28QSW_NB_TRY;
	
	/*Variable registre pour la demande et la récupération de la data*/
	ilps28qsw_all_sources_t all_sources;//Savoir si la data est disponible
	int16_t temp_val;
	/*sensor device data*/
	stmdev_ctx_t *ilps;

	//Get the device data embedded in the device
	ilps = dev_get_drvdata(dev);
	
	if(dev == NULL){
		dev_err(dev, "Something went wrong...");
		return -EIO;
	}
	
	//Trig the measurement
	ret = ilps28qsw_softtrig(ilps);
	if(ret < 0){
		goto _i2c_fail;
	}
	//Wait for data to be setup
	memset(&all_sources, 0, sizeof(ilps28qsw_all_sources_t));
	while (nb_try > 0 && !(all_sources.drdy_temp)){
		ret = ilps28qsw_all_sources_get(ilps, &all_sources);
		if (ret < 0)
		  goto _i2c_fail;
		msleep(ILPS28QSW_SLEEP_TIMEOUT);
		nb_try--;
	}

	if(!nb_try)
		goto _con_timeout;

	ret = ilps28qsw_temperature_raw_get(ilps, &temp_val);
	if (ret<0)
		goto _i2c_fail;

	ret = sprintf(buff,"Temperature: %d\n", temp_val);
	return ret;

	_i2c_fail:
	pr_err( "Error communicating with device: %ld\n", ret);
	return ret;
	_con_timeout:
	pr_err("Device took to much time to answer\n");
	return -EIO;
}

static ssize_t temp_scale_show(struct device *dev, struct device_attribute *attr, char *buff){
	return sprintf(buff,"Temperature scale: %d\n",100);
}
static ssize_t pres_scale_show(struct device *dev, struct device_attribute *attr, char *buff){
	
	/*sensor device data*/
	stmdev_ctx_t *ilps;
	ilps28qsw_md_t md;
	uint32_t scale;
	ssize_t ret;
	//Get the device data embedded in the device
	ilps = dev_get_drvdata(dev);
	
	if(dev == NULL){
		dev_err(dev, "Something went wrong...");
		return -EIO;
	}
	
	ret = ilps28qsw_mode_get(ilps, &md);
	if(ret<0){
		dev_err(dev, "Error communicating with device: %ld\n",ret);
		return ret;
	}
	
	if(md.fs)
		scale = 524288;
	else
		scale = 1048576;
	ret = sprintf(buff,"Pressure scale: %d\n", scale);
	return ret;
}
static ssize_t scale_mode_show(struct device *dev, struct device_attribute *attr, char *buff){
	
	/*sensor device data*/
	ssize_t ret;
	stmdev_ctx_t *ilps;
	ilps28qsw_md_t md;
	//Get the device data embedded in the device
	ilps = dev_get_drvdata(dev);
	
	if(dev == NULL){
		dev_err(dev, "Something went wrong...");
		return -EIO;
	}
	
	ret = ilps28qsw_mode_get(ilps, &md);
	if(ret<0){
		dev_err(dev, "Error communicating with device: %ld\n",ret);
		return ret;
	}
	
	ret = sprintf(buff,"%d\n", md.fs);
	return ret;
}
static ssize_t scale_mode_store(struct device *dev, struct device_attribute *attr, const char *buff, size_t count){
	
	/*sensor device data*/
	ssize_t ret;
	stmdev_ctx_t *ilps;
	ilps28qsw_md_t md;
	//Get the device data embedded in the device
	ilps = dev_get_drvdata(dev);
	
	if(dev == NULL){
		dev_err(dev, "Something went wrong...\n");
		return -EIO;
	}
	
	if(count != 1)
		return -EIO;
	
	ret = ilps28qsw_mode_get(ilps, &md);
	if(ret<0){
		dev_err(dev, "Error communicating with device: %ld\n",ret);
		return ret;
	}
	
	if(buff[0] != '0'){
		md.fs = ILPS28QSW_4060hPa;
		dev_info(dev, "setting mode 4060 --->%d\n", md.fs);
	}
	else{
		md.fs = ILPS28QSW_1260hPa;
		dev_info(dev, "setting mode 1260\n");

	}
	ret = ilps28qsw_mode_set(ilps, &md);
	if(ret<0)
		return ret;
	return count;
}



static ssize_t pres_reading_show(struct device *dev, struct device_attribute *attr, char *buff){
	
	ssize_t ret;//Variable pour return
	int nb_try = ILPS28QSW_NB_TRY;
	
	/*Variable registre pour la demande et la récupération de la data*/
	ilps28qsw_all_sources_t all_sources;//Savoir si la data est disponible
	uint32_t press_val;
	/*sensor device data*/
	stmdev_ctx_t *ilps;

	//Get the device data embedded in the device
	ilps = dev_get_drvdata(dev);
	
	if(dev == NULL){
		dev_err(dev, "Something went wrong...");
		return -EIO;
	}
	
	//Trig the measurement
	ret = ilps28qsw_softtrig(ilps);
	if(ret < 0){
		goto _i2c_fail;
	}
	//Wait for data to be setup
	memset(&all_sources, 0, sizeof(ilps28qsw_all_sources_t));
	while (nb_try > 0 && !(all_sources.drdy_pres)){
		ret = ilps28qsw_all_sources_get(ilps, &all_sources);
		if (ret < 0)
		  goto _i2c_fail;
		msleep(ILPS28QSW_SLEEP_TIMEOUT);
		nb_try--;
	}

	if(!nb_try)
		goto _con_timeout;

	ret = ilps28qsw_pressure_raw_get(ilps, &press_val);
	if (ret<0)
		goto _i2c_fail;

	ret = sprintf(buff,"Presure: %d\n", press_val);
	return ret;

	_i2c_fail:
	pr_err( "Error communicating with device: %ld\n", ret);
	return ret;
	_con_timeout:
	pr_err("Device took to much time to answer\n");
	return -EIO;
}

//SysFs Attributes static declaration
DEVICE_ATTR_RO(pres_reading);
DEVICE_ATTR_RO(temp_scale);
DEVICE_ATTR_RO(pres_scale);
DEVICE_ATTR_RO(temp_reading);
DEVICE_ATTR_RW(scale_mode);

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



// Probe function, add a device to the driver if compatible
static int ilps28qsw_probe(struct i2c_client *client){
	
	int ret = 0;
	stmdev_ctx_t *new_ilps;
	pr_info("ilps28qsw: Driver probing a new client\n");


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

	//Creating device sysfs attributes files:
	ret = device_create_file(&client->dev, &dev_attr_pres_reading);
	if(ret < 0){
		dev_err(&client->dev, "Can't creat device files: %d\n", ret);
		return ret; 
	}
	//Creating device sysfs attributes files:
	ret = device_create_file(&client->dev, &dev_attr_temp_reading);
	if(ret < 0){
		dev_err(&client->dev, "Can't creat device files: %d\n", ret);
		return ret; 
	}	//Creating device sysfs attributes files:
	ret = device_create_file(&client->dev, &dev_attr_scale_mode);
	if(ret < 0){
		dev_err(&client->dev, "Can't creat device files: %d\n", ret);
		return ret; 
	}	//Creating device sysfs attributes files:
	ret = device_create_file(&client->dev, &dev_attr_temp_scale);
	if(ret < 0){
		dev_err(&client->dev, "Can't creat device files: %d\n", ret);
		return ret; 
	}	//Creating device sysfs attributes files:
	ret = device_create_file(&client->dev, &dev_attr_pres_scale);
	if(ret < 0){
		dev_err(&client->dev, "Can't creat device files: %d\n", ret);
		return ret; 
	}	
	//Allocate data for the driver:
	new_ilps = kzalloc(sizeof(stmdev_ctx_t), GFP_KERNEL);
	if(IS_ERR(new_ilps)){
		pr_err("ilps28qsw: Can't allocate data for device\n");
		goto _ilps_device_alloc;
	}

	//Populate the rest of the device structure:
	new_ilps->write_reg = ilps28qsw_plateform_write;
	new_ilps->read_reg = ilps28qsw_plateform_read;
	new_ilps->mdelay= msleep;
	new_ilps->handle =  client;

	//Pass driver data to the client
	i2c_set_clientdata(client, new_ilps);

	//Init new device
	/* Restore default configuration */
	ilps28qsw_init_set(new_ilps, ILPS28QSW_RESET);
	do {///TODO This can block forever, add count down
		msleep(100);
		ilps28qsw_status_get(new_ilps, &status);
	} while (status.sw_reset);
	pr_info("ilps28qsw: Sensor RESET -> OK\n");

	/* Disable AH/QVAR to save power consumption */
	ret = ilps28qsw_ah_qvar_en_set(new_ilps, 0);
	if (ret< 0){
		goto _init_fail;
	}
	pr_info("ilps28qsw: Qvar Deactivated\n");

	/* Set bdu and if_inc recommended for driver usage */
	ret = ilps28qsw_init_set(new_ilps, ILPS28QSW_DRV_RDY);
	if (ret<0){
		goto _init_fail;
	}

	/* Select bus interface */
	bus_mode.filter = ILPS28QSW_AUTO;
	ret = ilps28qsw_bus_mode_set(new_ilps, &bus_mode);
	if (ret<0)
		goto _init_fail;

	/* Set Output Data Rate */
	md.odr = ILPS28QSW_ONE_SHOT;
	md.avg = ILPS28QSW_128_AVG;
	md.lpf = ILPS28QSW_LPF_ODR_DIV_4;
	md.fs = ILPS28QSW_4060hPa;
	ret = ilps28qsw_mode_set(new_ilps, &md);
	if (ret<0)
		goto _init_fail;
  

  pr_info("ilps28qsw: Sensor probed and initialised\n");
  return 0;

  //Error handling 
_init_fail:
  pr_err("ilps28qsw: device init failed\n");
  kfree(new_ilps);
_ilps_device_alloc:

  return ret;
}

static void ilps28qsw_remove(struct i2c_client *client){
	
	struct ilps28qsw_device *ilps = i2c_get_clientdata(client);

	device_remove_file(&client->dev, &dev_attr_pres_reading);
	device_remove_file(&client->dev, &dev_attr_temp_reading);
	device_remove_file(&client->dev, &dev_attr_pres_scale);
	device_remove_file(&client->dev, &dev_attr_temp_scale);
	device_remove_file(&client->dev, &dev_attr_scale_mode);

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

static int __init ilps28qsw_init(void){

  int ret;
  static struct i2c_client *i2c_client_ilps28qsw = NULL;

  ret = i2c_add_driver(&ilps28qsw_i2c_driver);
  if(ret<0){
    pr_err("Fail adding driver on system\n");
    goto _add_driver;
  }
  
	pr_info("Init of the driver is complete\n");
	goto exit;
	

_add_driver:
exit:
	return ret;

}
module_init(ilps28qsw_init);

static void __exit ilps28qsw_exit(void){
  
	i2c_del_driver(&ilps28qsw_i2c_driver);
	pr_info("exit driver\n");
}
module_exit(ilps28qsw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Martin VERDIER <martin.verdier@umontpellier.fr>");
MODULE_DESCRIPTION("Tentative de driver I2C \
                    pour capteur de pression (ILPS28qsw)");
MODULE_VERSION("0.1");

