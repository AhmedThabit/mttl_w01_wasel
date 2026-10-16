#include "qabes_remote.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;}}while(0)
int main(void){
    uint8_t key[32],other[32],f[128],*p;size_t n,m;uint64_t up=0,down=0,seen=0;uint8_t ch,on;
    const char *hex="000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    CHECK(qabes_key(hex,key)&&key[31]==0x1f);CHECK(!qabes_key("00",key+0));
    CHECK(!qabes_key("zz0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",other));
    qabes_key(hex,key);memcpy(other,key,32);other[0]^=1;
    /* app -> strip control */
    f[13]=1;f[14]=2;f[15]=1;n=qabes_seal(f,3,key,QABES_DOWN,1000000,&down);CHECK(n==3+29);
    m=0;CHECK(!qabes_open(f,n,other,QABES_DOWN,1000000,1,&seen,&p,&m));            /* wrong key */
    CHECK(!qabes_open(f,n,key,QABES_UP,1000000,1,&seen,&p,&m));                   /* wrong direction */
    CHECK(!qabes_open(f,n,key,QABES_DOWN,1000000,0,&seen,&p,&m));                 /* no clock */
    CHECK(!qabes_open(f,n,key,QABES_DOWN,1000000+300001,1,&seen,&p,&m));          /* too old */
    { uint8_t g[128];memcpy(g,f,n);CHECK(qabes_open(g,n,key,QABES_DOWN,1000000+299000,1,&seen,&p,&m)&&m==3&&qabes_command(p,m,&ch,&on)==1&&ch==2&&on==1);}
    CHECK(!qabes_open(f,n,key,QABES_DOWN,1000000,1,&seen,&p,&m));                 /* replay */
    /* stamps never repeat even if the clock does not move */
    f[13]=2;n=qabes_seal(f,1,key,QABES_DOWN,1000000,&down);CHECK(down==1000001);
    CHECK(qabes_open(f,n,key,QABES_DOWN,1000000,1,&seen,&p,&m)&&qabes_command(p,m,&ch,&on)==2);
    /* tamper with header (AAD) */
    f[13]=2;n=qabes_seal(f,1,key,QABES_DOWN,1000100,&down);f[11]^=0x40;CHECK(!qabes_open(f,n,key,QABES_DOWN,1000100,1,&seen,&p,&m));
    /* up status */
    memcpy(f+13,"\x80\x01\x02\x03\x04{}",7);n=qabes_seal(f,7,key,QABES_UP,5,&up);
    { uint64_t s2=0;CHECK(qabes_open(f,n,key,QABES_UP,5,1,&s2,&p,&m)&&m==7&&p[0]==0x80&&p[5]=='{');}
    CHECK(!qabes_command((const uint8_t*)"\x01\x05\x01",3,&ch,&on));CHECK(!qabes_command((const uint8_t*)"\x01\x01\x02",3,&ch,&on));
    /* golden frame for the Kotlin side */
    { uint64_t g=0;f[13]=1;f[14]=4;f[15]=0;n=qabes_seal(f,3,key,QABES_DOWN,1792177433298ULL,&g);printf("golden ");for(m=0;m<n;m++)printf("%02x",f[m]);printf("\n");}
    printf(fails?"FAILED %d\n":"ALL OK\n",fails);return fails!=0;
}
