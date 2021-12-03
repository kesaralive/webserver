#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/stat.h>
#include <strings.h>
#include <string.h>

#define ISspace(x) isspace((int)(x))

int startup(unsigned short *port);
void errorDie(const char *identifier);
void acceptRequests(int clientSock);
int getLine(int socket, char *buffer, int size);
void cat(int client, FILE *resource);
void serveFile(int client, const char *filename);

/*HEADERS*/
void headers(int client, const char *filename);
void badRequest(int client); //exec
void cannotExec(int client); //exec
void unimplemented(int client);
void notFound(int client);

/*************************************/
/**This function begins the listening for web connections on a specific port.
 * If the port is 0, then it will allocate a port to the server dynamically and,
 * modify the original port variable to reflect the actual port.
 * parameters: pointer to the variable containing the port to connect on
 * returns: the socket */
/*************************************/
int startup(unsigned short *port)
{
    int sockfd = 0;
    struct sockaddr_in name; 

    sockfd = socket(PF_INET, SOCK_STREAM, 0);

    if(sockfd == -1)
    {
        errorDie("socket");
    }

    memset(&name, 0, sizeof(name));

    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr *)&name, sizeof(name)) > 0)
    {
        errorDie("bind");
    }

    if(*port == 0)
    {
        int namelength = sizeof(name);
        if(getsockname(sockfd, (struct sockaddr *)&name, &namelength) == -1)
        {
            errorDie("getsockname");    
        }
        *port = ntohs(name.sin_port);
    }

    if(listen(sockfd, 10) < 0)
    {
        errorDie("listen");
    } 

    return sockfd;
}

/*************************************/
/** 
 * Task: process the request appropriately.
 * parameters: the socket connected to the client 
 */
/*************************************/
void acceptRequests(int clientSock)
{
    char buffer[1024];
    int numChars;
    char requestMethod[255];
    char url[255];
    char filePath[512];

    size_t i, j;
    struct stat st;
    int cgiF = 0;
    //becomes true(1) if the server decides this is a CGI script/program.

    char *queryString = NULL;

    numChars = getLine(clientSock, buffer, sizeof(buffer));
    i = 0; 
    j = 0;

    while(!ISspace(buffer[j]) && (i < sizeof(requestMethod) - 1))
    {
        requestMethod[i] = buffer[j];
        i++;
        j++;
    }

    requestMethod[i] = '\0';

    if(strcasecmp(requestMethod, "GET") && strcasecmp(requestMethod, "POST"))
    {
        unimplemented(clientSock); //send unimplemented header
    }

    if(strcasecmp(requestMethod, "POST") == 0)
    {
        cgiF = 1;
    }

    i = 0;
    while(ISspace(buffer[j]) && (i < sizeof(buffer)))
    {
        j++;
    }
    while(!ISspace(buffer[j]) && (i < sizeof(url) - 1) && (j < sizeof(buffer)))
    {
        url[i] = buffer[j];
        i++;
        j++;
    }
    url[i] = '\0';

    if(strcasecmp(requestMethod, "GET") == 0)
    {
        queryString = url;
        while((*queryString != '?') && (*queryString != '\0'))
        {
            queryString++;
        }
        if(*queryString == '?')
        {
            cgiF = 1;
            *queryString = '\0';
            queryString++;
        }
    }

    sprintf(filePath, "htdocs%s", url);

    if(filePath[strlen(filePath) - 1] == '/')
    {
        strcat(filePath,"index.html");
    }

    if(stat(filePath, &st) == -1)
    {
        while((numChars > 0) && strcmp("\n", buffer)) //read and discard headers
        {
            numChars = getLine(clientSock, buffer, sizeof(buffer));
        }
        notFound(clientSock); //send not found header
    }
    else
    {
        if((st.st_mode & __S_IFMT) == __S_IFDIR)
        {
            strcat(filePath, "/index.html");
        }

        if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
        {
            cgiF = 1;
        }

        if(!cgiF)
        {
            serveFile(clientSock, filePath);
        }
        else
        {
            // executeCGI(clientSock, filePath, requestMethod, queryString);
            exit(1);
        }
    }
    close(clientSock);
}

int get_line(int sock, char *buf, int size)
{
 int i = 0;
 char c = '\0';
 int n;

 while ((i < size - 1) && (c != '\n'))
 {
  n = recv(sock, &c, 1, 0);
  /* DEBUG printf("%02X\n", c); */
  if (n > 0)
  {
   if (c == '\r')
   {
    n = recv(sock, &c, 1, MSG_PEEK);
    /* DEBUG printf("%02X\n", c); */
    if ((n > 0) && (c == '\n'))
     recv(sock, &c, 1, 0);
    else
     c = '\n';
   }
   buf[i] = c;
   i++;
  }
  else
   c = '\n';
 }
 buf[i] = '\0';
 
 return(i);
}

/*************************************/
/** Task: Get a line from a socket, 
 * whether the line ends in a newline,
 * carriage return,
 * or a CRLF combination.
 * --> Terminates the string read with a null character 
 * If no newline indicator is found before the end of the buffer.
 * --> The string terminates with a null;
 * 
 * parameters:  the socket descriptor
 *              the buffer to save the data in
 *              the size of the buffer 
 * returns:     the number of bytes stored (exclueding null) */
