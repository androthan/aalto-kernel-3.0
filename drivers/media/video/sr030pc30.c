/*
 * drivers/media/video/mt9p012.c
 *
 * mt9p012 sensor driver
 *
 *
 * Copyright (C) 2008 Texas Instruments.
 *
 * Leverage OV9640.c
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *****************************************************
 *****************************************************
 * modules/camera/sr030pc30.c
 *
 * SR030PC30 sensor driver source file
 *
 * Modified by paladin in Samsung Electronics
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>

#include <mach/gpio.h>
#include <mach/hardware.h>

#include <media/v4l2-int-device.h>
#include "isp/isp.h"
#include "omap34xxcam.h"
#include "sr030pc30.h"
#include "sr030pc30_tune.h"

bool front_cam_in_use= false;
#if (CAM_SR030PC30_DBG_MSG)
#include "dprintk.h"
#else
#define dprintk(x, y...)
#endif

#define I2C_M_WRITE 0x0000 /* write data, from slave to master */
#define I2C_M_READ  0x0001 /* read data, from slave to master */

#define POLL_TIME_MS			10

static u32 sr030pc30_curr_state = SR030PC30_STATE_INVALID;
static u32 sr030pc30_pre_state = SR030PC30_STATE_INVALID;

/* Section Index */
#if 0
static int reg_init_qcif_index;
static int reg_init_cif_index;
static int reg_init_qvga_index;
static int reg_init_vga_index;
static int reg_init_qcif_vt_index;
static int reg_init_cif_vt_index;
static int reg_init_qvga_vt_index;
static int reg_init_vga_vt_index;
static int reg_wb_auto_index;
static int reg_wb_daylight_index;
static int reg_wb_cloudy_index;
static int reg_wb_incandescent_index;
static int reg_wb_fluorescent_index;
static int reg_ev_index;
static int reg_ev_vt_index;
static int reg_contrast_level_m5_index;
static int reg_contrast_level_m4_index;
static int reg_contrast_level_m3_index;
static int reg_contrast_level_m2_index;
static int reg_contrast_level_m1_index;
static int reg_contrast_level_default_index;
static int reg_contrast_level_p1_index;
static int reg_contrast_level_p2_index;
static int reg_contrast_level_p3_index;
static int reg_contrast_level_p4_index;
static int reg_contrast_level_p5_index;
static int reg_effect_none_index;
static int reg_effect_red_index;
static int reg_effect_gray_index;
static int reg_effect_sepia_index;
static int reg_effect_green_index;
static int reg_effect_aqua_index;
static int reg_effect_negative_index;
static int reg_flip_none_index;
static int reg_flip_water_index;
static int reg_flip_mirror_index;
static int reg_flip_water_mirror_index;
static int reg_pretty_none_index;
static int reg_pretty_level1_index;
static int reg_pretty_level2_index;
static int reg_pretty_level3_index;
static int reg_pretty_vt_none_index;
static int reg_pretty_vt_level1_index;
static int reg_pretty_vt_level2_index;
static int reg_pretty_vt_level3_index;
static int reg_7fps_index;
static int reg_10fps_index;
static int reg_15fps_index;
static int reg_self_capture_index;
#endif

static struct sr030pc30_sensor sr030pc30 = {
  .timeperframe = {
    .numerator    = 1,
    .denominator  = 15,
  //.denominator  = 30,//LYG
  },
  .mode           = SR030PC30_MODE_CAMERA,  
  .state          = SR030PC30_STATE_PREVIEW,
  .fps            = 15,
 //.fps            = 30,//LYG
  .preview_size   = SR030PC30_PREVIEW_SIZE_640_480, //LYG
  //.preview_size   = SR030PC30_PREVIEW_SIZE_320_240,
  .capture_size   = SR030PC30_IMAGE_SIZE_640_480,
  .detect         = SENSOR_NOT_DETECTED,
  .zoom           = SR030PC30_ZOOM_1P00X,
  .effect         = SR030PC30_EFFECT_OFF,
  .ev             = SR030PC30_EV_DEFAULT,
  .contrast       = SR030PC30_CONTRAST_DEFAULT,
  .wb             = SR030PC30_WB_AUTO,
  .pretty         = SR030PC30_PRETTY_NONE,
  .flip           = SR030PC30_FLIP_NONE,
};

struct v4l2_queryctrl sr030pc30_ctrl_list[] = {
  {
    .id            = V4L2_CID_SELECT_MODE,
    .type          = V4L2_CTRL_TYPE_INTEGER,
    .name          = "select mode",
    .minimum       = SR030PC30_MODE_CAMERA,
    .maximum       = SR030PC30_MODE_VT,
    .step          = 1,
    .default_value = SR030PC30_MODE_CAMERA,
  }, 
  {
    .id            = V4L2_CID_SELECT_STATE,
    .type          = V4L2_CTRL_TYPE_INTEGER,
    .name          = "select state",
    .minimum       = SR030PC30_STATE_PREVIEW,
    .maximum       = SR030PC30_STATE_CAPTURE,
    .step          = 1,
    .default_value = SR030PC30_STATE_PREVIEW,
  }, 
  {
    .id            = V4L2_CID_ZOOM,
    .type          = V4L2_CTRL_TYPE_INTEGER,
    .name          = "Zoom",
    .minimum       = SR030PC30_ZOOM_1P00X,
    .maximum       = SR030PC30_ZOOM_4P00X,
    .step          = 1,
    .default_value = SR030PC30_ZOOM_1P00X,
  },
  {
    .id            = V4L2_CID_BRIGHTNESS,
    .type          = V4L2_CTRL_TYPE_INTEGER,
    .name          = "Brightness",
    .minimum       = SR030PC30_EV_MINUS_2P0,
    .maximum       = SR030PC30_EV_PLUS_2P0,
    .step          = 1,
    .default_value = SR030PC30_EV_DEFAULT,
  },
  {
    .id            = V4L2_CID_WB,
    .type          = V4L2_CTRL_TYPE_INTEGER,
    .name          = "White Balance",
    .minimum       = SR030PC30_WB_AUTO,
    .maximum       = SR030PC30_WB_FLUORESCENT,
    .step          = 1,
    .default_value = SR030PC30_WB_AUTO,
  },
  {
    .id            = V4L2_CID_CONTRAST,
    .type          = V4L2_CTRL_TYPE_INTEGER,
    .name          = "Contrast",
    .minimum       = SR030PC30_CONTRAST_MINUS_3,
    .maximum       = SR030PC30_CONTRAST_PLUS_3,
    .step          = 1,
    .default_value = SR030PC30_CONTRAST_DEFAULT,
  },
  {
    .id            = V4L2_CID_EFFECT,
    .type          = V4L2_CTRL_TYPE_INTEGER,
    .name          = "Effect",
    .minimum       = SR030PC30_EFFECT_OFF,
    .maximum       = SR030PC30_EFFECT_PURPLE,
    .step          = 1,
    .default_value = SR030PC30_EFFECT_OFF,
  },
  {
    .id            = V4L2_CID_FLIP,
    .type          = V4L2_CTRL_TYPE_INTEGER,
    .name          = "Flip",
    .minimum       = SR030PC30_FLIP_NONE,
    .maximum       = SR030PC30_FLIP_WATER_MIRROR,
    .step          = 1,
    .default_value = SR030PC30_FLIP_NONE,
  },
  {
    .id            = V4L2_CID_PRETTY,
    .type          = V4L2_CTRL_TYPE_INTEGER,
    .name          = "Pretty",
    .minimum       = SR030PC30_PRETTY_NONE,
    .maximum       = SR030PC30_PRETTY_LEVEL3,
    .step          = 1,
    .default_value = SR030PC30_PRETTY_NONE,
  },  
};
#define NUM_SR030PC30_CONTROL ARRAY_SIZE(sr030pc30_ctrl_list)

/* list of image formats supported by sr030pc30 sensor */
const static struct v4l2_fmtdesc sr030pc30_formats[] = {
  {
    .description = "YUV422 (UYVY)",
    .pixelformat = V4L2_PIX_FMT_UYVY,
  },
  {
    .description = "YUV422 (YUYV)",
    .pixelformat = V4L2_PIX_FMT_YUYV,
  },
  {
    .description = "JPEG(without header)+ JPEG",
    .pixelformat = V4L2_PIX_FMT_JPEG,
  },
  {
    .description = "JPEG(without header)+ YUV",
    .pixelformat = V4L2_PIX_FMT_MJPEG,
  },  
};
#define NUM_SR030PC30_FORMATS ARRAY_SIZE(sr030pc30_formats)

extern struct sr030pc30_platform_data nowplus_sr030pc30_platform_data;


static char *SR030pc30_cam_tunning_table = NULL;

static int SR030pc30_cam_tunning_table_size;

static unsigned short SR030pc30_cam_tuning_data[1] = {
0xffff
};

//#define CAM_TUNING_MODE
static int CamTunningStatus = 1; 


/**
 * SR030pc30_i2c_read: Read 2 bytes from sensor 
 */

static inline int SR030pc30_i2c_read(struct i2c_client *client, unsigned char subaddr, unsigned char *data)
{
	unsigned char buf[1];
	int ret = 0;
	int retry_count = 1;
	struct i2c_msg msg = {client->addr, 0, 1, buf};

	buf[0] = subaddr;
	
	//printk(KERN_DEBUG "SR030pc30_i2c_read address buf[0] = 0x%x!!", buf[0]); 	
	
	while(retry_count--){
		ret = i2c_transfer(client->adapter, &msg, 1);
		if(ret == 1)
			break;
		msleep(POLL_TIME_MS);
	}

	if(ret < 0)
		return -EIO;

	//msg.flags = I2C_M_RD;
	msg.flags = I2C_M_READ;

	retry_count = 1;
	while(retry_count--){
		ret  = i2c_transfer(client->adapter, &msg, 1);
		if(ret == 1)
			break;
		msleep(POLL_TIME_MS);
	}
	
	/*
	 * [Arun c]Data comes in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */
	*data = buf[0];
	
	return (ret == 1) ? 0 : -EIO;
}

#if 1
static int SR030pc30_i2c_write_unit(struct i2c_client *client, unsigned char addr, unsigned char val)
{
	struct i2c_msg msg[1];
	unsigned char reg[2];
	int ret = 0;
	int retry_count = 1;

	if (!client->adapter)
		return -ENODEV;

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = reg;

	reg[0] = addr ;
	reg[1] = val ;	

	//printk("[wsj] ---> address: 0x%02x, value: 0x%02x\n", reg[0], reg[1]);
	
	while(retry_count--)
	{
		ret  = i2c_transfer(client->adapter, msg, 1);
		if(ret == 1)
			break;
		msleep(POLL_TIME_MS);
	}

	return (ret == 1) ? 0 : -EIO;
}

//static int SR030pc30_i2c_write(struct v4l2_subdev *sd, unsigned short i2c_data[], 
static int SR030pc30_i2c_write(struct i2c_client *client, unsigned short i2c_data[], 
							int index)
{
	//struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i =0, err =0;
	int delay;
	int count = index/sizeof(unsigned short);

	//dev_dbg(&client->dev, "%s: count %d \n", __func__, count);
	 printk(SR030PC30_MOD_NAME "SR030pc30_i2c_write start  count = %d, sizeof = %d \n",count, sizeof(unsigned short));
	for (i=0 ; i <count ; i++) {
		err = SR030pc30_i2c_write_unit(client, i2c_data[i] >>8, i2c_data[i] & 0xff );
		if (unlikely(err < 0)) {
			v4l_info(client, "%s: register set failed\n", \
			__func__);
			return err;
		}
	}
	printk(SR030PC30_MOD_NAME "SR030pc30_i2c_write sucess err = %d  \n",err);
	return 0;
}


int SR030pc30_CamTunning_table_init(void)
{
#if !defined(CAM_TUNING_MODE)
	printk(KERN_DEBUG "%s always sucess, L = %d!!", __func__, __LINE__);
	return 1;
#endif

	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int i;
	int ret;
	mm_segment_t fs = get_fs();

	printk(KERN_DEBUG "%s %d\n", __func__, __LINE__);

	set_fs(get_ds());

	filp = filp_open("/mnt/sdcard/SR030pc30.h", O_RDONLY, 0);

	if (IS_ERR(filp)) {
		printk(KERN_DEBUG "file open error or SR030pc30.h file is not.\n");
		return PTR_ERR(filp);
	}
	
	l = filp->f_path.dentry->d_inode->i_size;	
	printk(KERN_DEBUG "l = %ld\n", l);
	dp = kmalloc(l, GFP_KERNEL);
	if (dp == NULL) {
		printk(KERN_DEBUG "Out of Memory\n");
		filp_close(filp, current->files);
	}
	
	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	
	if (ret != l) {
		printk(KERN_DEBUG "Failed to read file ret = %d\n", ret);
		vfree(dp);
		filp_close(filp, current->files);
		return -EINVAL;
	}

	filp_close(filp, current->files);
		
	set_fs(fs);
	
	SR030pc30_cam_tunning_table = dp;
		
	SR030pc30_cam_tunning_table_size = l;
	
	*((SR030pc30_cam_tunning_table + SR030pc30_cam_tunning_table_size) - 1) = '\0';
	
	printk(KERN_DEBUG "SR030pc30_regs_table 0x%08x, %ld\n", dp, l);
	printk(KERN_DEBUG "%s end, line = %d\n",__func__, __LINE__);
	
	return 0;
}

static int SR030pc30_regs_table_write(struct v4l2_subdev *sd, char *name)
{
	char *start, *end, *reg;	
	unsigned short addr;
	unsigned int count = 0;
	char reg_buf[7];

	printk(KERN_DEBUG "%s start, name = %s\n",__func__, name);

	*(reg_buf + 6) = '\0';

	start = strstr(SR030pc30_cam_tunning_table, name);
	end = strstr(start, "};");

	while (1) {	
		/* Find Address */	
		reg = strstr(start,"0x");		
		if ((reg == NULL) || (reg > end))
		{
			break;
		}

		if (reg)
			start = (reg + 8); 

		/* Write Value to Address */	
		if (reg != NULL) {
			memcpy(reg_buf, reg, 6);	
			addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16); 
			if (((addr&0xff00)>>8) == 0xff)
			{
				mdelay(addr&0xff);
				printk(KERN_DEBUG "delay 0x%04x,\n", addr&0xff);
			}	
			else
			{
#ifdef VGA_CAM_DEBUG
				printk(KERN_DEBUG "addr = 0x%x, ", addr);
				if((count%10) == 0)
					printk(KERN_DEBUG "\n");
#endif
				SR030pc30_cam_tuning_data[0] = addr;
				SR030pc30_i2c_write(sd, SR030pc30_cam_tuning_data, 2); // 2byte
			}
		}
		count++;
	}
	printk(KERN_DEBUG "count = %d, %s end, line = %d\n", count, __func__, __LINE__);
	return 0;
}


