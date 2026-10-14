#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

/* Инициализация дисплея. Возвращает HAL_OK, если дисплей ответил на все команды. */
HAL_StatusTypeDef ssd1306_init(I2C_HandleTypeDef *hi2c);

/* Залить весь буфер: color = 1 -> все пиксели горят, 0 -> все погашены.
   Меняет только буфер в RAM, на экран ничего не уходит. */
void ssd1306_fill(uint8_t color);

/* Зажечь (1) или погасить (0) один пиксель в буфере. */
void ssd1306_set_pixel(uint8_t x, uint8_t y, uint8_t color);

/* Отправить весь буфер (1024 байта) на дисплей. */
HAL_StatusTypeDef ssd1306_update(void);

#endif /* SSD1306_H */
