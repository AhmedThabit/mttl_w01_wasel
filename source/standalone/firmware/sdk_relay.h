#ifndef MTTL_SDK_RELAY_H
#define MTTL_SDK_RELAY_H
#include "hardware_port.h"
/* Experimental command state, not mechanical contact feedback. */
void mttl_sdk_relay_init(mttl_hardware_port *);
/* Wasel LED policy applied to every frame on the bus: bit0 dark, bit1 hide "off" LEDs.
 * mttl_led_raw bypasses it for button-lock / find flashes. Coil bits are never touched. */
extern volatile uint8_t mttl_led_policy,mttl_led_raw;
#endif