//static int SR030pc30_regs_write(struct v4l2_subdev *sd, unsigned short i2c_data[], unsigned short length, char *name)
static int SR030pc30_regs_write(struct i2c_client *client, unsigned short i2c_data[], unsigned short length, char *name)
{
	int err = -EINVAL;	
	
	printk(KERN_DEBUG "%s start, Status is %s mode, parameter name = %s, length =  %d\n",\
						__func__, (CamTunningStatus != 0) ? "binary"  : "tuning",name,length);
	
	 if(CamTunningStatus) // binary mode
 	{
		//err = SR030pc30_i2c_write(sd, i2c_data, length);
		err = SR030pc30_i2c_write(client, i2c_data, length);
		//if(err==0)
			//printk(SR030PC30_MOD_NAME "SR030pc30_regs_write sucess err = %d  \n",err);
			
 	}
	 else // cam tuning mode
 	{
		//err = SR030pc30_regs_table_write(sd, name);
		err = SR030pc30_regs_table_write(client, name);
 	}

	return err;
}

#endif

#if 0
static int sr030pc30_i2c_write_read(struct i2c_client *client, u8 writedata_num, const u8* writedata, u8 readdata_num, u8* readdata)
{
  int err = 0, i = 0;
  struct i2c_msg msg[1];
  unsigned char writebuf[writedata_num];
  unsigned char readbuf[readdata_num];

  if (!client->adapter)
  {
    printk(SR030PC30_MOD_NAME "can't search i2c client adapter\n");
    return -ENODEV;
  }

  /* Write */
  msg->addr  = client->addr;
  msg->flags = I2C_M_WRITE;
  msg->len   = writedata_num;
  memcpy(writebuf, writedata, writedata_num);    
  msg->buf   = writebuf;
  
  for(i = 0; i < 10; i++)  
  {
    err = 0;//i2c_transfer(client->adapter, msg, 1) == 1 ? 0 : -EIO;
    if(err == 0) break;
    mdelay(1);
  }

  if(i == 10)
  {
    printk(SR030PC30_MOD_NAME "sr030pc30_i2c_write_read is failed... %d\n", err);
    return err;  
  }

  /* Read */
  msg->addr  = client->addr;
  msg->flags = I2C_M_READ;
  msg->len   = readdata_num;
  memset(readbuf, 0x0, readdata_num);
  msg->buf   = readbuf;
  
  for(i = 0; i < 10; i++)
  {
    err = 0;//i2c_transfer(client->adapter, msg, 1) == 1 ? 0 : -EIO;
    if (err == 0) 
    {
      memcpy(readdata, readbuf, readdata_num);
      return 0;
    }
    mdelay(1);
  }

  printk(SR030PC30_MOD_NAME "sr030pc30_i2c_write_read is failed... %d\n", err);

  return err;
}

static int sr030pc30_i2c_write(struct i2c_client *client, unsigned char length, u8 readdata[])
{
  unsigned char buf[length], i = 0;
  struct i2c_msg msg = {client->addr, I2C_M_WRITE, length, buf};
  int err = 0;

  if (!client->adapter)
  {
    printk(SR030PC30_MOD_NAME "can't search i2c client adapter\n");
    return -ENODEV;
  }

  for (i = 0; i < length; i++) 
  {
    buf[i] = readdata[i];
  }

  for (i = 0; i < 10; i++)
  {
    err =  0;//i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
    if(err == 0) return 0;
    mdelay(1);
  }

  printk(SR030PC30_MOD_NAME "sr030pc30_i2c_write is failed... %d\n", err);

  return err;
}

static int sr030pc30_set_data(struct i2c_client *client, u32 code)
{
  u8 data[2] = {0x00,};

  u32 cnt = 0;
  int ret = 0;

  data[0] = (u8)((code & 0x0000ff00) >> 8);
  data[1] = (u8)((code & 0x000000ff) >> 0);

  if (data[0] == 0xff)
  {
    goto data_fail;
  }
  else
  {
    while(cnt < 5)
    {
      ret = sr030pc30_i2c_write(client, sizeof(data), data);

      if(ret == 0)
      {
        break;
      }
      else
      {
        printk(SR030PC30_MOD_NAME "sr030pc30_i2c_write i2c write error....retry...ret=%d \n",ret);
        mdelay(5);
        cnt++;
      }
    }
  }

  if(cnt == 5)
  {
    goto data_fail;
  }

  return 0;

data_fail:  
  printk(SR030PC30_MOD_NAME "sr030pc30_set_data is failed!!\n");
  return -EINVAL; 
}	/* camsensor_sr030pc30_set_data */
#endif

static int sr030pc30_set_flip(s32 value)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;
  struct i2c_client *client = sensor->i2c_client;

  int i;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_flip is called... value = %d\n", value);

  //switch(sensor->flip)
  switch(value)
  {
    case SR030PC30_FLIP_NONE:
#ifndef CAM_TUNING_MODE		
	if(SR030pc30_regs_write(client, SR030pc30_FLIP_NONE_640x480, sizeof(SR030pc30_FLIP_NONE_640x480), "SR030pc30_FLIP_NONE_640x480")!=0)    
		 goto flip_fail;
#endif	
      //for (i = 0; i < reg_flip_none_index; i++) 
      //{
      //  if(sr030pc30_set_data(client, reg_flip_none_table[i]))
      //    goto flip_fail;
      //}       
      break;      
      
    case SR030PC30_FLIP_MIRROR:
#ifndef CAM_TUNING_MODE			
	if(SR030pc30_regs_write(client, SR030pc30_FLIP_MIRROR_640x480, sizeof(SR030pc30_FLIP_MIRROR_640x480), "SR030pc30_FLIP_MIRROR_640x480")!=0)    
		 goto flip_fail;
#endif	
      //for (i = 0; i < reg_flip_mirror_index; i++) 
      //{
     //   if(sr030pc30_set_data(client, reg_flip_mirror_table[i]))
      //    goto flip_fail;
      //}       
      break;      
     
    case SR030PC30_FLIP_WATER:
#if 0		
      for (i = 0; i < reg_flip_water_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_flip_water_table[i]))
          goto flip_fail;
      }
#endif	  
      break; 
      
    case SR030PC30_FLIP_WATER_MIRROR:
#if 0		
      for (i = 0; i < reg_flip_water_mirror_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_flip_water_mirror_table[i]))
          goto flip_fail;
      }   
#endif		  
      break;       
      
    default:
      printk(SR030PC30_MOD_NAME "[Flip]Invalid value is ordered!!!\n");
      goto flip_fail;
  }
  
  sensor->flip = value;

  return 0;

flip_fail:
  printk(SR030PC30_MOD_NAME "sr030pc30_set_flip is failed!!!\n");
  return -EINVAL; 
}

static void sr030pc30_set_skip(void)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;

  int skip_frame = 0;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_skip is called...\n");

  if(sensor->state == SR030PC30_STATE_PREVIEW)
  {
    if(sr030pc30_curr_state == SR030PC30_STATE_PREVIEW)
    {
      skip_frame = 3;
    }
    else
    {
      //wait for overlay creation (250ms ~ 300ms)
      skip_frame = sensor->fps / 3; 
    }
  }
  else
  {
    skip_frame = 1;
  }
  
  dprintk(CAM_INF, SR030PC30_MOD_NAME "skip frame = %d frame\n", skip_frame);

  isp_set_hs_vs(0,skip_frame);
}

static int sr030pc30_set_fps(void)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;
  struct i2c_client *client = sensor->i2c_client; 
  int err=-EINVAL;
  int i = 0;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_fps is called... state = %d\n", sensor->state);

  if(sensor->state != SR030PC30_STATE_CAPTURE)  
  {
    dprintk(CAM_INF, "sr030pc30_set_fps is called... size = %d\n", sensor->preview_size);
    dprintk(CAM_INF, "sr030pc30_set_fps is called... fps = %d\n", sensor->fps);
#if 0
	if(sensor->mode == SR030PC30_MODE_VT)
	{
		switch(sensor->fps)
		{
		case 7:
			err = SR030pc30_regs_write(client, SR030pc30_vt_fps_7, sizeof(SR030pc30_vt_fps_7), "SR030pc30_vt_fps_7");
			break;

		case 10:
			err = SR030pc30_regs_write(client, SR030pc30_vt_fps_10, sizeof(SR030pc30_vt_fps_10), "SR030pc30_vt_fps_10");
			break;
			
		case 15:
			err = SR030pc30_regs_write(client, SR030pc30_vt_fps_15, sizeof(SR030pc30_vt_fps_15), "SR030pc30_vt_fps_15");
			break;

		default:
			 printk(SR030PC30_MOD_NAME "[fps]Invalid value is ordered!!!\n");
			err = 0;
			break;
		}
	}
	else
	{
		switch(sensor->fps)
		{
		case 7:
			err = SR030pc30_regs_write(client, SR030pc30_fps_7, sizeof(SR030pc30_fps_7), "SR030pc30_fps_7");
			break;

		case 10:
			err = SR030pc30_regs_write(client, SR030pc30_fps_10, sizeof(SR030pc30_fps_10), "SR030pc30_fps_10");
			break;
			
		case 15:
			err = SR030pc30_regs_write(client, SR030pc30_fps_15, sizeof(SR030pc30_fps_15), "SR030pc30_fps_15");
			break;

		default:
			 printk(SR030PC30_MOD_NAME "[fps]Invalid value is ordered!!!\n");
			err = 0;
			break;
		}
	}

	if (err < 0)
	{
		goto fps_fail;
		//v4l_info(client, "%s: register set failed\n", __func__);
		//return -EINVAL;
	}
#endif
#if 0
    switch(sensor->fps)
    {
      case 15:
        for (i = 0; i < reg_15fps_index; i++) 
        {
          if(sr030pc30_set_data(client, reg_15fps_table[i]))
            goto fps_fail;
        }            
        break;

		
      case 10:
        for (i = 0; i < reg_10fps_index; i++) 
        {
          if(sr030pc30_set_data(client, reg_10fps_table[i]))
            goto fps_fail;
        }            
        break;
           
      case 7:
        for (i = 0; i < reg_7fps_index; i++) 
        {
          if(sr030pc30_set_data(client, reg_7fps_table[i]))
            goto fps_fail;
        }            
        break;   
            
      default:
        printk(SR030PC30_MOD_NAME "[fps]Invalid value is ordered!!!\n");
        goto fps_fail;
    }
#endif	
  }    

  return 0;

fps_fail:
  printk(SR030PC30_MOD_NAME "sr030pc30_set_fps is failed!!!\n");
  return -EINVAL;   
}

static int sr030pc30_set_ev(s32 value)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;
  struct i2c_client *client = sensor->i2c_client;
  int err=-EINVAL;
  
  //int i,j;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_ev is called... value = %d\n", value);

 if(sensor->mode== SR030PC30_MODE_VT)
	{
		switch(value)
		{	
			case SR030PC30_EV_MINUS_2P0:
				err = SR030pc30_regs_write(client, SR030pc30_ev_vt_m4, sizeof(SR030pc30_ev_vt_m4), "SR030pc30_ev_vt_m4");
			break;

			case SR030PC30_EV_MINUS_1P5:
				err = SR030pc30_regs_write(client, SR030pc30_ev_vt_m3, sizeof(SR030pc30_ev_vt_m3), "SR030pc30_ev_vt_m3");
			break;

			
			case SR030PC30_EV_MINUS_1P0:
				err = SR030pc30_regs_write(client, SR030pc30_ev_vt_m2, sizeof(SR030pc30_ev_vt_m2), "SR030pc30_ev_vt_m2");
			break;
			
			case SR030PC30_EV_MINUS_0P5:
				err = SR030pc30_regs_write(client, SR030pc30_ev_vt_m1, sizeof(SR030pc30_ev_vt_m1), "SR030pc30_ev_vt_m1");
			break;

			case SR030PC30_EV_DEFAULT:
				err = SR030pc30_regs_write(client, SR030pc30_ev_vt_default, sizeof(SR030pc30_ev_vt_default), "SR030pc30_ev_vt_default");
			break;

			case SR030PC30_EV_PLUS_0P5:
				err = SR030pc30_regs_write(client, SR030pc30_ev_vt_p1, sizeof(SR030pc30_ev_vt_p1), "SR030pc30_ev_vt_p1");
 			break;

			case SR030PC30_EV_PLUS_1P0:
				err = SR030pc30_regs_write(client, SR030pc30_ev_vt_p2, sizeof(SR030pc30_ev_vt_p2), "SR030pc30_ev_vt_p2");
 			break;

			case SR030PC30_EV_PLUS_1P5:
				err = SR030pc30_regs_write(client, SR030pc30_ev_vt_p3, sizeof(SR030pc30_ev_vt_p3), "SR030pc30_ev_vt_p3");
 			break;

			case SR030PC30_EV_PLUS_2P0:
				err = SR030pc30_regs_write(client, SR030pc30_ev_vt_p4, sizeof(SR030pc30_ev_vt_p4), "SR030pc30_ev_vt_p4");
 			break;	
			
			default:
				err = SR030pc30_regs_write(client, SR030pc30_ev_vt_default, sizeof(SR030pc30_ev_vt_default), "SR030pc30_ev_vt_default");
 			break;
		}
	}
	else
	{
		switch(value)
		{	
			case SR030PC30_EV_MINUS_2P0:
				err = SR030pc30_regs_write(client, SR030pc30_ev_m4, sizeof(SR030pc30_ev_m4), "SR030pc30_ev_m4");
 			break;

			case SR030PC30_EV_MINUS_1P5:
				err = SR030pc30_regs_write(client, SR030pc30_ev_m3, sizeof(SR030pc30_ev_m3), "SR030pc30_ev_m3");
 			break;
			
			case SR030PC30_EV_MINUS_1P0:
				err = SR030pc30_regs_write(client, SR030pc30_ev_m2, sizeof(SR030pc30_ev_m2), "SR030pc30_ev_m2");
 			break;
			
			case SR030PC30_EV_MINUS_0P5:
				err = SR030pc30_regs_write(client, SR030pc30_ev_m1, sizeof(SR030pc30_ev_m1), "SR030pc30_ev_m1");
 			break;

			case SR030PC30_EV_DEFAULT:
				err = SR030pc30_regs_write(client, SR030pc30_ev_default, sizeof(SR030pc30_ev_default), "SR030pc30_ev_default");
 			break;

			case SR030PC30_EV_PLUS_0P5:
				err = SR030pc30_regs_write(client, SR030pc30_ev_p1, sizeof(SR030pc30_ev_p1), "SR030pc30_ev_p1");
 			break;

			case SR030PC30_EV_PLUS_1P0:
				err = SR030pc30_regs_write(client, SR030pc30_ev_p2, sizeof(SR030pc30_ev_p2), "SR030pc30_ev_p2");
 			break;

			case SR030PC30_EV_PLUS_1P5:
				err = SR030pc30_regs_write(client, SR030pc30_ev_p3, sizeof(SR030pc30_ev_p3), "SR030pc30_ev_p3");
			break;

			case SR030PC30_EV_PLUS_2P0:
				err = SR030pc30_regs_write(client, SR030pc30_ev_p4, sizeof(SR030pc30_ev_p4), "SR030pc30_ev_p4");
			break;	
			
			default:
				err = SR030pc30_regs_write(client, SR030pc30_ev_default, sizeof(SR030pc30_ev_default), "SR030pc30_ev_default");
			break;
		}
	}
	if (err < 0)
	{
		goto ev_fail;
		//v4l_info(client, "%s: register set failed\n", __func__);
		//return -EINVAL;
	}


