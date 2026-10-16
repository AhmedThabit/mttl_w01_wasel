#ifndef MTTL_SETTINGS_H
#define MTTL_SETTINGS_H
#include <stddef.h>
#include <stdint.h>
#include "protection.h"
#include "scheduler.h"
typedef struct {
    char device[65],channel[4][65],ssid[33],wifi_password[65];
    char broker[254],mqtt_user[65],mqtt_password[65],topic[65];
    uint16_t port;
    uint8_t language,mqtt_enabled;
    uint8_t button_lock; /* Bit 0..3: inhibit physical channel buttons only. */
    uint16_t timer_minutes[4]; /* 0 unregistered, 1..1440 automatic OFF. */
    /* Wasel: LED options (MTTL_LED_*) and the night window in local minutes [start,end). */
    uint8_t leds;uint16_t night[2];
    /* Device-owned checkpoint; excluded from web configuration encoding. */
    uint32_t energy_raw[4],energy_wh[4];
    uint8_t energy_valid;
    uint8_t relay_known,relay_on; /* Last completed command, NOT contact feedback. */
    mttl_protection_config protection;
    mttl_schedule_config schedule;
} mttl_settings;
/* Only settings affecting the broker session or Discovery require reconnect. */
int mttl_settings_mqtt_changed(const mttl_settings *,const mttl_settings *);
#define MTTL_LED_NIGHT 1          /* all indicator LEDs dark */
#define MTTL_LED_HIDE_OFF 2       /* only "on" LEDs lit */
#define MTTL_LED_NIGHT_WINDOW 4   /* dark between night[0] and night[1] local time */
#define MTTL_LED_HOLD_LOCK 8      /* child lock (all buttons locked) also blocks the 10 s setup hold, except in the first minute after power-up */
#define MTTL_LED_ALL 15
/* Wasel body version: v7 + leds + night[2]. Distinct from upstream numbering. */
#define MTTL_SETTINGS_WASEL 0x57
/* Versioned length-prefixed encoding, never raw C structure/padding. */
#define MTTL_SETTINGS_BODY_MAX 1024
void mttl_settings_default(mttl_settings *s);
int mttl_settings_valid(const mttl_settings *s);
int mttl_settings_utf8(const char *,size_t capacity);
size_t mttl_settings_pack(const mttl_settings *s,uint8_t *out,size_t capacity);
int mttl_settings_unpack(const uint8_t *body,size_t size,mttl_settings *out);
size_t mttl_settings_storage_pack(const mttl_settings *,uint8_t *,size_t);
int mttl_settings_storage_unpack(const uint8_t *,size_t,mttl_settings *);
void mttl_settings_keep_energy(mttl_settings *,const mttl_settings *);
#endif
