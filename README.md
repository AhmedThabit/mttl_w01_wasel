# Wasel — MTTL-W01 firmware

Upstream documentation: [English](README_upstream_EN.md) · [한국어](README_upstream_KO.md)

Wasel is a fork of [af950833/mttl_w01_standalone](https://github.com/af950833/mttl_w01_standalone) 2.0.14
for the LG U+ MTTL-W01 smart power strip (Realtek RTL8710 / Ameba1). Everything the upstream
standalone firmware does is kept; Wasel adds **end-to-end encrypted control from anywhere** with no
server of your own and no extra device at home. Same MIT license; the original copyright stays.

---

## Variants
| File | Contents | For |
|---|---|---|
| `wasel-remote-<version>.fwr` | Encrypted control from anywhere | Reaching the strip away from home |
| `wasel-ha-<version>.fwr` | Home Assistant MQTT Discovery as upstream | Home Assistant users |

Both cannot fit together: the RTL8710 runs the whole image from SRAM and the remote build leaves 88 bytes
of static headroom. Switch between them any time from the strip's Firmware tab.

## Remote control without TLS
The chip has no room for TLS (about 60 KB of code would run from RAM, plus around 30 KB of heap during a handshake).
Instead, every MQTT payload is sealed with **ChaCha20-Poly1305 (RFC 8439)** under a 256-bit key that only the
phone and the strip know:

- The cipher calls ChaCha20 and Poly1305 in the chip's mask ROM, after a self-test against the RFC 8439 vectors
  (reported as `"qabes_aead":1` in `/api/memory`; if it fails, the channel stays disabled).
- Stamps are strictly increasing milliseconds and must be within 5 minutes of the strip's NTP clock, which stops replays.
  The 96-bit random topic and the key (stored as the MQTT password) are never sent to the broker.
- The broker sees traffic volume, timing and the online/offline Last Will. It cannot read or forge anything.

Configuration is written once over the LAN through the normal MQTT settings: broker (host name or IP), port 1883,
topic `wasel/<24 hex>`, password = 64-hex key, enabled. The frame format and topics are described in
[`qabes_remote.h`](source/standalone/firmware/qabes_remote.h). Host tests (RFC vectors, frame rules and the full MQTT engine
flow against a mock broker) run in CI.

## LED options and child lock (both variants, from 2.0.14-w2)
- **Night mode**: all indicator LEDs dark, switched manually or every night between two local times
  (the schedule's time zone). Outlets keep working.
- **Only "on" lights**: outlets that are off stay dark instead of showing their off colour.
- **Find the strip**: every outlet LED blinks for about 5 s (`POST /api/control` with `[5, 0]`, or remote op `[3]`).
- **Child lock also blocks the setup hold**: with every button locked, holding the main button for 10 s
  no longer opens Wi-Fi setup, except during the first minute after power-up, so Wi-Fi can always be reset.
- The device web page is English-only to free image RAM (the image runs from SRAM); build with
  `WASEL_KOREAN=1` to keep Korean.
- Stored as settings body `0x57` (v7 + LED byte + night window). Saves in the upstream v7 format, such as
  the web page, keep these options. Installing upstream firmware afterwards may reset settings, as any
  upstream downgrade can.

## Verified on hardware
- Builds by CI with the same GCC 4.8.3-2014q1 and Ameba1 SDK, web OTA install, and behaviour identical to 2.0.14.
- ROM cipher self-test passes.
- DNS lookup of the broker and encrypted status every 30 s.
- A remote command switched an outlet, and the auto-OFF timer then cut it.
- Stale, forged and replayed frames were all rejected while the session stayed up.

## Limits
- A public broker has no uptime guarantee; schedules, timers and protection keep running on the strip regardless. You can point it at your own broker.
- The LAN web/API stays unauthenticated, as upstream.
- As upstream, a failed install may need SWD recovery.
- Turn remote control off before installing other firmware, so the key is not left behind as an ordinary MQTT password.

## Build
GitHub Actions builds both variants on every push. A tag `v<version>-w<n>` publishes a release with SHA-256 sums.
With MTTL-Builder, set `QABES_REMOTE=1` in the environment for the remote variant.

## License
MIT — see [LICENSE](LICENSE) and [LICENSING.md](LICENSING.md). Original work © af950833; Wasel modifications © Ahmed Thabit.
Third-party SDK/compiler terms are unchanged ([THIRD_PARTY_NOTICES](builder/THIRD_PARTY_NOTICES.md)).
