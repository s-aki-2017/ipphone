#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
 
#define N 1024
 
void die(char*);
void* send_data(void*);
void* recv_data(void*);
void phone(int s);
void serv(char **c_info);
void client(char **s_info);
 
void die(char* s){
  perror(s);
  exit(1);
}
 
int mute = 0;
int voice = 1;
struct sockaddr_in addr;
int len = sizeof(addr);
 
void* send_data(void* a){
  int s = *(int *)a;
  
  FILE *fp1;
  char *cmdline1 = "rec -q -V0 -t raw -b 16 -c 1 -e s -r 44100 -";
  if((fp1 = popen(cmdline1,"r")) == NULL){
    die("popen");
  }
 
  int n1,m1;
  unsigned char data1[N];
 
  while(1){
    //読んで送る
    n1 = fread(data1,sizeof(unsigned char),N,fp1);
    if(n1 == -1) die("fread");
 
    if(voice == 1){
      m1 = sendto(s, data1, n1, 0,(struct sockaddr *)&addr,len);
    }  
  }
  return 0;
}
 
void* recv_data(void* a){
  int s = *(int *)a;
  
  FILE *fp2;
  char *cmdline2 = "play -q -V0 -t raw -b 16 -c 1 -e s -r 44100 -";
  if((fp2 = popen(cmdline2,"w")) == NULL){
    die("popen");
  }
 
  int n2,m2;
  unsigned char data2[N];
 
  while(1){
    //受けて書く
    n2 = recvfrom(s, data2, N, 0,(struct sockaddr *)&addr, &len);
    if(n2 == -1){ perror("recv"); exit(1);}
 
    if(mute == 0){
      m2 = fwrite(data2,sizeof(unsigned char),n2,fp2);
    }
    
  }
  return 0;
  
}
 
void phone(int s){
  pthread_t pthread1;
  pthread_t pthread2;
  int p1 = pthread_create(&pthread1,NULL,&send_data,&s);
  int p2 = pthread_create(&pthread2,NULL,&recv_data,&s);
 
  char op;
  while(1){
    scanf("%c",&op);
    if(op == 'm'){
      mute = 1;
      printf("mute on %d\n",mute);
    }
    if(op == 'r'){
      mute = 0;
      printf("mute off %d\n",mute);
    }
    if(op == 'v'){
      voice = 1;
      printf("voice on %d\n",voice);
    }
    if(op == 'w'){
      voice = 0;
      printf("voice off %d\n",voice);
    }
  }
  
  int j1 = pthread_join(pthread1,NULL);
  int j2 = pthread_join(pthread2,NULL);
  return;
}
  
void serv(char **c_info){
  
  int s = socket(PF_INET, SOCK_DGRAM, 0); //ソケット読み込み
 
  //待ち受け情報
  addr.sin_family = AF_INET;
  addr.sin_port = htons(atoi(c_info[1]));
  addr.sin_addr.s_addr = INADDR_ANY;
 
  bind(s, (struct sockaddr *)&addr, sizeof(addr)); //待ち受け番号を設定
 
  //受付
  struct sockaddr_in client_addr;
  unsigned char buf;
  
  recvfrom(s,&buf,1,0,(struct sockaddr *)&addr,&len);
  
  phone(s); //通話
 
  close(s);
 
  return;
}
 
 
void client(char **s_info){
 
  int s = socket(PF_INET, SOCK_DGRAM, 0); //ソケット読み込み
 
  //接続先情報
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(s_info[1]);
  addr.sin_port = htons(atoi(s_info[2]));
 
  sendto(s,"p",1,0,(struct sockaddr *)&addr,len);
  
  phone(s); //通話
  
  close(s);
 
  return;
}
 
 
int main(int argc, char** argv){
 
  if(argc == 2){serv(argv);}
  else if(argc == 3){client(argv);}
  else printf("入力ミス\n");
  
  return 0;
}
