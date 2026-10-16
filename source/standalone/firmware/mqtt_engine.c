#include "mqtt_engine.h"
#include "mqtt_command.h"
#include "deadline.h"
#include <stdio.h>
#include <string.h>
#if QABES_REMOTE
#include "qabes_remote.h"
#define QABES_ENTITIES 0
#define QABES_DISCOVERY(s,id,i,t,tc,j,jc) 0
#else
#define QABES_ENTITIES MTTL_HA_ENTITIES
#define QABES_DISCOVERY mttl_ha_discovery
/* Qabes fork: MQTT carries only the encrypted remote channel (qabes_remote.h);
 * Home Assistant Discovery is compiled out to make room for it. */
#endif
static void clear_tx(mttl_mqtt_engine *e){volatile uint8_t *p=e->tx;size_t i;for(i=0;i<sizeof(e->tx);i++)p[i]=0;e->tx_size=e->tx_sent=0;}
void mttl_mqtt_engine_init(mttl_mqtt_engine *e){memset(e,0,sizeof(*e));mttl_mq_init(&e->session);mttl_mq_rx_reset(&e->rx);}
static void failed(mttl_mqtt_engine *e,uint32_t now){mttl_mq_failed(&e->session,e->session.generation,now);clear_tx(e);}
static int queue(mttl_mqtt_engine *e,size_t n,uint32_t now){if(!n||n>sizeof(e->tx))return 0;e->tx_size=n;e->tx_sent=0;e->tx_since=now;return 1;}
#if QABES_REMOTE
int mttl_mqtt_engine_queue_built(mttl_mqtt_engine *e,size_t n,uint32_t now){if(e->session.state!=MTTL_MQ_ONLINE||e->tx_size||e->phase>=10)return 0;return queue(e,n,now);}
#endif
int mttl_mqtt_engine_publish(mttl_mqtt_engine *e,const char *topic,const uint8_t *p,size_t n,int retain,uint32_t now){if(e->session.state!=MTTL_MQ_ONLINE||e->tx_size||e->rediscover||e->phase>=10)return 0;return queue(e,mttl_mq_publish_packet(e->tx,sizeof(e->tx),topic,p,n,retain),now);}
void mttl_mqtt_engine_step(mttl_mqtt_engine *e,const mttl_settings *s,const char *id,int network,uint32_t revision,uint32_t now,const mttl_mqtt_io *io){
    char base[96],topic[128],payload[768],client[32];uint8_t incoming[64];size_t n,used,pos;int rc,accepted;mttl_mq_action action;mttl_command command;
    if(!e||!s||!io||!io->open||!io->close||!io->send||!io->receive)return;
#if QABES_REMOTE
    {uint8_t key[32];int ok=s->topic[0]&&qabes_key(s->mqtt_password,key)&&strlen(s->topic)<sizeof(base);memset(key,0,sizeof(key));
     if(ok)strcpy(base,s->topic);else{network=0;base[0]=0;}}
    e->cleanup_done=1; /* nothing retained to clean up in this mode */
#else
    if(!mttl_ha_base(s,id,base,sizeof(base)))network=0;
#endif
    /* A disabled configuration briefly connects to remove retained Discovery.
     * Retry after network loss/reboot; never subscribe or execute commands. */
#if !QABES_REMOTE
    if(s->mqtt_enabled)e->cleanup_done=0;
#endif
    mttl_mq_update(&e->session,network,s->mqtt_enabled||(!e->cleanup_done&&s->broker[0]),revision,now);action=mttl_mq_step(&e->session,now);
    if(action==MTTL_MQ_CLOSE){rc=io->close(io->context);if(rc==1){mttl_mq_stopped(&e->session,e->session.generation,now);clear_tx(e);mttl_mq_rx_reset(&e->rx);e->phase=e->ping_wait=e->rediscover=0;}return;}
    if(e->session.state==MTTL_MQ_OFF||e->session.state==MTTL_MQ_BACKOFF||e->session.state==MTTL_MQ_STOPPING||e->session.state==MTTL_MQ_FAULT)return;
    if(action==MTTL_MQ_CONNECT){e->phase=0;e->entity=0;clear_tx(e);mttl_mq_rx_reset(&e->rx);}
    if(e->session.state==MTTL_MQ_CONNECTING&&!e->phase){rc=io->open(io->context,s->broker,s->port);if(rc<0){failed(e,now);return;}if(!rc)return;
#if QABES_REMOTE
        /* The password field holds the frame key: never send it to the broker. */
        snprintf(client,sizeof(client),"wasel_%s",id);snprintf(topic,sizeof(topic),"%s/availability",base);
        if(!queue(e,mttl_mq_connect_packet(e->tx,512,client,topic,s->mqtt_user,"",30),now)){failed(e,now);return;}e->phase=1;}
#else
        snprintf(client,sizeof(client),"mttl2_%s",id);snprintf(topic,sizeof(topic),"%s/availability",base);
        if(!queue(e,mttl_mq_connect_packet(e->tx,512,client,topic,s->mqtt_user,s->mqtt_password,30),now)){failed(e,now);return;}e->phase=1;}
