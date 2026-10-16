#ifndef QABES_AEAD_H
#define QABES_AEAD_H
/* ChaCha20-Poly1305 AEAD (RFC 8439), small and allocation-free for the
 * Qabes remote channel. In-place operation (out == in) is supported. */
#include <stddef.h>
#include <stdint.h>
#define QABES_KEY_BYTES 32
#define QABES_NONCE_BYTES 12
#define QABES_TAG_BYTES 16
void qabes_chacha20(uint8_t *out,const uint8_t *in,size_t size,const uint8_t key[32],const uint8_t nonce[12],uint32_t counter);
void qabes_poly1305(uint8_t tag[16],const uint8_t *message,size_t size,const uint8_t key[32]);
/* Encrypts text in place and writes the tag. 0 if the cipher is unavailable. */
int qabes_aead_seal(uint8_t *text,size_t size,const uint8_t *aad,size_t aad_size,const uint8_t key[32],const uint8_t nonce[12],uint8_t tag[16]);
/* Checks the tag in constant time, then decrypts in place. 1 = authentic. */
int qabes_aead_open(uint8_t *text,size_t size,const uint8_t *aad,size_t aad_size,const uint8_t key[32],const uint8_t nonce[12],const uint8_t tag[16]);
/* 1 when the implementation passed its RFC 8439 self-test (always 1 in software). */
int qabes_aead_ready(void);
#endif
