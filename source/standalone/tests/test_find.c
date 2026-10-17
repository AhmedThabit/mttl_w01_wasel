/* Wasel find: both phases light every outlet and the main LED in one colour; coils stay off. */
#include "hardware_port.h"
#include <stdio.h>
#include <string.h>
static uint8_t last[4];static int writes,fails;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;}}while(0)
static int w(void *c,const uint8_t *b,size_t n){(void)c;if(n!=4)return 0;memcpy(last,b,4);writes++;return 1;}
int main(void){mttl_hardware_port p;mttl_hardware_init(&p);p.verified=1;p.write=w;
 p.shadow[1]=0x30;p.shadow[2]=0x3f; /* coil bits must never be driven by the overlay */
 CHECK(mttl_hardware_find_flash(&p,0));CHECK(last[0]==0x15&&last[1]==0x05&&last[2]==0);
 CHECK(mttl_hardware_find_flash(&p,1));CHECK(last[0]==0x2a&&last[1]==0x0a&&last[2]==0);
 CHECK(p.shadow[1]==0x30&&p.shadow[2]==0x3f); /* overlay only */
 p.active=1;CHECK(!mttl_hardware_find_flash(&p,0)); /* never during a relay pulse */
 printf(fails?"FAILED %d\n":"ALL OK find\n",fails);return fails!=0;}
