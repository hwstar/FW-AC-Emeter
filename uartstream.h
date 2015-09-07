#ifndef UARTSTREAM_H
#define UARTSTREAM_H

#include <stdio.h>
#include "uart.h"

FILE *uartstream_init(uint32_t baudrate);

#endif