/*************************************/
int getLine(int socket, char *buffer, int size)
{
    int i = 0;
    char c = '\0';
    int nBytes;

    while((i < size - 1) && (c != '\n'))
    {
        nBytes = recv(socket, &c, 1, 0);
        if(nBytes > 0)
        {
            if(c == '\r')
            {
                nBytes = recv(socket, &c, 1, MSG_PEEK);

                if((nBytes > 0) && (c == '\n'))
                {
                    recv(socket, &c, 1, 0);
                }
                else
                {
                    c = '\n';
                }
            }
            buffer[i] = c;
            i++;
        }
        else
        {
            c = '\n';
        }
    }
    buffer[i] = '\0';

    return(i); //returns number of bytes stored
}


/*************************************/
/** 
 * Display an error message with perror function. 
 * for system errors, based on value errno, which indicates system call errors.
 * and exit the program indicating an error
 * parameters: error-identifier 
 * */
/*************************************/
void errorDie(const char *identifier)
{
    perror(identifier);
    exit(1);
}

/*************************************/
/**
 * Task: Returns the informational HTTP headers
 * Parameters: the socket to print the headers on
 *              the name of the file.
*/
/*************************************/
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;
    strcpy(buf, "HTTP/1.1 200 OK\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "Content-Type: text/html; charset=UTF-8\r\n\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "<!DOCTYPE html>\r\n");
    send(client, buf, strlen(buf),0);
}

/***************************************/
/**
 * Task: Informs the client that a request has a problem
 * Parameters: client socket
*/
/***************************************/
void badRequest(int client)
{
    char buf[1024];
    strcpy(buf, "HTTP/1.1 400 BAD REQUEST\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "Content-Type: text/html; charset=UTF-8\r\n\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "<!DOCTYPE html>\r\n");
    send(client, buf, strlen(buf),0);
     sprintf(buf, "<h1>Your browser sent a bad request, </h1>");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<p>such as a POST without a Content-Length.</p>\r\n");
    send(client, buf, sizeof(buf), 0);
}

/***************************************/
/**
 * Task: Informs the client that the requested file could not be executed.
 * Parameters: client socket des..
*/
/***************************************/
void cannotExec(int client)
{
    char buf[1024];
    strcpy(buf, "HTTP/1.1 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "Content-Type: text/html; charset=UTF-8\r\n\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "<!DOCTYPE html>\r\n");
    send(client, buf, strlen(buf),0);
     sprintf(buf, "<h1>Internal Server Error. </h1>");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<p>Error: File execution failed, permission denied.</p>\r\n");
    send(client, buf, sizeof(buf), 0);
}

/***************************************/
/**
 * Task: Give a client a 404 NOTFOUND message.
 * Parameters: client socket des..
*/
/***************************************/
void notFound(int client)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/***************************************/
/**
 * Task: Give the message that the requested web method has not yet implemented.
 * Parameters: client socket des..
*/
/***************************************/
void unimplemented(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.1 501 Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</TITLE></HEAD>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}

/*************************************/
/**
 * Put the entire contents of a file out on a socket.
 * Parameters: client socket descriptor, FILE pointer 
*/
/*************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while(!feof(resource))
    {
        send(client, buf, strlen(buf),0);
        fgets(buf,sizeof(buf),resource);
    }
}

/**************************************/
/**
 * Task: Opens the file to read.
 * Parameters: client socket descriptor, pointer to the filename
*/
/**************************************/
void serveFile(int clientSock, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buffer[1024];

    buffer[0] = 'A'; buffer[1] = '\0';
    while((numchars>0) && strcmp("\n",buffer)) /* read & discard headers*/
        numchars = get_line(clientSock, buffer, sizeof(buffer));
    
    resource = fopen(filename, "r");
    if(resource == NULL)
        printf("NOT FOUND");
    else
    {
        headers(clientSock, filename);
        cat(clientSock, resource);
    }
    fclose(resource);
}

/***MAIN (Driver code)***/
int main()
{
    int serverSocket = -1;
    int clientSocket = -1;
    unsigned short port = 8080;

    struct sockaddr_in clientAddress;
    int clientAddressLength = sizeof(clientAddress);

    serverSocket = startup(&port);

    printf("web-server is running on port no: %d\n", port);

    // char webpage[] = 
    // "HTTP/1.1 200OK\r\n"
    // "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    // "<!DOCTYPE html>\r\n";

    // FILE *file = fopen("index.html","rb");
    // if(fseek(file,0,SEEK_END)==-1){
    //     perror("The file was not seeked");
    //     exit(1);
    // }

    // long fsize = ftell(file);
    // rewind(file);
    // char *msg = (char*) malloc(fsize);
    // fread(msg,fsize,1,file);
    // fclose(file);

    while(1){
        if((clientSocket = accept(serverSocket,(struct sockaddr *) &clientAddress, &clientAddressLength))<0)
        {
            errorDie("accept");
        }

        acceptRequests(clientSocket);

        // if(clientSocket>0){
        //     printf("The Client is connected... \n");
        // }

        // recv(clientSocket, buffer, bufsize, 0);
        // printf("%s\n", buffer);
        // write(clientSocket,webpage,sizeof(webpage));
        // write(clientSocket,msg,fsize);
        // close(clientSocket);
    }

    close(serverSocket);
    return 0;

}


