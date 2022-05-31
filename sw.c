/**
 * @Author: francesco
 * @Date:   2022-05-11T20:56:47+02:00
 * @Last edit by: francesco
 * @Last edit at: 2022-05-11T21:03:27+02:00
 */


/*
 * In the exam, it is required to put a comment on the top portion of the code explaining
 * the top-level steps
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

//
struct sockaddr_in local, remote;

// Create a request/response buffer in the static area so that they are predefined as all zeros
// This should be useful so that the string delimiter is already present (usefulness? not much,
// we change it anyways)
#define buffLen 1024*1024 // 1MB
// This is roughly the same code as the client, but here we are creating two pairs for buffer
// as the server recieves a request, and needs to compose a response.
char request[buffLen];
char hRequest[buffLen];
char response[buffLen];

// Define the header struct in order to save the headers returned in the response
struct Header {
   char *n;
   char *v;
};
// Create an array of headers to save them
struct Header headers[100];

// Declaration of functions
int readUntilNewLine(FILE* file, char* out);
void encodeB64(unsigned char* in, int t, char* out);

int main() {

   int s;
   if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      printf("Socket fallita con errno = %d\n", errno);
      perror("Socket fallita");
      return -1;
   }

   // Assign the required information to the local address object
   local.sin_family = AF_INET;
   local.sin_port = htons(21667);
   // But this time set the IP address to zero, instead of a remote IP
   local.sin_addr.s_addr = 0;

   // And do it again with the remote address object
   remote.sin_family = AF_INET;
   // And let's set these as 0 just for good mesure, it's not really necessary
   remote.sin_port = htons(0);
   remote.sin_addr.s_addr = 0;

   /*
    * We also need to set an extra option on the socket for the server:
    * the local address can be reused as long as there is no socket actively
    * listening to that address. This allowes to open a new server without
    * having to wait for the previous one to completely close.
    */
   int t;
   int yes = 1;
   if ( setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ) {
      perror("setsockopt fallita");
      return -1;
   }

   // The bind function binds to a specific socket a certain local address/port pair
   if ( bind(s, (struct sockaddr *)&local,sizeof(struct sockaddr_in)) == -1 ) {
      perror("Bind Fallita");
      return -1;
   }

   // Finally, before actually starting the server, the socket needs to be passive,
   // accepting remote connections
   //
   // int listen(int sockfd, int backlog);
   // Where  sockfd: the target socket
   //       backlog: the maximum number of pending connections to allow
   if ( listen(s, 10) == -1) {
      perror("Listen fallita");
      return -1;
   }

   // This is the main loop of the server, always waiting for new connections to be
   // started from client
   int len = sizeof(struct sockaddr);
   while (1) {

      /*
       * The accept call waits for a new connection on the specified 's' socket,
       * knowing the information of the peer socket contained at the sockaddr pointer
       * and the length of the sockaddr struct.
       * The accept call returns a non-negative integer that is the file descriptor
       * created for that accepted socket.
       */
      int ss = accept(s, (struct sockaddr *) &remote, &len);

      // Let's reset the request headers buffer
      bzero(hRequest, buffLen);

      // Clean the headers
      for (int i=0; i<100; i++) {
         headers[i].n = 0;
         headers[i].v = 0;
      }
      int headersCount = 0;

      // The request line is a pointer to the request buffer that contains the HTTP request
      // And is also
      char* reqLine = headers[0].n = hRequest;

      // As we do for the client, let's read the request one character at a time until there
      // is something to read. When a termination char is reached, the loop will be broken
      for (int i=0; read(ss, hRequest+i, 1); i++) {

         // When the termination is reached
         if (hRequest[i] == '\n' && hRequest[i-1] == '\r') {
            // Let's terminate the previous string (which is at i-1 since the CRLF is 2 chars)
            hRequest[i-1] = 0;

            /*
             * We know we reached the end of the headers part of the response when we see
             * 2 consecutive CRLF. But the program at this point does not know how to
             * differenciate between a header terminator and a headers end.
             * Since a CRLF stars a new header, and the value is terminated with a 0 as a
             * string terminator, if the previous header's key name is all 0s (=false),
             * then the last header was null and that is the indicator that we reached
             * the end of the headers section.
             */
            if (!headers[headersCount].n[0]) break;

            // The value is the string beginning just after the \n char
            headers[++headersCount].n = hRequest + i + 1;
         }

         // When a ":" is reached, that is the signal that the value for the previous key is
         // now beign transmitted
         // If the current header has something as value, then I don't want to overwrite it
         if (hRequest[i] == ':' && !headers[headersCount].v) {
            // Let's terminate the previous string (the key name)
            hRequest[i] = 0;
            // And place the pointer for the value string
            headers[headersCount].v = hRequest + i + 1;
         }

      }

      // Printing the request line
      printf("%s\n", reqLine);
      // And then all the recieved headers, starting from index 1 as index 0 contains ony
      // the request line
      // WARNING: This cycle has been disabled using the 0=false
      for (int i=1; i<headersCount & 0; i++)
         printf("%s: %s\n", headers[i].n, headers[i].v);

      // Now we are going to "pollute" the reuqest line, by adding string termination characters,
      // but they are not really a problem as we actually need to understand each single part of
      // the request separately in order to be able to serve it
      // The request line follows this structure:
      //    Request-Line = Methodtruct Header headers[100];
      //    108       int headersCount = 0;
      //    SP Request-URI SP HTTP-Version CRLF
      char *method, *url, *version;
      method = reqLine;

      // Using a pointer to
      int p = 0;
      for (p=0; reqLine[p] != ' '; p++);
      reqLine[p++] = 0;
      url = reqLine + p;
      for (; reqLine[p] != ' '; p++);
      reqLine[p++] = 0;
      version = reqLine + p;
      for (; reqLine[p] == '\n' && reqLine[p-1] == '\r'; p++);
      reqLine[p - 1] = 0;

      /*
         printf("Method : %s\n", method);
         printf("Url    : %s\n", url);
         printf("Version: %s\n", version);
       */

      // Handing GET requests
      if (!strcmp(method, "GET")) {

         // The file that the client wants is contained in the url parameter, starting from the
         // second char, as the first one is '/'
         char* filename = url+1;

         // If the request is for the /cgi-bin/ dir, the a new program needs to be spun up, passing
         // all the env variables, in order to fulfill the request


         // Try to read the file requested by the client
         FILE* file = fopen(filename, "rt");

         // If no file is found, tell the client that
         if (file == NULL) {
            sprintf(response, "HTTP/1.1 404 Not Found\r\n\r\n");
            write(ss, response, strlen(response));
         }
         /*
          * When the file is found, send it back to the client
          * The structure used is:
          * Response      = Status-Line               ; Section 6.1
          *                 *(( general-header        ; Section 4.5
          *                  | response-header        ; Section 6.2
          *                  | entity-header ) CRLF)  ; Section 7.1
          *                 CRLF
          *                 [ message-body ]          ; Section 7.2
          */
         else {

            // First of all, make sure the request is not for a secure file,
            // meaning a file in the /secure/ dir, for which the user is required to provide
            // Basic http auth
            if (strncmp(url, "/secure/", strlen("/secure/")) == 0) {

               // Check if the request comes with an auth header
               char* authHeader = 0;
               int i = 0;
               while (!authHeader && i < headersCount - 1) {
                  if (strcmp(headers[++i].n, "Authorization") == 0)
                     authHeader = headers[i].v;
               }

               printf("Auth found is: %s\n", authHeader);

               // If the auth header is not provided, return a 401 error and inform the user that
               // basic authication is available using the 'WWW-Authenticate' header
               // This same type of response is given if the auth token is not a Bearer token
               if (!authHeader || (strncmp(authHeader, " Basic", strlen(" Basic")) != 0)) {
                  sprintf(response, "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate:basic\r\n\r\n");
                  write(ss, response, strlen(response));

                  printf("-- End of request\n\n");

                  fclose(file);
                  close(ss);
                  continue;
               }

               // Scan the credentials file and see if the token provided by the client is a valid
               // combination of username:password
               FILE *credentials = fopen("users.txt", "r");
               char *token = malloc(100), *encodedToken = malloc(134);
               int l, userFound = 0;
               while ((l = readUntilNewLine(credentials, token)) > 0) {
                  // Try to B64 encode each line and compare to the encoded one provided
                  // It was equally correct to decode the provided token and compare to the
                  // ones on file
                  encodeB64((unsigned char*) token, l, encodedToken);
                  // The token starts with " Basic ", so we need to exclude it when comparing it
                  // to the credentials on file (by adding the corrisponding offset)
                  if (strcmp(authHeader + strlen(" Basic "), encodedToken) == 0) {
                     userFound = 1;
                     break;
                  }
               }
               free(token); free(encodedToken);

               // If a user is not found by the end of the cycle, this combination of username
               // and password is not valid and the request is unauthorized
               if (!userFound) {
                  sprintf(response, "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate:basic\r\n\r\n");
                  write(ss, response, strlen(response));

                  printf("-- End of request\n\n");

                  fclose(file);
                  close(ss);
                  continue;
               }

            }

            // Now that we know the user can ask this resource, determine the length of the
            // file to be transferred in bytes
            fseek(file, 0L, SEEK_END);
            int size = ftell(file);
            rewind(file);

            /*
             * If the file is larger than 5KB (arbitrary amount, really), use the chunked
             * transfer encoding
             *
             * Chunked-Body   = *chunk
             *                  last-chunk
             *                  trailer
             *                  CRLF
             *
             * chunk          = chunk-size [ chunk-extension ] CRLF
             *                  chunk-data CRLF
             * chunk-size     = 1*HEX
             * last-chunk     = 1*("0") [ chunk-extension ] CRLF
             *
             * chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
             * chunk-ext-name = token
             * chunk-ext-val  = token | quoted-string
             * chunk-data     = chunk-size(OCTET)
             * trailer        = *(entity-header CRLF)
             */
            if (size > (1024 * 5)) {

               sprintf(response, "HTTP/1.1 200 OK\r\nTransfer-Encoding:chunked\r\n\r\n");
               write(ss, response, strlen(response));

               // Create a buffer in which to write the portion read from the file
               char* fileBuff[1024*5];

               // Then trying to fill up the buffer, sending the client the amount of the
               // file read in that cycle, until the whole file has been sent
               int c=0;
               for (int l=0; (c = fread(fileBuff, sizeof(char), 1024*5, file)) > 0; l += c) {

                  // Send in the chunk size as hex + CRLF
                  sprintf(response, "%x\r\n", c);
                  write(ss, response, strlen(response));
                  // Followed by the chunk itself
                  write(ss, fileBuff, c);
                  // + CRLF
                  sprintf(response, "\r\n");
                  write(ss, response, strlen(response));
               }

               // Sending the last empty chunk per standard
               sprintf(response, "0\r\n\r\n");
               write(ss, response, strlen(response));

            }
            // Otherwise, use the standard transfer method with the Content-Length header
            else {
               // Which means: send a 200 OK response and include the Content-Length of the
               // response body
               sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length:%d\r\n\r\n", size);
               write(ss, response, strlen(response));

               // Then send each char to the client
               int c;
               while(( c = fgetc(file)) != EOF ) write(ss, &c, 1);
            }
            fclose(file);

         }

      }
      // When the method is not one of the implemented one, send the client a 501 HTTP response
      else {
         sprintf(response, "HTTP/1.1 501 Not Implemented\r\n\r\n");
         write(ss, response, strlen(response));
      }

      // And close the connection socket, to signal that the data is over.
      close(ss);

      printf("-- End of request\n\n");

   }

}

