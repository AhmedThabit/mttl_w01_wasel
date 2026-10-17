#include "qabes_remote.h"
#include "qabes_aead.h"
#include <string.h>
static int nibble(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
int qabes_key(const char *hex,uint8_t key[32]){
    unsigned i;int a,b;if(!hex)return 0;
    for(i=0;i<32;i++){a=nibble(hex[2*i]);if(a<0)return 0;b=nibble(hex[2*i+1]);if(b<0)return 0;key[i]=(uint8_t)(a<<4|b);}
    return hex[64]==0;
}
static void nonce(uint8_t *p,unsigned direction,uint64_t stamp){unsigned i;p[0]=p[1]=p[2]=p[3]=0;for(i=0;i<7;i++)p[4+i]=(uint8_t)(stamp>>(8*i));p[11]=(uint8_t)direction;}
size_t qabes_seal(uint8_t *frame,size_t plain,const uint8_t key[32],unsigned direction,uint64_t now_ms,uint64_t *last){
    uint64_t stamp=now_ms>*last?now_ms:*last+1;*last=stamp;
    frame[0]='Q';nonce(frame+1,direction,stamp);
    if(!qabes_aead_seal(frame+QABES_HEAD,plain,frame,QABES_HEAD,key,frame+1,frame+QABES_HEAD+plain))return 0;
    return plain+QABES_OVERHEAD;
}
int qabes_open(uint8_t *frame,size_t size,const uint8_t key[32],unsigned direction,uint64_t now_ms,int time_valid,uint64_t *last,uint8_t **plain,size_t *plain_size){
    uint64_t stamp=0;unsigned i;size_t n;
    if(size<QABES_OVERHEAD+1||frame[0]!='Q'||frame[1]||frame[2]||frame[3]||frame[4]||frame[12]!=direction||!time_valid)return 0;
    for(i=0;i<7;i++)stamp|=(uint64_t)frame[5+i]<<(8*i);
    if(stamp<=*last||(stamp>now_ms?stamp-now_ms:now_ms-stamp)>QABES_MAX_SKEW_MS)return 0;
    n=size-QABES_OVERHEAD;
    if(!qabes_aead_open(frame+QABES_HEAD,n,frame,QABES_HEAD,key,frame+1,frame+QABES_HEAD+n))return 0;
    *last=stamp;*plain=frame+QABES_HEAD;*plain_size=n;return 1;
}
int qabes_command(const uint8_t *p,size_t n,uint8_t *channel,uint8_t *on){
    if(n==3&&p[0]==QABES_OP_CONTROL&&p[1]<=4&&p[2]<=1){*channel=p[1];*on=p[2];return 1;}
    if(n==1&&p[0]==QABES_OP_REFRESH)return 2;
    if(n==1&&p[0]==QABES_OP_FIND){*channel=5;*on=1;return 3;}
    return 0;
}

int qabes_move(const uint8_t *p,size_t n,char host[254],uint16_t *port){
    size_t i;if(n<4||n>3+253||p[0]!=QABES_OP_MOVE||!(p[1]|p[2]))return 0;
    for(i=3;i<n;i++){uint8_t c=p[i];if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='-'))return 0;host[i-3]=(char)c;}
    host[n-3]=0;*port=(uint16_t)(p[1]<<8|p[2]);return 1;
}
/* One ChaCha20 block over zeros: k = key, nonce = tag | v (u64 LE at byte 4). */
static void prf(uint8_t *out,size_t n,const uint8_t key[32],const char *tag,uint64_t v){uint8_t c[12];unsigned i;memset(c,0,12);memcpy(c,tag,4);for(i=0;i<8&&v;i++,v>>=8)c[4+i]=(uint8_t)v;memset(out,0,n);qabes_chacha20(out,out,n,key,c,0);}
void qabes_auth_key(const uint8_t key[32],uint8_t auth[32]){prf(auth,32,key,"WAUT",'H');}
void qabes_login(const char *user,const char *hex,int time_valid,uint64_t unix_ms,char pass[33]){
    uint8_t key[32];pass[0]=0;if(user[0]&&time_valid&&qabes_key(hex,key))qabes_broker_token(key,unix_ms,pass);memset(key,0,32);}
void qabes_broker_token(const uint8_t key[32],uint64_t unix_ms,char out[33]){
    uint8_t a[32],t[16];unsigned i,x;
    qabes_auth_key(key,a);prf(t,16,a,"WTOK",unix_ms/60000u);memset(a,0,32);
    for(i=0;i<32;i++){x=(t[i/2]>>(i&1?0:4))&15;out[i]=(char)(x<10?'0'+x:'a'-10+x);}out[32]=0;
}
