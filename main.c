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
#include <strings.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define ISspace(x) isspace((int)(x))

typedef struct {
 char *ext;
 char *mediatype;
} extn;

size_t total_bytes_sent = 0;

//Possible media types
extn extensions[] ={
 {"gif", "image/gif"},
 {"txt", "text/plain"},
 {"jpg", "image/jpg"},
 {"jpeg","image/jpeg"},
 {"png", "image/png"},
 {"ico", "image/ico"},
 {"zip", "image/zip"},
 {"gz",  "image/gz"},
 {"tar", "image/tar"},
 {"htm", "text/html"},
 {"css", "text/css"},
 {"html","text/html"},
 {"php", "text/html"},
 {"pdf","application/pdf"},
 {"zip","application/octet-stream"},
 {"rar","application/octet-stream"},
 {0,0}};

int startup(unsigned short *port);
void errorDie(const char *identifier);
void acceptRequests(int clientSock);
int getLine(int socket, char *buffer, int size);
void cat(int client, FILE *resource);
void serveFile(int client, const char *filename);
void executeCGI(int client, const char *path, const char *method, const char *query_string);
int get_file_size(int fd);

/*HEADERS*/
void headers(int client, const char *filename);
void pdfheaders(int client, const char *filename);
void cssheaders(int client, const char *filename);
void rarheaders(int client, const char *filename);
void zipheaders(int client, const char *filename);
void jsheaders(int client, const char *filename);
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

    if(listen(sockfd, 100) < 0)
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
        return;
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
        if((st.st_mode & S_IFMT) == S_IFDIR)
        {
            strcat(filePath, "/index.html");
        }

        int len = strlen(filePath);
        const char *last_four = &filePath[len-4];
        printf("\n%s\n",last_four);

        char* s = strchr(filePath, '.');
        printf("\n%s\n",s);

        if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
        {
            cgiF = 1;
        }

        serveFile(clientSock, filePath);

        // if(!cgiF)
        // {
        //     serveFile(clientSock, filePath);
        // }
        // else if(strcmp(s,".css") == 0)
        // {
        //     serveFile(clientSock, filePath);
        // }
        // else if(cgiF && strcmp(s,".php") == 0)
        // {
        //     printf("\nPHP SCRIPT\n");
        //     executeCGI(clientSock, filePath, requestMethod, queryString);
        // }else
        // {
        //     serveFile(clientSock, filePath);
        // }
    }
    close(clientSock);
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
 * Task: Returns the informational HTTP headers for CSS
 * Parameters: the socket to print the headers on
 *              the name of the file.
