/* Host test: Wasel settings body (0x57) with LED options; older bodies still load. */
#include "settings.h"
#include "qabes_remote.h"
#include "settings_record.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;}}while(0)
int main(void){
    static mttl_settings a,b;uint8_t body[1024];size_t n;uint8_t ch,on;
    mttl_settings_default(&a);a.leds=MTTL_LED_HIDE_OFF|MTTL_LED_NIGHT_WINDOW|MTTL_LED_HOLD_LOCK;a.night[0]=23*60;a.night[1]=7*60;a.timer_minutes[2]=30;
    CHECK(mttl_settings_valid(&a));
    n=mttl_settings_pack(&a,body,sizeof(body));CHECK(n&&body[0]==MTTL_SETTINGS_WASEL);
    CHECK(mttl_settings_unpack(body,n,&b));CHECK(b.leds==a.leds&&b.night[0]==1380&&b.night[1]==420&&b.timer_minutes[2]==30);
    /* storage record round trip */
    n=mttl_settings_storage_pack(&a,body,sizeof(body));memset(&b,0,sizeof(b));CHECK(n&&mttl_settings_storage_unpack(body,n,&b)&&b.leds==a.leds&&b.night[1]==420);
    /* an upstream v7 body (web page) still loads, with LED options at their default */
    n=mttl_settings_pack(&a,body,sizeof(body));body[0]=7;n-=5;CHECK(mttl_settings_unpack(body,n,&b)&&b.leds==0&&b.timer_minutes[2]==30);
    /* range checks */
    a.leds=16;CHECK(!mttl_settings_valid(&a));a.leds=1;a.night[0]=1440;CHECK(!mttl_settings_valid(&a));
    /* PIN: 4..8 digits or none; kept in the flash record (MTTL_STORAGE_PIN), never in the web body */
    a.night[0]=0;a.leds=0;CHECK(mttl_settings_pin_valid("")&&mttl_settings_pin_valid("1234")&&mttl_settings_pin_valid("12345678"));
    CHECK(!mttl_settings_pin_valid("123")&&!mttl_settings_pin_valid("12a4")&&!mttl_settings_pin_valid("123456789"));
    strcpy(a.pin,"482916");CHECK(mttl_settings_valid(&a));
    n=mttl_settings_storage_pack(&a,body,sizeof(body));CHECK(n&&body[0]==MTTL_STORAGE_PIN);
    memset(&b,0,sizeof(b));CHECK(mttl_settings_storage_unpack(body,n,&b)&&!strcmp(b.pin,"482916")&&b.relay_known==a.relay_known);
    CHECK(!mttl_settings_storage_unpack(body,n-1,&b));                      /* truncated */
    n=mttl_settings_pack(&a,body,sizeof(body));CHECK(n&&!memmem(body,n,"482916",6)); /* not in the web body */
    memset(&b,0,sizeof(b));CHECK(mttl_settings_unpack(body,n,&b)&&b.pin[0]==0);
    /* through the flash slot encoder and back, as after a reboot */
    {static uint8_t slot[MTTL_SETTINGS_SLOT],back[1024];size_t m;
     n=mttl_settings_storage_pack(&a,body,sizeof(body));CHECK(mttl_settings_encode(slot,body,n,7));slot[MTTL_SETTINGS_SLOT-1]=0; /* commit byte, written last by the flash code */
     CHECK(mttl_settings_extract(slot,back,sizeof(back),&m)&&m==n&&!memcmp(back,body,n));
     memset(&b,0,sizeof(b));CHECK(mttl_settings_storage_unpack(back,m,&b)&&!strcmp(b.pin,"482916")&&!strcmp(b.ssid,a.ssid));}
    a.pin[0]=0;n=mttl_settings_storage_pack(&a,body,sizeof(body));CHECK(n&&body[0]==3);   /* no PIN: old record type */
    /* remote find command */
    CHECK(qabes_command((const uint8_t *)"\x03",1,&ch,&on)==3&&ch==5&&on==1);
    printf(fails?"FAILED %d\n":"ALL OK\n",fails);return fails!=0;
}
