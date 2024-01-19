#include <stdio.h>
#include <stdint.h>
#include "string.h"
#include <math.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "hal/rmt_types.h"
#include "driver/rmt_types.h"
#include "driver/rmt_tx.h"

#include "OLED.h"
#include "ws2812.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM 7
#define LED_NUMBERS 6
#define Breath_SPEED_MS 50
#define Water_SPEED_MS 1000

rmt_channel_handle_t led_chan = NULL;
rmt_encoder_handle_t led_encoder = NULL;
rmt_transmit_config_t tx_config = {
	.loop_count = 0, // no transfer loop
};

static const char *TAG = "example";

static uint8_t led_strip_pixels[LED_NUMBERS * 3];
uint8_t led_data[3 * LED_NUMBERS];

uint32_t red = 0;
uint32_t green = 0;
uint32_t blue = 0;
uint16_t hue = 0;
int Color_Change_Water_Count = LED_NUMBERS;

/* 呼吸灯曲线表 */
const uint16_t index_wave[] =
	{
		1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7,
		7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
		41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
		61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
		81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 98, 99, 99, 100,
		100, 100, 99, 99, 98, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84, 83, 82, 81,
		80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61,
		60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41,
		40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21,
		20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 10, 9, 9, 9, 8, 8, 8, 7, 7, 7, 6, 6, 6, 5, 5,
		5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1};
//RGB
const uint8_t Color[] = {
	255, 0, 0,
	76, 38, 0,
	214, 107, 0,
	0, 255, 0,
	0, 255, 255,
	0, 0, 255,
	127, 0, 255};
/*******************************
	esp_restart();
	vTaskDelay(1000 / portTICK_PERIOD_MS);
*******************************/
// led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
// h = 0~359,s = 0~100,v = 0~100
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
	h %= 360; // h -> [0,360]
	uint32_t rgb_max = v * 2.55f;
	uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

	uint32_t i = h / 60;
	uint32_t diff = h % 60;

	// RGB adjustment amount by hue
	uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

	switch (i)
	{
	case 0:
		*r = rgb_max;
		*g = rgb_min + rgb_adj;
		*b = rgb_min;
		break;
	case 1:
		*r = rgb_max - rgb_adj;
		*g = rgb_max;
		*b = rgb_min;
		break;
	case 2:
		*r = rgb_min;
		*g = rgb_max;
		*b = rgb_min + rgb_adj;
		break;
	case 3:
		*r = rgb_min;
		*g = rgb_max - rgb_adj;
		*b = rgb_max;
		break;
	case 4:
		*r = rgb_min + rgb_adj;
		*g = rgb_min;
		*b = rgb_max;
		break;
	default:
		*r = rgb_max;
		*g = rgb_min;
		*b = rgb_max - rgb_adj;
		break;
	}
}

void RGB_Color_Set(int R, int G, int B)
{
	// memset(led_data, 0, sizeof(led_data));
	for (int i = 0; i < LED_NUMBERS; i++)
	{
		led_data[i * 3] = G;
		led_data[i * 3 + 1] = R;
		led_data[i * 3 + 2] = B;
	}
	// ESP_LOGI(TAG, "Set RGB Color down");
}

void Task1(void *pvParam)
{

	while (1)
	{
		printf("\nTask1 is running\n");
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void RMT_WS2812_Init()
{
	ESP_LOGI(TAG, "Create RMT TX channel");
	// rmt_channel_handle_t led_chan = NULL;
	rmt_tx_channel_config_t tx_chan_config = {
		.clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
		.gpio_num = RMT_LED_STRIP_GPIO_NUM,
		.mem_block_symbols = 64, // increase the block size can make the LED less flickering
		.resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
		.trans_queue_depth = 4, // set the number of transactions that can be pending in the background
	};
	ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

	ESP_LOGI(TAG, "Install led strip encoder");
	// rmt_encoder_handle_t led_encoder = NULL;
	led_strip_encoder_config_t encoder_config = {
		.resolution = RMT_LED_STRIP_RESOLUTION_HZ,
	};
	ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

	ESP_LOGI(TAG, "Enable RMT TX channel");
	ESP_ERROR_CHECK(rmt_enable(led_chan));

	ESP_LOGI(TAG, "Start LED rainbow chase");
}

void WS2812_On(int Red, int Green, int Blue)
{
	RGB_Color_Set(Red, Green, Blue);
	ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_data, sizeof(led_data), &tx_config));
	ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}

void WS2812_Breath(int h, int s)
{
	for (int i = 0; i < sizeof(index_wave) / sizeof(index_wave[0]); i++)
	{
		led_strip_hsv2rgb(h, s, index_wave[i], &red, &green, &blue);
		RGB_Color_Set(red, green, blue);
		// Flush RGB values to LEDs
		ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_data, sizeof(led_data), &tx_config));
		ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
		vTaskDelay(pdMS_TO_TICKS(Breath_SPEED_MS));
	}
}

void WS2812_Water(int h, int s, int v)
{
	for (int i = 0; i < LED_NUMBERS; i++)
	{
		memset(led_data, 0, sizeof(led_data));
		led_strip_hsv2rgb(h, s, v, &red, &green, &blue);
		// RGB_Color_Set(red, green, blue);
		led_data[i * 3] = green;
		led_data[i * 3 + 1] = red;
		led_data[i * 3 + 2] = blue;
		ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_data, sizeof(led_data), &tx_config));
		ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
		vTaskDelay(pdMS_TO_TICKS(Water_SPEED_MS));
	}
}

void WS2812_Color_Change_Water()
{
	for (int i = 0; i < sizeof(Color) / (sizeof(Color[0]) * 3); i++)
	{
		memset(led_data, 0, sizeof(led_data));
		led_data[Color_Change_Water_Count * 3] = Color[i * 3 + 1];	   // R
		led_data[Color_Change_Water_Count * 3 + 1] = Color[i * 3];	   // G
		led_data[Color_Change_Water_Count * 3 + 2] = Color[i * 3 + 2]; // B
		ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_data, sizeof(led_data), &tx_config));
		ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
		vTaskDelay(pdMS_TO_TICKS(Water_SPEED_MS));
		Color_Change_Water_Count++;
		if (Color_Change_Water_Count >= LED_NUMBERS)
		{
			Color_Change_Water_Count = 0;
		}
	}
}

void app_main(void)
{
	OLED_Init();
	OLED_ShowString(0, 0, (unsigned char *)("OLED Test"), 16);
	OLED_ShowString(0, 2, (unsigned char *)("ESP32-S3"), 16);
	// OLED_ShowNum(0, 2, 23333, 5, 16);

	// TaskHandle_t Task = NULL;
	// xTaskCreate(Task1, "Task1", 9012, NULL, 1, Task);

	RMT_WS2812_Init();
	// WS2812_On(255,0,0);
	while (1)
	{

		// WS2812_Breath(300, 100);
		// WS2812_Water(100, 100, 100);
		WS2812_Color_Change_Water();
	}
}