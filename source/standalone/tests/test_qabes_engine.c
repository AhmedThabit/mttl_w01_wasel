/* Host test of the remote channel through the real MQTT engine and device app. */
#include "device_app.h"
#include "qabes_remote.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;}}while(0)
static const char *HEX="000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
static uint8_t key[32];
typedef struct{uint8_t out[4096];size_t out_n;uint8_t in[600];size_t in_n;char host[64];int opened;} broker;
static broker b;
static int commands,last_channel,last_on;
static int hw_write(void *p,const uint8_t *d,size_t n){(void)p;(void)d;(void)n;return 1;}
static int relay(unsigned ch,unsigned on){commands++;last_channel=(int)ch;last_on=(int)on;return 1;}
static int o(void *p,const char *h,uint16_t port){(void)p;snprintf(b.host,sizeof(b.host),"%s:%u",h,port);b.opened++;return 1;}
static int c(void *p){(void)p;return 1;}
static int s(void *p,const uint8_t *d,size_t n){(void)p;if(b.out_n+n>sizeof(b.out))return -1;memcpy(b.out+b.out_n,d,n);b.out_n+=n;return (int)n;}
static int r(void *p,uint8_t *d,size_t n){(void)p;if(n>b.in_n)n=b.in_n;memcpy(d,b.in,n);memmove(b.in,b.in+n,b.in_n-n);b.in_n-=n;return (int)n;}
static void push(const uint8_t *d,size_t n){memcpy(b.in+b.in_n,d,n);b.in_n+=n;}
/* Splits b.out into packets; returns count, fills type/offset/length. */
static size_t packets(size_t at[],size_t len[],uint8_t type[],size_t max){size_t i=0,k=0;while(i<b.out_n&&k<max){size_t m=1,body=0,j=i+1;do{body+=(b.out[j]&127)*m;m*=128;}while(b.out[j++]&128);type[k]=b.out[i];at[k]=j;len[k]=body;i=j+body;k++;}return k;}
int main(void){
    static mttl_device_app app;mttl_mqtt_io io={0};uint32_t now=1000;unsigned i;size_t at[64],len[64],k;uint8_t type[64];uint64_t t0=1792177433298ULL;
    qabes_key(HEX,key);
    mttl_device_init(&app,1767225600ULL);app.relay_request=relay;app.hardware.verified=1;app.hardware.write=hw_write;
    strcpy(app.settings.broker,"broker.emqx.io");app.settings.port=1883;strcpy(app.settings.topic,"qabes/0123456789abcdef01234567");
    strcpy(app.settings.mqtt_password,HEX);app.settings.mqtt_enabled=1;
    CHECK(mttl_settings_valid(&app.settings));
    mttl_time_set(&app.ntp.time,t0,now);
    io.open=o;io.close=c;io.send=s;io.receive=r;
    for(i=0;i<400;i++){mttl_device_tick(&app,"2cfdb382d352",1,now,&io);now+=50;
        k=packets(at,len,type,64);
        if(k&&type[k-1]==0x10&&b.in_n==0&&app.mqtt.phase==1){const uint8_t ack[]={0x20,2,0,0};push(ack,4);}
        if(k&&type[k-1]==0x82&&b.in_n==0&&app.mqtt.phase==3){uint8_t ack[]={0x90,3,0,0,0};ack[2]=b.out[at[k-1]];ack[3]=b.out[at[k-1]+1];push(ack,5);}
        if(app.mqtt.session.state==MTTL_MQ_ONLINE)break;}
    CHECK(app.mqtt.session.state==MTTL_MQ_ONLINE);
    CHECK(!strcmp(b.host,"broker.emqx.io:1883"));
    k=packets(at,len,type,64);
    /* CONNECT: no password flag, no key bytes anywhere on the wire */
    CHECK(type[0]==0x10);CHECK((b.out[at[0]+7]&0x40)==0);
    {size_t j;int leak=0;for(j=0;j+8<b.out_n;j++)if(!memcmp(b.out+j,HEX,8))leak=1;CHECK(!leak);}
    CHECK(type[1]==0x82);CHECK(len[1]==2+2+32+1&&!memcmp(b.out+at[1]+4,"qabes/0123456789abcdef01234567/d",32));
    /* run until the first status is published */
    for(i=0;i<20;i++){mttl_device_tick(&app,"2cfdb382d352",1,now,&io);now+=50;}
    k=packets(at,len,type,64);
    {size_t j,found=0;for(j=0;j<k;j++){if(type[j]==0x31){size_t tl=(size_t)b.out[at[j]]<<8|b.out[at[j]+1];uint8_t frame[1024],*plain;size_t pn;uint64_t last=0;
        if(tl==32&&!memcmp(b.out+at[j]+2,"qabes/0123456789abcdef01234567/u",32)){size_t fn=len[j]-2-tl;memcpy(frame,b.out+at[j]+2+tl,fn);
            CHECK(qabes_open(frame,fn,key,QABES_UP,t0+3000,1,&last,&plain,&pn));CHECK(plain[0]==0x80);CHECK(pn>10&&!memcmp(plain+5,"{\"name\":",8));
            printf("status %u bytes: %.*s\n",(unsigned)pn,(int)(pn-5>120?120:pn-5),plain+5);found++;}}}
     CHECK(found==1);}
    /* encrypted command: outlet 3 (index 2) ON */
    {uint8_t f[64],pkt[96];uint64_t down=0;size_t n,p=0;const char *t="qabes/0123456789abcdef01234567/d";f[13]=1;f[14]=2;f[15]=1;
     n=qabes_seal(f,3,key,QABES_DOWN,t0+5000,&down);pkt[p++]=0x30;pkt[p++]=(uint8_t)(2+strlen(t)+n);pkt[p++]=0;pkt[p++]=(uint8_t)strlen(t);memcpy(pkt+p,t,strlen(t));p+=strlen(t);memcpy(pkt+p,f,n);p+=n;
     push(pkt,p);for(i=0;i<5;i++){mttl_device_tick(&app,"2cfdb382d352",1,now,&io);now+=50;}
     CHECK(commands==1&&last_channel==2&&last_on==1);
     /* the same frame again is a replay */
     push(pkt,p);for(i=0;i<5;i++){mttl_device_tick(&app,"2cfdb382d352",1,now,&io);now+=50;}CHECK(commands==1);
     /* a frame sealed with another key is ignored */
     {uint8_t other[32];memcpy(other,key,32);other[3]^=9;f[13]=1;f[14]=0;f[15]=0;n=qabes_seal(f,3,other,QABES_DOWN,t0+6000,&down);memcpy(pkt+4+strlen(t),f,n);
      push(pkt,p);for(i=0;i<5;i++){mttl_device_tick(&app,"2cfdb382d352",1,now,&io);now+=50;}CHECK(commands==1);}
     CHECK(app.mqtt.session.state==MTTL_MQ_ONLINE);}
    /* a non-hex password disables the channel */
    {static mttl_device_app q;mttl_device_init(&q,1767225600ULL);q.settings=app.settings;strcpy(q.settings.mqtt_password,"secret");b.opened=0;
     for(i=0;i<50;i++){mttl_device_tick(&q,"2cfdb382d352",1,now,&io);now+=50;}CHECK(b.opened==0);}
    printf(fails?"FAILED %d\n":"ALL OK\n",fails);return fails!=0;
}
