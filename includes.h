#ifndef INCLUDES_H
#define INCLUDES_H


#if defined(__AVR__)
#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#endif


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "defs.h"
#include "pins.h"
#include "u8g.h"
#include "jsmn.h"
#include "em.h"
#include "uart.h"
#include "uartstream.h"
#include "timer0.h"
#include "button.h"
#include "menu.h"

#endif