#endif
    /* Same owner also performs synchronous NOR checkpoints. Queue age is
     * not proof of a stalled socket; allow that bounded scheduling gap. */
#if !QABES_REMOTE
    if(!s->mqtt_enabled&&!e->cleanup_done&&e->phase<10&&(e->session.state==MTTL_MQ_ONLINE||e->phase==6||e->phase==7||e->phase==8)){e->phase=10;e->entity=0;e->rediscover=0;}
#endif
    if(e->tx_size){if(!mttl_remaining(e->tx_since,now,10000)){failed(e,now);return;}rc=io->send(io->context,e->tx+e->tx_sent,e->tx_size-e->tx_sent);
        if(rc<0||(size_t)rc>e->tx_size-e->tx_sent){failed(e,now);return;}e->tx_sent+=(size_t)rc;if(e->tx_sent<e->tx_size)return;clear_tx(e);e->last_tx=now;
        if(e->phase==2)e->phase=3;else if(e->phase==4)e->phase=5;else if(e->phase==6)e->phase=7;
    }
    if(mttl_mq_rx_feed(&e->rx,0,0,now,&used)<0){failed(e,now);return;}
    rc=io->receive(io->context,incoming,sizeof(incoming));if(rc<0||rc>(int)sizeof(incoming)){failed(e,now);return;}
    pos=0;while(pos<(size_t)rc){int ready=mttl_mq_rx_feed(&e->rx,incoming+pos,(size_t)rc-pos,now,&used);pos+=used;if(ready<0){failed(e,now);return;}if(!ready)break;
        if(e->phase==1){if(!mttl_mq_connack(e->rx.bytes,e->rx.total,&accepted)||!accepted||!mttl_mq_connected(&e->session,e->session.generation,now)){failed(e,now);return;}if(!s->mqtt_enabled){e->phase=10;e->entity=0;}else{snprintf(topic,sizeof(topic),QABES_REMOTE?"%s/d":"%s/+/set",base);if(!queue(e,mttl_mq_subscribe_packet(e->tx,512,topic,1),now)){failed(e,now);return;}e->phase=2;}}
        else if(e->phase==3||e->phase==5){unsigned packet=e->phase==3?1:2;if(!mttl_mq_suback(e->rx.bytes,e->rx.total,(uint16_t)packet,&accepted)||!accepted){failed(e,now);return;}
#if QABES_REMOTE
            (void)packet;e->phase=6;e->entity=0;e->status_wanted=1;}
#else
            if(packet==1){if(!queue(e,mttl_mq_subscribe_packet(e->tx,512,"homeassistant/status",2),now)){failed(e,now);return;}e->phase=4;}else{e->phase=6;e->entity=0;}}
#endif
#if !QABES_REMOTE
        else if(e->phase>=10){if(e->phase==12&&e->rx.total==2&&e->rx.bytes[0]==0xd0&&e->rx.bytes[1]==0){e->cleanup_done=1;e->ping_wait=0;}}
