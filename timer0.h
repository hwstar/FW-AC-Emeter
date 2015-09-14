#ifndef TIMER0_H
#define TIMER0_H

extern volatile uint64_t timer0_ticks64;

void timer0_future_ms(uint32_t msec, uint64_t *future);
int timer0_test_future_ms(uint64_t *future);
void timer0_delay_ms(uint32_t value);
void timer0_elapsed_time(char *elap, uint8_t size);

#endif
