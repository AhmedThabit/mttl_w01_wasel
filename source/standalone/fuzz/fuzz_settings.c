/* Settings bodies (web POST /api/config) and flash records (storage + slot): must never crash,
 * and whatever they accept must pack and load back to the same values. */
#include "settings.h"
#include "settings_record.h"
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size){
 static mttl_settings a,b;static uint8_t body[2048],slot[MTTL_SETTINGS_SLOT];size_t n,m;
 if(mttl_settings_unpack(data,size,&a)){
  if(!mttl_settings_valid(&a))__builtin_trap();
  n=mttl_settings_pack(&a,body,sizeof(body));if(!n||!mttl_settings_unpack(body,n,&b))__builtin_trap();
  if(strcmp(a.device,b.device)||a.leds!=b.leds||a.button_lock!=b.button_lock)__builtin_trap();
 }
 if(mttl_settings_storage_unpack(data,size,&a)){
  if(!mttl_settings_valid(&a))__builtin_trap();
  n=mttl_settings_storage_pack(&a,body,sizeof(body));if(!n||!mttl_settings_storage_unpack(body,n,&b))__builtin_trap();
  if(strcmp(a.pin,b.pin)||strcmp(a.ssid,b.ssid))__builtin_trap();
 }
 /* a raw flash slot */
 memset(slot,255,sizeof(slot));memcpy(slot,data,size<sizeof(slot)?size:sizeof(slot));
 if(mttl_settings_extract(slot,body,sizeof(body),&m))(void)mttl_settings_storage_unpack(body,m,&a);
 return 0;}
