/* Host test: RFC 8439 vectors. cc -I../firmware test_qabes_aead.c ../firmware/qabes_aead.c */
#include "qabes_aead.h"
#include <stdio.h>
#include <string.h>
static int fails;
static void hex(const char *s,uint8_t *o,size_t *n){size_t k=0;unsigned v;while(*s){if(*s==' '||*s==':'||*s=='\n'){s++;continue;}sscanf(s,"%2x",&v);o[k++]=(uint8_t)v;s+=2;}*n=k;}
static void eq(const char *what,const uint8_t *a,const uint8_t *b,size_t n){if(memcmp(a,b,n)){printf("FAIL %s\n",what);fails++;}else printf("ok %s\n",what);}
int main(void){
    uint8_t key[32],nonce[12],buf[256],want[256],tag[16],aad[16];size_t n,m,k;
    const char *sun="Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
    /* 2.4.2 */
    for(k=0;k<32;k++)key[k]=(uint8_t)k;hex("000000000000004a00000000",nonce,&n);
    m=strlen(sun);memcpy(buf,sun,m);qabes_chacha20(buf,buf,m,key,nonce,1);
    hex("6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0bf91b65c5524733ab8f593dabcd62b3571639d624e65152ab8f530c359f0861d807ca0dbf500d6a6156a38e088a22b65e52bc514d16ccf806818ce91ab77937365af90bbf74a35be6b40b8eedf2785e42874d",want,&n);
    eq("chacha20 2.4.2",buf,want,m);
    /* 2.5.2 */
    hex("85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b",key,&n);
    qabes_poly1305(tag,(const uint8_t*)"Cryptographic Forum Research Group",34,key);
    hex("a8061dc1305136c6c22b8baf0c0127a9",want,&n);eq("poly1305 2.5.2",tag,want,16);
    /* 2.8.2 */
    for(k=0;k<32;k++)key[k]=(uint8_t)(0x80+k);hex("070000004041424344454647",nonce,&n);hex("50515253c0c1c2c3c4c5c6c7",aad,&n);
    memcpy(buf,sun,m);qabes_aead_seal(buf,m,aad,12,key,nonce,tag);
    hex("d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116",want,&n);
    eq("aead 2.8.2 ciphertext",buf,want,m);hex("1ae10b594f09e26a7e902ecbd0600691",want,&n);eq("aead 2.8.2 tag",tag,want,16);
    if(!qabes_aead_open(buf,m,aad,12,key,nonce,tag)||memcmp(buf,sun,m)){printf("FAIL open\n");fails++;}else printf("ok open\n");
    qabes_aead_seal(buf,m,aad,12,key,nonce,tag);buf[5]^=1;
    if(qabes_aead_open(buf,m,aad,12,key,nonce,tag)){printf("FAIL tamper accepted\n");fails++;}else printf("ok tamper rejected\n");
    /* lengths around block edges */
    for(m=0;m<140;m++){memset(buf,(int)m,m);qabes_aead_seal(buf,m,aad,(m%13),key,nonce,tag);if(!qabes_aead_open(buf,m,aad,(m%13),key,nonce,tag)){printf("FAIL len %u\n",(unsigned)m);fails++;break;}}
    printf(fails?"FAILED %d\n":"ALL OK\n",fails);return fails!=0;
}
