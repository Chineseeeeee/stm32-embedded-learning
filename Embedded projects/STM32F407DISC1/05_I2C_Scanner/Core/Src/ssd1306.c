#include "ssd1306.h"
#include <string.h>

#define SSD1306_ADDR    (0x3C << 1)  /* 7-битный адрес, сдвинутый для HAL */
#define CTRL_CMD        0x00         /* контрольный байт: дальше команды */
#define CTRL_DATA       0x40         /* контрольный байт: дальше данные GDDRAM */
#define I2C_TIMEOUT_MS  1000

static I2C_HandleTypeDef *s_i2c;

/* Видеобуфер: 128 столбцов x 8 страниц, 1 байт = 8 вертикальных пикселей */
static uint8_t s_fb[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

/* Отправить одну команду: [адрес] [0x00] [cmd]
   HAL_I2C_Mem_Write сам подставляет контрольный байт как "адрес регистра". */
static HAL_StatusTypeDef write_cmd(uint8_t cmd)
{
    return HAL_I2C_Mem_Write(s_i2c, SSD1306_ADDR, CTRL_CMD,
                             I2C_MEMADD_SIZE_8BIT, &cmd, 1, I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef ssd1306_init(I2C_HandleTypeDef *hi2c)
{
    static const uint8_t init_seq[] = {
        0xAE,        /* дисплей выключен на время настройки */
        0x20, 0x00,  /* горизонтальная адресация: после конца строки - следующая страница */
        0x8D, 0x14,  /* включить charge pump (без этого экран чёрный) */
        0xA1,        /* отзеркалить по горизонтали (столбец 127 -> SEG0) */
        0xC8,        /* отзеркалить по вертикали (сканирование COM снизу вверх) */
        0xAF,        /* дисплей включён */
    };

    s_i2c = hi2c;
    HAL_Delay(100);  /* дать дисплею проснуться после подачи питания */

    for (uint32_t i = 0; i < sizeof(init_seq); i++) {
        HAL_StatusTypeDef st = write_cmd(init_seq[i]);
        if (st != HAL_OK) {
            return st;
        }
    }

    ssd1306_fill(0);
    return ssd1306_update();
}

void ssd1306_fill(uint8_t color)
{
    memset(s_fb, color ? 0xFF : 0x00, sizeof(s_fb));
}

void ssd1306_set_pixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return;  /* за пределами экрана - игнорируем, чтобы не испортить память */
    }

    uint16_t index = x + (y / 8) * SSD1306_WIDTH;  /* какой байт */
    uint8_t  bit   = 1 << (y % 8);                 /* какой бит в нём */

    if (color) {
        s_fb[index] |= bit;
    } else {
        s_fb[index] &= ~bit;
    }
}

HAL_StatusTypeDef ssd1306_update(void)
{
    /* Окно записи = весь экран: столбцы 0..127, страницы 0..7 */
    static const uint8_t window[] = { 0x21, 0, 127, 0x22, 0, 7 };

    for (uint32_t i = 0; i < sizeof(window); i++) {
        HAL_StatusTypeDef st = write_cmd(window[i]);
        if (st != HAL_OK) {
            return st;
        }
    }

    /* Одна транзакция: [адрес] [0x40] [1024 байта буфера] */
    return HAL_I2C_Mem_Write(s_i2c, SSD1306_ADDR, CTRL_DATA,
                             I2C_MEMADD_SIZE_8BIT, s_fb, sizeof(s_fb),
                             I2C_TIMEOUT_MS);
}
