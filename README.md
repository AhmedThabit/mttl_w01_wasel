# واصل · Wasel — MTTL-W01 firmware

[العربية](#العربية) · [English](#english) · Upstream: [English](README_upstream_EN.md) · [한국어](README_upstream_KO.md)

Wasel is a fork of [af950833/mttl_w01_standalone](https://github.com/af950833/mttl_w01_standalone) 2.0.14
for the LG U+ MTTL-W01 smart power strip (Realtek RTL8710 / Ameba1). Everything the upstream
standalone firmware does is kept; Wasel adds **end-to-end encrypted control from anywhere** with no
server of your own and no extra device at home. Same MIT license; the original copyright stays.

---

## العربية

### ده إيه؟
«واصل» نسخة متفرّعة من فيرموير [mttl_w01_standalone](https://github.com/af950833/mttl_w01_standalone)
بتاع af950833 (نسخة 2.0.14) للمشترك الذكي LG U+ MTTL-W01. كل مميزات الفيرموير الأصلي موجودة زي ما هي،
وزوّدنا عليها **التحكم من أي مكان بتشفير كامل** من غير سيرفر خاص بيك ومن غير أي جهاز زيادة في البيت.

### النسختين
| الملف | فيه إيه | يناسب مين |
|---|---|---|
| `wasel-remote-<version>.fwr` | التحكم المشفّر من أي مكان | اللي عايز يتحكم في المشترك وهو برّه البيت |
| `wasel-ha-<version>.fwr` | Home Assistant MQTT Discovery زي الأصلي | اللي عنده Home Assistant في البيت |

الاتنين مايتجمعوش في نسخة واحدة: الشريحة بتشغّل الكود كله من الرام، وبعد إضافة الريموت فاضل 88 بايت بس.
تقدر تبدّل بين النسختين في أي وقت من تبويب Firmware في صفحة المشترك.

### التحكم من أي مكان — إزاي بيشتغل
- المشترك بيتصل بسيرفر MQTT عام ومجاني (الافتراضي `broker.emqx.io:1883`). الشريحة مافيهاش رام تكفّي TLS
  (الكود كله شغال من الرام، والرام اللي بيتحجز وقت التشغيل بيقلّ في الاستخدام الطويل)، فبدل TLS **كل رسالة متشفّرة
  وموقّعة بـ ChaCha20-Poly1305 (RFC 8439)** بمفتاح 256-bit محدش يعرفه غير التطبيق والمشترك.
- التشفير بيستخدم دوال ChaCha20 و Poly1305 الموجودة في ROM الشريحة نفسها، بعد ما المشترك يتأكد منها
  أوتوماتيك بمتجهات RFC 8439. لو الاختبار فشل، القناة بتفضل مقفولة. نتيجة الاختبار بتظهر في `/api/memory` تحت `"qabes_aead":1`.
- كل رسالة فيها ختم وقت بالمللي ثانية، لازم يزيد دايماً، وفرقه عن ساعة المشترك (NTP) مايزيدش عن 5 دقايق.
  ده بيمنع إعادة إرسال أي رسالة قديمة، وأي رسالة اتغيّر فيها بايت واحد بتترفض.
- الـ topic عشوائي 96-bit (`wasel/<24 hex>`)، والمفتاح عمره ما بيتبعت للسيرفر.
- **السيرفر بيشوف:** إن فيه رسايل بتروح وتيجي، وحجمها، ومواعيدها، و«online/offline» بتاعة المشترك. **مابيشوفش:** أي محتوى، ومايقدرش يبعت أوامر.

### الإعداد
الإعداد بيتعمل مرة واحدة من الشبكة المحلية، عن طريق إعدادات MQTT العادية في `/api/config` (التطبيق بيعملها لوحده):

| الحقل | القيمة |
|---|---|
| Broker | `broker.emqx.io` (أو أي سيرفر MQTT، بالاسم أو IP) |
| Port | `1883` |
| Topic | `wasel/<24 hex عشوائي>` |
| Password | المفتاح: 64 حرف hex (عمره ما بيتبعت للسيرفر) |
| Enabled | ✓ |

البروتوكول (لو عايز تكتب عميل بنفسك):
- الإطار: `'Q' | nonce(12) | ciphertext | tag(16)`، والـ nonce عبارة عن `00 00 00 00 | stamp (7 bytes LE, ms) | direction`، والـ AAD هو أول 13 بايت.
- `<topic>/d` (من التطبيق للمشترك، direction=2): `[1, ch(0..3 / 4=الكل), on]` أو `[2]` = ابعت الحالة دلوقتي.
- `<topic>/u` (من المشترك للتطبيق، direction=1، retained): `[0x80, uptime_ms u32 LE, JSON بنفس شكل /api/device]`. بيتبعت فوراً لما عين تتغيّر، أو عند الطلب، وكل 30 ثانية.
- `<topic>/availability`: `online` / `offline` (Last Will).

التفاصيل والاختبارات: [`source/standalone/firmware/qabes_remote.h`](source/standalone/firmware/qabes_remote.h)، و[`source/standalone/tests/`](source/standalone/tests/).

### اللي اتجرّب على مشترك حقيقي
- البناء بالـ CI (نفس GCC 4.8.3 ونفس الـ SDK) والتركيب من الويب، وأداء الفيرموير زي 2.0.14 بالظبط.
- اختبار التشفير في ROM: `qabes_aead=1`.
- الاتصال بـ broker.emqx.io بالاسم (DNS)، واستقبال حالة مشفّرة كل 30 ثانية وفك تشفيرها.
- أمر مشفّر من برّه الشبكة شغّل عين، والمؤقت قفلها بعد دقيقة.
- رسالة قديمة، ورسالة بمفتاح غلط، وإعادة إرسال نفس الأمر: الكل اترفض، والاتصال فضل شغال.

### حدود لازم تعرفها
- السيرفر العام مالوش أي ضمان إنه يفضل شغال. الجداول والمؤقت والحماية بيشتغلوا جوه المشترك من غير إنترنت. وتقدر تستخدم سيرفر MQTT بتاعك بدل العام.
- واجهة الويب والـ API على الشبكة المحلية من غير كلمة سر، زي الفيرموير الأصلي.
- زي الأصلي: لو التركيب فشل، ممكن تحتاج مبرمج SWD عشان ترجّع المشترك. استخدمه على مسؤوليتك.
- قبل ما تركّب فيرموير مختلف (الأصلي أو نسخة Home Assistant)، اقفل الريموت عشان المفتاح مايفضلش متخزّن ككلمة سر MQTT عادية. تطبيق واصل بيعمل ده لوحده.

### التركيب
- **من فيرموير Standalone:** افتح تبويب Firmware في صفحة المشترك، وارفع ملف `.fwr`.
- **من الفيرموير الأصلي:** استخدم أداة OTA المذكورة في [README الأصلي](README_upstream_EN.md#initial-installation-windows--macos-ota-tool)، وحط ملف Wasel بدل الملف الأصلي.
- **البناء بنفسك:** على GitHub Actions كل push بيبني النسختين. ولو بتستخدم MTTL-Builder، شغّله ومعاه `QABES_REMOTE=1` في متغيرات البيئة عشان تطلع نسخة الريموت.

---

## English

### Variants
| File | Contents | For |
|---|---|---|
| `wasel-remote-<version>.fwr` | Encrypted control from anywhere | Reaching the strip away from home |
| `wasel-ha-<version>.fwr` | Home Assistant MQTT Discovery as upstream | Home Assistant users |

Both cannot fit together: the RTL8710 runs the whole image from SRAM and the remote build leaves 88 bytes
of static headroom. Switch between them any time from the strip's Firmware tab.

### Remote control without TLS
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

### Verified on hardware
- Builds by CI with the same GCC 4.8.3-2014q1 and Ameba1 SDK, web OTA install, and behaviour identical to 2.0.14.
- ROM cipher self-test passes.
- DNS lookup of the broker and encrypted status every 30 s.
- A remote command switched an outlet, and the auto-OFF timer then cut it.
- Stale, forged and replayed frames were all rejected while the session stayed up.

### Limits
- A public broker has no uptime guarantee; schedules, timers and protection keep running on the strip regardless. You can point it at your own broker.
- The LAN web/API stays unauthenticated, as upstream.
- As upstream, a failed install may need SWD recovery.
- Turn remote control off before installing other firmware, so the key is not left behind as an ordinary MQTT password.

### Build
GitHub Actions builds both variants on every push. A tag `v<version>-w<n>` publishes a release with SHA-256 sums.
With MTTL-Builder, set `QABES_REMOTE=1` in the environment for the remote variant.

## License
MIT — see [LICENSE](LICENSE) and [LICENSING.md](LICENSING.md). Original work © af950833; Wasel modifications © Ahmed Thabit.
Third-party SDK/compiler terms are unchanged ([THIRD_PARTY_NOTICES](builder/THIRD_PARTY_NOTICES.md)).
