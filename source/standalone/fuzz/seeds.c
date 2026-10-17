/* Valid starting inputs, so the fuzzers begin inside the interesting code paths. */
#include "settings.h"
#include "qabes_remote.h"
#include <stdio.h>
#include <string.h>
static void put(const char *path,const uint8_t *b,size_t n){FILE *f=fopen(path,"wb");if(f){fwrite(b,1,n,f);fclose(f);}}
int main(int argc,char **argv){static mttl_settings a;static uint8_t b[2048];char p[512];size_t n;uint8_t key[32],f[256];uint64_t last=0;const char *o=argc>1?argv[1]:".";
 mttl_settings_default(&a);strcpy(a.ssid,"Home");strcpy(a.wifi_password,"secret123");strcpy(a.broker,"broker.emqx.io");strcpy(a.topic,"wasel/0123456789abcdef01234567");
 a.leds=MTTL_LED_HOLD_LOCK;a.timer_minutes[1]=30;a.button_lock=3;
 n=mttl_settings_pack(&a,b,sizeof b);snprintf(p,sizeof p,"%s/corpus-settings/body",o);put(p,b,n);
 n=mttl_settings_storage_pack(&a,b,sizeof b);snprintf(p,sizeof p,"%s/corpus-settings/record",o);put(p,b,n);
 strcpy(a.pin,"482916");n=mttl_settings_storage_pack(&a,b,sizeof b);snprintf(p,sizeof p,"%s/corpus-settings/record-pin",o);put(p,b,n);
 memset(key,7,sizeof key);f[13]=1;f[14]=2;f[15]=1;n=qabes_seal(f,3,key,2,1792228012000ULL,&last);snprintf(p,sizeof p,"%s/corpus-qabes/control",o);put(p,f,n);
 return 0;}
