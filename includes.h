#ifndef INCLUDES_H
#define INCLUDES_H


#if defined(__AVR__)
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "defs.h"
#include "pins.h"
#include "u8g.h"
#include "em.h"
#include "uart.h"
#include "uartstream.h"
#include "button.h"


#endif