#endif
        else if(e->session.state==MTTL_MQ_ONLINE&&s->mqtt_enabled){
            if(e->rx.total==2&&e->rx.bytes[0]==0xd0&&e->rx.bytes[1]==0)e->ping_wait=0;
#if QABES_REMOTE
            else{mttl_publish p;size_t b=strlen(base);uint8_t key[32],*plain,channel,on;size_t plain_size;int op;
                (void)command;
                if(mttl_parse_publish(e->rx.bytes,e->rx.total,&p)&&p.topic_size==b+2&&!memcmp(p.topic,base,b)&&!memcmp(p.topic+b,"/d",2)&&qabes_key(s->mqtt_password,key)){
                    if(qabes_open((uint8_t *)p.payload,p.payload_size,key,QABES_DOWN,e->unix_ms,e->time_valid,&e->stamp_down,&plain,&plain_size)){
                        op=qabes_command(plain,plain_size,&channel,&on);
                        if((op==1||op==3)&&io->command)io->command(io->command_context,channel,on); /* 3: find (channel 5) */
                        if(op)e->status_wanted=1;}
                    memset(key,0,sizeof(key));}}
#else
            else if(mttl_mq_command(&e->session,e->session.generation,base,e->rx.bytes,e->rx.total,&command)){if(io->command)io->command(io->command_context,command.channel,command.on);}
            else{mttl_publish p;if(mttl_parse_publish(e->rx.bytes,e->rx.total,&p)&&p.topic_size==20&&!memcmp(p.topic,"homeassistant/status",20)&&p.payload_size==6&&!memcmp(p.payload,"online",6)){e->rediscover=1;e->entity=0;}}
#endif
        }else if(e->session.state!=MTTL_MQ_SYNCING){failed(e,now);return;}mttl_mq_rx_reset(&e->rx);
    }
#if !QABES_REMOTE
    if(e->phase>=10){
        if(e->cleanup_done)return;
        if(e->phase==12){if(!mttl_remaining(e->ping_at,now,10000))failed(e,now);return;}
        if(e->tx_size)return;
        if(e->phase==10&&e->entity<QABES_ENTITIES){n=QABES_DISCOVERY(s,id,e->entity,topic,sizeof(topic),payload,sizeof(payload));if(!n||!queue(e,mttl_mq_publish_packet(e->tx,sizeof(e->tx),topic,0,0,1),now)){failed(e,now);return;}++e->entity;return;}
        if(e->phase==10){snprintf(topic,sizeof(topic),"%s/availability",base);if(!queue(e,mttl_mq_publish_packet(e->tx,sizeof(e->tx),topic,0,0,1),now)){failed(e,now);return;}e->phase=11;return;}
        /* TCP ordering + PINGRESP fences all preceding retained deletions. */
        e->tx[0]=0xc0;e->tx[1]=0;queue(e,2,now);e->ping_at=now;e->phase=12;return;
    }
#endif
    if(e->phase==6||e->rediscover){if(e->tx_size)return;
        if(e->entity<QABES_ENTITIES){n=QABES_DISCOVERY(s,id,e->entity,topic,sizeof(topic),payload,sizeof(payload));if(!n||!queue(e,mttl_mq_publish_packet(e->tx,sizeof(e->tx),topic,(uint8_t *)payload,n,1),now)){failed(e,now);return;}++e->entity;e->phase=6;}
        else{snprintf(topic,sizeof(topic),"%s/availability",base);if(!queue(e,mttl_mq_publish_packet(e->tx,512,topic,(const uint8_t *)"online",6,1),now)){failed(e,now);return;}e->phase=8;e->rediscover=0;}return;}
    if(e->phase==7){e->phase=6;return;}
    if(e->phase==8&&!e->tx_size){if(e->session.state==MTTL_MQ_SYNCING&&!mttl_mq_ready(&e->session,e->session.generation,now)){failed(e,now);return;}e->phase=9;}
    if(e->session.state==MTTL_MQ_ONLINE){if(e->ping_wait&&!mttl_remaining(e->ping_at,now,10000)){failed(e,now);return;}if(!e->tx_size&&!e->ping_wait&&!mttl_remaining(e->last_tx,now,20000)){e->tx[0]=0xc0;e->tx[1]=0;queue(e,2,now);e->ping_wait=1;e->ping_at=now;}}
}
