/* 
 * Quando viene effettuata una connessione da parte di un client, si verifica se l'indirizzo IP
 * di provenienza e' all'interno della blacklist. In tal caso e' possibile trasferire solamente
 * file con header Content-Type 'text/html' o 'text/plain'.
 * Tutti gli altri utenti possono utilizzare il proxy liberamente.
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

int s, t;

int pid;
struct sockaddr_in local, remote, server;

struct hostent * he;

char* blacklist[4] = {"109.118.90.251", "5.90.123.205", "", ""};

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
int s2;
s2=accept(s,(struct sockaddr *)&remote,&len);
printf("Remote address: %.8X\n",remote.sin_addr.s_addr);
if (fork()) continue;
int i,j,s3;
if(s2 == -1){perror("Accept fallita"); exit(1);}
char request[10000];
char request2[10000];
char response[1000];
char response2[10000];

struct header {
  char * n;
  char * v;
} h[100];

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
    h[j].v = hbuffer + i + 2;
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
                printf("%d.%d.%d.%d\n",(unsigned char) he->h_addr[0],(unsigned char) he->h_addr[1],(unsigned char) he->h_addr[2],(unsigned char) he->h_addr[3]); 
                if (( s3 = socket(AF_INET, SOCK_STREAM, 0 )) == -1)
                        { printf("errno = %d\n",errno); perror("Socket Fallita"); exit(-1); }

                server.sin_family = AF_INET;
                server.sin_port =htons(80);
                server.sin_addr.s_addr = *(unsigned int *)(he->h_addr);

                if(-1 == connect(s3,(struct sockaddr *) &server, sizeof(struct sockaddr_in)))
                                {perror("Connect Fallita"); exit(1);}
                sprintf(request,"GET /%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n\r\n",filename,hostname);
                printf("%s\n",request);
                write(s3,request,strlen(request));

      int headersScanned = 1;
      
      char* clientIP = malloc(15);
      sprintf(clientIP, "%d.%d.%d.%d",*((unsigned char*) &remote.sin_addr.s_addr),*((unsigned char*) &remote.sin_addr.s_addr+1),  *((unsigned char*) &remote.sin_addr.s_addr+2), *((unsigned char*) &remote.sin_addr.s_addr+3));
      printf("Client IP is %s\n", clientIP);
      for (i=0; i<sizeof(blacklist)/sizeof(char *) && headersScanned; i++)
         if (!strncmp(clientIP, blacklist[i], strlen(blacklist[i])))
            headersScanned = 0;
     
      printf("headersScanned is %s\n", (headersScanned ? "ON" : "OFF"));

      bzero(h, sizeof(struct header)*100);
      char* contentType = 0;
      int l = 0;
      // Leggo ogni chunk di 2000 byte che mi arriva
      while (t = read(s3, buffer, 2000)) {
         
         // Se ho gia' trovato la fine degli header e ho controllato il tipo di file allora
         // posso direttamente invaire il chunk al client
         if (headersScanned) {
            write(s2, buffer, t);
            continue;
         }

         // Altrimenti continuo a copiare il chunk appena arrivato in coda a quelli arrivati in passato
         strncpy(response+l, buffer, t);

         // Finche' non trovo la fine della sezione header denotata da CRLFCRLF
         for (int k=l + 3; k<l+t; k++) {
            if (response[k] == '\n' && response[k-1] == '\r' && response[k-2] == '\n' && response[k-3] == '\r') {
               headersScanned = 1;
            }
         }
         // Se non ho trovato CRLFCRLF attendo il prossimo chunk
         if (!headersScanned) continue;

         // Oppure posso andare a cercare se negli header c'e' 'Content-Type'
         l += t;

         // Mi faccio anche una copia della risposta "pulita" da poi inviare al client
         // se vedo che i controlli vengono superati
         strncpy(response2, response, l);

         h[0].n = response;
         for (i=0,j=0; i<l; i++) {
            if(response[i]=='\n' && response[i-1]=='\r'){
               response[i-1]=0; // Termino il token attuale
               if (!h[j].n[0]) break;
               h[++j].n=response+i+1;
            }
            if (response[i]==':' && !h[j].v && j>0){
               response[i]=0;
               h[j].v = response + i + 2;

               if (!strcmp(h[j].n, "Content-Type")) {
                  contentType = h[j].v;
                  break;
               }
            }
         }
         
         // Se finito il ciclo non trovo Content-Type, di default non facciamo accedere l'utente alla
         // risorsa richiesta
         // Stessa cosa succede se il tipo non e' ne 'text/plain' ne '/text/html'
         //printf("Content type vale: %s\n", contentType);
         
         if (!contentType || !( strncmp(contentType, "text/plain", strlen("text/plain")) == 0 || strncmp(contentType, "text/html", strlen("text/html")) == 0 )) {
            printf("Niente content-type o sbaglito: %.10s\n", contentType);
            sprintf(response, "HTTP/1.1 404 Not Found\r\nContent-Length:0\r\nConnection:close\r\n\r\n");
            printf("response: %s\n",response);
            write(s2, response, strlen(response));
            break;
         }
            
         // Se arriviamo a questo punto sappiamo che il file e' del tipo consentito, quindi possiamo mandare
         // al client tutto quello che abbiamo bufferizzato intanto che aspettavamo di ricevere tutti gli
         // header
         write(s2, response2, l);

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
