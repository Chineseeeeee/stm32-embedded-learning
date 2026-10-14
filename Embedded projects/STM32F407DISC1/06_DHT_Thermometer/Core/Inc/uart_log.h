/**
  ******************************************************************************
  * @file    uart_log.h
  * @brief   Мини-вывод текста в USART2 (пин PA2 = TX) без HAL и без printf.
  *          Подключение: PA2 -> RX USB-UART адаптера, GND -> GND.
  *          Терминал на ПК: 115200 бод, 8N1.
  ******************************************************************************
  */
#ifndef UART_LOG_H
#define UART_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

void uart_log_init(uint32_t baud);
void uart_log_str(const char *s);
void uart_log_fixed1(int32_t value_x10);   /* 245 -> "24.5", -37 -> "-3.7" */

#ifdef __cplusplus
}
#endif

#endif /* UART_LOG_H */
