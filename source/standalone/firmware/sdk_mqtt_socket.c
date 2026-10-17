#include "sdk_mqtt_socket.h"
#include "lwip/sockets.h"
#include <errno.h>
#include <string.h>
/* SDK prebuilt lwIP stores errors per socket but does not update global errno. */
static int socket_error(int fd){int error=0;socklen_t length=sizeof(error);return getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)==0?error:-1;}
static int ready(int fd,int writing){fd_set set;struct timeval timeout;FD_ZERO(&set);FD_SET(fd,&set);timeout.tv_sec=timeout.tv_usec=0;return select(fd+1,writing?0:&set,writing?&set:0,0,&timeout);}
static int address(const char *s,uint32_t *value){unsigned i,n,parts=0;uint32_t out=0;while(parts<4){n=0;i=0;while(*s>='0'&&*s<='9'){n=n*10+(unsigned)(*s++-'0');if(++i>3||n>255)return 0;}if(!i)return 0;out=(out<<8)|n;++parts;if(parts<4){if(*s++!='.')return 0;}else if(*s)return 0;}if((out>>24)==0||(out>>24)>=224||out==0xffffffffu)return 0;*value=out;return 1;}
#if QABES_REMOTE
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
/* Host names (public brokers). Blocking lookup, bounded by lwIP's DNS retries (a few
 * seconds), once per connection attempt; 1.1.1.1 backs up the DHCP-provided server. */
static int resolve(const char *host,uint32_t *value){struct hostent *h;ip_addr_t backup=dns_getserver(1);
    if(!backup.addr){IP4_ADDR(&backup,1,1,1,1);dns_setserver(1,&backup);}
    h=lwip_gethostbyname(host);if(!h||h->h_addrtype!=AF_INET||!h->h_addr_list||!h->h_addr_list[0])return 0;
    *value=ntohl(*(const uint32_t *)h->h_addr_list[0]);return *value!=0;}
#else
static int resolve(const char *host,uint32_t *value){(void)host;(void)value;return 0;}
#endif
static int drop(void *context){mttl_sdk_mqtt_socket *s=context;if(s->socket>=0&&close(s->socket)!=0)return -1;s->socket=-1;s->connected=0;return 1;}
static int open_socket(void *context,const char *host,uint16_t port){mttl_sdk_mqtt_socket *s=context;uint32_t ip;struct sockaddr_in a;int error,rc;unsigned long nonblocking=1;
    if(s->connected)return 1;
    if(s->socket<0){if(!port||(!address(host,&ip)&&!resolve(host,&ip)))return -1;s->socket=socket(AF_INET,SOCK_STREAM,0);if(s->socket<0)return -1;
        if(lwip_ioctl(s->socket,FIONBIO,&nonblocking)!=0){drop(s);return -1;}
        memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(ip);
        rc=connect(s->socket,(struct sockaddr *)&a,sizeof(a));if(rc==0){s->connected=1;return 1;}
        error=socket_error(s->socket);if(error!=EINPROGRESS&&error!=EWOULDBLOCK&&error!=EALREADY){drop(s);return -1;}return 0;}
    rc=ready(s->socket,1);if(rc<0)return -1;if(!rc)return 0;
    if(socket_error(s->socket)!=0)return -1;s->connected=1;return 1;
}
static int transmit(void *context,const uint8_t *data,size_t size){mttl_sdk_mqtt_socket *s=context;int rc;if(s->socket<0||!s->connected)return -1;rc=ready(s->socket,1);if(rc<=0)return rc;rc=send(s->socket,data,size,0);return rc<0&&socket_error(s->socket)==EWOULDBLOCK?0:rc;}
static int receive(void *context,uint8_t *data,size_t size){mttl_sdk_mqtt_socket *s=context;int rc;if(s->socket<0||!s->connected)return -1;rc=ready(s->socket,0);if(rc<=0)return rc;rc=recv(s->socket,data,size,0);if(rc==0)return -1;return rc<0&&socket_error(s->socket)==EWOULDBLOCK?0:rc;}
void mttl_sdk_mqtt_socket_init(mttl_sdk_mqtt_socket *s){s->socket=-1;s->connected=0;}
void mttl_sdk_mqtt_socket_io(mttl_sdk_mqtt_socket *s,mttl_mqtt_io *io){io->context=s;io->open=open_socket;io->close=drop;io->send=transmit;io->receive=receive;io->command=0;io->move=0;io->command_context=0;}
