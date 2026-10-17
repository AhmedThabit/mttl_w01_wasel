#ifndef QABES_REMOTE_H
#define QABES_REMOTE_H
/* Qabes remote channel: end-to-end encrypted frames carried over a plain MQTT
 * broker (the chip has no RAM for TLS). Frame = 'Q' | nonce(12) | ciphertext | tag(16);
 * nonce = direction(1: up, 2: down) | 0 0 0 | stamp (u64 LE, ms since epoch, strictly
 * increasing per direction). AAD = the 13 header bytes. The key is the 64-hex-char MQTT
 * password, which is never sent to the broker in this mode.
 * Down (app -> strip, topic <base>/d): [1, channel 0..3 | 4 all, on], [2] (send status) or [3] (blink to find).
 * Up (strip -> app, topic <base>/u, retained): [0x80, uptime ms u32 LE, device JSON]. */
#include <stddef.h>
#include <stdint.h>
#define QABES_HEAD 13
#define QABES_OVERHEAD (QABES_HEAD+16)
#define QABES_UP 1
#define QABES_DOWN 2
#define QABES_MAX_SKEW_MS 300000u
#define QABES_OP_CONTROL 1
#define QABES_OP_REFRESH 2
#define QABES_OP_FIND 3
#define QABES_OP_STATUS 0x80
/* 1 when [hex] is exactly 64 hex digits; writes the 32-byte key. */
int qabes_key(const char *hex,uint8_t key[32]);
/* Plaintext is at frame+QABES_HEAD; seals in place, returns the frame size (0 = no cipher).
 * *last is the previous stamp of this direction and is advanced. */
size_t qabes_seal(uint8_t *frame,size_t plain,const uint8_t key[32],unsigned direction,uint64_t now_ms,uint64_t *last);
/* Opens a frame of [direction] in place. Rejects bad tags, replays (stamp <= *last) and,
 * when [time_valid], stamps more than QABES_MAX_SKEW_MS away from now; without a valid
 * clock nothing is accepted. On success *plain points into frame. */
int qabes_open(uint8_t *frame,size_t size,const uint8_t key[32],unsigned direction,uint64_t now_ms,int time_valid,uint64_t *last,uint8_t **plain,size_t *plain_size);
/* Broker login for a Wasel cloud broker, so the frame key itself never leaves the strip:
 * auth = ChaCha20(key, nonce "WAUTH"0..0) over 32 zero bytes (what the cloud stores),
 * token = hex of ChaCha20(auth, nonce "WTOK" | minute u64 LE) over 16 zero bytes, where
 * minute = unix_ms / 60000. 32 lowercase hex digits + NUL. */
void qabes_auth_key(const uint8_t key[32],uint8_t auth[32]);
void qabes_broker_token(const uint8_t key[32],uint64_t unix_ms,char out[33]);
/* Parses a down command. 1 control (channel/on set), 2 refresh, 3 find, 0 invalid. */
int qabes_command(const uint8_t *plain,size_t size,uint8_t *channel,uint8_t *on);
#endif
