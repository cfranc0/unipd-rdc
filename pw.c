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
#include <netinet/in.h> // Hostname resolution
#include <netdb.h>      // Same "
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

// 
struct sockaddr_in local, remote, server;

/**
 * - The client connects to the proxy using the 's' socket
 * - The connection is moved to the 'rs' socket, with a unique port
 *   for each client
 * - The proxy connects to the server requested by the client
 *   using the 'ss' socket
 * 
 *  +--------+        +-------+       +--------+
 *  |        | ==s==> |       |       |        |
 *  | client |<==rs==>| proxy |<==ss=>| server |
 *  |        |        |       |       |        |
 *  +--------+        +-------+       +--------+
 **/

// Create a request/response buffer in the static area so that they are predefined as all zeros
// This should be useful so that the string delimiter is already present (usefulness? not much,
// we change it anyways)
#define buffLen 1024*1024 // 1MB

// Request coming from the client
char request[buffLen];
// Response to be sent back to the client
char response[buffLen];
char hRequest[buffLen];

// Define the header struct in order to save the headers returned in the response
struct Header {
   char *n;
   char *v;
};
// Create an array of headers to save them
struct Header headers[100];

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

      // Fork the program to handle multiple requests
      if (fork()) continue;

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
      for (int i=0; read(rs, hRequest+i, 1); i++) {
         
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
         if (hRequest[i] == ':' && !headers[headersCount].v && headersCount > 0) {
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
      
      // Splitting the request line into its individual components: separating url, method and version
      method = reqLine;
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

      /*
       * When a GET request is recieved, the client is accessing an HTTP website
       * The proxy catches this request, makes an equivalent one to the server (without headers, in this
       * implementation) and sends the response back to the client.
       * The client is aware that a proxy is present and it needs to set it up through the system settings,
       * but this technique can also be used to create "invisible" proxy that can see all the traffic going
       * through a network node.
       */
      if (!strcmp(method, "GET")) {
        
         
         // Parsing the url to get only the hostname
         //    http://www.domain.tld:80/page
         char *hostname, *filename;
         // Cutting the url to remove port numbers

         int i = 0;
         // Going ahead until I find the first ':'
         for (; url[i] != ':' && url[i]; i++);
         // Making sure that after the ':', comes '//'
         if (url[i+1] != '/' || url[i+2] != '/') {
            perror("GET url parsing error");
            return(-1);
         }
         // The hostname starts here
         i = i + 3;
         hostname = url + i;
         // Stop the hostname at the ':' or '/' char
         for (; url[i] != ':' && url[i] != '/' && url[i]; i++);
         url[i] = 0;
         // If the scan stopped because of a ':' char, continue going until a '/'
         // is found which denotes the beginning of the filename
         if (url[i] == ':') {
            for (; url[i] != '/' && url[i]; i++);
            
         }
         filename = url + i + 1;

         // Resolve the remote hostname in order to get the IP address to use in the socket
         struct hostent *remoteIP;
         remoteIP = gethostbyname(hostname);
         printf("Connecting to: %s [%d.%d.%d.%d] \n",hostname, (unsigned char) remoteIP->h_addr[0],(unsigned char) remoteIP->h_addr[1],(unsigned char) remoteIP->h_addr[2],(unsigned char) remoteIP->h_addr[3]);

         // Creating a socket towards the remote server IP that we just resolved
         int ss;
         if ((ss = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("Socket server failure");
            exit(-1);
         }

         server.sin_family = AF_INET;
         server.sin_port   = htons(80);
         server.sin_addr.s_addr = *(unsigned int*)(remoteIP->h_addr);

         // And using the previously created socket to connect to the remote server
         if (connect(ss, (struct sockaddr*) &server, sizeof(struct sockaddr_in)) == -1) {
            perror("Connect server failure");
            exit(-1);
         }

         // TODO: vedere perche connection:close
         sprintf(request, "GET /%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n\r\n", filename, hostname);
         write(ss, request, strlen(request));

         // And when a response is heard from the server, send it back to the client
         for (int t; t = read(ss, response, 2000); ) {
            write(rs, response, t);
         }
         close(ss);

      }
      /*
       * If a CONNECT request is received, the client wants to connect to the specified remote server
       * using a tunnelling approach.
       * This means that the request coming from the server is directly forwareded to the remote
       * server, without having the proxy create a new one.
       * This method allows for TLS encrypted connections to work, as the proxy just exposes its IP
       * address, but the client is the one dictating what to send.
       */
      else if (!strcmp(method, "CONNECT")) {
         
         char *hostname, *port;
         // Parsing the hostname and port from the connection string
         hostname = url;

         int i = 0;
         // Cutting the url at the ':' char which devides hostname and port
         for(; url[i] != ':' && url[i]; i++);
         url[i] = 0;
         port = url + i + 1;

         /*
          printf("Hostna: %s\n", hostname);
          printf("Port  : %s\n", port);
         */

         // Resolving the hostname to an IP address
         struct hostent *remoteIP;
         remoteIP = gethostbyname(hostname);
         printf("Connecting to: %s [%d.%d.%d.%d] \n",hostname, (unsigned char) remoteIP->h_addr[0],(unsigned char) remoteIP->h_addr[1],(unsigned char) remoteIP->h_addr[2],(unsigned char) remoteIP->h_addr[3]);        

         // Creating the ss socket towards the remote server IP
         int ss;
         if ((ss = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("Socket server failure");
            exit(-1);
         }

         server.sin_family = AF_INET;
         server.sin_port = htons((unsigned short)atoi(port));
         server.sin_addr.s_addr = *(unsigned int*) remoteIP->h_addr;

         // Connecting to the remote IP
         if (connect(ss, (struct sockaddr*) &server, sizeof(struct sockaddr_in)) == -1) {
            perror("Server connect failure");
            exit(-1);
         }

         // Once the connection has been done, respond with the established command
         sprintf(response, "HTTP/1.1 200 Estrablished\r\n\r\n");
         write(rs, response, strlen(response));
         
         if (fork()) { // Parent
            
            // The parent keeps listing out for requests coming from the client and for the remote server
            // If one is recieved, forward it to the socket connected with the final IP
            for (int t; t=read(rs, request, 2000);) {
               write(ss, request, t);
               // printf("S << C : %s\n", request2);
            }


         } else { // Child

            // Reading from the ss socket (connection between the proxy and the website server) which contains the response
            // that the server intends to send the client
            for (int t; t=read(ss, response, 2000); ) {
               // And forwarding the response to the client, instead of keeping it to us
               write(rs, response, t);
               // printf("S >> C : %s\n", response2);
            }
            
            close(ss);
            exit(0);
         }

      }

      close(rs);
   
   }

}