/**
 * readUntilNewLine reads from the file pointed by file and
 * puts in the out buffer the characters until a \n char is reached
 */
int readUntilNewLine(FILE* file, char* out) {

   int i = 0;
   for (char c; (c = fgetc(file)) != EOF && c != '\n'; i++) out[i] = c;
   out[i + 1] = 0;

   return i;

}

/*
 * In this little picture, each letter rapresents a different char
 *
 * unsigned int | aaaaaa aabbbb bbbbcc cccccc
 *          B64 | AAAAAA BBBBBB CCCCCC DDDDDD
 */

/**
 * encodeB64 allows to encode a
 * char* in   string to encode
 * int   t    lenght of the in string
 * char* out  B64 result
 */
void encodeB64(unsigned char* in, int t, char* out) {

   char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

   // Every 3 unsigned int (1Byte variables) make up 4 B64 chars
   // Using a loop, I will take groups of 3 chars to then converted
   int l = 0;
   int e = 0;

   for (int i = 0; l < t; i = ++i%3, l++) {

      // If we are reading the first character we need to remove the last
      // 2 characters
      if (i==0){
         out[e++] = b64[in[l] >> 2];
         out[e++] = b64[(in[l] & 0x03) << 4];
      }
      // If we are reading the second char we need to construct the second and third B64
      // chars
      if (i==1) {
         out[e - 1] = b64[((in[l - 1] & 0x03) << 4) | (in[l] >> 4)];
         out[e++] = b64[(in[l] & 0x0F) << 2];
      }
      // Finally, we are at the third char, let's complete the 3rd B64 char
      // and create the 4th
      if (i==2) {
         out[e - 1] = b64[((in[l - 1] & 0x0F) << 2) | (in[l] >> 6)];
         out[e++]   = b64[in[l] & 0x3F];
      }

   }
   if (l%3 == 2 || l%3 == 1) out[e++] = '=';
   if (l%3 == 1) out[e++] = '=';
   out[e] = 0;

}