#if 0
  i = 0;
  j = value;
  if(sr030pc30_set_data(client, reg_ev_table[i]))
    goto ev_fail;
  if(sr030pc30_set_data(client, reg_ev_table[2*j-1]))
    goto ev_fail;
  if(sr030pc30_set_data(client, reg_ev_table[2*j]))
    goto ev_fail;
#endif
  sensor->ev = value;
  
  return 0;
  
ev_fail:
  printk(SR030PC30_MOD_NAME "sr030pc30_set_ev is failed!!!\n");
  return -EINVAL;   
}

#if 0
static int sr030pc30_get_rev(void)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;
  struct i2c_client *client = sensor->i2c_client;

  u8 vendor_id = 0xff, sw_ver = 0xff;
  int ret0 = 0, ret1 = 0, ret2 = 0;

  u8 data[2] = {0xEF, 0x01};
  u8 vender[1] = {0xC5};
  u8 version[1] = {0xC6};

  
  printk("----------------------------------------------------\n");
  printk("   [VGA CAM]   camsensor_sr030pc30_check_sensor_rev\n");
  printk("----------------------------------------------------\n");

  /*===========================================================================
  * there's no way to decide which one to use before sensor revision check,
  * so we use reset-default page number (0x00) without specifying explicitly
  ===========================================================================*/

  ret0 = sr030pc30_i2c_write(client, sizeof(data), data);
  ret1 = sr030pc30_i2c_write_read(client, 1, vender, 1, &vendor_id);
  ret2 = sr030pc30_i2c_write_read(client, 1, version, 1, &sw_ver);

  if (!(ret0 & ret1 & ret2))
  {
      printk(SR030PC30_MOD_NAME"=================================\n");
      printk(SR030PC30_MOD_NAME"   [VGA CAM] vendor_id ID : 0x%x\n", vendor_id);
      printk(SR030PC30_MOD_NAME"   [VGA CAM] software version : 0x%x\n", sw_ver);        
      printk(SR030PC30_MOD_NAME"=================================\n");            

      if(vendor_id == 0xAB && sw_ver == 0x03)
      {
          printk(SR030PC30_MOD_NAME"===============================\n");
          printk(SR030PC30_MOD_NAME"   [VGA CAM] sr030pc30  OK!!\n");
          printk(SR030PC30_MOD_NAME"===============================\n");
      }
      else
      {
          printk(SR030PC30_MOD_NAME"==========================================\n");
          printk(SR030PC30_MOD_NAME"   [VGA CAM] camsemsor operation fail!!\n");
          printk(SR030PC30_MOD_NAME"===========================================\n");
          return -EINVAL;
      }
  }
  else
  {
      printk(SR030PC30_MOD_NAME"-------------------------------------------------\n");
      printk(SR030PC30_MOD_NAME"   [VGA CAM] sensor reset failure detected!!\n");
      printk(SR030PC30_MOD_NAME"-------------------------------------------------\n");
      goto get_rev_fail;
  }
 
  return 0;

get_rev_fail:
  printk(SR030PC30_MOD_NAME "sr030pc30_get_rev is failed!!!\n");
  return -EINVAL;   
}


static int sr030pc30_set_table (void)
{
  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_table is called...\n");

    reg_init_qcif_index = 0;
    reg_init_cif_index = 0;
    reg_init_qvga_index = 0;
    reg_init_vga_index = 0;
    reg_init_qcif_vt_index = 0;
    reg_init_cif_vt_index = 0;
    reg_init_qvga_vt_index = 0;
    reg_init_vga_vt_index = 0;
    reg_wb_auto_index = 0;
    reg_wb_daylight_index = 0;
    reg_wb_cloudy_index = 0;
    reg_wb_incandescent_index = 0;
    reg_wb_fluorescent_index = 0;
    reg_ev_index = 0;
    reg_ev_vt_index = 0;
	reg_contrast_level_m5_index = 0;
	reg_contrast_level_m4_index = 0;
	reg_contrast_level_m3_index = 0;
	reg_contrast_level_m2_index = 0;
	reg_contrast_level_m1_index = 0;
	reg_contrast_level_default_index = 0;
	reg_contrast_level_p1_index = 0;
	reg_contrast_level_p2_index = 0;
	reg_contrast_level_p3_index = 0;
	reg_contrast_level_p4_index = 0;
	reg_contrast_level_p5_index = 0;
    reg_effect_none_index = 0;
    reg_effect_red_index = 0;
    reg_effect_gray_index = 0;
    reg_effect_sepia_index = 0;
    reg_effect_green_index = 0;
    reg_effect_aqua_index = 0;
    reg_effect_negative_index = 0;
    reg_flip_none_index = 0;
    reg_flip_water_index = 0;
    reg_flip_mirror_index = 0;
    reg_flip_water_mirror_index = 0;
    reg_pretty_none_index = 0;
    reg_pretty_level1_index = 0;
    reg_pretty_level2_index = 0;
    reg_pretty_level3_index = 0;
    reg_pretty_vt_none_index = 0;
    reg_pretty_vt_level1_index = 0;
    reg_pretty_vt_level2_index = 0;
    reg_pretty_vt_level3_index = 0;
    reg_7fps_index = 0;
    reg_10fps_index = 0;
    reg_15fps_index = 0;    
	reg_self_capture_index = 0;

#if !(IS_USE_REGISTER_CONFIGURE_FILE_LSI)

    /* Section Index */
    reg_init_qcif_index = sizeof(reg_init_qcif_table)/sizeof(u32);
    reg_init_cif_index = sizeof(reg_init_cif_table)/sizeof(u32);
    reg_init_qvga_index = sizeof(reg_init_qvga_table)/sizeof(u32);
    reg_init_vga_index = sizeof(reg_init_vga_table)/sizeof(u32);
    reg_init_qcif_vt_index = sizeof(reg_init_qcif_vt_table)/sizeof(u32);
    reg_init_cif_vt_index = sizeof(reg_init_cif_vt_table)/sizeof(u32);
    reg_init_qvga_vt_index = sizeof(reg_init_qvga_vt_table)/sizeof(u32);
    reg_init_vga_vt_index = sizeof(reg_init_vga_vt_table)/sizeof(u32);
    reg_ev_index = sizeof(reg_ev_table)/sizeof(u32);
    reg_ev_vt_index = sizeof(reg_ev_vt_table)/sizeof(u32);    
	reg_contrast_level_m5_index = sizeof(reg_contrast_level_m5_table)/sizeof(u32);
	reg_contrast_level_m4_index = sizeof(reg_contrast_level_m4_table)/sizeof(u32);
	reg_contrast_level_m3_index = sizeof(reg_contrast_level_m3_table)/sizeof(u32);
	reg_contrast_level_m2_index = sizeof(reg_contrast_level_m2_table)/sizeof(u32);
	reg_contrast_level_m1_index = sizeof(reg_contrast_level_m1_table)/sizeof(u32);
	reg_contrast_level_default_index = sizeof(reg_contrast_level_default_table)/sizeof(u32);
	reg_contrast_level_p1_index = sizeof(reg_contrast_level_p1_table)/sizeof(u32);
	reg_contrast_level_p2_index = sizeof(reg_contrast_level_p2_table)/sizeof(u32);
	reg_contrast_level_p3_index = sizeof(reg_contrast_level_p3_table)/sizeof(u32);
	reg_contrast_level_p4_index = sizeof(reg_contrast_level_p4_table)/sizeof(u32);
	reg_contrast_level_p5_index = sizeof(reg_contrast_level_p5_table)/sizeof(u32);
    reg_wb_auto_index = sizeof(reg_wb_auto_table)/sizeof(u32);
    reg_wb_daylight_index = sizeof(reg_wb_daylight_table)/sizeof(u32);
    reg_wb_cloudy_index = sizeof(reg_wb_cloudy_table)/sizeof(u32);
    reg_wb_incandescent_index = sizeof(reg_wb_incandescent_table)/sizeof(u32);
    reg_wb_fluorescent_index = sizeof(reg_wb_fluorescent_table)/sizeof(u32);
    reg_effect_none_index = sizeof(reg_effect_none_table)/sizeof(u32);
    reg_effect_gray_index = sizeof(reg_effect_gray_table)/sizeof(u32);
    reg_effect_red_index = sizeof(reg_effect_red_table)/sizeof(u32);    
    reg_effect_sepia_index = sizeof(reg_effect_sepia_table)/sizeof(u32);
    reg_effect_green_index = sizeof(reg_effect_green_table)/sizeof(u32);
    reg_effect_aqua_index = sizeof(reg_effect_aqua_table)/sizeof(u32);
    reg_effect_negative_index = sizeof(reg_effect_negative_table)/sizeof(u32);
    reg_flip_none_index = sizeof(reg_flip_none_table)/sizeof(u32);
    reg_flip_water_index = sizeof(reg_flip_water_table)/sizeof(u32);
    reg_flip_mirror_index = sizeof(reg_flip_mirror_table)/sizeof(u32);
    reg_flip_water_mirror_index = sizeof(reg_flip_water_mirror_table)/sizeof(u32);
    reg_pretty_none_index = sizeof(reg_pretty_none_table)/sizeof(u32);
    reg_pretty_level1_index = sizeof(reg_pretty_level1_table)/sizeof(u32);
    reg_pretty_level2_index = sizeof(reg_pretty_level2_table)/sizeof(u32);
    reg_pretty_level3_index = sizeof(reg_pretty_level3_table)/sizeof(u32);   
    reg_pretty_vt_none_index = sizeof(reg_pretty_vt_none_table)/sizeof(u32);
    reg_pretty_vt_level1_index = sizeof(reg_pretty_vt_level1_table)/sizeof(u32);
    reg_pretty_vt_level2_index = sizeof(reg_pretty_vt_level2_table)/sizeof(u32);
    reg_pretty_vt_level3_index = sizeof(reg_pretty_vt_level3_table)/sizeof(u32);
    reg_7fps_index = sizeof(reg_7fps_table)/sizeof(u32);
    reg_10fps_index = sizeof(reg_10fps_table)/sizeof(u32);
    reg_15fps_index = sizeof(reg_15fps_table)/sizeof(u32);    
	reg_self_capture_index = sizeof(reg_self_capture_table)/sizeof(u32);  

#else

    memset(&reg_init_qcif_table, 0, sizeof(reg_init_qcif_table));
    memset(&reg_init_cif_table, 0, sizeof(reg_init_cif_table));
    memset(&reg_init_qvga_table, 0, sizeof(reg_init_qvga_table));
    memset(&reg_init_vga_table, 0, sizeof(reg_init_vga_table));
    memset(&reg_init_qcif_vt_table, 0, sizeof(reg_init_qcif_vt_table));
    memset(&reg_init_cif_vt_table, 0, sizeof(reg_init_cif_vt_table));
    memset(&reg_init_qvga_vt_table, 0, sizeof(reg_init_qvga_vt_table));
    memset(&reg_init_vga_vt_table, 0, sizeof(reg_init_vga_vt_table));
    memset(&reg_wb_auto_table, 0, sizeof(reg_wb_auto_table));
    memset(&reg_wb_daylight_table, 0, sizeof(reg_wb_daylight_table));
    memset(&reg_wb_cloudy_table, 0, sizeof(reg_wb_cloudy_table));
    memset(&reg_wb_incandescent_table, 0, sizeof(reg_wb_incandescent_table));
    memset(&reg_wb_fluorescent_table, 0, sizeof(reg_wb_fluorescent_table));
    memset(&reg_ev_table, 0, sizeof(reg_ev_table));
    memset(&reg_ev_vt_table, 0, sizeof(reg_ev_vt_table));
	memset(&reg_contrast_level_m5_table, 0, sizeof(reg_contrast_level_m5_table));
	memset(&reg_contrast_level_m4_table, 0, sizeof(reg_contrast_level_m4_table));
	memset(&reg_contrast_level_m3_table, 0, sizeof(reg_contrast_level_m3_table));
	memset(&reg_contrast_level_m2_table, 0, sizeof(reg_contrast_level_m2_table));
	memset(&reg_contrast_level_m1_table, 0, sizeof(reg_contrast_level_m1_table));
	memset(&reg_contrast_level_default_table, 0, sizeof(reg_contrast_level_default_table));
	memset(&reg_contrast_level_p1_table, 0, sizeof(reg_contrast_level_p1_table));
	memset(&reg_contrast_level_p2_table, 0, sizeof(reg_contrast_level_p2_table));
	memset(&reg_contrast_level_p3_table, 0, sizeof(reg_contrast_level_p3_table));
	memset(&reg_contrast_level_p4_table, 0, sizeof(reg_contrast_level_p4_table));
	memset(&reg_contrast_level_p5_table, 0, sizeof(reg_contrast_level_p5_table));
    memset(&reg_effect_none_table, 0, sizeof(reg_effect_none_table));
    memset(&reg_effect_gray_table, 0, sizeof(reg_effect_gray_table));
    memset(&reg_effect_red_table, 0, sizeof(reg_effect_red_table));    
    memset(&reg_effect_sepia_table, 0, sizeof(reg_effect_sepia_table));
    memset(&reg_effect_green_table, 0, sizeof(reg_effect_green_table));
    memset(&reg_effect_aqua_table, 0, sizeof(reg_effect_aqua_table));
    memset(&reg_effect_negative_table, 0, sizeof(reg_effect_negative_table));
    memset(&reg_flip_none_table, 0, sizeof(reg_flip_none_table));
    memset(&reg_flip_water_table, 0, sizeof(reg_flip_water_table));
    memset(&reg_flip_mirror_table, 0, sizeof(reg_flip_mirror_table));
    memset(&reg_flip_water_mirror_table, 0, sizeof(reg_flip_water_mirror_table));
    memset(&reg_pretty_none_table, 0, sizeof(reg_pretty_none_table));
    memset(&reg_pretty_level1_table, 0, sizeof(reg_pretty_level1_table));
    memset(&reg_pretty_level2_table, 0, sizeof(reg_pretty_level2_table));
    memset(&reg_pretty_level3_table, 0, sizeof(reg_pretty_level3_table));
    memset(&reg_pretty_vt_none_table, 0, sizeof(reg_pretty_vt_none_table));
    memset(&reg_pretty_vt_level1_table, 0, sizeof(reg_pretty_vt_level1_table));
    memset(&reg_pretty_vt_level2_table, 0, sizeof(reg_pretty_vt_level2_table));
    memset(&reg_pretty_vt_level3_table, 0, sizeof(reg_pretty_vt_level3_table));  
    memset(&reg_7fps_table, 0, sizeof(reg_7fps_table));  
    memset(&reg_10fps_table, 0, sizeof(reg_10fps_table)); 
    memset(&reg_15fps_table, 0, sizeof(reg_15fps_table)); 
	memset(&reg_self_capture_table, 0, sizeof(reg_self_capture_table)); 
    
#endif    

  return 0;
}
#endif

