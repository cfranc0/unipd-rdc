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
char response[buffLen];

// Define the header struct in order to save the headers returned in the response
struct Header {
   char *n;
   char *v;
};
// Create an array of headers to save them
struct Header headers[100];

/*
 * As specified in the RFC6455, the opening handshake (Section 1.3) requires an exchange of tokens,
 * one of which needs to be (for no apparent reason) concatenated with this predefined token
 */
char* webSocketToken = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
char  webSocketAccept[50], webSocketAcceptToken[50];
/*
 * Importing the SHA1 algorithm as per specifications
 */
#include <openssl/sha.h>

// Declaration of functions
void encodeB64(unsigned char* in, int t, char* out);

int main(int argc, char* argv[], char* env[]) {
   
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
      int rs = accept(s, (struct sockaddr *) &remote, &len);

      // Let's reset the request headers buffer
      bzero(request, buffLen);
      bzero(webSocketAccept, 50);
      bzero(webSocketAcceptToken, 50);
       
      // Clean the headers
      for (int i=0; i<100; i++) {
         headers[i].n = 0;
         headers[i].v = 0;
      }
      int headersCount = 0;

      // The request line is a pointer to the request buffer that contains the HTTP request
      // And is also 
      char* reqLine = headers[0].n = request;

      // As we do for the client, let's read the request one character at a time until there
      // is something to read. When a termination char is reached, the loop will be broken
      for (int i=0; read(rs, request+i, 1); i++) {
         
         // When the termination is reached
         if (request[i] == '\n' && request[i-1] == '\r') {
            // Let's terminate the previous string (which is at i-1 since the CRLF is 2 chars)
            request[i-1] = 0;

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
            headers[++headersCount].n = request + i + 1;
         }

         // When a ":" is reached, that is the signal that the value for the previous key is
         // now beign transmitted
         // If the current header has something as value, then I don't want to overwrite it
         if (request[i] == ':' && !headers[headersCount].v) {
            // Let's terminate the previous string (the key name)
            request[i] = 0;
            // And place the pointer for the value string
            headers[headersCount].v = request + i + 2;
         }

      }

      // Printing the request line
      printf("%s\n", reqLine);
      // And then all the recieved headers, starting from index 1 as index 0 contains ony
      // the request line
      // WARNING: This cycle has been disabled using the 0=false
      for (int i=1; i<headersCount && 0; i++)
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
       
      // Search for the 'Sec-WebSocket-Key' header name and value to perform the initial handshake
      char* webSocketKey = 0;
      for (int i=1; i<headersCount; i++) {
         if (!strcmp(headers[i].n, "Sec-WebSocket-Key")) {
            webSocketKey = headers[i].v;
            break;
         }
      }

      //
      if (!webSocketKey) {
         sprintf(response, "HTTP/1.1 501 Not Implemented\r\n\r\n");
         write(rs, response, strlen(response));
         close(rs);
         continue;
      }

      // This is a websocket request

      /*
       * Performing the handshake:
       *
       * For this header field, the server has to take the value (as present
       * in the header field, e.g., the base64-encoded [RFC4648] version minus
       * any leading and trailing whitespace) and concatenate this with the
       * Globally Unique Identifier (GUID, [RFC4122]) "258EAFA5-E914-47DA-
       * 95CA-C5AB0DC85B11" in string form, which is unlikely to be used by
       * network endpoints that do not understand the WebSocket Protocol.  A
       * SHA-1 hash (160 bits) [FIPS.180-3], base64-encoded (see Section 4 of
       * [RFC4648]), of this concatenation is then returned in the server's
       * handshake.
       */
      strcat(webSocketKey, webSocketToken);
      SHA1(webSocketKey, strlen(webSocketKey), webSocketAccept);
      encodeB64(webSocketAccept, strlen(webSocketAccept), webSocketAcceptToken);

      printf("New connection opened successfully\n");

      sprintf(response, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: WebSocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", webSocketAcceptToken);
      write(rs, response, strlen(response));

      // Sending a sample data packet following the format specified in the
      // 5.2 section of the RFC
      
      sprintf(response + 2, "Ciao dal server\r\n");

      /*
       *  0               1               2               3
       *  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
       *  +-+-+-+-+-------+-+-------------+-------------------------------+
       *  |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
       *  |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
       *  |N|V|V|V|       |S|             |   (if payload len==126/127)   |
       *  | |1|2|3|       |K|             |                               |
       *  +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
       *  |                              ...                              |
       *
       * The first byte contains the FIN code and OP code, alongside some
       * reserved flag bits.
       *    In this implementation, each package is indipendent and every message
       *    is sent in a single package.
       *
       * The second byte contains the mask bit and the payload length. To make
       * sure the mask bit is always 0, a bit-wise and is performed between the
       * length and 0x7F (which is 01111111).
       */
      response[0] = 0x81;
      response[1] = 0x7F & strlen(response + 2);

      write(rs, response, strlen(response));

      // And close the connection socket, to signal that the data is over.
      close(rs);

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
