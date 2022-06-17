/*
 * ⚠️ Questa versione non è stata completamente testata
 *    Usare a proprio rischio e pericolo ;)
 */

#include<stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <signal.h>



int pid;
struct sockaddr_in local, remote, server;
char request[10000];
char request2[10000];
char response[10000];
char response2[10000];

struct header {
  char * n;
  char * v;
} h[100];

struct hostent * he;

char* blacklist[] = {"192.168.0.1", "127.0.0.1", "5.90.123.205"};

int main()
{
char hbuffer[10000];
char buffer[2000];
char * reqline;
char * method, *url, *ver, *scheme, *hostname, *port;
char * filename;
FILE * fin;
int c;
int n;
int i,j,t, s,s2,s3;
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
printf("Remote address: %.8X\n",remote.sin_addr.s_addr);
if (fork()) continue;
if(s2 == -1){perror("Accept fallita"); exit(1);}
bzero(hbuffer,10000);
bzero(h,100*sizeof(struct header));
reqline = h[0].n = hbuffer;
for (i=0,j=0; read(s2,hbuffer+i,1); i++) {
  if(hbuffer[i]=='\n' && hbuffer[i-1]=='\r'){
    hbuffer[i-1]=0; // Termino il token attuale
   if (!h[j].n[0]) break;
   h[++j].n=hbuffer+i+1;
  }
  if (hbuffer[i]==':' && !h[j].v && j>0){
    hbuffer[i]=0;
    h[j].v = hbuffer + i + 1;
  }
 }

        printf("Request line: %s\n",reqline);
        method = reqline;
        for(i=0;i<100 && reqline[i]!=' ';i++); reqline[i++]=0; 
        url=reqline+i;
        for(;i<100 && reqline[i]!=' ';i++); reqline[i++]=0; 
        ver=reqline+i;
        for(;i<100 && reqline[i]!='\r';i++); reqline[i++]=0; 
        if ( !strcmp(method,"GET")){
                scheme=url;
                        // GET http://www.aaa.com/file/file 
                for(i=0;url[i]!=':' && url[i] ;i++);
                if(url[i]==':') url[i++]=0;
                else {printf("Parse error, expected ':'"); exit(1);}
                if(url[i]!='/' || url[i+1] !='/') 
                {printf("Parse error, expected '//'"); exit(1);}
                i=i+2; hostname=url+i;
                for(;url[i]!='/'&& url[i];i++);
                if(url[i]=='/') url[i++]=0;
                else {printf("Parse error, expected '/'"); exit(1);}
                filename = url+i;

                he = gethostbyname(hostname);
                if (( s3 = socket(AF_INET, SOCK_STREAM, 0 )) == -1)
                        { printf("errno = %d\n",errno); perror("Socket Fallita"); exit(-1); }

                server.sin_family = AF_INET;
                server.sin_port =htons(80);
                server.sin_addr.s_addr = *(unsigned int *)(he->h_addr);

                if(-1 == connect(s3,(struct sockaddr *) &server, sizeof(struct sockaddr_in)))
                                {perror("Connect Fallita"); exit(1);}
                sprintf(request,"GET /%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n\r\n",filename,hostname);
                write(s3,request,strlen(request));

      char* myIP = malloc(16);
      sprintf(myIP, "%d.%d.%d.%d", *((unsigned char*) &remote.sin_addr.s_addr), *((unsigned char*) &remote.sin_addr.s_addr+1), *((unsigned char*) &remote.sin_addr.s_addr+2), *((unsigned char*) &remote.sin_addr.s_addr+3));

      printf("Client IP is %s\n", myIP);
      
      // Check if the client IP is in the blacklist
      int blacklisted = 0;
      for (i=0; i<sizeof(blacklist)/sizeof(char*); i++)
         if (!strncmp(myIP, blacklist[i], strlen(blacklist[i])))
            blacklisted = 1;

      free(myIP);

      printf("Blacklisted: %s\n", (blacklisted ? "ON" : "OFF"));

      // Client is not in the whitelist, check Content-Type
      if (blacklisted) {
         int length = 0;
         for (; (t = read(s3, response+length, 2000)) > 0; length = length + t);
         strncpy(response2, response, length);

         char* contentType = 0;
         
         bzero(h, 100*sizeof(struct header));
         h[0].n = response;
         for (i=0,j=0; response[i]; i++) {
            if(response[i]=='\n' && response[i-1]=='\r') {
               response[i-1]=0;
               if (!h[j].n[0]) break;
               h[++j].n=response+i+1;
            }
            if (response[i]==':' && !h[j].v && j>0) {
               response[i]=0;
               h[j].v = response + i + 2;

               if (!strcmp(h[j].n, "Content-Type")) {
                  contentType = h[j].v;
                  break;
               }
            }
         }

         if (!contentType || !( strncmp(contentType, "text/plain", strlen("text/plain")) == 0 || strncmp(contentType, "text/html", strlen("text/html")) == 0 )) {
            sprintf(response2, "HTTP/1.1 404 Not Found\r\nContent-Length:0\r\nConnection:close\r\n\r\n");
         }

         write(s2, response2, strlen(response2));

      } else {
         while ( t=read(s3,buffer,2000)) write(s2,buffer,t);
      }
                close(s3);
                }
        else if(!strcmp("CONNECT",method)) { // it is a connect  host:port 
                hostname=url;
                for(i=0;url[i]!=':';i++); url[i]=0;
                port=url+i+1;
                printf("hostname:%s, port:%s\n",hostname,port);
                he = gethostbyname(hostname);
                if (he == NULL) { printf("Gethostbyname Fallita\n"); return 1;}
                printf("Connecting to address = %u.%u.%u.%u\n", (unsigned char ) he->h_addr[0],(unsigned char ) he->h_addr[1],(unsigned char ) he->h_addr[2],(unsigned char ) he->h_addr[3]); 
                s3=socket(AF_INET,SOCK_STREAM,0);

                if(s3==-1){perror("Socket to server fallita"); return 1;}
                server.sin_family=AF_INET;
                server.sin_port=htons((unsigned short)atoi(port));
                server.sin_addr.s_addr=*(unsigned int*) he->h_addr;
                t=connect(s3,(struct sockaddr *)&server,sizeof(struct sockaddr_in));
                if(t==-1){perror("Connect to server fallita"); exit(0);}
                sprintf(response,"HTTP/1.1 200 Established\r\n\r\n");
                write(s2,response,strlen(response));
                        // <==============
                if(!(pid=fork())){ //Child
                        while(t=read(s2,request2,2000)){
                                write(s3,request2,t);
                        //printf("CL >>>(%d)%s \n",t,hostname); //SOLO PER CHECK
                                }
                        exit(0);
                        }
                else { //Parent
                        while(t=read(s3,response2,2000)){
                                write(s2,response2,t);
                        //printf("CL <<<(%d)%s \n",t,hostname);
                        }
                        kill(pid,SIGTERM);
                        close(s3);
                        }
                }
        else {
                        sprintf(response,"HTTP/1.1 501 Not Implemented\r\n\r\n");
        write(s2,response,strlen(response));
        }
        close(s2);
        exit(1);
}
close(s);
}