static int sr030pc30_set_init(void)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;
  struct i2c_client *client = sensor->i2c_client;

  int i = 0;
 
 int err = -EINVAL;


  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_init is called...\n");
  
  //sr030pc30_set_table();

//#if (IS_USE_REGISTER_CONFIGURE_FILE_LSI)
//  sr030pc30_make_table();
//#endif

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_init vga mode = %d\n", sensor->mode);
  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_init preview size = %d\n", sensor->preview_size);
  dprintk(CAM_INF, SR030PC30_MOD_NAME "sensor->check_dataline : %d \n", sensor->check_dataline);

  if(sensor->mode == SR030PC30_MODE_VT)
  {
 #if 0 
    switch(sensor->preview_size)
    {
      case SR030PC30_PREVIEW_SIZE_640_480:
        for (i = 0; i < reg_init_vga_vt_index; i++) 
        {
          if(sr030pc30_set_data(client, reg_init_vga_vt_table[i]))
            goto init_fail;
        }
        break;

      case SR030PC30_PREVIEW_SIZE_320_240:
        for (i = 0; i < reg_init_qvga_vt_index; i++) 
        {
          if(sr030pc30_set_data(client, reg_init_qvga_vt_table[i]))
            goto init_fail;
        }
        break;

      case SR030PC30_PREVIEW_SIZE_352_288:
        for (i = 0; i < reg_init_cif_vt_index; i++) 
        {
          if(sr030pc30_set_data(client, reg_init_cif_vt_table[i]))
            goto init_fail;
        }
        break;

      case SR030PC30_PREVIEW_SIZE_176_144:
        for (i = 0; i < reg_init_qcif_vt_index; i++) 
        {
          if(sr030pc30_set_data(client, reg_init_qcif_vt_table[i]))
            goto init_fail;
        }
        break;        

      default:
        printk(SR030PC30_MOD_NAME "[size]Invalid value is ordered!!!\n");
        goto init_fail;
    }
#endif	
  }
  else
  {	
	if(sensor->check_dataline)
	{
		//for (i = 0; i < 5; i++) {
		//	if(sr030pc30_set_data(client, sr030pc30_dataline[i]))
		//		goto init_fail;
		//}	
		err = SR030pc30_i2c_write(client, SR030pc30_dataline, \
						sizeof(SR030pc30_dataline));
		if (err < 0)
		{
			v4l_info(client, "%s: register set failed\n", \
			__func__);
		}
		sensor->check_dataline = 0;
		return 0;
	}
	
    switch(sensor->preview_size)
    {

      case SR030PC30_PREVIEW_SIZE_640_480:
	  	err = SR030pc30_regs_write(client, SR030pc30_preview_640x480, sizeof(SR030pc30_preview_640x480), "SR030pc30_preview_640x480");
        //for (i = 0; i < reg_init_vga_index; i++) 
       // {
       //   if(sr030pc30_set_data(client, reg_init_vga_table[i]))
       //     goto init_fail;
       // }
        break;

      case SR030PC30_PREVIEW_SIZE_320_240:
	  	err = SR030pc30_regs_write(client, SR030pc30_preview_320x240, sizeof(SR030pc30_preview_320x240), "SR030pc30_preview_320x240");    
        //for (i = 0; i < reg_init_qvga_index; i++) 
        //{
       //   if(sr030pc30_set_data(client, reg_init_qvga_table[i]))
       //     goto init_fail;
       // }
        break;

      case SR030PC30_PREVIEW_SIZE_352_288:
#if 0	  	
        for (i = 0; i < reg_init_cif_index; i++) 
        {
          if(sr030pc30_set_data(client, reg_init_cif_table[i]))
            goto init_fail;
        }
#endif		
        break;

      case SR030PC30_PREVIEW_SIZE_176_144:
#if 0	  	
        for (i = 0; i < reg_init_qcif_index; i++) 
        {
          if(sr030pc30_set_data(client, reg_init_qcif_table[i]))
            goto init_fail;
        }
#endif			
        break;        

      default:
        printk(SR030PC30_MOD_NAME "[size]Invalid value is ordered!!!\n");
        goto init_fail;
    }    
  }

  sr030pc30_set_fps();
  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_ev = %d\n", sensor->ev);  
  sr030pc30_set_ev(sensor->ev);  

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_init success...\n");

  return 0;

init_fail:
  printk(SR030PC30_MOD_NAME "sr030pc30_set_init is failed!!!\n");
  return -EINVAL;     
}

static int sr030pc30_set_mode(s32 value)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_mode is called... mode = %d\n", value);  
  
  sensor->mode = value;
  
  return 0;
}

static int sr030pc30_set_state(s32 value)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_state is called... state = %d\n", value);  
  
  sensor->state = value;
  
  return 0;
}

static int sr030pc30_set_zoom(s32 value)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;

  sensor->zoom = value;

  return 0;
}

static int sr030pc30_set_effect(s32 value)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;
  struct i2c_client *client = sensor->i2c_client;
 int err=-EINVAL;
  //int i;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_effect is called... effect = %d\n", value);
  
  switch(value)
  {
  	case SR030PC30_EFFECT_OFF:
		err = SR030pc30_regs_write(client, SR030pc30_effect_none, sizeof(SR030pc30_effect_none), "SR030pc30_effect_none");
		break;

	case SR030PC30_EFFECT_GREY:		//Gray
		err = SR030pc30_regs_write(client, SR030pc30_effect_gray, sizeof(SR030pc30_effect_gray), "SR030pc30_effect_gray");
		break;

	case SR030PC30_EFFECT_SEPIA:
		err = SR030pc30_regs_write(client, SR030pc30_effect_sepia, sizeof(SR030pc30_effect_sepia), "SR030pc30_effect_sepia");
		break;

	case SR030PC30_EFFECT_AQUA:
		err = SR030pc30_regs_write(client, SR030pc30_effect_aqua, sizeof(SR030pc30_effect_aqua), "SR030pc30_effect_aqua");
		break;

	case SR030PC30_EFFECT_NEGATIVE:
		err = SR030pc30_regs_write(client, SR030pc30_effect_negative, sizeof(SR030pc30_effect_negative), "SR030pc30_effect_negative");
		break;
  
#if 0
  
    case SR030PC30_EFFECT_OFF:
      for (i = 0; i < reg_effect_none_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_effect_none_table[i]))
          goto effect_fail;
      }
      break;

    case SR030PC30_EFFECT_BW:
      break;    

    case SR030PC30_EFFECT_GREY:
      for (i = 0; i < reg_effect_gray_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_effect_gray_table[i]))
          goto effect_fail;
      }      
      break;      
      
    case SR030PC30_EFFECT_SEPIA:
      for (i = 0; i < reg_effect_sepia_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_effect_sepia_table[i]))
          goto effect_fail;
      }        
      break;

    case SR030PC30_EFFECT_SHARPEN:
      break;      
      
    case SR030PC30_EFFECT_NEGATIVE:
      for (i = 0; i < reg_effect_negative_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_effect_negative_table[i]))
          goto effect_fail;
      }      
      break;
      
    case SR030PC30_EFFECT_ANTIQUE:
      break;
      
    case SR030PC30_EFFECT_AQUA:
      for (i = 0; i < reg_effect_aqua_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_effect_aqua_table[i]))
          goto effect_fail;
      }      
      break;

    case SR030PC30_EFFECT_RED:
      for (i = 0; i < reg_effect_red_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_effect_red_table[i]))
          goto effect_fail;
      }      
      break; 

    case SR030PC30_EFFECT_PINK:
      break; 

    case SR030PC30_EFFECT_YELLOW:
      break;       

    case SR030PC30_EFFECT_GREEN:
      for (i = 0; i < reg_effect_green_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_effect_green_table[i]))
          goto effect_fail;
      }      
      break; 

    case SR030PC30_EFFECT_BLUE:
      break; 

    case SR030PC30_EFFECT_PURPLE:
      break;
#endif	  
      
    default:
      printk(SR030PC30_MOD_NAME "[Effect]Invalid value is ordered!!!\n");
      goto effect_fail;
  }

   if (err < 0)
  {
	goto effect_fail;
  }

  sensor->effect = value;
  
  return 0;

effect_fail:
  printk(SR030PC30_MOD_NAME "sr030pc30_set_effect is failed!!!\n");
  return -EINVAL;       
}

static int sr030pc30_set_contrast(s32 value)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;
  struct i2c_client *client = sensor->i2c_client;
  
  int i;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_contrast is called... value = %d\n", value);

  sensor->contrast = value;
 #if 0 
  switch(sensor->contrast)
  {
    case SR030PC30_CONTRAST_MINUS_5:
      for (i = 0; i < reg_contrast_level_m5_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_m5_table[i]);
      }       
      break;
      
    case SR030PC30_CONTRAST_MINUS_4:
      for (i = 0; i < reg_contrast_level_m4_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_m4_table[i]);
      }       
      break;
      
    case SR030PC30_CONTRAST_MINUS_3:
      for (i = 0; i < reg_contrast_level_m3_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_m3_table[i]);
      }       
      break;
      
    case SR030PC30_CONTRAST_MINUS_2:
      for (i = 0; i < reg_contrast_level_m2_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_m2_table[i]);
      }       
      break;
      
    case SR030PC30_CONTRAST_MINUS_1:
      for (i = 0; i < reg_contrast_level_m1_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_m1_table[i]);
      }       
      break;

    case SR030PC30_CONTRAST_DEFAULT:
      for (i = 0; i < reg_contrast_level_default_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_default_table[i]);
      }       
      break;

    case SR030PC30_CONTRAST_PLUS_1:
      for (i = 0; i < reg_contrast_level_p1_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_p1_table[i]);
      }       
      break;
      
    case SR030PC30_CONTRAST_PLUS_2:
      for (i = 0; i < reg_contrast_level_p2_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_p2_table[i]);
      }       
      break;

    case SR030PC30_CONTRAST_PLUS_3:
      for (i = 0; i < reg_contrast_level_p3_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_p3_table[i]);
      }       
      break;

    case SR030PC30_CONTRAST_PLUS_4:
      for (i = 0; i < reg_contrast_level_p4_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_p4_table[i]);
      }       
      break;

    case SR030PC30_CONTRAST_PLUS_5:
      for (i = 0; i < reg_contrast_level_p5_index; i++) 
      {
        sr030pc30_set_data(client, reg_contrast_level_p5_table[i]);
      }       
      break;

    default:
      printk(SR030PC30_MOD_NAME "[WB]Invalid value is ordered!!!\n");
      return -EINVAL;
  }
#endif
  return 0;
}


static int sr030pc30_set_wb(s32 value)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;
  struct i2c_client *client = sensor->i2c_client;
  int err=-EINVAL;
  //int i;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_wb is called... value = %d\n",value);
  
  switch(value)
  {   
 
  	case SR030PC30_WB_AUTO:
		err = SR030pc30_regs_write(client, SR030pc30_wb_auto, sizeof(SR030pc30_wb_auto), "SR030pc30_wb_auto");
		break;

	case SR030PC30_WB_DAYLIGHT:
		err = SR030pc30_regs_write(client, SR030pc30_wb_sunny, sizeof(SR030pc30_wb_sunny), "SR030pc30_wb_sunny");
		break;

	case SR030PC30_WB_CLOUDY:
		err = SR030pc30_regs_write(client, SR030pc30_wb_cloudy, sizeof(SR030pc30_wb_cloudy), "SR030pc30_wb_cloudy");
		break;

	case SR030PC30_WB_INCANDESCENT:
		err = SR030pc30_regs_write(client, SR030pc30_wb_tungsten, sizeof(SR030pc30_wb_tungsten), "SR030pc30_wb_tungsten");
		break;

	case SR030PC30_WB_FLUORESCENT:
		err = SR030pc30_regs_write(client, SR030pc30_wb_fluorescent, sizeof(SR030pc30_wb_fluorescent), "SR030pc30_wb_fluorescent");
		break;
#if 0
    case SR030PC30_WB_AUTO:
		
      for (i = 0; i < reg_wb_auto_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_wb_auto_table[i]))
          goto wb_fail;
      }       
      break;
      
    case SR030PC30_WB_DAYLIGHT:
      for (i = 0; i < reg_wb_daylight_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_wb_daylight_table[i]))
          goto wb_fail;
      }       
      break;
      
    case SR030PC30_WB_INCANDESCENT:
      for (i = 0; i < reg_wb_incandescent_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_wb_incandescent_table[i]))
          goto wb_fail;
      }       
      break;
      
    case SR030PC30_WB_FLUORESCENT:
      for (i = 0; i < reg_wb_fluorescent_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_wb_fluorescent_table[i]))
          goto wb_fail;
      }       
      break;
      
    case SR030PC30_WB_CLOUDY:
      for (i = 0; i < reg_wb_cloudy_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_wb_cloudy_table[i]))
          goto wb_fail;
      }       
      break;
#endif      
    default:
      printk(SR030PC30_MOD_NAME "[WB]Invalid value is ordered!!!\n");
      goto wb_fail;
  }

  if (err < 0)
  {
	goto wb_fail;
  }

  sensor->wb = value;

  return 0;

wb_fail:
  printk(SR030PC30_MOD_NAME "sr030pc30_set_wb is failed!!!\n");
  return -EINVAL;   
}

