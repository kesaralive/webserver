#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_STRING "Server: kesaralive/0.1.0\r\n"

int startup(u_short *port);
void errorDie(const char *identifier);
void acceptRequests(int clientSock);



/*************************************/
/**This function begins the listening for web connections on a specific port.
 * If the port is 0, then it will allocate a port to the server dynamically and,
 * modify the original port variable to reflect the actual port.
 * parameters: pointer to the variable containing the port to connect on
 * returns: the socket */
/*************************************/
int startup(u_short *port)
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
/** Task: process the request appropriately.
 * parameters: the socket connected to the client */
/*************************************/
void acceptRequests(int clientSock)
{
    char buffer[1024];
    int numChars;
    char requestMethod[255];
    char url[255];
    char pathToFile[512];

    size_t i, j;
    struct stat stat;
    int cgiF = 0;
    //becomes true(1) if the server decides this is a CGI script/program.

    
}

/*************************************/
/** Display an error message with perror function. 
 * for system errors, based on value errno, which indicates system call errors.
 * and exit the program indicating an error
 * parameters: error-identifier */
/*************************************/
void errorDie(const char *identifier)
{
    perror(*identifier);
    exit(1);
}



// void headers(int client, const char *filename)
// {
//     char buf[1024];
//     (void)filename;
//     strcpy(buf, "HTTP/1.1 200 OK\r\n");
//     send(client, buf, strlen(buf),0);
//     strcpy(buf, SERVER_STRING);
//     send(client, buf, strlen(buf),0);
//     strcpy(buf, "Content-Type: text/html\r\n");
//     send(client, buf, strlen(buf),0);
// }

// void cat(int client, FILE *resource)
// {
//     char buf[1024];

//     fgets(buf, sizeof(buf), resource);
//     while(!feof(resource))
//     {
//         send(client, buf, strlen(buf),0);
//         fgets(buf,sizeof(buf),resource);
//     }
// }

// void serve_file(int client, const char *filename)
// {
//     FILE *resource = NULL;
//     int numchars = 1;
//     char buf[1024];

//     buf[0] = 'A'; buf[1] = '\0';
//     while((numchars>0) && strcmp("\n",buf)) /* read & discard headers*/
//         numchars = get_line(client, buf, sizeof(buf));
    
//     resource = fopen(filename, "rb");
//     if(resource == NULL)
//         printf("NOT FOUND");
//     else
//     {
//         headers(client, filename);
//         cat(client, resource);
//     }
//     fclose(resource);

// }

int main(int argc, char *argv[])
{
    int serverSocket = -1;
    int clientSocket = -1;
    u_short port = 0;

    struct sockaddr_in clientAddress;
    int clientAddressLength = sizeof(clientAddress);

    serverSocket = startup(&port);

    printf("web-server is running on port no: %d\n", port);

    //wait
    socklen_t addrlen;
    int bufsize = 1024;
    char *buffer = malloc(bufsize);


    char webpage[] = 
    "HTTP/1.1 200OK\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html>\r\n";

    FILE *file = fopen("index.html","rb");
    if(fseek(file,0,SEEK_END)==-1){
        perror("The file was not seeked");
        exit(1);
    }

    long fsize = ftell(file);
    rewind(file);
    char *msg = (char*) malloc(fsize);
    fread(msg,fsize,1,file);
    fclose(file);

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


