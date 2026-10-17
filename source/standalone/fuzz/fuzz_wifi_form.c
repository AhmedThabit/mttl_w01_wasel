/* POST /api/wifi form body (open in the setup access point). */
#include <stdint.h>
#include <stddef.h>
#include "wifi_form.h"
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size){
 char ssid[33],password[65];
 if(size>320)return 0;
 if(mttl_wifi_form(data,size,ssid,password)){if(strlen(ssid)>32||strlen(password)>64)__builtin_trap();}
 return 0;}
