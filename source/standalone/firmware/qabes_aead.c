#include "qabes_aead.h"
#include <string.h>
/* ChaCha20 + Poly1305 per RFC 8439. Poly1305 uses 26-bit limbs (poly1305-donna-32). */
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
#define ROTL(v,n) (((v)<<(n))|((v)>>(32-(n))))
#define QR(a,b,c,d) a+=b;d^=a;d=ROTL(d,16);c+=d;b^=c;b=ROTL(b,12);a+=b;d^=a;d=ROTL(d,8);c+=d;b^=c;b=ROTL(b,7);
static void block(uint8_t out[64],const uint8_t key[32],const uint8_t nonce[12],uint32_t counter){
    uint32_t in[16],x[16];int i;
    in[0]=0x61707865;in[1]=0x3320646e;in[2]=0x79622d32;in[3]=0x6b206574;
    for(i=0;i<8;i++)in[4+i]=le32(key+4*i);
    in[12]=counter;in[13]=le32(nonce);in[14]=le32(nonce+4);in[15]=le32(nonce+8);
    memcpy(x,in,sizeof(x));
    for(i=0;i<10;i++){QR(x[0],x[4],x[8],x[12])QR(x[1],x[5],x[9],x[13])QR(x[2],x[6],x[10],x[14])QR(x[3],x[7],x[11],x[15])
        QR(x[0],x[5],x[10],x[15])QR(x[1],x[6],x[11],x[12])QR(x[2],x[7],x[8],x[13])QR(x[3],x[4],x[9],x[14])}
    for(i=0;i<16;i++)put32(out+4*i,x[i]+in[i]);
    memset(x,0,sizeof(x));memset(in,0,sizeof(in));
}
void qabes_chacha20(uint8_t *out,const uint8_t *in,size_t size,const uint8_t key[32],const uint8_t nonce[12],uint32_t counter){
    uint8_t ks[64];size_t i,n;
    while(size){block(ks,key,nonce,counter++);n=size<64?size:64;for(i=0;i<n;i++)out[i]=in[i]^ks[i];out+=n;in+=n;size-=n;}
    memset(ks,0,sizeof(ks));
}
typedef struct{uint32_t r[5],h[5],pad[4];uint8_t buf[16];size_t used;} poly;
static void poly_init(poly *p,const uint8_t key[32]){
    memset(p,0,sizeof(*p));
    p->r[0]=le32(key)&0x3ffffff;p->r[1]=(le32(key+3)>>2)&0x3ffff03;p->r[2]=(le32(key+6)>>4)&0x3ffc0ff;
    p->r[3]=(le32(key+9)>>6)&0x3f03fff;p->r[4]=(le32(key+12)>>8)&0x00fffff;
    p->pad[0]=le32(key+16);p->pad[1]=le32(key+20);p->pad[2]=le32(key+24);p->pad[3]=le32(key+28);
}
static void poly_blocks(poly *p,const uint8_t *m,size_t size,uint32_t hibit){
    uint32_t r0=p->r[0],r1=p->r[1],r2=p->r[2],r3=p->r[3],r4=p->r[4],s1=r1*5,s2=r2*5,s3=r3*5,s4=r4*5;
    uint32_t h0=p->h[0],h1=p->h[1],h2=p->h[2],h3=p->h[3],h4=p->h[4],c;uint64_t d0,d1,d2,d3,d4;
    while(size>=16){
        h0+=le32(m)&0x3ffffff;h1+=(le32(m+3)>>2)&0x3ffffff;h2+=(le32(m+6)>>4)&0x3ffffff;h3+=(le32(m+9)>>6)&0x3ffffff;h4+=(le32(m+12)>>8)|hibit;
        d0=(uint64_t)h0*r0+(uint64_t)h1*s4+(uint64_t)h2*s3+(uint64_t)h3*s2+(uint64_t)h4*s1;
        d1=(uint64_t)h0*r1+(uint64_t)h1*r0+(uint64_t)h2*s4+(uint64_t)h3*s3+(uint64_t)h4*s2;
        d2=(uint64_t)h0*r2+(uint64_t)h1*r1+(uint64_t)h2*r0+(uint64_t)h3*s4+(uint64_t)h4*s3;
        d3=(uint64_t)h0*r3+(uint64_t)h1*r2+(uint64_t)h2*r1+(uint64_t)h3*r0+(uint64_t)h4*s4;
        d4=(uint64_t)h0*r4+(uint64_t)h1*r3+(uint64_t)h2*r2+(uint64_t)h3*r1+(uint64_t)h4*r0;
        c=(uint32_t)(d0>>26);h0=(uint32_t)d0&0x3ffffff;d1+=c;c=(uint32_t)(d1>>26);h1=(uint32_t)d1&0x3ffffff;
        d2+=c;c=(uint32_t)(d2>>26);h2=(uint32_t)d2&0x3ffffff;d3+=c;c=(uint32_t)(d3>>26);h3=(uint32_t)d3&0x3ffffff;
        d4+=c;c=(uint32_t)(d4>>26);h4=(uint32_t)d4&0x3ffffff;h0+=c*5;c=h0>>26;h0&=0x3ffffff;h1+=c;
        m+=16;size-=16;
    }
    p->h[0]=h0;p->h[1]=h1;p->h[2]=h2;p->h[3]=h3;p->h[4]=h4;
}
static void poly_update(poly *p,const uint8_t *m,size_t size){
    size_t n;
    if(p->used){n=16-p->used;if(n>size)n=size;memcpy(p->buf+p->used,m,n);p->used+=n;m+=n;size-=n;if(p->used<16)return;poly_blocks(p,p->buf,16,1u<<24);p->used=0;}
    n=size&~(size_t)15;if(n){poly_blocks(p,m,n,1u<<24);m+=n;size-=n;}
    if(size){memcpy(p->buf,m,size);p->used=size;}
}
static void poly_finish(poly *p,uint8_t tag[16]){
    uint32_t h0,h1,h2,h3,h4,c,g0,g1,g2,g3,g4,mask;uint64_t f;
    if(p->used){p->buf[p->used]=1;memset(p->buf+p->used+1,0,15-p->used);poly_blocks(p,p->buf,16,0);}
    h0=p->h[0];h1=p->h[1];h2=p->h[2];h3=p->h[3];h4=p->h[4];
    c=h1>>26;h1&=0x3ffffff;h2+=c;c=h2>>26;h2&=0x3ffffff;h3+=c;c=h3>>26;h3&=0x3ffffff;h4+=c;c=h4>>26;h4&=0x3ffffff;h0+=c*5;c=h0>>26;h0&=0x3ffffff;h1+=c;
    g0=h0+5;c=g0>>26;g0&=0x3ffffff;g1=h1+c;c=g1>>26;g1&=0x3ffffff;g2=h2+c;c=g2>>26;g2&=0x3ffffff;g3=h3+c;c=g3>>26;g3&=0x3ffffff;g4=h4+c-(1u<<26);
    mask=(g4>>31)-1;g0&=mask;g1&=mask;g2&=mask;g3&=mask;g4&=mask;mask=~mask;
    h0=(h0&mask)|g0;h1=(h1&mask)|g1;h2=(h2&mask)|g2;h3=(h3&mask)|g3;h4=(h4&mask)|g4;
    h0=h0|(h1<<26);h1=(h1>>6)|(h2<<20);h2=(h2>>12)|(h3<<14);h3=(h3>>18)|(h4<<8);
    f=(uint64_t)h0+p->pad[0];h0=(uint32_t)f;f=(uint64_t)h1+p->pad[1]+(f>>32);h1=(uint32_t)f;
    f=(uint64_t)h2+p->pad[2]+(f>>32);h2=(uint32_t)f;f=(uint64_t)h3+p->pad[3]+(f>>32);h3=(uint32_t)f;
    put32(tag,h0);put32(tag+4,h1);put32(tag+8,h2);put32(tag+12,h3);
    memset(p,0,sizeof(*p));
}
void qabes_poly1305(uint8_t tag[16],const uint8_t *m,size_t size,const uint8_t key[32]){poly p;poly_init(&p,key);poly_update(&p,m,size);poly_finish(&p,tag);}
static void pad16(poly *p,size_t size){static const uint8_t zero[16];if(size&15)poly_update(p,zero,16-(size&15));}
static void mac(uint8_t tag[16],const uint8_t *text,size_t size,const uint8_t *aad,size_t aad_size,const uint8_t key[32],const uint8_t nonce[12]){
    uint8_t otk[64],lens[16];poly p;
    block(otk,key,nonce,0);poly_init(&p,otk);memset(otk,0,sizeof(otk));
    poly_update(&p,aad,aad_size);pad16(&p,aad_size);poly_update(&p,text,size);pad16(&p,size);
    put32(lens,(uint32_t)aad_size);put32(lens+4,0);put32(lens+8,(uint32_t)size);put32(lens+12,0);
    poly_update(&p,lens,16);poly_finish(&p,tag);
}
int qabes_aead_seal(uint8_t *text,size_t size,const uint8_t *aad,size_t aad_size,const uint8_t key[32],const uint8_t nonce[12],uint8_t tag[16]){
    qabes_chacha20(text,text,size,key,nonce,1);mac(tag,text,size,aad,aad_size,key,nonce);return 1;
}
int qabes_aead_ready(void){return 1;}
int qabes_aead_open(uint8_t *text,size_t size,const uint8_t *aad,size_t aad_size,const uint8_t key[32],const uint8_t nonce[12],const uint8_t tag[16]){
    uint8_t want[16];unsigned diff=0,i;
    mac(want,text,size,aad,aad_size,key,nonce);for(i=0;i<16;i++)diff|=(unsigned)(want[i]^tag[i]);memset(want,0,sizeof(want));
    if(diff)return 0;qabes_chacha20(text,text,size,key,nonce,1);return 1;
}
