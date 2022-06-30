/*
 * Quando l'utente accede al file 1.html viene inviato anche un cookie che contiene la parola segreta per 
 * ottenere l'accesso al file 2.html.
 * Quando si tenta l'accesso al file 2.html, il server controlla se il cookie e' correttamente impostato
 * e che abbia il valore corretto, e procede col inviare una versione non corretta dello stesso cookie per
 * far si che ad una prossima richiesta il client non abbia piu' la parola segreta salvata.
 * Questo server e' anche in grado di fare il parsing dei cookie quando ne e' presente piu' d'uno e
 * selezionare solamente quello necessario.
 */
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

struct header {
  char * n;
  char * v;
} h[100];


int main()
{
char hbuffer[10000];
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
char* responseHeader = malloc(512);
while ( 1 ){
        s2=accept(s,(struct sockaddr *)&remote,&len);
bzero(hbuffer,10000);
bzero(h,sizeof(struct header)*100);
reqline = h[0].n = hbuffer;
for (i=0,j=0; read(s2,hbuffer+i,1); i++) {
  if(hbuffer[i]=='\n' && hbuffer[i-1]=='\r'){
    hbuffer[i-1]=0; // Termino il token attuale
   if (!h[j].n[0]) break;
   h[++j].n=hbuffer+i+1;
  }
  if (hbuffer[i]==':' && !h[j].v){
    hbuffer[i]=0;
    h[j].v = hbuffer + i + 2;
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
                fin=fopen(filename,"rt");
                if (fin == NULL){
                        sprintf(response,"HTTP/1.1 404 Not Found\r\n\r\n");
                        write(s2,response,strlen(response));
                        }
                else {

         responseHeader = "\r\n";

         if (!strncmp(filename, "1.html", strlen("1.html"))) {
            printf("Sono sul file 1\n");
            responseHeader = "Set-Cookie: access-token=albicocca\r\n\r\n";
         }
         else if (!strncmp(filename, "2.html", strlen("2.html"))) {
            printf("Sono sul file 2\n");
            responseHeader = "Set-Cookie: access-token=1\r\n\r\n";

            char* cookies = 0;
            for(i =1; i<j; i++) {
               if (!strcmp(h[i].n, "Cookie")) {
                  cookies = h[i].v;
                  break;
               }
            }

            if (!cookies) {
               sprintf(response,"HTTP/1.1 403 Forbidden\r\n\r\n");
               write(s2, response, strlen(response));
               fclose(fin);
               close(s2);
               continue;
            }

            char* accessCookie = 0;
            char *hn, *hv;
            hn = cookies;
            for (i=0; cookies[i]; i++) {
               if (cookies[i] == '=') {
                  cookies[i] = 0;
                  hv = cookies + i + 1;

                  if (!strcmp(hn, "access-token")) {
                     accessCookie = hv;
                     break;
                  }

               } else if (cookies[i] == ';') {
                  cookies[i] = 0;
                  hn = cookies + i +2;
               }
            }

            if (!accessCookie || !!strncmp(accessCookie, "albicocca", strlen("albicocca"))) {
               sprintf(response,"HTTP/1.1 403 Forbidden\r\n\r\n");
               write(s2, response, strlen(response));
               fclose(fin);
               close(s2);
               continue;
            }
         }

         sprintf(response,"HTTP/1.1 200 OK\r\n%s", responseHeader);
         printf("HTTP/1.1 200 OK\r\n%s", responseHeader);

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
