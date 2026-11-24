#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void parse(const unsigned char *b,size_t n){
    if(n<2){puts("Ошибка: нужны минимум 2 байта заголовка");return;}
    unsigned fin=b[0]>>7, rsv=(b[0]>>4)&7, opcode=b[0]&15, mask=b[1]>>7;
    uint64_t len=b[1]&127;size_t pos=2;
    printf("FIN: %u\nRSV: %u\nOpcode: 0x%X\nMASK: %u\n",fin,rsv,opcode,mask);
    if(len==126){if(n<pos+2){puts("Ошибка: нет extended length (16 bit)");return;}len=((uint64_t)b[pos]<<8)|b[pos+1];pos+=2;}
    else if(len==127){if(n<pos+8){puts("Ошибка: нет extended length (64 bit)");return;}len=0;for(int i=0;i<8;i++)len=(len<<8)|b[pos+i];pos+=8;}
    printf("Payload Length: %llu\n",(unsigned long long)len);
    unsigned char key[4]={0};
    if(mask){if(n<pos+4){puts("Ошибка: отсутствует Masking Key");return;}memcpy(key,b+pos,4);pos+=4;printf("Masking Key: %02X %02X %02X %02X\n",key[0],key[1],key[2],key[3]);}
    else puts("Masking Key: отсутствует");
    if(len>n-pos){printf("Ошибка: ожидалось %llu байт payload, доступно %zu\n",(unsigned long long)len,n-pos);return;}
    printf("Payload (decoded): ");
    for(uint64_t i=0;i<len;i++){unsigned char c=b[pos+(size_t)i];if(mask)c^=key[i%4];if(c>=32&&c<=126)putchar(c);else printf("\\x%02X",c);}
    putchar('\n');
}
int main(int argc,char **argv){
    unsigned char bytes[4096];size_t n=0;
    if(argc>1){
        for(int i=1;i<argc&&n<sizeof(bytes);i++){char *end;long v=strtol(argv[i],&end,16);if(*end||v<0||v>255){fprintf(stderr,"Invalid hex byte: %s\n",argv[i]);return 1;}bytes[n++]=(unsigned char)v;}
    }else{
        const unsigned char sample[]={0x81,0x82,0x12,0x34,0x56,0x78,0x7A,0x5D};
        memcpy(bytes,sample,sizeof(sample));n=sizeof(sample);
    }
    parse(bytes,n);return 0;
}
