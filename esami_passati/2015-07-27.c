 /*
 * Al momento della richiesta il client controlla se la risorsa richiesta e' presente in cache
 * controllando la directory /cache/ per un file nominato secondo la convenzione definita nella
 * consegna.
 * Se questo viene trovato, viene mostrato direttamente il contenuto del file, senza inviare
 * la richiesta al server,
 * altrimenti viene inviata la richiesta, effettuato il parsing degli header per recuperare
 * 'Last-Modified' e salvato il risultato in locale
 *
 */

#define _XOPEN_SOURCE

#include <stdio.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

struct sockaddr_in remote;
char response[1000001];
struct header {
        char * n;
        char * v;
} h[100];

int main()
{
size_t len = 0;
int i,j,n;

char* resourceName = "/";
char _resourceName[1000];
strcpy(_resourceName, "cache/");
char request[10000];

void normalizeString(char* in, char* out);

// Normalizzo il nome della risorsa secondo le regole indicate
normalizeString(resourceName, _resourceName + 6);

char * statusline;
char hbuffer[10000];
unsigned char ipserver[4] = { 93,184,216,34 };
int s;

if (( s = socket(AF_INET, SOCK_STREAM, 0 )) == -1)
        { printf("errno = %d\n",errno); perror("Socket Fallita"); return -1; }
remote.sin_family = AF_INET;
remote.sin_port = htons(80);
remote.sin_addr.s_addr = *((uint32_t *) ipserver);
if ( -1 == connect(s, (struct sockaddr *)&remote,sizeof(struct sockaddr_in)))
{ perror("Connect Fallita"); return -1;}

// Scrivo nella request line il nome della risorsa che voglio andare a prendere
// ma con il metodo HEAD, quindi prendendo solamente gli header senza il body
sprintf(request, "HEAD %s HTTP/1.0\r\nHost:www.example.com\r\n\r\n", resourceName);

FILE* cached;
if ((cached = fopen(_resourceName, "r")) != NULL) {
   // A questo punto del client dovro' effettuare comunque la richiesta per la
   // risorsa, dei soli header, per controllare che il file che ho salvato
   // sia equivalente alla versione online

   printf("%s\n", request);
   write(s,request,strlen(request));
   
   statusline = h[0].n = hbuffer;
   for (i=0,j=0; read(s,hbuffer+i,1); i++) {
      if(hbuffer[i]=='\n' && hbuffer[i-1]=='\r'){
              hbuffer[i-1]=0; // Termino il token attuale
              if (!h[j].n[0]) break;
              h[++j].n=hbuffer + i + 1;
      }
      if (hbuffer[i]==':' && !h[j].v){
              hbuffer[i]=0;
              h[j].v = hbuffer + i + 2;
      }
   }

   // Cerco l'header Last-Modified tra gli header della risposta
   char* modified = 0;
   for(i=1; i<j; i++) {
      if (!strcmp(h[i].n, "Last-Modified")) {
         printf("**");
         modified = h[i].v;
         break;
      }
      printf("%s: %s\n", h[i].n, h[i].v);
   }
   
   // Prendo l'epoch indicato nel file di cache
   char cacheTime[10];
   fread(cacheTime, sizeof(char), 10, cached);

   time_t cacheTimeInt = atoi(cacheTime);

   // Prendo la data dalla riposta http
   /* 
    * ⚠️ Questa parte del codice non funziona!
    * Il parsing della data a partire dall'header html non ritorna la data corretta
    */
   struct tm* httpTime = malloc(sizeof(struct tm));
   char* format = "%A, %d %b %Y %T";
   time_t httpTimeInt;
   if (modified) {
      char* primiduepunti = strptime(modified, format, httpTime);
      printf("Errore strptime %s\n", primiduepunti);
      printf("%d %d/%d/%d %d:%d:%d\n", httpTime->tm_wday, httpTime->tm_mday, httpTime->tm_mon, httpTime->tm_year, httpTime->tm_hour, httpTime->tm_min, httpTime->tm_sec);

      httpTimeInt = mktime(httpTime);
   }
   free(httpTime);

   // Se l'header non e stato fornito dal server nella risposta mando di defualt
   // quello che ho in cache all'utente.
   // 
   // Se invece l'header e' presente ed e' nel passato rispetto alla data di 
   // scaricamento della cache invio all'utente quello che ho in cache.
   // 
   // Infine, se la cache risulta obsoleta -> Effettuo comunque l'operazione di GET
   // per salvarmi il nuovo file
   if (!modified || (httpTimeInt > cacheTimeInt)) {
      /* */
   
      printf("Invio al client la cache!\n");

      fclose(cached);
      return 0;
   }

   fclose(cached);

}

// A questo punto del file dovro' sicuramente andarmi a prendere il file completo
// dal server, quindi preparo la request line con il metodo GET
sprintf(request, "GET %s HTTP/1.0\r\nHost:www.example.it\r\n\r\n", resourceName);

write(s,request,strlen(request));
statusline = h[0].n = hbuffer;
for (i=0,j=0; read(s,hbuffer+i,1); i++) {
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

// Apro in scrittura+creazione il file di cache
cached = fopen(_resourceName, "w+");

// Ci scrivo il timestamp attuale
fwrite(response, sizeof(response[0]), strlen(response), cached);

// E ci inserisco anche il contenuto della response body
for(len=0; (n = read(s,response + len,1000000 - len))>0; len+=n);
if (n==-1) { perror("Read fallita"); return -1;}
fwrite(response, sizeof(response[0]), strlen(response), cached);

fclose(cached);

response[len]=0;
printf("%s\n", response);
}

void normalizeString(char* in, char* out) {
   for (int l=0; in[l]; l++) {
      if (in[l]=='/') out[l] = '_';
      else out[l] = in[l];
   }
}
