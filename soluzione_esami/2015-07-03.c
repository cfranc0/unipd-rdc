#include<stdio.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

struct sockaddr_in local, remote;
char request[1000001];
char response[1000];
char hbuffer[1000001];

struct header {
  char * n;
  char * v;
} h[100];


int main()
{
char * reqline;
char * method, *url, *ver;
char * filename;
FILE * fin;
int c;
int n;
int i,j,t, s,s2;
int yes = 1;
int len;
if (( s = socket(AF_INET, SOCK_STREAM, 0 )) == -1)
        { printf("errno = %d\n",errno); perror("Socket Fallita"); return -1; }
local.sin_family = AF_INET;
local.sin_port = htons(21667);
local.sin_addr.s_addr = 0;

t= setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int));
if (t==-1){perror("setsockopt fallita"); return 1;}

if ( -1 == bind(s, (struct sockaddr *)&local,sizeof(struct sockaddr_in)))
{ perror("Bind Fallita"); return -1;}

if ( -1 == listen(s,10)) { perror("Listen Fallita"); return -1;}
remote.sin_family = AF_INET;
remote.sin_port = htons(0);
remote.sin_addr.s_addr = 0;
len = sizeof(struct sockaddr_in);
while ( 1 ){
        s2=accept(s,(struct sockaddr *)&remote,&len);
bzero(hbuffer,10000);
bzero(h,sizeof(struct header)*100);
reqline = h[0].n = hbuffer;

int k,l;
for (l=0; (k = read(s2, request+l, 2000)) > 0;) {
   l = l+k;
   if (request[l-1] == '\n' && request[l-2] == '\r') break;
}
request[l] = 0;
strncpy(hbuffer, request, l);

for (i=0,j=0; hbuffer[i]; i++) {
  if(hbuffer[i]=='\n' && hbuffer[i-1]=='\r'){
    hbuffer[i-1]=0; // Termino il token attuale
   if (!h[j].n[0]) break;
   h[++j].n=hbuffer+i+1;
  }
  if (hbuffer[i]==':' && !h[j].v){
    hbuffer[i]=0;
    h[j].v = hbuffer + i + 1;
  }
 }

        printf("%s\n",reqline);
        if(len == -1) { perror("Read Fallita"); return -1;}
        method = reqline;
        for(i=0;reqline[i]!=' ';i++); reqline[i++]=0; 
        url=reqline+i;
        for(; reqline[i]!=' ';i++); reqline[i++]=0; 
        ver=reqline+i;
        for(; reqline[i]!=0;i++); reqline[i++]=0; 
        if ( !strcmp(method,"GET")){
                filename = url+1;
   
      // Se sono nella directory reflect
      if (!strncmp(filename, "reflect", strlen("reflect"))) {
         
         char* ip = malloc(15);
         sprintf(ip, "%d.%d.%d.%d", *((unsigned char*) &remote.sin_addr.s_addr),
           *((unsigned char*) &remote.sin_addr.s_addr+1),
             *((unsigned char*) &remote.sin_addr.s_addr+2),
               *((unsigned char*) &remote.sin_addr.s_addr+3));

         int port = ntohs(remote.sin_port);
   
         sprintf(response, "HTTP/1.1 200 OK\r\n\r\n%s\r\n%s\r\n%d", request, ip, port);
   
         write(s2, response, strlen(response));
         
         free(ip);
         close(s2);
         continue;
      }
      
      fin=fopen(filename,"rt");
                if (fin == NULL){
                        sprintf(response,"HTTP/1.1 404 Not Found\r\n\r\n");
                        write(s2,response,strlen(response));
                        }
                else{ 
                        sprintf(response,"HTTP/1.1 200 OK\r\n\r\n");
                        write(s2,response,strlen(response));
                        while ( (c = fgetc(fin))!=EOF) write(s2,&c,1);
                        fclose(fin);
                        }
        }
        else {
                        sprintf(response,"HTTP/1.1 501 Not Implemented\r\n\r\n");
        write(s2,response,strlen(response));
        }
        close(s2);
}
close(s);
}
