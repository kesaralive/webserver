#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int create_socket, new_socket;
    socklen_t addrlen;
    int bufsize = 1024;
    char *buffer = malloc(bufsize);
    struct sockaddr_in address;

    if((create_socket = socket(AF_INET, SOCK_STREAM, 0)) > 0){
        printf("The socket was created\n");
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(14000);

    if(bind(create_socket,(struct sockaddr *) &address, sizeof(address)) == 0){
        printf("Binding Socket\n");
    }

    while(1){
        if(listen(create_socket,10)<0){
            perror("server: listen");
            exit(1);
        }
        if((new_socket = accept(create_socket,(struct sockaddr *) &address, &addrlen))<0){
            perror("server: accept");
            exit(1);
        }
        if(new_socket>0){
            printf("The Client is connected... \n");
        }
        recv(new_socket, buffer, bufsize, 0);
        printf("%s\n", buffer);
        write(new_socket, "hello world\n", 12);
        close(new_socket);
    }
    write(new_socket, "HTTP/1.1 200OK\n", 16);
    write(new_socket, "Content-length: 46\n", 19);
    write(new_socket, "Content-Type: text/html\n\n", 25);
    write(new_socket,"<html><body><h3>Hello, World!</h3></body></html>", 46);
    close(create_socket);
    return 0;
}