static int sr030pc30_set_pretty(s32 value)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;
  struct i2c_client *client = sensor->i2c_client;

  int i;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_set_pretty is called... value = %d\n", value);
#if 0
  switch(value)
  {
    case SR030PC30_PRETTY_NONE:
      for (i = 0; i < reg_pretty_none_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_pretty_none_table[i]))
          goto pretty_fail;
      }       
      break;
      
    case SR030PC30_PRETTY_LEVEL1:
      for (i = 0; i < reg_pretty_level1_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_pretty_level1_table[i]))
          goto pretty_fail;
      }       
      break;
      
    case SR030PC30_PRETTY_LEVEL2:
      for (i = 0; i < reg_pretty_level2_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_pretty_level2_table[i]))
          goto pretty_fail;
      }       
      break;
      
    case SR030PC30_PRETTY_LEVEL3:
      for (i = 0; i < reg_pretty_level3_index; i++) 
      {
        if(sr030pc30_set_data(client, reg_pretty_level3_table[i]))
          goto pretty_fail;
      }       
      break;
      
    default:
      printk(SR030PC30_MOD_NAME "[Pretty]Invalid value is ordered!!!\n");
      goto pretty_fail;
  }
#endif
  sensor->pretty = value;

  return 0;

pretty_fail:
  printk(SR030PC30_MOD_NAME "sr030pc30_set_pretty is failed!!!\n");
  return -EINVAL;     
}

#if 0
#if (IS_USE_REGISTER_CONFIGURE_FILE_LSI)

int parsing_section;

static u32 sr030pc30_util_hex_val(char hex)
{
  if ( hex >= 'a' && hex <= 'f' )
  {
    return (hex-'a'+10);
  }
  else if ( hex >= 'A' && hex <= 'F' )
  {
    return (hex - 'A' + 10 );
  }
  else if ( hex >= '0' && hex <= '9' )
  {
    return (hex - '0');
  }
  else
  {
    return 0;
  }
}

static u32 sr030pc30_util_gets(char* buffer, char* line, int is_start)
{
  int          i;
  char*        _r_n_ptr;
  static char* buffer_ptr;

  memset(line, 0, 1024);

  if ( is_start )
      buffer_ptr = buffer;

  _r_n_ptr = strstr(buffer_ptr, "\r\n");

  //\n  
  if ( _r_n_ptr )
  {
    for ( i = 0 ; ; i++ )
    {
      if ( buffer_ptr+i == _r_n_ptr )
      {
        buffer_ptr = _r_n_ptr+1;
        break;
      }
      line[i] = buffer_ptr[i];
    }
    line[i] = '\0';

    return 1;
  }
  //\n  
  else
  {
    if ( strlen(buffer_ptr) > 0 )
    {
      strcpy(line, buffer_ptr);
      return 0;
    }
    else
    {
      return 0;
    }
  }
}

static u32 sr030pc30_util_atoi(char* str)
{
  unsigned int i,j=0;
  unsigned int val_len;
  unsigned int ret_val=0;

  if (str == NULL)
      return 0;

  //decimal
  if(strlen(str) <= 4 || (strstr(str, "0x")==NULL && strstr(str, "0X")==NULL ))
  {
    for( ; ; str++ ) {
      switch( *str ) {
        case '0'...'9':
          ret_val= 10 * ret_val + ( *str - '0' ) ;
          break ;
        default:
          break ;
      }
    }

    return ret_val;
  }

  //hex ex:0xa0c
  val_len = strlen(str);

  for (i = val_len-1 ; i >= 2 ; i--)
  {
    ret_val = ret_val + (sr030pc30_util_hex_val(str[i])<<(j*4));
    j++;
  }

  return ret_val;
}

static int sr030pc30_util_trim(char* buff)
{
  int         left_index;
  int         right_index;
  int         buff_len;
  int         i;

  buff_len	= strlen(buff);
  left_index	= -1;
  right_index = -1;

  if ( buff_len == 0 )
  {
    return 0;
  }

  /* left index(  white space  ) */
  for ( i = 0 ; i < buff_len ; i++ )
  {
    if ( buff[i] != ' ' && buff[i] != '\t' && buff[i] != '\n' && buff[i] != '\r')
    {
      left_index = i;
      break;
    }
  }

  /* right index(  white space  ) */
  for ( i = buff_len-1 ; i >= 0 ; i-- )
  {
    if ( buff[i] != ' ' && buff[i] != '\t' && buff[i] != '\n' && buff[i] != '\r')
    {
      right_index = i;
      buff[i+1] = '\0';
      break;
    }
  }

  if ( left_index == -1 && right_index == -1 )
  {
    strcpy(buff, "");
  }
  else if ( left_index <= right_index )
  {
    strcpy(buff, buff+left_index);
  }
  else
  {
    return -EINVAL;
  }

  return 0;
}


static u32 sr030pc30_insert_register_table(char* line)
{
  int   i;
  char  reg_val_str[7];
  int   reg_val_str_idx=0;

  unsigned int  reg_val;

  sr030pc30_util_trim(line);
  
  if ( strlen(line) == 0 || (line[0] == '/' && line[1] == '/' ) || (line[0] == '/' && line[1] == '*' ) || line[0] == '{' || line[0] == '}' )
  {
    return 0;
  }

  for (i = 0 ; ; i++)
  {
    if ( line[i] == ' ' || line[i] == '\t' || line[i] == '/' || line[i] == '\0')
      continue;

    if ( line[i] == ',' )
      break;

      reg_val_str[reg_val_str_idx++] = line[i];
  }

  reg_val_str[reg_val_str_idx] = '\0';

  reg_val = sr030pc30_util_atoi(reg_val_str);

  if      ( parsing_section == REG_INIT_QCIF_SECTION)         reg_init_cif_table[reg_init_qcif_index++] = reg_val;
  else if ( parsing_section == REG_INIT_CIF_SECTION)          reg_init_qcif_table[reg_init_cif_index++] = reg_val;
  else if ( parsing_section == REG_INIT_QVGA_SECTION)         reg_init_qvga_table[reg_init_qvga_index++] = reg_val;
  else if ( parsing_section == REG_INIT_VGA_SECTION)          reg_init_vga_table[reg_init_vga_index++] = reg_val;
  else if ( parsing_section == REG_INIT_QCIF_VT_SECTION)      reg_init_qcif_vt_table[reg_init_qcif_vt_index++] = reg_val;
  else if ( parsing_section == REG_INIT_CIF_VT_SECTION)       reg_init_cif_vt_table[reg_init_cif_vt_index++] = reg_val;
  else if ( parsing_section == REG_INIT_QVGA_VT_SECTION)      reg_init_qvga_vt_table[reg_init_qvga_vt_index++] = reg_val;
  else if ( parsing_section == REG_INIT_VGA_VT_SECTION)       reg_init_vga_vt_table[reg_init_vga_vt_index++] = reg_val;
  else if ( parsing_section == REG_WB_AUTO_SECTION)           reg_wb_auto_table[reg_wb_auto_index++] = reg_val;
  else if ( parsing_section == REG_WB_DAYLIGHT_SECTION)       reg_wb_daylight_table[reg_wb_daylight_index++] = reg_val;
  else if ( parsing_section == REG_WB_CLOUDY_SECTION)         reg_wb_cloudy_table[reg_wb_cloudy_index++] = reg_val;
  else if ( parsing_section == REG_WB_INCANDESCENT_SECTION)   reg_wb_incandescent_table[reg_wb_incandescent_index++] = reg_val;
  else if ( parsing_section == REG_WB_FLUORESCENT_SECTION)    reg_wb_fluorescent_table[reg_wb_fluorescent_index++] = reg_val;
  else if ( parsing_section == REG_EV_SECTION)                reg_ev_table[reg_ev_index++] = reg_val;
  else if ( parsing_section == REG_EV_VT_SECTION)             reg_ev_vt_table[reg_ev_vt_index++]	= reg_val;    
  else if ( parsing_section == REG_CONTRAST_LEVEL_M5_SECTION) reg_contrast_level_m5_table[reg_contrast_level_m5_index++] = reg_val;
  else if ( parsing_section == REG_CONTRAST_LEVEL_M4_SECTION) reg_contrast_level_m4_table[reg_contrast_level_m4_index++] = reg_val;
  else if ( parsing_section == REG_CONTRAST_LEVEL_M3_SECTION) reg_contrast_level_m3_table[reg_contrast_level_m3_index++] = reg_val;
  else if ( parsing_section == REG_CONTRAST_LEVEL_M2_SECTION) reg_contrast_level_m2_table[reg_contrast_level_m2_index++] = reg_val;
  else if ( parsing_section == REG_CONTRAST_LEVEL_M1_SECTION) reg_contrast_level_m1_table[reg_contrast_level_m1_index++] = reg_val;
  else if ( parsing_section == REG_CONTRAST_DEFAULT_SECTION) reg_contrast_level_default_table[reg_contrast_level_default_index++] = reg_val;
  else if ( parsing_section == REG_CONTRAST_LEVEL_P1_SECTION) reg_contrast_level_p1_table[reg_contrast_level_p1_index++] = reg_val;
  else if ( parsing_section == REG_CONTRAST_LEVEL_P2_SECTION) reg_contrast_level_p2_table[reg_contrast_level_p2_index++] = reg_val;
  else if ( parsing_section == REG_CONTRAST_LEVEL_P3_SECTION) reg_contrast_level_p3_table[reg_contrast_level_p3_index++] = reg_val;
  else if ( parsing_section == REG_CONTRAST_LEVEL_P4_SECTION) reg_contrast_level_p4_table[reg_contrast_level_p4_index++] = reg_val;
  else if ( parsing_section == REG_CONTRAST_LEVEL_P5_SECTION) reg_contrast_level_p5_table[reg_contrast_level_p5_index++] = reg_val;
  else if ( parsing_section == REG_EFFECT_NONE_SECTION)       reg_effect_none_table[reg_effect_none_index++] = reg_val;
  else if ( parsing_section == REG_EFFECT_GRAY_SECTION)       reg_effect_gray_table[reg_effect_gray_index++] = reg_val;
  else if ( parsing_section == REG_EFFECT_RED_SECTION)        reg_effect_red_table[reg_effect_red_index++] = reg_val;    
  else if ( parsing_section == REG_EFFECT_SEPIA_SECTION)      reg_effect_sepia_table[reg_effect_sepia_index++] = reg_val;
  else if ( parsing_section == REG_EFFECT_GREEN_SECTION)      reg_effect_green_table[reg_effect_green_index++] = reg_val;
  else if ( parsing_section == REG_EFFECT_AQUA_SECTION)       reg_effect_aqua_table[reg_effect_aqua_index++] = reg_val;
  else if ( parsing_section == REG_EFFECT_NEGATIVE_SECTION)   reg_effect_negative_table[reg_effect_negative_index++] = reg_val;
  else if ( parsing_section == REG_FLIP_NONE_SECTION)         reg_flip_none_table[reg_flip_none_index++] = reg_val;
  else if ( parsing_section == REG_FLIP_WATER_SECTION)        reg_flip_water_table[reg_flip_water_index++] = reg_val;
  else if ( parsing_section == REG_FLIP_MIRROR_SECTION)       reg_flip_mirror_table[reg_flip_mirror_index++] = reg_val;
  else if ( parsing_section == REG_FLIP_WATER_MIRROR_SECTION) reg_flip_water_mirror_table[reg_flip_water_mirror_index++] = reg_val;
  else if ( parsing_section == REG_PRETTY_NONE_SECTION)       reg_pretty_none_table[reg_pretty_none_index++] = reg_val;
  else if ( parsing_section == REG_PRETTY_LEVEL1_SECTION)     reg_pretty_level1_table[reg_pretty_level1_index++] = reg_val;
  else if ( parsing_section == REG_PRETTY_LEVEL2_SECTION)     reg_pretty_level2_table[reg_pretty_level2_index++] = reg_val;
  else if ( parsing_section == REG_PRETTY_LEVEL3_SECTION)     reg_pretty_level3_table[reg_pretty_level3_index++] = reg_val;
  else if ( parsing_section == REG_PRETTY_VT_NONE_SECTION)    reg_pretty_vt_none_table[reg_pretty_vt_none_index++] = reg_val;
  else if ( parsing_section == REG_PRETTY_VT_LEVEL1_SECTION)  reg_pretty_vt_level1_table[reg_pretty_vt_level1_index++] = reg_val;
  else if ( parsing_section == REG_PRETTY_VT_LEVEL2_SECTION)  reg_pretty_vt_level2_table[reg_pretty_vt_level2_index++] = reg_val;
  else if ( parsing_section == REG_PRETTY_VT_LEVEL3_SECTION)  reg_pretty_vt_level3_table[reg_pretty_vt_level3_index++] = reg_val;
  else if ( parsing_section == REG_7FPS_SECTION)			  reg_7fps_table[reg_7fps_index++] = reg_val;  
  else if ( parsing_section == REG_10FPS_SECTION)			  reg_10fps_table[reg_10fps_index++] = reg_val;  
  else if ( parsing_section == REG_15FPS_SECTION)			  reg_15fps_table[reg_15fps_index++] = reg_val;    
  else if ( parsing_section == REG_SELF_CAPTURE_SECTION)	  reg_self_capture_table[reg_self_capture_index++] = reg_val;    

  return 0;
}