*/
/*************************************/
void cssheaders(int client, const char *filename)
{
    char buf[1024];
    (void)filename;
    strcpy(buf, "HTTP/1.1 200 OK\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "Content-Type: text/css; charset=UTF-8\r\n\r\n");
    send(client, buf, strlen(buf),0);
}



/*************************************/
/**
 * Task: Returns the informational HTTP headers for PDF
 * Parameters: the socket to print the headers on
 *              the name of the file.
*/
/*************************************/
void pdfheaders(int client, const char *filename)
{
    char buf[1024];
    (void)filename;
    strcpy(buf, "HTTP/1.1 200 OK\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "Content-Type: application/pdf; charset=UTF-8\r\n\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "<!DOCTYPE html>\r\n");
    send(client, buf, strlen(buf),0);
}

/*************************************/
/**
 * Task: Returns the informational HTTP headers for RAR
 * Parameters: the socket to print the headers on
 *              the name of the file.
*/
/*************************************/
void rarheaders(int client, const char *filename)
{
    char buf[1024];
    (void)filename;
    strcpy(buf, "HTTP/1.1 200 OK\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "Content-Type: application/octet-stream; charset=UTF-8\r\n\r\n");
    send(client, buf, strlen(buf),0);
}

/*************************************/
/**
 * Task: Returns the informational HTTP headers for ZIP
 * Parameters: the socket to print the headers on
 *              the name of the file.
*/
/*************************************/
void zipheaders(int client, const char *filename)
{
    char buf[1024];
    (void)filename;
    strcpy(buf, "HTTP/1.1 200 OK\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "Content-Type: application/octet-stream; charset=UTF-8\r\n\r\n");
    send(client, buf, strlen(buf),0);
}

/*************************************/
/**
 * Task: Returns the informational HTTP headers for Javascript
 * Parameters: the socket to print the headers on
 *              the name of the file.
*/
/*************************************/
void jsheaders(int client, const char *filename)
{
    char buf[1024];
    (void)filename;
    strcpy(buf, "HTTP/1.1 200 OK\r\n");
    send(client, buf, strlen(buf),0);
    strcpy(buf, "Content-Type: application/javascript; charset=UTF-8\r\n\r\n");
    send(client, buf, strlen(buf),0);
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

/*************************************/
/**
 * Task: Returns the informational HTTP headers
 * Parameters: the socket to print the headers on
 *              the name of the php file.
*/
/*************************************/
void headersPHP(int client, const char *filename)
{
    char buf[1024];
    (void)filename;
    strcpy(buf, "HTTP/1.1 200 OK\n Server: Web Server in C\n Connection: close\n");
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
    sprintf(buf, "<BODY><h1>Not Found</h1>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<p>The requested url is not found in this server</p>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<hr>\r\n");
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

/***/
//helper function to get filesize
/***/
int get_file_size(int fd) {
 struct stat stat_struct;
 if (fstat(fd, &stat_struct) == -1)
  return (1);
 return (int) stat_struct.st_size;
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
    char buffer[2048];


    memset(buffer,0,2048);
    read(clientSock, buffer, 2047);
    printf("%s\n",buffer);

    int fd1,length;
    fd1 = open(filename, O_RDONLY);


    if(fd1 == -1)
    {
        notFound(clientSock);
    }
    else
    {
        int len = strlen(filename);
        const char *last_four = &filename[len-4];
        printf("\n%s\n",last_four);

        char* s = strchr(filename, '.');
        printf("\n%s\n",s+1);

        for(int i=0; extensions[i].ext != NULL; i++)
        {
            if(strcmp(s + 1, extensions[i].ext) == 0)
            {
                if(strcmp(extensions[i].ext, "php") == 0)
                {
                    executeCGI(clientSock, filename,"GET", NULL);
                }else if(strcmp(extensions[i].ext, "html") == 0)
                {
                    headers(clientSock, filename);
                }else if(strcmp(extensions[i].ext, "pdf") == 0)
                {
                    pdfheaders(clientSock, filename);
                }else if(strcmp(extensions[i].ext, "css") == 0)
                {
                    cssheaders(clientSock, filename);
                }else if(strcmp(extensions[i].ext, "rar") == 0)
                {
                    rarheaders(clientSock, filename);
                }else if(strcmp(extensions[i].ext, "zip") == 0)
                {
                    zipheaders(clientSock, filename);
                }else if(strcmp(extensions[i].ext, "js") == 0)
                {
                    jsheaders(clientSock, filename);
                }else{
                    notFound(clientSock);
                }
            }
        }

        if((length = get_file_size(fd1)) == -1)
        {
            printf("Error in getting size! \n");
        }
        ssize_t bytes_sent;
        // size_t total_bytes_sent = 0;

        char buffer[length];
        if ((bytes_sent = sendfile(clientSock, fd1, NULL, length)) <= 0) 
        {
         perror("sendfile");
        }
        printf("File %s , Sent %i\n",filename, bytes_sent);
        close(fd1);
    }



    // char buffer[1024];

    // buffer[0] = 'A'; buffer[1] = '\0';
    // while((numchars>0) && strcmp("\n",buffer)) /* read & discard headers*/
    //     numchars = getLine(clientSock, buffer, sizeof(buffer));
    
    // resource = fopen(filename, "r");
    // if(resource == NULL)
    //     notFound(clientSock);
    // else
    // {
    //     headers(clientSock, filename);
    //     cat(clientSock, resource);
    // }
    // fclose(resource);
}

/**********************************************************************/
/* Task: Execute a CGI script.  Will need to set environment variables as 
 *       appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void executeCGI(int client, const char *path, const char *method, const char *query_string)
{
 char buf[1024];
 int cgi_output[2];
 int cgi_input[2];
 pid_t pid;
 int status;
 int i;
 char c;
 int numchars = 1;
 int content_length = -1;

 printf("\n Client: %i, Path: %s, Method: %s, Query String: %s\n", client, path, method, query_string);

 buf[0] = 'A'; buf[1] = '\0';
//  if (strcasecmp(method, "GET") == 0)
//   while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
//    numchars = getLine(client, buf, sizeof(buf));
//  else    /* POST */
//  {
//   numchars = getLine(client, buf, sizeof(buf));
//   while ((numchars > 0) && strcmp("\n", buf))
//   {
//    buf[15] = '\0';
//    if (strcasecmp(buf, "Content-Length:") == 0)
//     content_length = atoi(&(buf[16]));
//    numchars = getLine(client, buf, sizeof(buf));
//   }
//   if (content_length == -1) {
//    badRequest(client);
//    return;
//   }
//  }
 
 printf("\nphp headers\n");
//  sprintf(buf, "HTTP/1.0 200 OK\r\n");
//  send(client, buf, strlen(buf), 0);
 headersPHP(client, path);
 printf("\nheaders sent\n");

 if (pipe(cgi_output) < 0) {
  cannotExec(client);
  return;
 }
 if (pipe(cgi_input) < 0) {
  cannotExec(client);
  return;
 }

 if ( (pid = fork()) < 0 ) {
  cannotExec(client);
  return;
 }

 printf("\nsuccess\n");

 if (pid == 0)  /* child: CGI script */
 {
  char meth_env[255];
  char query_env[255];
  char length_env[255];

  printf("\nchild process\n");

  dup2(cgi_output[1], 1);
  dup2(cgi_input[0], 0);
  close(cgi_output[0]);
  close(cgi_input[1]);

  dup2(client, STDOUT_FILENO);
  char script[500];
  strcpy(script, "SCRIPT_FILENAME=");
  strcat(script, path);
  putenv("GATEWAY_INTERFACE=CGI/1.1");
  putenv(script);

  sprintf(meth_env, "REQUEST_METHOD=%s", method);
  putenv(meth_env);
  if (strcasecmp(method, "GET") == 0) {
   sprintf(query_env, "QUERY_STRING=%s", query_string);
   putenv(query_env);
  }
  else {   /* POST */
   sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
   printf("\nContent Length: %i\n",content_length);
   putenv(length_env);
  }
//   execl(path, path, NULL);
  putenv("SERVER_PROTOCOL=HTTP/1.1");
  putenv("REMOTE_HOST=127.0.0.1");
  putenv("REDIRECT_STATUS=true");
  execl("/usr/bin/php-cgi", "php-cgi", NULL);
  exit(0);
 } else {    /* parent */

  printf("\nparent process\n");

  close(cgi_output[1]);
  close(cgi_input[0]);
  if (strcasecmp(method, "POST") == 0)
   for (i = 0; i < content_length; i++) {
    recv(client, &c, 1, 0);
    write(cgi_input[1], &c, 1);
   }
  while (read(cgi_output[0], &c, 1) > 0)
   send(client, &c, 1, 0);

  close(cgi_output[0]);
  close(cgi_input[1]);
  waitpid(pid, &status, 0);
 }
}

/***MAIN (Driver code)***/
int main()
{
    int serverSocket = -1;
    int clientSocket = -1;
    unsigned short port = 0;

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


