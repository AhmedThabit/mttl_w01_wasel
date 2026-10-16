**Wasel 2.0.14-w2** — based on upstream standalone 2.0.14.

New in w2:
- LED options: night mode (all indicator LEDs dark, manual or on a nightly window),
  "only on LEDs", and *find the strip* (all outlet LEDs blink for about 5 s).
- Child lock: with every button locked, the 10 s setup hold is refused too, except in the
  first minute after power-up so Wi-Fi can always be reset.
- The device web page is English-only to free RAM (the image runs from SRAM).
- Settings are stored as body 0x57 (v7 + LED options). Installing upstream firmware afterwards
  may reset settings, as with any upstream downgrade.

From w1: `wasel-remote` encrypted remote control; `wasel-ha` Home Assistant MQTT Discovery.
Check the SHA-256 sums in `SHA256SUMS`. A failed install may need SWD recovery; see README.