static u32 sr030pc30_parsing_section(char* line)
{
  if ( strstr(line, CAMIF_SET_SENSOR_QCIF_INIT) != NULL )
    parsing_section = REG_INIT_QCIF_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_CIF_INIT) != NULL )
    parsing_section = REG_INIT_CIF_SECTION;  
  else if ( strstr(line, CAMIF_SET_SENSOR_QVGA_INIT) != NULL )
    parsing_section = REG_INIT_QVGA_SECTION;  
  else if ( strstr(line, CAMIF_SET_SENSOR_VGA_INIT) != NULL )
    parsing_section = REG_INIT_VGA_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_QCIF_VT_INIT) != NULL )
    parsing_section = REG_INIT_QCIF_VT_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_CIF_VT_INIT) != NULL )
    parsing_section = REG_INIT_CIF_VT_SECTION;  
  else if ( strstr(line, CAMIF_SET_SENSOR_QVGA_VT_INIT) != NULL )
    parsing_section = REG_INIT_QVGA_VT_SECTION;  
  else if ( strstr(line, CAMIF_SET_SENSOR_VGA_VT_INIT) != NULL )
    parsing_section = REG_INIT_VGA_VT_SECTION;     
  else if ( strstr(line, CAMIF_SET_SENSOR_WB_AUTO) != NULL )
    parsing_section = REG_WB_AUTO_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_WB_DAYLIGHT ) != NULL )
    parsing_section = REG_WB_DAYLIGHT_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_WB_CLOUDY ) != NULL )
    parsing_section = REG_WB_CLOUDY_SECTION;
  else if( strstr(line, CAMIF_SET_SENSOR_WB_INCANDESCENT) != NULL)
    parsing_section = REG_WB_INCANDESCENT_SECTION;
  else if( strstr(line, CAMIF_SET_SENSOR_WB_FLUORESCENT) != NULL)
    parsing_section = REG_WB_FLUORESCENT_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_EV) != NULL )
    parsing_section = REG_EV_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_EV_VT) != NULL )
    parsing_section = REG_EV_VT_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_LEVEL_M5_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_LEVEL_M5_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_LEVEL_M4_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_LEVEL_M4_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_LEVEL_M3_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_LEVEL_M3_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_LEVEL_M2_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_LEVEL_M2_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_LEVEL_M1_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_LEVEL_M1_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_DEFAULT_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_DEFAULT_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_LEVEL_P1_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_LEVEL_P1_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_LEVEL_P2_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_LEVEL_P2_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_LEVEL_P3_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_LEVEL_P3_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_LEVEL_P4_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_LEVEL_P4_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_LEVEL_P5_CONTRAST) != NULL )
    parsing_section = REG_CONTRAST_LEVEL_P5_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_EFFECT_NONE) != NULL )
    parsing_section = REG_EFFECT_NONE_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_EFFECT_GRAY) != NULL )
    parsing_section = REG_EFFECT_GRAY_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_EFFECT_RED) != NULL )
    parsing_section = REG_EFFECT_RED_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_EFFECT_SEPIA) != NULL )
    parsing_section = REG_EFFECT_SEPIA_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_EFFECT_GREEN) != NULL )
    parsing_section = REG_EFFECT_GREEN_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_EFFECT_AQUA) != NULL )
    parsing_section = REG_EFFECT_AQUA_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_EFFECT_NEGATIVE) != NULL )
    parsing_section = REG_EFFECT_NEGATIVE_SECTION;
  else if ( strstr(line, CAMIF_SET_SENSOR_FLIP_NONE) != NULL )
    parsing_section = REG_FLIP_NONE_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_FLIP_WATER) != NULL )
    parsing_section = REG_FLIP_WATER_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_FLIP_MIRROR) != NULL )
    parsing_section = REG_FLIP_MIRROR_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_FLIP_WATER_MIRROR) != NULL )
    parsing_section = REG_FLIP_WATER_MIRROR_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_PRETTY_NONE) != NULL )
    parsing_section = REG_PRETTY_NONE_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_PRETTY_LEVEL1) != NULL )
    parsing_section = REG_PRETTY_LEVEL1_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_PRETTY_LEVEL2) != NULL )
    parsing_section = REG_PRETTY_LEVEL2_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_PRETTY_LEVEL3) != NULL )
    parsing_section = REG_PRETTY_LEVEL3_SECTION;        
  else if ( strstr(line, CAMIF_SET_SENSOR_PRETTY_VT_NONE) != NULL )
    parsing_section = REG_PRETTY_VT_NONE_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_PRETTY_VT_LEVEL1) != NULL )
    parsing_section = REG_PRETTY_VT_LEVEL1_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_PRETTY_VT_LEVEL2) != NULL )
    parsing_section = REG_PRETTY_VT_LEVEL2_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_PRETTY_VT_LEVEL3) != NULL )
    parsing_section = REG_PRETTY_VT_LEVEL3_SECTION;     
  else if ( strstr(line, CAMIF_SET_SENSOR_FPS7) != NULL )
    parsing_section = REG_7FPS_SECTION;    
  else if ( strstr(line, CAMIF_SET_SENSOR_FPS10) != NULL )
    parsing_section = REG_10FPS_SECTION;   
  else if ( strstr(line, CAMIF_SET_SENSOR_FPS15) != NULL )
    parsing_section = REG_15FPS_SECTION;   
  else if ( strstr(line, CAMIF_SET_SENSOR_SELF_CAPTURE) != NULL )
    parsing_section = REG_SELF_CAPTURE_SECTION;
  else
    return -EINVAL;

  return 0;
}

static int sr030pc30_make_table(void)
{
  char*        buffer = NULL;
  char         line[1024];
  unsigned int file_size = 0;

  struct file *filep = NULL;
  mm_segment_t oldfs;
  struct firmware *firmware;
  int ret = 0;  

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_make_table is called...\n");

  filep = filp_open(CAMIF_CONFIGURE_FILE_LSI, O_RDONLY, 0) ;

  if (filep && (filep!= 0xfffffffe))
  {
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    file_size = filep->f_op->llseek(filep, 0, SEEK_END);
    filep->f_op->llseek(filep, 0, SEEK_SET);
    buffer = (char*)kmalloc(file_size+1, GFP_KERNEL);
    filep->f_op->read(filep, buffer, file_size, &filep->f_pos);
    buffer[file_size] = '\0';
    filp_close(filep, current->files);
    set_fs(oldfs);
    printk(SR030PC30_MOD_NAME "File size : %d\n", file_size);
  }
  else
  {
    return -EINVAL;
  }

  // init table index
  parsing_section = 0;

  sr030pc30_util_gets(buffer, line, 1);
  if ( sr030pc30_parsing_section(line) )
  {
    sr030pc30_insert_register_table(line);
  }

  while(sr030pc30_util_gets(buffer, line, 0))
  {
    if ( sr030pc30_parsing_section(line) )
    {
      sr030pc30_insert_register_table(line);
    }
  }

  sr030pc30_insert_register_table(line);

  kfree(buffer);

  return 0;
}

#endif
#endif

static int ioctl_streamoff(struct v4l2_int_device *s)
{  
        int err = 0;
	dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_streamoff is called...\n");
	
  return 0;
}

static int ioctl_streamon(struct v4l2_int_device *s)
{
  struct sr030pc30_sensor *sensor = s->priv;
   struct i2c_client *client = sensor->i2c_client;

  int err = 0;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_streamon is called...(%x)\n", sensor->state);   

  if(sensor->state != SR030PC30_STATE_CAPTURE)
  {
    printk(SR030PC30_MOD_NAME "start preview....................\n");
    sr030pc30_pre_state = sr030pc30_curr_state;
    sr030pc30_curr_state = SR030PC30_STATE_PREVIEW; 
#if 0
        {
    switch(sensor->preview_size)
    {
      case SR030PC30_PREVIEW_SIZE_640_480:
	  	err = SR030pc30_regs_write(client, SR030pc30_init_reg, sizeof(SR030pc30_init_reg), "SR030pc30_init_reg");
       	//err = SR030pc30_regs_write(client, SR030pc30_preview_640x480, sizeof(SR030pc30_preview_640x480), "SR030pc30_preview_640x480");
       	//err = SR030pc30_regs_write(client, SR030pc30_preview_320x240, sizeof(SR030pc30_preview_320x240), "SR030pc30_preview_320x240");    
        break;

      case SR030PC30_PREVIEW_SIZE_320_240:
    		err = SR030pc30_regs_write(client, SR030pc30_preview_320x240, sizeof(SR030pc30_preview_320x240), "SR030pc30_preview_320x240");    
        break;
      default:
        printk(SR030PC30_MOD_NAME "[size]Invalid value is ordered!!!\n");
        
    }
  }
#endif
  }
  else
  {
    printk(SR030PC30_MOD_NAME "start capture....................\n");
    sr030pc30_pre_state = sr030pc30_curr_state;
    sr030pc30_curr_state = SR030PC30_STATE_CAPTURE;
#if 0	
    switch(sensor->preview_size)
    {
      case SR030PC30_PREVIEW_SIZE_640_480:
       	err = SR030pc30_regs_write(client, SR030pc30_capture_640x480, sizeof(SR030pc30_preview_320x240), "SR030pc30_preview_320x240");    
        break;

      //case SR030PC30_PREVIEW_SIZE_320_240:
    	//	err = SR030pc30_regs_write(client, SR030pc30_preview_320x240, sizeof(SR030pc30_preview_320x240), "SR030pc30_preview_320x240");    
      //  break;
      default:
        printk(SR030PC30_MOD_NAME "[size]Invalid value is ordered!!!\n");
        
    }
 #endif
  }

  if (err < 0)
  {
   	goto streamon_fail;	
  }
  return 0;
  
streamon_fail:
  printk(SR030PC30_MOD_NAME "ioctl_streamon is failed!!!\n");
  return -EINVAL;   
}


/**
 * ioctl_queryctrl - V4L2 sensor interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the sr030pc30_ctrl_list[] array.
 * Otherwise, returns -EINVAL if the control is not supported.
 */
static int ioctl_queryctrl(struct v4l2_int_device *s, struct v4l2_queryctrl *qc)
{
  int i;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_queryctrl is called...\n");

  for (i = 0; i < NUM_SR030PC30_CONTROL; i++) 
  {
    if (qc->id == sr030pc30_ctrl_list[i].id)
    {
      break;
    }
  }
  if (i == NUM_SR030PC30_CONTROL)
  {
    dprintk(CAM_DBG, SR030PC30_MOD_NAME "Control ID is not supported!!\n");
    qc->flags = V4L2_CTRL_FLAG_DISABLED;

    return -EINVAL;
  }

  *qc = sr030pc30_ctrl_list[i];

  return 0;
}

static int sr030pc30_check_dataline_stop()
{
	struct sr030pc30_sensor *sensor = &sr030pc30;
	struct i2c_client *client = sensor->i2c_client;
	int err = -EINVAL, i;

	//for (i = 0; i < 2; i++) {
		//err = sr030pc30_i2c_write(client, sizeof(sr030pc30_dataline_stop[i]), sr030pc30_dataline_stop[i]);
		err = SR030pc30_regs_write(client, SR030pc30_dataline_stop, \
				sizeof(SR030pc30_dataline_stop), "SR030pc30_dataline_stop");
		if (err < 0)
		{
			v4l_info(client, "%s: register set failed\n", __func__);
			return -EIO;
		}
	//}
	sensor->check_dataline = 0;
	sensor->pdata->power_set(V4L2_POWER_OFF);
	mdelay(5);
	sensor->pdata->power_set(V4L2_POWER_ON);
	mdelay(5);
	sr030pc30_set_init();
	
	err = SR030pc30_regs_write(client, SR030pc30_init_reg, \
			sizeof(SR030pc30_init_reg), "SR030pc30_init_reg");

	if (err < 0)
	{
		v4l_info(client, "%s: register set failed\n", __func__);
		return -EIO;
	}
	return err;
}

/**
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the ce13 sensor struct.
 * Otherwise, returns -EINVAL if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
  struct sr030pc30_sensor *sensor = s->priv;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_g_ctrl is called...(%d)\n", vc->id);

  switch (vc->id) 
  {
    case V4L2_CID_AF:
      vc->value = 2;
      break; 
    case V4L2_CID_SELECT_MODE:
      vc->value = sensor->mode;
      break;  
    case V4L2_CID_SELECT_STATE:
      vc->value = sensor->state;
      break;       
    case V4L2_CID_BRIGHTNESS:
      vc->value = sensor->ev;
      break;
    case V4L2_CID_CONTRAST:
      vc->value = sensor->contrast;
	  break;
    case V4L2_CID_WB:
      vc->value = sensor->wb;
      break;      
    case V4L2_CID_EFFECT:
      vc->value = sensor->effect;
      break;
    case V4L2_CID_FLIP:
      vc->value = sensor->flip;
      break;
    case V4L2_CID_PRETTY:
      vc->value = sensor->pretty;
      break;    
    default:
       printk(SR030PC30_MOD_NAME "[id]Invalid value is ordered!!!\n");
      break;
  }

  return 0;
}

/**
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the sr030pc30 sensor struct).
 * Otherwise, * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
  struct sr030pc30_sensor *sensor = &sr030pc30;
  int retval = 0;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_s_ctrl is called...(%d)\n", vc->id);

  switch (vc->id) 
  {
    case V4L2_CID_SELECT_MODE:
      retval = sr030pc30_set_mode(vc->value);
      break;  
    case V4L2_CID_SELECT_STATE:
      retval = sr030pc30_set_state(vc->value);
      break;  
    case V4L2_CID_ZOOM:
      retval = sr030pc30_set_zoom(vc->value);
      break;      
    case V4L2_CID_BRIGHTNESS:
      retval = sr030pc30_set_ev(vc->value);
      break;
    case V4L2_CID_CONTRAST:
      retval = sr030pc30_set_contrast(vc->value);
      break;
    case V4L2_CID_WB:
      retval = sr030pc30_set_wb(vc->value);
      break;
    case V4L2_CID_EFFECT:
      retval = sr030pc30_set_effect(vc->value);
      break;
    case V4L2_CID_FLIP:
      retval = sr030pc30_set_flip(vc->value);
      break;
    case V4L2_CID_PRETTY:
      retval = sr030pc30_set_pretty(vc->value);
      break;    
    case V4L2_CID_CAMERA_CHECK_DATALINE:
	{
	  sensor->check_dataline = vc->value;
	   dprintk(CAM_DBG, SR030PC30_MOD_NAME "V4L2_CID_CAMERA_CHECK_DATALINE : sensor->check_dataline=%d\n", sensor->check_dataline);
	  retval = 0;
	}
	  break;	
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
	  retval = sr030pc30_check_dataline_stop();
	  break;	  
    default:
       printk(SR030PC30_MOD_NAME "[id]Invalid value is ordered!!!\n");
      break;
  }

  return retval;
}


/**
 * ioctl_enum_fmt_cap - Implement the CAPTURE buffer VIDIOC_ENUM_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @fmt: standard V4L2 VIDIOC_ENUM_FMT ioctl structure
 *
 * Implement the VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s, struct v4l2_fmtdesc *fmt)
{
  int index = 0;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_enum_fmt_cap is called...\n");

  switch (fmt->type) 
  {
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
      switch(fmt->pixelformat)
      {
        case V4L2_PIX_FMT_UYVY:
          index = 0;
          break;

        case V4L2_PIX_FMT_YUYV:
          index = 1;
          break;

        case V4L2_PIX_FMT_JPEG:
          index = 2;
          break;

        case V4L2_PIX_FMT_MJPEG:
          index = 3;
          break;

        default:
           printk(SR030PC30_MOD_NAME "[format]Invalid value is ordered!!!\n");
          return -EINVAL;
      }
      break;
      
    default:
       printk(SR030PC30_MOD_NAME "[type]Invalid value is ordered!!!\n");
      return -EINVAL;
  }

  fmt->flags = sr030pc30_formats[index].flags;
  fmt->pixelformat = sr030pc30_formats[index].pixelformat;
  strlcpy(fmt->description, sr030pc30_formats[index].description, sizeof(fmt->description));  

  dprintk(CAM_DBG, SR030PC30_MOD_NAME "ioctl_enum_fmt_cap flag : %d\n", fmt->flags);
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "ioctl_enum_fmt_cap description : %s\n", fmt->description);

  return 0;
}

/**
 * ioctl_try_fmt_cap - Implement the CAPTURE buffer VIDIOC_TRY_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_TRY_FMT ioctl structure
 *
 * Implement the VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.  This
 * ioctl is used to negotiate the image capture size and pixel format
 * without actually making it take effect.
 */
