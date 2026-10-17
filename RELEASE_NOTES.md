**Wasel 2.0.14-w4-rc3** — based on upstream standalone 2.0.14.

New in w4:
- Watchdog fed only by full worker passes (meter, timers, status): a loop that spins without
  doing the real work now ends in a reset after 30 s instead of a silent freeze.
  `/api/memory` reports `worker_max_gap_ms`, the longest gap between full passes since boot.
- Local PIN (4–8 digits) for the strip's write API: switching outlets, saving settings, Wi-Fi
  and firmware upload need header `X-Wasel-Pin` once a PIN is set (`POST /api/pin`, body = new
  PIN, empty = remove). Reading stays open. Five wrong PINs lock the API for 60 s. Holding the main
  button 10 s (Wi-Fi setup) clears the PIN, so a forgotten PIN never locks you out; it is not
  asked in the setup access point either. The device
  web page asks for the PIN once per session. The PIN is stored in flash only (record 0x5C),
  never in the settings body or any response; `/api/memory` reports `"pin":1` when one is set.

**Wasel 2.0.14-w3**

Tested on the strip (MTTL-W01, remote build): readings with all four outlets on, find the
strip, encrypted remote control. The setup-hold lock on its own was not yet pressed by hand.

Changed in w3-rc3:
- Find the strip: every outlet LED and the main-button LED switch between their two colours
  (about 5 s) and the Wi-Fi LED blinks, instead of a plain blink of the outlet LEDs only.
  The button-lock feedback stays a plain blink, so the two are easy to tell apart.

Changed in w3-rc2:
- The setup-hold lock works on its own: the 10 s hold of the main button is refused whether or
  not the outlet buttons are locked (still allowed in the first minute after power-up).

Fixed in w3:
- Remote build: with all four outlets on, readings and status stopped updating (the worker
  waited forever on a confirmation flag the encrypted status never cleared). Fixed, and the
  fast path that waits for MQTT confirmations is now bounded so it can never starve the meter.
- `/api/memory` reports `"wasel"` (release) and `"variant"` (`remote` or `ha`), so apps can
  tell Wasel builds from upstream 2.0.14.

From w2:
- LED options: night mode (all indicator LEDs dark, manual or on a nightly window),
  "only on LEDs", and *find the strip* (all outlet LEDs blink for about 5 s).
- Child lock: with every button locked, the 10 s setup hold is refused too, except in the
  first minute after power-up so Wi-Fi can always be reset.
- The device web page is English-only to free RAM (the image runs from SRAM).
- Settings are stored as body 0x57 (v7 + LED options). Installing upstream firmware afterwards
  may reset settings, as with any upstream downgrade.

From w1: `wasel-remote` encrypted remote control; `wasel-ha` Home Assistant MQTT Discovery.
Check the SHA-256 sums in `SHA256SUMS`. A failed install may need SWD recovery; see README.
