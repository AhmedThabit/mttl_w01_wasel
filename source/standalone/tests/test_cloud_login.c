/* Wasel cloud login: one-minute broker token derived from the frame key, and the
 * a./b. backup broker, through the real MQTT engine and device app. */
#include "device_app.h"
#include "qabes_remote.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;}}while(0)
static const char *HEX="000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
typedef struct{uint8_t out[4096];size_t out_n;uint8_t in[600];size_t in_n;char host[64];int opened,refused_a;} broker;
static broker b;
static int hw_write(void *p,const uint8_t *d,size_t n){(void)p;(void)d;(void)n;return 1;}
static int relay(unsigned ch,unsigned on){(void)ch;(void)on;return 1;}
static int o(void *p,const char *h,uint16_t port){(void)p;snprintf(b.host,sizeof(b.host),"%s:%u",h,port);b.opened++;if(h[0]=='a'&&b.refused_a)return -1;return 1;}
static int c(void *p){(void)p;return 1;}
static int s(void *p,const uint8_t *d,size_t n){(void)p;if(b.out_n+n>sizeof(b.out))b.out_n=0;memcpy(b.out+b.out_n,d,n);b.out_n+=n;return (int)n;}
static unsigned flags(void){size_t i=1;while(b.out[i]&128)i++;return b.out[i+1+7];}
static int r(void *p,uint8_t *d,size_t n){(void)p;if(n>b.in_n)n=b.in_n;memcpy(d,b.in,n);memmove(b.in,b.in+n,b.in_n-n);b.in_n-=n;return (int)n;}
int main(void){
    static mttl_device_app app;mttl_mqtt_io io={0};uint32_t now=1000;unsigned i;uint8_t key[32],auth[32];char t1[33],t2[33],t3[33];
    uint64_t t0=1792177433298ULL; /* 2026-10-17 */
    qabes_key(HEX,key);
    /* token: stable inside a minute, new every minute, lowercase hex */
    qabes_broker_token(key,t0,t1);qabes_broker_token(key,t0-(t0%60000)+59999,t2);qabes_broker_token(key,t0+60000,t3);
    CHECK(strlen(t1)==32&&!strcmp(t1,t2)&&strcmp(t1,t3));
    for(i=0;i<32;i++)CHECK((t1[i]>='0'&&t1[i]<='9')||(t1[i]>='a'&&t1[i]<='f'));
    qabes_auth_key(key,auth);CHECK(memcmp(auth,key,32));
    printf("vector: key=%s minute=%llu auth=",HEX,(unsigned long long)(t0/60000));for(i=0;i<32;i++)printf("%02x",auth[i]);printf(" token=%s\n",t1);

    mttl_device_init(&app,1767225600ULL);app.relay_request=relay;app.hardware.verified=1;app.hardware.write=hw_write;
    strcpy(app.settings.broker,"a.cloud.test");app.settings.port=1883;strcpy(app.settings.topic,"wasel/0123456789abcdef01234567");
    strcpy(app.settings.mqtt_user,"0123456789abcdef01234567");strcpy(app.settings.mqtt_password,HEX);app.settings.mqtt_enabled=1;
    CHECK(mttl_settings_valid(&app.settings));
    mttl_time_set(&app.ntp.time,t0,now);
    io.open=o;io.close=c;io.send=s;io.receive=r;
    b.refused_a=1;
    /* "a." refuses: three tries there, then "b." */
    for(i=0;i<20000&&!(b.opened>=4&&b.host[0]=='b');i++){mttl_device_tick(&app,"2cfdb382d352",1,now,&io);now+=50;}
    CHECK(b.opened==4);CHECK(!strcmp(b.host,"b.cloud.test:1883"));
    /* CONNECT carries user + token, never the key */
    for(i=0;i<5;i++){mttl_device_tick(&app,"2cfdb382d352",1,now,&io);now+=50;}
    {size_t j;int leak=0,found=0;uint64_t unix_ms;char want[33];
     for(j=0;j+8<b.out_n;j++)if(!memcmp(b.out+j,HEX,8))leak=1;CHECK(!leak);
     CHECK(b.out_n>0&&b.out[0]==0x10);
     CHECK((flags()&0xc0)==0xc0); /* user + password flags */
     mttl_time_now(&app.ntp.time,now,&unix_ms);qabes_broker_token(key,unix_ms,want);
     for(j=0;j+32<=b.out_n;j++)if(!memcmp(b.out+j,want,32))found=1;
     if(!found){qabes_broker_token(key,unix_ms-60000,want);for(j=0;j+32<=b.out_n;j++)if(!memcmp(b.out+j,want,32))found=1;}
     CHECK(found);}
    /* a host without the a./b. pair never changes */
    {static mttl_device_app d2;mttl_device_init(&d2,1767225600ULL);d2.relay_request=relay;d2.hardware.verified=1;d2.hardware.write=hw_write;
     strcpy(d2.settings.broker,"broker.emqx.io");d2.settings.port=1883;strcpy(d2.settings.topic,"wasel/0123456789abcdef01234567");
     strcpy(d2.settings.mqtt_password,HEX);d2.settings.mqtt_enabled=1;mttl_time_set(&d2.ntp.time,t0,now);
     b.opened=0;b.refused_a=0;b.out_n=0;
     for(i=0;i<3000;i++){mttl_device_tick(&d2,"2cfdb382d352",1,now,&io);now+=50;}
     CHECK(b.opened>=1);CHECK(!strcmp(b.host,"broker.emqx.io:1883"));
     CHECK(b.out[0]==0x10&&(flags()&0xc0)==0);} /* no user name: no password, as before */
    printf(fails?"FAILED (%d)\n":"cloud login OK\n",fails);return fails!=0;
}
