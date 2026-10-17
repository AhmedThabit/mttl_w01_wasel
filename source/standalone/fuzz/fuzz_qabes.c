/* Encrypted remote frames from the public broker: anyone can publish to the topic. */
#include "qabes_remote.h"
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size){
 static uint8_t frame[1024];uint8_t key[32],*plain,ch,on;size_t pn;uint64_t last=0;
 if(size>sizeof(frame))return 0;memset(key,7,sizeof(key));memcpy(frame,data,size);
 if(qabes_open(frame,size,key,2,1792228012784ULL,1,&last,&plain,&pn)){
  if(plain<frame||plain+pn>frame+size)__builtin_trap();
  (void)qabes_command(plain,pn,&ch,&on);
 }
 (void)qabes_command(data,size,&ch,&on);
 {char host[254];uint16_t port;if(qabes_move(data,size,host,&port)&&(strlen(host)!=size-3||!port))__builtin_trap();}
 (void)qabes_key((const char *)(size?"0":""),key);
 return 0;}
