/* Bytes from the broker socket: framing, CONNACK/SUBACK and command topics. */
#include "mqtt_wire.h"
#include "mqtt_command.h"
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size){
 static mttl_mq_rx rx;static mttl_mqtt_session session;mttl_command cmd;int accepted;size_t at=0,used;uint32_t now=0;
 mttl_mq_rx_reset(&rx);
 while(at<size){size_t chunk=(data[at]&15)+1;if(chunk>size-at)chunk=size-at;
  used=0;if(mttl_mq_rx_feed(&rx,data+at,chunk,now+=10,&used)<0)break;if(used>chunk)__builtin_trap();at+=used?used:chunk;}
 (void)mttl_mq_connack(data,size,&accepted);(void)mttl_mq_suback(data,size,1,&accepted);
 memset(&session,0,sizeof(session));(void)mttl_mq_command(&session,0,"mttl/97c0123",data,size,&cmd);
 return 0;}
