#include "OLED.h"
#include "stdlib.h"
#include "oledfont.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "OLED.h"
#include "stdio.h"

static const char *TAG = "0.91-OLED";

#define I2C_MASTER_SCL_IO 18		/*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO 17		/*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM 0			/*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ 800000	/*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS 1000

#define oledaddress 0x78
#define SIZE 16
#define XLevelL 0x02
#define XLevelH 0x10
#define Max_Column 128
#define Max_Row 64
#define Brightness 0xFF
#define X_WIDTH 128
#define Y_WIDTH 32

unsigned char Init_Data[] = {
	0xAE, 0x00, 0x10, 0x00, 0xB0, 0x81, 0xFF, 0xA1, 0xA6, 0xA8, 0x1F,

	0xC8, 0xD3, 0x00, 0xD5, 0x80, 0xD9, 0x1F, 0xDA, 0x00, 0xDB, 0x40,

	0x8D, 0x14, 0xAF};

typedef void *i2c_cmd_handle_t;

/*i2c master initialization*/
void i2c_master_init()
{
	int i2c_master_port = I2C_MASTER_NUM;
	static i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_MASTER_SDA_IO,
		.scl_io_num = I2C_MASTER_SCL_IO,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_MASTER_FREQ_HZ,
	};

	i2c_param_config(i2c_master_port, &conf);
	i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

void OLED_Init(void)
{
	i2c_master_init();
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, oledaddress, I2C_MASTER_ACK);
	i2c_master_write(cmd, Init_Data, 25, I2C_MASTER_ACK);
	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 500 / portTICK_PERIOD_MS); // wait?
	i2c_cmd_link_delete(cmd);
	OLED_Clear();
	printf("\nOLED has inited!\n");
}

void Write_IIC_Command(unsigned char IIC_Command)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, 0x78, I2C_MASTER_ACK); // Slave address,SA0=0
	i2c_master_write_byte(cmd, 0x00, I2C_MASTER_ACK); // write command
	i2c_master_write_byte(cmd, IIC_Command, I2C_MASTER_ACK);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS); // wait?
	i2c_cmd_link_delete(cmd);
}

void Write_IIC_Data(unsigned char IIC_Data)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, 0x78, I2C_MASTER_ACK); // Slave address,SA0=0
	i2c_master_write_byte(cmd, 0x40, I2C_MASTER_ACK); // write command
	i2c_master_write_byte(cmd, IIC_Data, I2C_MASTER_ACK);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS); // wait?
	i2c_cmd_link_delete(cmd);
}

void OLED_Set_Pos(unsigned char x, unsigned char y)
{
	Write_IIC_Command(0xb0 + y);
	Write_IIC_Command(((x & 0xf0) >> 4) | 0x10);
	Write_IIC_Command((x & 0x0f));
}

void OLED_Clear(void)
{
	unsigned char i, n;
	for (i = 0; i < 8; i++)
	{
		Write_IIC_Command(0xb0 + i);
		Write_IIC_Command(0x00);
		Write_IIC_Command(0x10);
		for (n = 0; n < 128; n++)
			Write_IIC_Data(0);
	}
}

uint32_t oled_pow(unsigned char m, unsigned char n)
{
	uint32_t result = 1;
	while (n--)
		result *= m;
	return result;
}

void OLED_ShowChar(unsigned char x, unsigned char y, unsigned char chr, unsigned char Char_Size)
{
	unsigned char c = 0, i = 0;
	c = chr - ' ';
	if (x > Max_Column - 1)
	{
		x = 0;
		y = y + 2;
	}
	if (Char_Size == 16)
	{
		OLED_Set_Pos(x, y);
		for (i = 0; i < 8; i++)
			Write_IIC_Data(F8X16[c * 16 + i]);
		OLED_Set_Pos(x, y + 1);
		for (i = 0; i < 8; i++)
			Write_IIC_Data(F8X16[c * 16 + i + 8]);
	}
	else
	{
		OLED_Set_Pos(x, y);
		for (i = 0; i < 6; i++)
			Write_IIC_Data(F6x8[c][i]);
	}
}

void OLED_ShowString(unsigned char x, unsigned char y, unsigned char *chr, unsigned char Char_Size)
{
	unsigned char j = 0;
	while (chr[j] != '\0')
	{
		OLED_ShowChar(x, y, chr[j], Char_Size);
		x += 8;
		if (x > 120)
		{
			x = 0;
			y += 2;
		}
		j++;
	}
}

void OLED_ShowNum(unsigned char x, unsigned char y, unsigned int num, unsigned char len, unsigned char size2)
{
	unsigned char t, temp;
	unsigned char enshow = 0;
	for (t = 0; t < len; t++)
	{
		temp = (num / oled_pow(10, len - t - 1)) % 10;
		if (enshow == 0 && t < (len - 1))
		{
			if (temp == 0)
			{
				OLED_ShowChar(x + (size2 / 2) * t, y, ' ', size2);
				continue;
			}
			else
				enshow = 1;
		}
		OLED_ShowChar(x + (size2 / 2) * t, y, temp + '0', size2);
	}
}
