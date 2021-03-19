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
#include <assert.h>
#include <complex.h>
#include <math.h>
#include <string.h>

#define N 512
#define FS 44100
#define f_low 300
#define f_high 3400

typedef short sample_t;

void die(char*);
void* send_data(void*);
void* recv_data(void*);
void phone(int s);
void serv(char **c_info);
void client(char **s_info);

ssize_t write_n(int fd, ssize_t n, void * buf);
void sample_to_complex(sample_t * s, complex double * X, long n);
void complex_to_sample(complex double * X, sample_t * s, long n);
void fft_r(complex double * x, complex double * y, long n, complex double w);
void fft(complex double * x, complex double * y, long n);
void ifft(complex double * y, complex double * x, long n);
int pow2check(long NUM);
void print_complex(FILE * wp, complex double * Y, long n);
void bnadpass(complex double *Y, long n);


void die(char* s){
  perror(s);
  exit(1);
}


void sample_to_complex(sample_t * s, complex double * X, long n) {
  long i;
  for (i = 0; i < n; i++) X[i] = s[i];
}

void complex_to_sample(complex double * X, sample_t * s, long n) {
  long i;
  for (i = 0; i < n; i++) {
    s[i] = creal(X[i]);
  }
}

void fft_r(complex double * x, complex double * y, long n, complex double w) {
  if (n == 1) { y[0] = x[0]; }
  else {
    complex double W = 1.0;
    long i;
    for (i = 0; i < n/2; i++) {
      y[i]     =     (x[i] + x[i+n/2]);
      y[i+n/2] = W * (x[i] - x[i+n/2]);
      W *= w;
    }
    fft_r(y,     x,     n/2, w * w);
    fft_r(y+n/2, x+n/2, n/2, w * w);
    for (i = 0; i < n/2; i++) {
      y[2*i]   = x[i];
      y[2*i+1] = x[i+n/2];
    }
  }
}

void fft(complex double * x, complex double * y, long n) {
  long i;
  double arg = 2.0 * M_PI / n;
  complex double w = cos(arg) - 1.0j * sin(arg);
  fft_r(x, y, n, w);
  for (i = 0; i < n; i++) y[i] /= n;
}

void ifft(complex double * y, complex double * x, long n) {
  double arg = 2.0 * M_PI / n;
  complex double w = cos(arg) + 1.0j * sin(arg);
  fft_r(y, x, n, w);
}

int pow2check(long NUM) {
  long n = NUM;
  while (n > 1) {
    if (n % 2) return 0;
    n = n / 2;
  }
  return 1;
}

void print_complex(FILE * wp, complex double * Y, long n) {
  long i;
  for (i = 0; i < n; i++) {
    fprintf(wp, "%ld %f %f %f %f\n",
        i,
        creal(Y[i]), cimag(Y[i]),
        cabs(Y[i]), atan2(cimag(Y[i]), creal(Y[i])));
  }
}

void bnadpass(complex double *Y, long n){
  long i;
  for(i = 1; i < n/2; i++){
    if((i*FS/n) < f_low){
      Y[i] = 0;
      Y[n-i] = 0;
    }
    else if((i*FS/n) > f_high){
      Y[i] = 0;
      Y[n-i] = 0;
    }
  }
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
  short data1[N];

  while(1){

    n1 = fread(data1,sizeof(short),N,fp1);
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
  short data2[N];

  while(1){

    n2 = recvfrom(s, data2, N, 0,(struct sockaddr *)&addr, &len);
    if(n2 == -1){ perror("recv"); exit(1);}

    long n = (long)n2;
    if (!pow2check(n)) {
      fprintf(stderr, "error : n (%ld) not a power of two\n", n);
      exit(1);
    }

    //sample_t * buf = calloc(sizeof(sample_t), n);

    //for(int i = 0; i < n2; i++){
      //buf[i] = data2[i];
    //}



    complex double * X = calloc(sizeof(complex double), n);
    complex double * Y = calloc(sizeof(complex double), n);

    sample_to_complex(data2, X, n);

    fft(X, Y, n);

    bnadpass(Y,n);

    ifft(Y, X, n);

    complex_to_sample(X, data2, n);

    //for(int i = 0; i < n2; i++){
      //data2[i] = buf[i];
    //}

    if(mute == 0){
      m2 = fwrite(data2,sizeof(short),n2,fp2);
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

  int s = socket(PF_INET, SOCK_DGRAM, 0);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(atoi(c_info[1]));
  addr.sin_addr.s_addr = INADDR_ANY;

  bind(s, (struct sockaddr *)&addr, sizeof(addr));

  struct sockaddr_in client_addr;
  unsigned char buf;

  recvfrom(s,&buf,1,0,(struct sockaddr *)&addr,&len);

  phone(s);

  close(s);

  return;
}


void client(char **s_info){

  int s = socket(PF_INET, SOCK_DGRAM, 0);

  //接続先情報
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(s_info[1]);
  addr.sin_port = htons(atoi(s_info[2]));

  sendto(s,"p",1,0,(struct sockaddr *)&addr,len);

  phone(s);

  close(s);

  return;
}


int main(int argc, char** argv){

  if(argc == 2){serv(argv);}
  else if(argc == 3){client(argv);}
  else printf("入力ミス\n");

  return 0;
}
