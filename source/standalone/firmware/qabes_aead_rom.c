/* ChaCha20-Poly1305 on the RTL8710 mask ROM (BoringSSL-derived CRYPTO_* functions in
 * export-rom_v03.txt), so the cipher costs almost no image RAM. The ROM headers are not
 * published, so both historical ChaCha20 call forms are tried once against RFC 8439
 * vectors; if neither matches (or Poly1305 differs) the channel stays disabled.
 * Same API and results as qabes_aead.c, which is the reference used by the host tests. */
#include "qabes_aead.h"
#include <string.h>
extern void CRYPTO_chacha_20(uint8_t *out,const uint8_t *in,size_t in_len,const uint8_t key[32],const uint8_t *nonce,size_t counter);
/* BoringSSL declares the state as 512 opaque bytes. */
typedef struct{uint32_t align[128];} rom_poly_state;
extern void CRYPTO_poly1305_init(rom_poly_state *state,const uint8_t key[32]);
extern void CRYPTO_poly1305_update(rom_poly_state *state,const uint8_t *in,size_t in_len);
extern void CRYPTO_poly1305_finish(rom_poly_state *state,uint8_t mac[16]);
/* 0 untested, 1 IETF form (12-byte nonce), 2 original form (8-byte nonce), -1 unusable. */
static int8_t form;
/* Form 2 sees nonce[4..11] with a 64-bit counter whose high word is 0, which equals
 * the IETF state whenever nonce[0..3] is zero (always true for Qabes frames). */
static void chacha(int f,uint8_t *out,const uint8_t *in,size_t size,const uint8_t key[32],const uint8_t nonce[12],uint32_t counter){
    CRYPTO_chacha_20(out,in,size,key,f==2?nonce+4:nonce,counter);
}
void qabes_chacha20(uint8_t *out,const uint8_t *in,size_t size,const uint8_t key[32],const uint8_t nonce[12],uint32_t counter){chacha(form,out,in,size,key,nonce,counter);}
void qabes_poly1305(uint8_t tag[16],const uint8_t *m,size_t size,const uint8_t key[32]){
    rom_poly_state p;CRYPTO_poly1305_init(&p,key);CRYPTO_poly1305_update(&p,m,size);CRYPTO_poly1305_finish(&p,tag);memset(&p,0,sizeof(p));
}
int qabes_aead_ready(void){
    static const uint8_t chacha_ct[16]={0x6e,0x2e,0x35,0x9a,0x25,0x68,0xf9,0x80,0x41,0xba,0x07,0x28,0xdd,0x0d,0x69,0x81};
    static const uint8_t poly_key[32]={0x85,0xd6,0xbe,0x78,0x57,0x55,0x6d,0x33,0x7f,0x44,0x52,0xfe,0x42,0xd5,0x06,0xa8,0x01,0x03,0x80,0x8a,0xfb,0x0d,0xb2,0xfd,0x4a,0xbf,0xf6,0xaf,0x41,0x49,0xf5,0x1b};
    static const uint8_t poly_tag[16]={0xa8,0x06,0x1d,0xc1,0x30,0x51,0x36,0xc6,0xc2,0x2b,0x8b,0xaf,0x0c,0x01,0x27,0xa9};
    uint8_t key[32],nonce[12]={0,0,0,0,0,0,0,0x4a,0,0,0,0},out[16];unsigned i;int f;
    if(form)return form>0;
    for(i=0;i<32;i++)key[i]=(uint8_t)i;
    for(f=1;f<=2;f++){memcpy(out,"Ladies and Gentl",16);chacha(f,out,out,16,key,nonce,1);if(!memcmp(out,chacha_ct,16))break;}
    if(f<=2){qabes_poly1305(out,(const uint8_t *)"Cryptographic Forum Research Group",34,poly_key);if(memcmp(out,poly_tag,16))f=-1;}else f=-1;
    form=(int8_t)f;return f>0;
}
static void pad16(rom_poly_state *p,size_t size){static const uint8_t zero[16];if(size&15)CRYPTO_poly1305_update(p,zero,16-(size&15));}
static void mac(uint8_t tag[16],const uint8_t *text,size_t size,const uint8_t *aad,size_t aad_size,const uint8_t key[32],const uint8_t nonce[12]){
    uint8_t otk[64],lens[16];rom_poly_state p;
    memset(otk,0,sizeof(otk));qabes_chacha20(otk,otk,64,key,nonce,0);CRYPTO_poly1305_init(&p,otk);memset(otk,0,sizeof(otk));
    CRYPTO_poly1305_update(&p,aad,aad_size);pad16(&p,aad_size);CRYPTO_poly1305_update(&p,text,size);pad16(&p,size);
    memset(lens,0,sizeof(lens));lens[0]=(uint8_t)aad_size;lens[1]=(uint8_t)(aad_size>>8);lens[8]=(uint8_t)size;lens[9]=(uint8_t)(size>>8);
    CRYPTO_poly1305_update(&p,lens,16);CRYPTO_poly1305_finish(&p,tag);memset(&p,0,sizeof(p));
}
int qabes_aead_seal(uint8_t *text,size_t size,const uint8_t *aad,size_t aad_size,const uint8_t key[32],const uint8_t nonce[12],uint8_t tag[16]){
    if(!qabes_aead_ready())return 0;
    qabes_chacha20(text,text,size,key,nonce,1);mac(tag,text,size,aad,aad_size,key,nonce);return 1;
}
int qabes_aead_open(uint8_t *text,size_t size,const uint8_t *aad,size_t aad_size,const uint8_t key[32],const uint8_t nonce[12],const uint8_t tag[16]){
    uint8_t want[16];unsigned diff=0,i;
    if(!qabes_aead_ready())return 0;
    mac(want,text,size,aad,aad_size,key,nonce);for(i=0;i<16;i++)diff|=(unsigned)(want[i]^tag[i]);memset(want,0,sizeof(want));
    if(diff)return 0;
    qabes_chacha20(text,text,size,key,nonce,1);return 1;
}
