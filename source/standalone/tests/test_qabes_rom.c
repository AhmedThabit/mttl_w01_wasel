/* Host test of qabes_aead_rom.c: fake ROM functions in either historical ChaCha20 form,
 * built from the reference implementation (renamed), must give identical frames. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
void ref_chacha20(uint8_t *out,const uint8_t *in,size_t size,const uint8_t key[32],const uint8_t nonce[12],uint32_t counter);
void ref_poly1305(uint8_t tag[16],const uint8_t *m,size_t size,const uint8_t key[32]);
int ref_seal(uint8_t *t,size_t n,const uint8_t *aad,size_t an,const uint8_t key[32],const uint8_t nonce[12],uint8_t tag[16]);
int FORM=1;
void CRYPTO_chacha_20(uint8_t *out,const uint8_t *in,size_t len,const uint8_t key[32],const uint8_t *nonce,size_t counter){
    uint8_t n12[12];if(FORM==1)memcpy(n12,nonce,12);else{memset(n12,0,4);memcpy(n12+4,nonce,8);}ref_chacha20(out,in,len,key,n12,(uint32_t)counter);}
typedef struct{uint8_t key[32];uint8_t buf[400];size_t n;} st;
void CRYPTO_poly1305_init(void *s,const uint8_t key[32]){st *p=s;memcpy(p->key,key,32);p->n=0;}
void CRYPTO_poly1305_update(void *s,const uint8_t *in,size_t n){st *p=s;memcpy(p->buf+p->n,in,n);p->n+=n;}
void CRYPTO_poly1305_finish(void *s,uint8_t mac[16]){st *p=s;ref_poly1305(mac,p->buf,p->n,p->key);}
int qabes_aead_seal(uint8_t *,size_t,const uint8_t *,size_t,const uint8_t *,const uint8_t *,uint8_t *);
int qabes_aead_open(uint8_t *,size_t,const uint8_t *,size_t,const uint8_t *,const uint8_t *,const uint8_t *);
int qabes_aead_ready(void);
int main(void){
    uint8_t key[32],nonce[12]={0,0,0,0,1,2,3,4,5,6,7,2},a[100],b[100],ta[16],tb[16],aad[13]={'Q'};int i,fails=0;
    for(i=0;i<32;i++)key[i]=(uint8_t)(i*7);for(i=0;i<100;i++)a[i]=b[i]=(uint8_t)i;
    FORM=FORMSEL;
    if(!qabes_aead_ready()){printf("FAIL self-test\n");return 1;}
    qabes_aead_seal(a,100,aad,13,key,nonce,ta);ref_seal(b,100,aad,13,key,nonce,tb);
    if(memcmp(a,b,100)||memcmp(ta,tb,16)){printf("FAIL mismatch\n");fails++;}
    if(!qabes_aead_open(a,100,aad,13,key,nonce,ta)||a[50]!=50){printf("FAIL open\n");fails++;}
    printf(fails?"FAILED\n":"ALL OK form %d\n",FORMSEL);return fails;
}
