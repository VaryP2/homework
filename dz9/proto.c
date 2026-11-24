#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct { uint32_t id; const char *sender; const char *text; uint64_t timestamp; } Message;
static size_t varint(uint64_t v,unsigned char *out){
    size_t n=0;while(v>=128){out[n++]=(unsigned char)((v&127)|128);v>>=7;}out[n++]=(unsigned char)v;return n;
}
static size_t put_string(unsigned field,const char *s,unsigned char *out){
    size_t n=0,l=strlen(s);n+=varint(((uint64_t)field<<3)|2,out+n);n+=varint(l,out+n);memcpy(out+n,s,l);return n+l;
}
static size_t encode(const Message *m,unsigned char *out){
    size_t n=0;n+=varint((1u<<3)|0,out+n);n+=varint(m->id,out+n);
    n+=put_string(2,m->sender,out+n);n+=put_string(3,m->text,out+n);
    n+=varint((4u<<3)|0,out+n);n+=varint(m->timestamp,out+n);return n;
}
static size_t json_size(const Message *m,char *out,size_t cap){
    int n=snprintf(out,cap,"{\"id\":%u,\"sender\":\"%s\",\"text\":\"%s\",\"timestamp\":%llu}",
        m->id,m->sender,m->text,(unsigned long long)m->timestamp);
    return n>0?(size_t)n:0;
}
int main(void){
    const Message m={42,"alice","Hello from a Protocol Buffers demo",1730000000ULL};
    unsigned char wire[1024];char json[1024];
    size_t pb=encode(&m,wire),js=json_size(&m,json,sizeof(json));
    printf("JSON: %s\n",json);
    printf("Protobuf wire bytes: ");
    for(size_t i=0;i<pb;i++) printf("%02X ",wire[i]);
    putchar('\n');
    printf("JSON size: %zu bytes\nProtobuf size: %zu bytes\n",js,pb);
    printf("Size reduction: %.1f%%\n",js?100.0*(double)(js-pb)/(double)js:0.0);
    const unsigned loops=500000;volatile size_t sink=0;clock_t a=clock();
    for(unsigned i=0;i<loops;i++)sink+=encode(&m,wire);
    clock_t b=clock();
    for(unsigned i=0;i<loops;i++)sink+=json_size(&m,json,sizeof(json));
    clock_t c=clock();
    printf("Encoding %u iterations:\n",loops);
    printf("Protobuf encoder: %.3f s\n",(double)(b-a)/CLOCKS_PER_SEC);
    printf("JSON snprintf baseline: %.3f s\n",(double)(c-b)/CLOCKS_PER_SEC);
    (void)sink;
    puts("Note: timing compares this compact encoder with snprintf JSON formatting, not full production libraries.");
    return 0;
}
