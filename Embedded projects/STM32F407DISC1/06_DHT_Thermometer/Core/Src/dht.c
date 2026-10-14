/**
  ******************************************************************************
  * @file    dht.c
  * @brief   Драйвер DHT11 / DHT22. Описание протокола - в dht.h.
  ******************************************************************************
  */
#include "dht.h"

#define CYCLES_PER_US()   (SystemCoreClock / 1000000U)
#define TIMEOUT_FAIL      0xFFFFFFFFU

/* ---- Счётчик тактов ядра (DWT) ------------------------------------------ */

static void DWT_Enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   /* включить блок трассировки */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;             /* запустить счётчик тактов */
}

/* ---- Работа с линией ----------------------------------------------------- */

static inline void line_low(const DHT_Handle *d)      { d->port->BSRR = (uint32_t)d->pin << 16U; }
static inline void line_release(const DHT_Handle *d)  { d->port->BSRR = d->pin; }
static inline uint32_t line_is_high(const DHT_Handle *d) { return (d->port->IDR & d->pin) != 0U; }

/*
 * Ждёт, пока линия примет уровень `level` (1 или 0).
 * Возвращает, сколько микросекунд прошло, или TIMEOUT_FAIL, если не дождались
 * за timeout_us.
 */
static uint32_t wait_for_level(const DHT_Handle *d, uint32_t level, uint32_t timeout_us)
{
    const uint32_t cpu_per_us = CYCLES_PER_US();
    const uint32_t limit      = timeout_us * cpu_per_us;
    const uint32_t start      = DWT->CYCCNT;

    while (line_is_high(d) != level)
    {
        if ((DWT->CYCCNT - start) > limit)
        {
            return TIMEOUT_FAIL;
        }
    }
    return (DWT->CYCCNT - start) / cpu_per_us;
}

/* ---- Публичный интерфейс ------------------------------------------------- */

void DHT_Init(DHT_Handle *dht, GPIO_TypeDef *port, uint16_t pin, DHT_Type type)
{
    dht->port     = port;
    dht->pin      = pin;
    dht->type     = type;
    dht->temp_x10 = 0;
    dht->hum_x10  = 0;

    DWT_Enable();
    line_release(dht);   /* линия в покое = высокий уровень (подтяжка) */
}

DHT_Status DHT_Read(DHT_Handle *dht)
{
    uint8_t  data[5] = {0, 0, 0, 0, 0};
    uint32_t t;

    /* 1) Сигнал "старт": прижать линию к земле.
     *    DHT11 - не меньше 18 мс, DHT22 - 1..20 мс. HAL_Delay даёт +0..1 мс. */
    line_low(dht);
    HAL_Delay((dht->type == DHT_TYPE_DHT11) ? 20U : 2U);

    /* Дальше важны микросекунды: на время обмена (~5 мс) запрещаем прерывания. */
    __disable_irq();

    /* 2) Отпустить линию - подтяжка поднимет её, датчик "проснётся". */
    line_release(dht);

    /* 3) Ответ датчика: ~80 мкс низкий уровень, ~80 мкс высокий. */
    if (wait_for_level(dht, 0U, 100U) == TIMEOUT_FAIL)    /* ждём, пока он потянет вниз */
    {
        __enable_irq();
        return DHT_ERR_NO_RESPONSE;
    }
    if (wait_for_level(dht, 1U, 120U) == TIMEOUT_FAIL)    /* конец низкого импульса ответа */
    {
        __enable_irq();
        return DHT_ERR_NO_RESPONSE;
    }
    if (wait_for_level(dht, 0U, 120U) == TIMEOUT_FAIL)    /* конец высокого импульса ответа */
    {
        __enable_irq();
        return DHT_ERR_NO_RESPONSE;
    }

    /* 4) 40 бит данных. Каждый бит: ~50 мкс низкий уровень (разделитель), потом
     *    высокий: ~26 мкс = "0", ~70 мкс = "1". Порог между ними - 40 мкс. */
    for (uint8_t i = 0; i < 40U; i++)
    {
        if (wait_for_level(dht, 1U, 100U) == TIMEOUT_FAIL)   /* конец разделителя */
        {
            __enable_irq();
            return DHT_ERR_TIMEOUT;
        }

        t = wait_for_level(dht, 0U, 120U);                  /* длительность "высокого" */
        if (t == TIMEOUT_FAIL)
        {
            __enable_irq();
            return DHT_ERR_TIMEOUT;
        }

        data[i / 8U] <<= 1U;
        if (t > 40U)
        {
            data[i / 8U] |= 1U;
        }
    }

    __enable_irq();

    /* 5) Контрольная сумма: сумма первых 4 байт (младший байт) == 5-й байт. */
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
    {
        return DHT_ERR_CHECKSUM;
    }

    /* 6) Разбор данных. */
    int16_t  temp;
    uint16_t hum;

    if (dht->type == DHT_TYPE_DHT22)
    {
        /* DHT22: два 16-битных числа в десятых долях. Старший бит температуры - знак. */
        hum  = (uint16_t)((data[0] << 8) | data[1]);
        temp = (int16_t)(((data[2] & 0x7F) << 8) | data[3]);
        if (data[2] & 0x80)
        {
            temp = -temp;
        }
        if (hum > 1000 || temp < -400 || temp > 800)
        {
            return DHT_ERR_RANGE;
        }
    }
    else
    {
        /* DHT11: целая часть в data[0]/data[2], дробная (обычно 0) в data[1]/data[3]. */
        uint8_t frac_t = data[3] & 0x7F;
        uint8_t frac_h = data[1];
        if (frac_t > 9U) { frac_t = 0U; }
        if (frac_h > 9U) { frac_h = 0U; }

        hum  = (uint16_t)(data[0] * 10U + frac_h);
        temp = (int16_t)(data[2] * 10 + frac_t);
        if (data[3] & 0x80)
        {
            temp = -temp;
        }
        if (hum > 1000 || temp < -200 || temp > 600)
        {
            return DHT_ERR_RANGE;
        }
    }

    dht->hum_x10  = hum;
    dht->temp_x10 = temp;
    return DHT_OK;
}

const char *DHT_StatusText(DHT_Status status)
{
    switch (status)
    {
        case DHT_OK:              return "ok";
        case DHT_ERR_NO_RESPONSE: return "no response (check wiring / power / pull-up)";
        case DHT_ERR_TIMEOUT:     return "timeout during transfer";
        case DHT_ERR_CHECKSUM:    return "checksum mismatch";
        case DHT_ERR_RANGE:       return "value out of range";
        default:                  return "unknown";
    }
}
