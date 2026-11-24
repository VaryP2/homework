#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 16
#define MAX_FRAME 8192
#define HTTP_BUF 4096

typedef struct { int fd; int active; } Client;
static Client clients[MAX_CLIENTS];

typedef struct { uint32_t h[5]; uint64_t bits; unsigned char block[64]; size_t used; } SHA1;
static uint32_t rol(uint32_t x, unsigned n) { return (x << n) | (x >> (32-n)); }
static void sha1_transform(SHA1 *s, const unsigned char *p) {
    uint32_t w[80], a,b,c,d,e;
    for(int i=0;i<16;i++) w[i]=((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for(int i=16;i<80;i++) w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    a=s->h[0];b=s->h[1];c=s->h[2];d=s->h[3];e=s->h[4];
    for(int i=0;i<80;i++){
        uint32_t f,k;
        if(i<20){f=(b&c)|((~b)&d);k=0x5a827999;}
        else if(i<40){f=b^c^d;k=0x6ed9eba1;}
        else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdc;}
        else{f=b^c^d;k=0xca62c1d6;}
        uint32_t t=rol(a,5)+f+e+k+w[i];e=d;d=c;c=rol(b,30);b=a;a=t;
    }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;
}
static void sha1_init(SHA1 *s){s->h[0]=0x67452301;s->h[1]=0xefcdab89;s->h[2]=0x98badcfe;s->h[3]=0x10325476;s->h[4]=0xc3d2e1f0;s->bits=0;s->used=0;}
static void sha1_update(SHA1 *s,const unsigned char *p,size_t n){
    s->bits+=(uint64_t)n*8;
    while(n){size_t take=64-s->used;if(take>n)take=n;memcpy(s->block+s->used,p,take);s->used+=take;p+=take;n-=take;if(s->used==64){sha1_transform(s,s->block);s->used=0;}}
}
static void sha1_final(SHA1 *s,unsigned char out[20]){
    s->block[s->used++]=0x80;
    if(s->used>56){while(s->used<64)s->block[s->used++]=0;sha1_transform(s,s->block);s->used=0;}
    while(s->used<56)s->block[s->used++]=0;
    for(int i=7;i>=0;i--)s->block[s->used++]=(unsigned char)(s->bits>>(i*8));
    sha1_transform(s,s->block);
    for(int i=0;i<5;i++){out[i*4]=(unsigned char)(s->h[i]>>24);out[i*4+1]=(unsigned char)(s->h[i]>>16);out[i*4+2]=(unsigned char)(s->h[i]>>8);out[i*4+3]=(unsigned char)s->h[i];}
}
static void base64(const unsigned char *in,size_t n,char *out){
    static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t j=0;
    for(size_t i=0;i<n;i+=3){
        unsigned v=(unsigned)in[i]<<16;
        if(i+1<n)v|=(unsigned)in[i+1]<<8;
        if(i+2<n)v|=in[i+2];
        out[j++]=t[(v>>18)&63];out[j++]=t[(v>>12)&63];
        out[j++]=(i+1<n)?t[(v>>6)&63]:'=';
        out[j++]=(i+2<n)?t[v&63]:'=';
    }
    out[j]='\0';
}
static int send_all(int fd,const void *buf,size_t n){
    const unsigned char *p=buf;
    while(n){ssize_t k=send(fd,p,n,0);if(k<=0)return -1;p+=k;n-=(size_t)k;}
    return 0;
}
static int send_frame(int fd,unsigned opcode,const unsigned char *data,size_t len){
    unsigned char h[10];size_t n=0;h[n++]=(unsigned char)(0x80|(opcode&0x0f));
    if(len<126)h[n++]=(unsigned char)len;
    else if(len<=65535){h[n++]=126;h[n++]=(unsigned char)(len>>8);h[n++]=(unsigned char)len;}
    else return -1;
    if(send_all(fd,h,n)<0)return -1;
    return len?send_all(fd,data,len):0;
}
static void remove_client(int i){if(clients[i].active){close(clients[i].fd);clients[i].active=0;clients[i].fd=-1;}}
static void broadcast(const unsigned char *msg,size_t len){
    for(int i=0;i<MAX_CLIENTS;i++)if(clients[i].active)
        if(send_frame(clients[i].fd,1,msg,len)<0)remove_client(i);
}
static int handshake(int fd){
    char req[HTTP_BUF];size_t used=0;
    while(used<sizeof(req)-1){
        ssize_t n=recv(fd,req+used,sizeof(req)-1-used,0);if(n<=0)return -1;
        used+=(size_t)n;req[used]='\0';if(strstr(req,"\r\n\r\n"))break;
    }
    char *key=strcasestr(req,"Sec-WebSocket-Key:");
    if(!key || !strstr(req,"Upgrade: websocket"))return -1;
    key+=18;while(*key==' '||*key=='\t')key++;
    char client_key[128]={0};size_t k=0;
    while(key[k]&&key[k]!='\r'&&key[k]!='\n'&&k<sizeof(client_key)-1){client_key[k]=key[k];k++;}
    const char *guid="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[256];snprintf(combined,sizeof(combined),"%s%s",client_key,guid);
    SHA1 s;unsigned char digest[20];char accept[32];sha1_init(&s);sha1_update(&s,(unsigned char*)combined,strlen(combined));sha1_final(&s,digest);base64(digest,20,accept);
    char response[512];int n=snprintf(response,sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",accept);
    return send_all(fd,response,(size_t)n);
}
static int read_exact(int fd,unsigned char *p,size_t n){
    size_t got=0;while(got<n){ssize_t r=recv(fd,p+got,n-got,0);if(r<=0)return -1;got+=(size_t)r;}return 0;
}
/* Returns 0 for handled frame, -1 for connection close/error. */
static int handle_frame(int fd){
    unsigned char h[2];if(read_exact(fd,h,2)<0)return -1;
    unsigned fin=h[0]>>7, opcode=h[0]&15, masked=h[1]>>7;
    uint64_t len=h[1]&127;
    if((h[0]&0x70)!=0 || !masked || !fin)return -1;
    if(len==126){unsigned char x[2];if(read_exact(fd,x,2)<0)return -1;len=((uint64_t)x[0]<<8)|x[1];}
    else if(len==127){unsigned char x[8];if(read_exact(fd,x,8)<0)return -1;len=0;for(int i=0;i<8;i++)len=(len<<8)|x[i];}
    if(len>MAX_FRAME || ((opcode&8)&&len>125))return -1;
    unsigned char mask[4],payload[MAX_FRAME+1];
    if(read_exact(fd,mask,4)<0 || read_exact(fd,payload,(size_t)len)<0)return -1;
    for(uint64_t i=0;i<len;i++)payload[i]^=mask[i%4];
    if(opcode==0x8){(void)send_frame(fd,0x8,payload,(size_t)len);return -1;}
    if(opcode==0x9){if(send_frame(fd,0xA,payload,(size_t)len)<0)return -1;return 0;}
    if(opcode==0xA)return 0;
    if(opcode==0x1){payload[len]='\0';printf("chat: %s\n",payload);fflush(stdout);broadcast(payload,(size_t)len);return 0;}
    return 0;
}
static void serve_static(int fd){
    FILE *f=fopen("public/index.html","rb");
    if(!f){const char *m="HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";send_all(fd,m,strlen(m));return;}
    fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    char *data=malloc((size_t)size);if(!data){fclose(f);return;}
    size_t n=fread(data,1,(size_t)size,f);fclose(f);
    char h[256];int hn=snprintf(h,sizeof(h),"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",n);
    send_all(fd,h,(size_t)hn);send_all(fd,data,n);free(data);
}
int main(int argc,char **argv){
    int port=argc>1?atoi(argv[1]):8080;if(port<1||port>65535){fprintf(stderr,"Invalid port\n");return 1;}
    signal(SIGPIPE,SIG_IGN);for(int i=0;i<MAX_CLIENTS;i++){clients[i].fd=-1;clients[i].active=0;}
    int s=socket(AF_INET,SOCK_STREAM,0);if(s<0){perror("socket");return 1;}
    int yes=1;setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons((unsigned short)port);
    if(bind(s,(struct sockaddr*)&a,sizeof(a))<0||listen(s,16)<0){perror("bind/listen");close(s);return 1;}
    printf("WebSocket chat: http://127.0.0.1:%d/ (max %d clients)\n",port,MAX_CLIENTS);fflush(stdout);
    for(;;){
        fd_set set;FD_ZERO(&set);FD_SET(s,&set);int max=s;
        for(int i=0;i<MAX_CLIENTS;i++)if(clients[i].active){FD_SET(clients[i].fd,&set);if(clients[i].fd>max)max=clients[i].fd;}
        int ready=select(max+1,&set,NULL,NULL,NULL);if(ready<0){if(errno==EINTR)continue;perror("select");break;}
        if(FD_ISSET(s,&set)){
            int c=accept(s,NULL,NULL);
            if(c>=0){
                char peek[8];ssize_t n=recv(c,peek,sizeof(peek),MSG_PEEK);
                if(n>0 && n>=3 && memcmp(peek,"GET",3)==0){
                    int slot=-1;for(int i=0;i<MAX_CLIENTS;i++)if(!clients[i].active){slot=i;break;}
                    if(slot<0){close(c);}
                    else if(handshake(c)==0){clients[slot].fd=c;clients[slot].active=1;printf("client connected (%d)\n",slot);}
                    else {serve_static(c);close(c);}
                } else {serve_static(c);close(c);}
            }
        }
        for(int i=0;i<MAX_CLIENTS;i++)if(clients[i].active&&FD_ISSET(clients[i].fd,&set))
            if(handle_frame(clients[i].fd)<0){printf("client disconnected (%d)\n",i);remove_client(i);}
    }
    close(s);return 0;
}
