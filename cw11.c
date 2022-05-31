/**
 * @Author: francesco
 * @Date:   2022-04-27T14:29:55-05:00
 * @Last edit by: francesco
 * @Last edit at: 2022-04-27T14:29:55-05:00
 */


#include <stdio.h>
#include <string.h>
// This includes the types used for
#include <sys/types.h>          /* See NOTES */
//
#include <sys/socket.h>
#include <errno.h>
// struct sockaddr_in
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

struct sockaddr_in remote;

// Create a response buffer in the static area so that they are predefined as all zeros
// This should be useful so that the string delimiter is already present (usefulness? not much,
// we change it anyways)
#define buffLen 1024*1024 // 1MB
char response[buffLen];

// Define the header struct in order to save the headers returned in the response
struct Header {
  char *n;
  char *v;
};
// Then create an array of headers to save them
struct Header headers[100];
// And a buffer in which the headers part of the response is store. Please, note that the Header
// structure only has pointers, so each key/value string will need to be terminted with a null
// character. When reading each Header key/value, the program will look back at this buffer
// and read until a termination char is found.
char hResponse[buffLen];

int main() {
   // Remote server IP
   // Can be found using: nslookup <domain>
   unsigned char ipserver[4] = { 142,250,179,228 };
   int s;

   // Create a socket using
   // Domain:   AF_INET       Domain of the connection (Address family: Internet protocol)
   // Type:     SOCK_STREAM   Type of connection to be opened (TCP)
   // Protocol: 0             Specifies which protocol to use for a given domain/type pair.
   //                         The documentation (man socket) specifies that usually only one
   //                         protocol exists, but it can still be possibile to have more.
   if (( s = socket(AF_INET, SOCK_STREAM, 0 )) == -1) {
      printf("errno = %d\n",errno);
      perror("Socket Fallita");
      return -1;
   }

   // When using HTTP1.1, the Host header must be specified when making a request.
   // The Host header identifies the URI requested from the server as multiple
   // hosts may be working at the same IP address
   /*
    * 14.23 Host
    *    [...]
    *    A client MUST include a Host header field in all HTTP/1.1 request
    *    messages . If the requested URI does not include an Internet host
    *    name for the service being requested, then the Host header field MUST
    *    be given with an empty value.
    */
   char *request = "GET / HTTP/1.1\r\nHost:www.google.com\r\n\r\n";
   /*
    * The request string follows the RFC2616 format
    *
    * Request       = Request-Line              ; Section 5.1
    *                 *(( general-header        ; Section 4.5
    *                  | request-header         ; Section 5.3
    *                  | entity-header ) CRLF)  ; Section 7.1
    *                 CRLF
    *                 [ message-body ]          ; Section 4.3
    *
    * Since there is at least one header, another CRLF is required before the body
    */


   // Assign the required information to the remote address object (?)
   remote.sin_family = AF_INET; //
   remote.sin_port = htons(80); // Port HTTP
   remote.sin_addr.s_addr = *((uint32_t *) ipserver); // Remote IP

   // Open a connection on the socket created previously
   if ( -1 == connect(s, (struct sockaddr *) &remote, sizeof(struct sockaddr_in)) ) {
      perror("Connect Fallita");
      return -1;
   }

   // Use the write system call to write on the stream described file descriptor
   // S: socket (file descriptor)
   write(s, request, strlen(request));

   // I will read the response one char at a time to parse the headers, until the termination
   // character is reached
   /*
    * The headers follow this structure:
    *    message-header = field-name ":" [ field-value ]
    * and we can see from the Request grammar that after each header there will be a CRLF
    * plus an extra one after the last header.
    */
   int headersCount = 0;

   headers[0].n = hResponse;
   for (int i=0; read(s, hResponse+i, 1); i++) {

      // When the termination is reached
      if (hResponse[i] == '\n' && hResponse[i-1] == '\r') {
         // Let's terminate the previous string (which is at i-1 since the CRLF is 2 chars)
         hResponse[i-1] = 0;

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
         headers[++headersCount].n = hResponse + i + 1;
      }

      // When a ":" is reached, that is the signal that the value for the previous key is
      // now beign transmitted
      // If the current header has something as value, then I don't want to overwrite it
      if (hResponse[i] == ':' && !headers[headersCount].v) {
         // Let's terminate the previous string (the key name)
         hResponse[i] = 0;
         // And place the pointer for the value string
         headers[headersCount].v = hResponse + i + 1;
      }

   }

   printf("Status string: %s\n\n", headers[0].n);

   printf("Found %d headers\n", headersCount);

   // Looping through the headers received, skipping the first one as it contains the
   // status string

   /*
    * Bodylength contains: the length of the response if provided
    *                      0 if the response has no length
    *                      -1 if chunked
    */
   int bodyLength = 0;

   for (int i=1; i<headersCount; i++) {

      printf("%s: %s\n", headers[i].n, headers[i].v);

      if (!strcmp("Content-Length", headers[i].n)) bodyLength = atoi(headers[i].v);
      if (!strcmp("Transfer-Encoding", headers[i].n) && !strcmp(" chunked", headers[i].v)) bodyLength = -1;

   }

   printf("\nBody length: %d\n", bodyLength);

   // Chunked transfer mode
   if (bodyLength == -1) {

      /*
       * When using chunked transfer, the respose body will follow this structure
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
       *
       */

      // Create a buffer where we put the chunk length chars for parsing
      char chunkBuffer[8];
      // Until we have read the whole response, keep processing new chunks
      int chunkLength = 1;
      int readChunkOffset = 0;
      while(chunkLength) {
         // I read from s one character at a time, knowing that now it contains only the body
         // since the headers have been read by the previous for loop

         // This for loop will read the *chunk portion of the body and stop at the CRLF after
         // the chunk size

         chunkLength = 0;
         for (int j=0; read(s, chunkBuffer + j, 1) && !(chunkBuffer[j] == '\n' && chunkBuffer[j-1] == '\r'); j++) {

            // Convert the chunkSize as hex into an int
            if (chunkBuffer[j] >= 'A' && chunkBuffer[j] <= 'F') chunkLength = chunkLength * 16 + (chunkBuffer[j] - 'A' + 10);
            if (chunkBuffer[j] >= 'a' && chunkBuffer[j] <= 'f') chunkLength = chunkLength * 16 + (chunkBuffer[j] - 'a' + 10);
            if (chunkBuffer[j] >= '0' && chunkBuffer[j] <= '9') chunkLength = chunkLength * 16 + (chunkBuffer[j] - '0');

         }
         printf("Chunk length: %d\n", chunkLength);

         // Now that we know how long the next chunk is going to be: read it and append it
         // to the response buffer
         // In doing so, remember how far the buffer has been used by previous iterations and
         // keep reading until the whole chunk has been processed
         // Saving in 'n' how far I've read from the buffer, as read can read up to
         // chunkLength chars, and in 't' the total numer of chars read from this chunk
         for (int t=0, n=0; t < chunkLength && (n = read(s, response + readChunkOffset, chunkLength - t)) > 0; t += n, readChunkOffset += n);

         // Ensure that the chunk ends with a CRLF
         for (int i=0, n=0; i < 2 && (n = read(s, chunkBuffer + i, 2)); i += n);
         if (chunkBuffer[0] != '\r' || chunkBuffer[1] != '\n') {
            perror("Errore nel chunk");
            return -1;
         }

      }

   }
   // Body-length method
   else if (bodyLength > 0) {

      for (int i=0, n=0; i < bodyLength && (n = read(s, response + i, 2)) > 0; i += n);

   }

   // Terminating the response body with a string terminator
   reponse[readChunkOffset] = 0;
   printf("%s\n", response);

   return 0;
}