static int ioctl_try_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
  struct v4l2_pix_format *pix = &f->fmt.pix;
  struct sr030pc30_sensor *sensor = s->priv;
  struct v4l2_pix_format *pix2 = &sensor->pix;

  int index = 0;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_try_fmt_cap is called...\n");
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "ioctl_try_fmt_cap. mode : %d\n", sensor->mode);
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "ioctl_try_fmt_cap. state : %d\n", sensor->state);
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "ioctl_try_fmt_cap. pix width : %d\n", pix->width);
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "ioctl_try_fmt_cap. pix height : %d\n", pix->height);    

  sr030pc30_set_skip();

  if(sensor->state == SR030PC30_STATE_CAPTURE)
  {
    for(index = 0; index < ARRAY_SIZE(sr030pc30_image_sizes); index++)
    {
      if(sr030pc30_image_sizes[index].width == pix->width
      && sr030pc30_image_sizes[index].height == pix->height)
      {
        sensor->capture_size = index;
        break;
      }
    }   

    if(index == ARRAY_SIZE(sr030pc30_image_sizes))
    {
      printk(SR030PC30_MOD_NAME "Capture Image Size is not supported!\n");
      goto try_fmt_fail;
    }  

    dprintk(CAM_DBG, SR030PC30_MOD_NAME "capture size = %d\n", sensor->capture_size);  
    
    pix->field = V4L2_FIELD_NONE;
    if(pix->pixelformat == V4L2_PIX_FMT_UYVY || pix->pixelformat == V4L2_PIX_FMT_YUYV)
    {
      pix->bytesperline = pix->width * 2;
      pix->sizeimage = pix->bytesperline * pix->height;
      dprintk(CAM_DBG, SR030PC30_MOD_NAME "V4L2_PIX_FMT_YUYV\n");
    }
    else
    {
      /* paladin[08.10.14]: For JPEG Capture, use fixed buffer size @LDK@ */
      pix->bytesperline = JPEG_CAPTURE_WIDTH * 2; /***** !!!fixme mingyu diffent sensor!!!**********/
      pix->sizeimage = pix->bytesperline * JPEG_CAPTURE_HEIGHT;
      dprintk(CAM_DBG, SR030PC30_MOD_NAME "V4L2_PIX_FMT_JPEG\n");
    }

    if(sr030pc30_curr_state == SR030PC30_STATE_INVALID)
    {
      if(sr030pc30_set_init())
      {
        printk(SR030PC30_MOD_NAME "Unable to detect " SR030PC30_DRIVER_NAME " sensor\n");
        goto try_fmt_fail;
      }
    }     
  }  

  switch (pix->pixelformat) 
  {
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_JPEG:
    case V4L2_PIX_FMT_MJPEG:
      pix->colorspace = V4L2_COLORSPACE_JPEG;
      break;
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB565X:
    case V4L2_PIX_FMT_RGB555:
    case V4L2_PIX_FMT_SGRBG10:
    case V4L2_PIX_FMT_RGB555X:
    default:
      pix->colorspace = V4L2_COLORSPACE_SRGB;
      break;
  }

  *pix2 = *pix;

  return 0;

try_fmt_fail:
  printk(SR030PC30_MOD_NAME "ioctl_try_fmt_cap is failed\n"); 
  return -EINVAL;  
}

/**
 * ioctl_s_fmt_cap - V4L2 sensor interface handler for VIDIOC_S_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_S_FMT ioctl structure
 *
 * If the requested format is supported, configures the HW to use that
 * format, returns error code if format not supported or HW can't be
 * correctly configured.
 */
static int ioctl_s_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
  struct v4l2_pix_format *pix = &f->fmt.pix;
  struct sr030pc30_sensor *sensor = s->priv;
  struct v4l2_pix_format *pix2 = &sensor->pix;

  int index = 0;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_s_fmt_cap is called...\n");
  
  printk(SR030PC30_MOD_NAME "camera mode  : %d (1:camera , 2:camcorder, 3:vt)\n", sensor->mode);
  printk(SR030PC30_MOD_NAME "camera state : %d (0:preview, 1:snapshot)\n", sensor->state);
  printk(SR030PC30_MOD_NAME "set width  : %d\n", pix->width);
  printk(SR030PC30_MOD_NAME "set height  : %d\n", pix->height);    

  if(sensor->state == SR030PC30_STATE_CAPTURE)
  {
    sr030pc30_set_skip();
  
    for(index = 0; index < ARRAY_SIZE(sr030pc30_image_sizes); index++)
    {
      if(sr030pc30_image_sizes[index].width == pix->width
      && sr030pc30_image_sizes[index].height == pix->height)
      {
        sensor->capture_size = index;
        break;
      }
    }   

    if(index == ARRAY_SIZE(sr030pc30_image_sizes))
    {
      printk(SR030PC30_MOD_NAME "Capture Image %d x %d Size is not supported!\n", pix->width, pix->height);
      goto s_fmt_fail;
    }  

    dprintk(CAM_DBG, SR030PC30_MOD_NAME "capture size = %d\n", sensor->capture_size);  
    
    pix->field = V4L2_FIELD_NONE;
    if(pix->pixelformat == V4L2_PIX_FMT_UYVY || pix->pixelformat == V4L2_PIX_FMT_YUYV)
    {
      pix->bytesperline = pix->width * 2;
      pix->sizeimage = pix->bytesperline * pix->height;
      dprintk(CAM_DBG, SR030PC30_MOD_NAME "V4L2_PIX_FMT_YUYV\n");
    }
    else
    {
      /* paladin[08.10.14]: For JPEG Capture, use fixed buffer size @LDK@ */
      pix->bytesperline = JPEG_CAPTURE_WIDTH * 2; /***** !!!fixme mingyu diffent sensor!!!**********/
      pix->sizeimage = pix->bytesperline * JPEG_CAPTURE_HEIGHT;
      dprintk(CAM_DBG, SR030PC30_MOD_NAME "V4L2_PIX_FMT_JPEG\n");
    }

    if(sr030pc30_curr_state == SR030PC30_STATE_INVALID)
    {
      if(sr030pc30_set_init())
      {
        printk(SR030PC30_MOD_NAME "Unable to detect " SR030PC30_DRIVER_NAME " sensor\n");
        goto s_fmt_fail;
      }
    }     
  }  
  else
  {
    //sr030pc30_set_skip();
  
    for(index = 0; index < ARRAY_SIZE(sr030pc30_preview_sizes); index++)
    {
      if(sr030pc30_preview_sizes[index].width == pix->width
      && sr030pc30_preview_sizes[index].height == pix->height)
      {
        sensor->preview_size = index;
        break;
      }
    }   

    if(index == ARRAY_SIZE(sr030pc30_preview_sizes))
    {
      printk(SR030PC30_MOD_NAME "Preview Image %d x %d Size is not supported!\n", pix->width, pix->height);
      goto s_fmt_fail;
    }
  
    dprintk(CAM_DBG, SR030PC30_MOD_NAME "preview size = %d\n", sensor->preview_size);
    
    pix->field = V4L2_FIELD_NONE;
    pix->bytesperline = pix->width * 2;
    pix->sizeimage = pix->bytesperline * pix->height;  

    if(sr030pc30_curr_state == SR030PC30_STATE_INVALID)
    {
      if(sr030pc30_set_init())
      {
        printk(SR030PC30_MOD_NAME "Unable to detect " SR030PC30_DRIVER_NAME " sensor\n");
        goto s_fmt_fail;
      }
    }    
  }      

  switch (pix->pixelformat) 
  {
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_JPEG:
    case V4L2_PIX_FMT_MJPEG:
      pix->colorspace = V4L2_COLORSPACE_JPEG;
      break;
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB565X:
    case V4L2_PIX_FMT_RGB555:
    case V4L2_PIX_FMT_SGRBG10:
    case V4L2_PIX_FMT_RGB555X:
    default:
      pix->colorspace = V4L2_COLORSPACE_SRGB;
      break;
  }

  *pix2 = *pix;

  return 0;

s_fmt_fail:
  printk(SR030PC30_MOD_NAME "ioctl_s_fmt_cap is failed\n"); 
  return -EINVAL;    
}


/**
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
  struct sr030pc30_sensor *sensor = s->priv;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_g_fmt_cap is called...\n");
  
  f->fmt.pix = sensor->pix;

  return 0;
}

/**
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
  struct sr030pc30_sensor *sensor = s->priv;
  struct v4l2_captureparm *cparm = &a->parm.capture;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_g_parm is called...\n");

  if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
  {
    return -EINVAL;
  }

  memset(a, 0, sizeof(*a));
  a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  cparm->capability = V4L2_CAP_TIMEPERFRAME;
  cparm->timeperframe = sensor->timeperframe;

  return 0;
}

/**
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
  struct sr030pc30_sensor *sensor = s->priv;
  struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_s_parm is called...\n");

  /* Set mode (camera/camcorder/vt) & state (preview/capture) */
  sensor->mode = a->parm.capture.capturemode;
  sensor->state = a->parm.capture.currentstate;

  if(sensor->mode < 1 || sensor->mode > 3) sensor->mode = SR030PC30_MODE_CAMERA;
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "mode = %d, state = %d\n", sensor->mode, sensor->state); 

  /* Set time per frame (FPS) */
  if((timeperframe->numerator == 0)&&(timeperframe->denominator == 0))
  {
    sensor->fps = 15;
    //sensor->fps = 30;//LYG
  }
  else
  {
    sensor->fps = timeperframe->denominator / timeperframe->numerator;
  }

  sensor->timeperframe = *timeperframe;
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "fps = %d\n", sensor->fps);  
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "numerator : %d, denominator: %d\n", timeperframe->numerator, timeperframe->denominator);
  
  return 0;
}

/**
 * ioctl_g_ifparm - V4L2 sensor interface handler for vidioc_int_g_ifparm_num
 * @s: pointer to standard V4L2 device structure
 * @p: pointer to standard V4L2 vidioc_int_g_ifparm_num ioctl structure
 *
 * Gets slave interface parameters.
 * Calculates the required xclk value to support the requested
 * clock parameters in p.  This value is returned in the p
 * parameter.
 */
static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
  struct sr030pc30_sensor *sensor = s->priv;
  int rval;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_g_ifparm is called...\n");

  rval = sensor->pdata->ifparm(p);
  if (rval)
  {
    return rval;
  }

  p->u.bt656.clock_curr = SR030PC30_XCLK;

  return 0;
}

/**
 * ioctl_g_priv - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's private data address
 *
 * Returns device's (sensor's) private data area address in p parameter
 */
static int ioctl_g_priv(struct v4l2_int_device *s, void *p)
{
  struct sr030pc30_sensor *sensor = s->priv;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_g_priv is called...\n");
  
  return sensor->pdata->priv_data_set(p);
}


/* added following functins for v4l2 compatibility with omap34xxcam */

/**
 * ioctl_enum_framesizes - V4L2 sensor if handler for vidioc_int_enum_framesizes
 * @s: pointer to standard V4L2 device structure
 * @frms: pointer to standard V4L2 framesizes enumeration structure
 *
 * Returns possible framesizes depending on choosen pixel format
 **/
static int ioctl_enum_framesizes(struct v4l2_int_device *s, struct v4l2_frmsizeenum *frms)
{
  struct sr030pc30_sensor* sensor = s->priv;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_enum_framesizes fmt\n");   

  if (sensor->state == SR030PC30_STATE_CAPTURE)
  {
    dprintk(CAM_DBG, SR030PC30_MOD_NAME "Size enumeration for image capture = %d\n", sensor->capture_size);
  
    frms->index = sensor->capture_size;
    frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    frms->discrete.width = sr030pc30_image_sizes[sensor->capture_size].width;
    frms->discrete.height = sr030pc30_image_sizes[sensor->capture_size].height;       
  }
  else
  {    
    dprintk(CAM_DBG, SR030PC30_MOD_NAME "Size enumeration for preview = %d\n", sensor->preview_size);
    
    frms->index = sensor->preview_size;
    frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    frms->discrete.width = sr030pc30_preview_sizes[sensor->preview_size].width;
    frms->discrete.height = sr030pc30_preview_sizes[sensor->preview_size].height;       
  }

  dprintk(CAM_DBG, SR030PC30_MOD_NAME "framesizes width : %d\n", frms->discrete.width); 
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "framesizes height : %d\n", frms->discrete.height); 

  return 0;
}

static int ioctl_enum_frameintervals(struct v4l2_int_device *s, struct v4l2_frmivalenum *frmi)
{
  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_enum_frameintervals \n"); 
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "ioctl_enum_frameintervals numerator : %d\n", frmi->discrete.numerator); 
  dprintk(CAM_DBG, SR030PC30_MOD_NAME "ioctl_enum_frameintervals denominator : %d\n", frmi->discrete.denominator); 

  return 0;
}


/**
 * ioctl_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requrested state, if possible.
 */

