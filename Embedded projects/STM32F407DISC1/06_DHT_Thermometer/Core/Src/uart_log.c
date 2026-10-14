/**
  ******************************************************************************
  * @file    uart_log.c
  * @brief   Передача текста через USART2 (только TX, регистры напрямую).
  ******************************************************************************
  */
#include "uart_log.h"

void uart_log_init(uint32_t baud)
{
    /* 1) Включить тактирование: GPIOA (шина AHB1) и USART2 (шина APB1). */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;                         /* небольшая задержка после включения */

    /* 2) PA2 -> альтернативная функция AF7 (USART2_TX). */
    GPIOA->MODER   &= ~(3U << (2U * 2U));
    GPIOA->MODER   |=  (2U << (2U * 2U));       /* 10 = alternate function */
    GPIOA->AFR[0]  &= ~(0xFU << (2U * 4U));
    GPIOA->AFR[0]  |=  (7U   << (2U * 4U));     /* AF7 = USART2 */

    /* 3) Скорость: при OVER8=0 значение BRR = f_PCLK1 / baud. */
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    USART2->BRR = (pclk1 + (baud / 2U)) / baud;

    /* 4) Включить передатчик и сам USART. */
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart_log_char(char c)
{
    while ((USART2->SR & USART_SR_TXE) == 0U) { }   /* ждём, пока регистр данных освободится */
    USART2->DR = (uint8_t)c;
}

void uart_log_str(const char *s)
{
    while (*s != '\0')
    {
        uart_log_char(*s++);
    }
}

void uart_log_fixed1(int32_t value_x10)
{
    char buf[12];
    uint8_t n = 0;

    if (value_x10 < 0)
    {
        uart_log_char('-');
        value_x10 = -value_x10;
    }

    uint32_t whole = (uint32_t)value_x10 / 10U;
    uint32_t frac  = (uint32_t)value_x10 % 10U;

    if (whole == 0U)
    {
        buf[n++] = '0';
    }
    while (whole > 0U)                       /* цифры получаются в обратном порядке */
    {
        buf[n++] = (char)('0' + (whole % 10U));
        whole /= 10U;
    }
    while (n > 0U)
    {
        uart_log_char(buf[--n]);
    }

    uart_log_char('.');
    uart_log_char((char)('0' + frac));
}