static int power_Enable =0;
static int ioctl_s_power(struct v4l2_int_device *s, enum v4l2_power on)
{
  struct sr030pc30_sensor *sensor = s->priv;
  struct i2c_client *client = sensor->i2c_client;
  unsigned char read_value=0xff;//425

  int err = 0;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_s_power is called......ON=%x, detect= %x\n", on, sensor->detect);
  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_s_power is called......I2Caddr=0x%x, name = %s\n", client->addr,client->name);

  if(power_Enable ==0||power_Enable ==1){
  	if (gpio_request(OMAP3430_GPIO_CAMERA_EN4,"CAM EN4") != 0) 
  	{
    		printk(SR030PC30_MOD_NAME "Could not request GPIO %d", OMAP3430_GPIO_CAMERA_EN4);
   		// return -EIO;
  	}
  	  gpio_direction_output(OMAP3430_GPIO_CAMERA_EN4, 0);
  	udelay(1);
	 gpio_free(OMAP3430_GPIO_CAMERA_EN4);
  	power_Enable++;
	return 0;
  }
  sensor->pdata->power_set(on);

#ifdef CAM_TUNING_MODE
  CamTunningStatus = SR030pc30_CamTunning_table_init();
  err = CamTunningStatus;
  if (CamTunningStatus==0) {
	msleep(100);
  }
#endif

  switch(on)
  {
    case V4L2_POWER_ON:
    {
      dprintk(CAM_DBG, SR030PC30_MOD_NAME "pwr on-----!\n");
	front_cam_in_use= 1 ;
      //err = sr030pc30_get_rev();
      err = SR030pc30_i2c_read(client,0x04, &read_value); //device ID //LYG
      if(err == 0){
		printk(SR030PC30_MOD_NAME "SR030pc30_i2c_read n success I2C is OK " SR030PC30_DRIVER_NAME " sensor\n");
	  }
      else{
	 front_cam_in_use= 0 ;
        printk(SR030PC30_MOD_NAME "Unable to detect-i2c not connect  " SR030PC30_DRIVER_NAME " sensor\n");
        sensor->pdata->power_set(V4L2_POWER_OFF);
        return err;
      }
#if 0
	printk(SR030PC30_MOD_NAME "SR030pc30 Device ID 0x8c= 0x%x \n!!", read_value); 
	mdelay(3);

	 err = SR030pc30_i2c_read(client,0x11, &read_value); //device ID //LYG
      if(err == 0){
		printk(SR030PC30_MOD_NAME "SR030pc30_i2c_read n success I2C is OK " SR030PC30_DRIVER_NAME " sensor\n");
	  }
      else{
        printk(SR030PC30_MOD_NAME "Unable to detect-i2c not connect  " SR030PC30_DRIVER_NAME " sensor\n");
        sensor->pdata->power_set(V4L2_POWER_OFF);
        return err;
      }

	printk(SR030PC30_MOD_NAME "SR030pc30 Device ID 0xb1 = 0x%x \n!!", read_value); 
	mdelay(3);	

	 err = SR030pc30_i2c_read(client,0x10, &read_value); //device ID //LYG
      if(err == 0){
		printk(SR030PC30_MOD_NAME "SR030pc30_i2c_read n success I2C is OK " SR030PC30_DRIVER_NAME " sensor\n");
	  }
      else{
        printk(SR030PC30_MOD_NAME "Unable to detect-i2c not connect  " SR030PC30_DRIVER_NAME " sensor\n");
        sensor->pdata->power_set(V4L2_POWER_OFF);
        return err;
      }

	printk(SR030PC30_MOD_NAME "SR030pc30 Device ID 0x8c = 0x%x \n!!", read_value); 
	mdelay(3);	
#endif	
      /* Make the default zoom */
      sensor->zoom = SR030PC30_ZOOM_1P00X;      

      /* Make the default detect */
      sensor->detect = SENSOR_DETECTED;

      /* Make the state init */
      sr030pc30_curr_state = SR030PC30_STATE_INVALID;  

#if 1  
    	err = SR030pc30_regs_write(client, SR030pc30_init_reg, \
				sizeof(SR030pc30_init_reg), "SR030pc30_init_reg");
	if (err < 0)
	{
		v4l_info(client, "%s: register set failed\n", __func__);
	}
#else	
    	err =  SR030pc30_regs_write(client, SR030pc30_init_vt_reg, \
					sizeof(SR030pc30_init_vt_reg), "SR030pc30_init_vt_reg");
	if (err < 0)
	{
		v4l_info(client, "%s: register set failed\n", __func__);
	}
#endif
#if 0 //15fps ���� ���� 
	err = SR030pc30_regs_write(client, SR030pc30_fps_15, sizeof(SR030pc30_fps_15), "SR030pc30_fps_15");
	if (err < 0)
	{
		v4l_info(client, "%s: register set failed\n", __func__);
	}
#endif

#if 0	
 	dprintk(CAM_DBG, SR030PC30_MOD_NAME "GPIO103 ===%d\n",gpio_get_value(103));
	dprintk(CAM_DBG, SR030PC30_MOD_NAME "GPIO104 ===%d\n",gpio_get_value(104));
	dprintk(CAM_DBG, SR030PC30_MOD_NAME "GPIO105 ===%d\n",gpio_get_value(105));
	dprintk(CAM_DBG, SR030PC30_MOD_NAME "GPIO106 ===%d\n",gpio_get_value(106));
	dprintk(CAM_DBG, SR030PC30_MOD_NAME "GPIO107 ===%d\n",gpio_get_value(107));
	dprintk(CAM_DBG, SR030PC30_MOD_NAME "GPIO108 ===%d\n",gpio_get_value(108));
	dprintk(CAM_DBG, SR030PC30_MOD_NAME "GPIO109 ===%d\n",gpio_get_value(109));
	dprintk(CAM_DBG, SR030PC30_MOD_NAME "GPIO110 ===%d\n",gpio_get_value(110));
#endif	

    }
    break;

    case V4L2_POWER_RESUME:
    {
      dprintk(CAM_DBG, SR030PC30_MOD_NAME "pwr resume-----!\n");
    }
    break;

    case V4L2_POWER_STANDBY:
    {
      dprintk(CAM_DBG, SR030PC30_MOD_NAME "pwr stanby-----!\n");
    }
    break;

    case V4L2_POWER_OFF:
    {
      dprintk(CAM_DBG, SR030PC30_MOD_NAME "pwr off-----!\n");
	          front_cam_in_use= 0 ;


      /* Make the default zoom */
      sensor->zoom = SR030PC30_ZOOM_1P00X;

      /* Make the default detect */
      sensor->detect = SENSOR_NOT_DETECTED;
      
      /* Make the state init */
      sr030pc30_pre_state = SR030PC30_STATE_INVALID;       
    }
    break;
  }

  return err;
}

static int ioctl_g_exif(struct v4l2_int_device *s, struct v4l2_exif *exif)
{
  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_g_exif is called...\n");

  return 0;
}


/**
 * ioctl_deinit - V4L2 sensor interface handler for VIDIOC_INT_DEINIT
 * @s: pointer to standard V4L2 device structure
 *
 * Deinitialize the sensor device
 */
static int ioctl_deinit(struct v4l2_int_device *s)
{
  struct sr030pc30_sensor *sensor = s->priv;
  
  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_init is called...\n");

  sensor->state = SR030PC30_STATE_INVALID; //init problem
  
  return 0;
}


/**
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 *
 * Initialize the sensor device (call sr030pc30_configure())
 */
static int ioctl_init(struct v4l2_int_device *s)
{
  struct sr030pc30_sensor *sensor = s->priv;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "ioctl_init is called...\n");

  //init value
  sensor->timeperframe.numerator    = 1,
  sensor->timeperframe.denominator  = 15,
  //sensor->timeperframe.denominator  = 30,//LYG
  sensor->mode                      = SR030PC30_MODE_CAMERA;
  sensor->state                     = SR030PC30_STATE_INVALID;
  sensor->fps                       = 15;
  //sensor->fps                       = 30;//LYG
  sensor->preview_size              = SR030PC30_PREVIEW_SIZE_640_480;
  //sensor->preview_size              = SR030PC30_PREVIEW_SIZE_320_240;
  sensor->capture_size              = SR030PC30_IMAGE_SIZE_640_480;
  sensor->detect                    = SENSOR_NOT_DETECTED;
  sensor->zoom                      = SR030PC30_ZOOM_1P00X;
  sensor->effect                    = SR030PC30_EFFECT_OFF;
  sensor->ev                        = SR030PC30_EV_DEFAULT;
  sensor->wb                        = SR030PC30_WB_AUTO;
  sensor->pretty                    = SR030PC30_PRETTY_NONE;
  sensor->flip                      = SR030PC30_FLIP_NONE;

  memcpy(&sr030pc30, sensor, sizeof(struct sr030pc30_sensor));
  
  return 0;
}

static struct v4l2_int_ioctl_desc sr030pc30_ioctl_desc[] = {
  { .num = vidioc_int_enum_framesizes_num,
    .func = (v4l2_int_ioctl_func *)ioctl_enum_framesizes},
  { .num = vidioc_int_enum_frameintervals_num,
    .func = (v4l2_int_ioctl_func *)ioctl_enum_frameintervals},
  { .num = vidioc_int_s_power_num,
    .func = (v4l2_int_ioctl_func *)ioctl_s_power },
  { .num = vidioc_int_g_priv_num,
    .func = (v4l2_int_ioctl_func *)ioctl_g_priv },
  { .num = vidioc_int_g_ifparm_num,
    .func = (v4l2_int_ioctl_func *)ioctl_g_ifparm },
  { .num = vidioc_int_init_num,
    .func = (v4l2_int_ioctl_func *)ioctl_init },
  { .num = vidioc_int_deinit_num,
    .func = (v4l2_int_ioctl_func *)ioctl_deinit },
  { .num = vidioc_int_enum_fmt_cap_num,
    .func = (v4l2_int_ioctl_func *)ioctl_enum_fmt_cap },
  { .num = vidioc_int_try_fmt_cap_num,
    .func = (v4l2_int_ioctl_func *)ioctl_try_fmt_cap },
  { .num = vidioc_int_g_fmt_cap_num,
    .func = (v4l2_int_ioctl_func *)ioctl_g_fmt_cap },
  { .num = vidioc_int_s_fmt_cap_num,
    .func = (v4l2_int_ioctl_func *)ioctl_s_fmt_cap },
  { .num = vidioc_int_g_parm_num,
    .func = (v4l2_int_ioctl_func *)ioctl_g_parm },
  { .num = vidioc_int_s_parm_num,
    .func = (v4l2_int_ioctl_func *)ioctl_s_parm },
  { .num = vidioc_int_queryctrl_num,
    .func = (v4l2_int_ioctl_func *)ioctl_queryctrl },
  { .num = vidioc_int_g_ctrl_num,
    .func = (v4l2_int_ioctl_func *)ioctl_g_ctrl },
  { .num = vidioc_int_s_ctrl_num,
    .func = (v4l2_int_ioctl_func *)ioctl_s_ctrl },
  { .num = vidioc_int_streamon_num,
    .func = (v4l2_int_ioctl_func *)ioctl_streamon },
  { .num = vidioc_int_streamoff_num,
    .func = (v4l2_int_ioctl_func *)ioctl_streamoff },
  { .num = vidioc_int_g_exif_num,
    .func = (v4l2_int_ioctl_func *)ioctl_g_exif },     
};

static struct v4l2_int_slave sr030pc30_slave = {
  .ioctls = sr030pc30_ioctl_desc,
  .num_ioctls = ARRAY_SIZE(sr030pc30_ioctl_desc),
};

static struct v4l2_int_device sr030pc30_int_device = {
  .module = THIS_MODULE,
  .name = SR030PC30_DRIVER_NAME,
  .priv = &sr030pc30,
  .type = v4l2_int_type_slave,
  .u = {
    .slave = &sr030pc30_slave,
  },
};


/**
 * sr030pc30_probe - sensor driver i2c probe handler
 * @client: i2c driver client device structure
 *
 * Register sensor as an i2c client device and V4L2
 * device.
 */

static int probestate =0;

static int
sr030pc30_probe(struct i2c_client *client, const struct i2c_device_id *device)
{
if(probestate==0){
   probestate = 1;
  struct sr030pc30_sensor *sensor = &sr030pc30;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_probe is called...\n");

  if (i2c_get_clientdata(client))
  {
    dprintk(CAM_DBG, SR030PC30_MOD_NAME "can't get i2c client data!!");
    return -EBUSY;
  }

 sensor->pdata = &nowplus_sr030pc30_platform_data;

  if (!sensor->pdata) 
  {
    dprintk(CAM_DBG, SR030PC30_MOD_NAME "no platform data?\n");
    return -ENODEV;
  }

  sensor->v4l2_int_device = &sr030pc30_int_device;
  sensor->i2c_client = client;

  /* Make the default capture size VGA */
  sensor->pix.width = 640;
  sensor->pix.height = 480;
  //sensor->pix.width = 320;
  //sensor->pix.height = 240;
  /* Make the default capture format V4L2_PIX_FMT_UYVY */
  sensor->pix.pixelformat = V4L2_PIX_FMT_UYVY;

  i2c_set_clientdata(client, sensor);

  if (v4l2_int_device_register(sensor->v4l2_int_device))
  {
    dprintk(CAM_DBG, SR030PC30_MOD_NAME "fail to init device register \n");
    i2c_set_clientdata(client, NULL);
  }
}
  return 0;
}

/**
 * sr030pc30_remove - sensor driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister sensor as an i2c client device and V4L2
 * device.  Complement of sr030pc30_probe().
 */
static int __exit
sr030pc30_remove(struct i2c_client *client)
{
  struct sr030pc30_sensor *sensor = i2c_get_clientdata(client);

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_remove is called...\n");

  if (!client->adapter)
  {
    dprintk(CAM_DBG, SR030PC30_MOD_NAME "no i2c client adapter!!");
    return -ENODEV; /* our client isn't attached */
  }

  v4l2_int_device_unregister(sensor->v4l2_int_device);
  i2c_set_clientdata(client, NULL);

  return 0;
}

static const struct i2c_device_id sr030pc30_id[] = {
  { SR030PC30_DRIVER_NAME, 0 },
  { },
};

MODULE_DEVICE_TABLE(i2c, sr030pc30_id);


static struct i2c_driver sr030pc30sensor_i2c_driver = {
  .driver = {
    .name = SR030PC30_DRIVER_NAME,
  },
  .probe = sr030pc30_probe,
  .remove = __exit_p(sr030pc30_remove),
  .id_table = sr030pc30_id,
};

/**
 * sr030pc30_sensor_init - sensor driver module_init handler
 *
 * Registers driver as an i2c client driver.  Returns 0 on success,
 * error code otherwise.
 */
static int __init sr030pc30_sensor_init(void)
{
  int err;

  dprintk(CAM_INF, SR030PC30_MOD_NAME "sr030pc30_sensor_init is called...\n");

  err = i2c_add_driver(&sr030pc30sensor_i2c_driver);
  if (err) 
  {
    dprintk(CAM_DBG, SR030PC30_MOD_NAME "Failed to register" SR030PC30_DRIVER_NAME ".\n");
    return err;
  }
  
  return 0;
}

module_init(sr030pc30_sensor_init);

/**
 * sr030pc30sensor_cleanup - sensor driver module_exit handler
 *
 * Unregisters/deletes driver as an i2c client driver.
 * Complement of sr030pc30_sensor_init.
 */
static void __exit sr030pc30sensor_cleanup(void)
{
  i2c_del_driver(&sr030pc30sensor_i2c_driver);
}
module_exit(sr030pc30sensor_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SR030PC30 camera sensor driver");