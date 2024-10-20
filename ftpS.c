#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <stdio.h>
#define SERVER_CONTROL_PORT 50000
#define CLIENT_DATA_PORT 55000
#define MAX 81



bool isDigit(char num)
{
    return (num >= '0' && num <= '9');
}

void parseFileName(char *buffer, char *filename, int i)
{
    strcpy(filename,buffer+i); // first i blocks contain command and space
}

// function to verify port of the client
int verifyPort(char *buffer)
{
    int len = sizeof(buffer);
    int port = atoi(buffer);
    /*for (int i = 0; i < len && buffer[i] != '\0'; i++)
    {
        if (!isDigit(buffer[i]))
        {
            return 503; // invalid client response
        }
        port = port * 10 + (buffer[i] - '0');
    }*/

    if (port >= 1024 && port <= 65535)
    {
        printf("Port Y: %d\n",port);
        return 200;
    }
    else
        return 550; // port invalid
}

int getServerDataSocket()
{
    int sockFD;

    sockFD = socket(AF_INET,SOCK_STREAM,0);
    if(sockFD<0)
    {
        printf("failed to create server data socket\n");
        exit(0);
    }

    return sockFD;
}

// send file to the client
void sendFileFunc(FILE *fp)
{
    char buffer[MAX];
    int bytes_read;
    struct sockaddr_in client_addr;

    // create server data socket
    int serverD_FD = getServerDataSocket();
    bzero(&client_addr,sizeof(client_addr));

    // assign port and ip
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(CLIENT_DATA_PORT);

    //connect with the client data port
    if(connect(serverD_FD,(struct sockaddr *)&client_addr,sizeof(client_addr))<0)
    {
        printf("error connecting with server...\n");
        exit(0);
    }

    // send blocks of data 
    while ((bytes_read = fread(buffer + 3, sizeof(char), MAX, fp)) > 0) {
        // Set flag character based on remaining data
        buffer[0] = (feof(fp)) ? 'L' : '*';  // 'L' for last block, '*' for others

        // Convert data length (excluding header) to network byte order
        // | L/* | length | length | data | data...
        short data_length = htons(bytes_read);
        memcpy(buffer + 1, &data_length, sizeof(short));

        // Send the entire block (header + data)
        int bytes_sent = write(serverD_FD, buffer, MAX);
        if (bytes_sent < 0) {
            perror("send failed");
            break;
        }
    }

}

// receive file from the client
void getFile(char *filename)
{
    char buffer[MAX];
    int bytes_read;
    struct sockaddr_in client_addr;

    // create server data socket
    int serverD_FD = getServerDataSocket();
    bzero(&client_addr,sizeof(client_addr));

    // assign port and ip
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(CLIENT_DATA_PORT);

    //connect with the client data port
    if(connect(serverD_FD,(struct sockaddr *)&client_addr,sizeof(client_addr))<0)
    {
        printf("error connecting with server...\n");
        exit(0);
    }

    FILE *fp = fopen("server.txt", "wb");  // Open file for writing in binary mode
    while (1) {
        int bytes_recv = recv(serverD_FD, buffer, MAX, 0);
        //printf("%d\n",bytes_recv);
        if (bytes_recv <= 0) {
            break;
        }

        char flag = buffer[0];
        short data_length = ntohs(*(short*)(buffer + 1));

        // write data into file
        fwrite(buffer + 3, sizeof(char), data_length, fp);  // Write received data to file
        if (flag == 'L') {
            // Last block received, break out of loop
            break;
        }
    }

    fclose(fp);
    close(serverD_FD);
}

// control socket functionality
void handleConnection(int clientC_FD)
{
    char buffer[MAX], server_response[MAX];
    bzero(buffer, MAX);
    bzero(server_response, MAX);
    // receive port Y from client
    recv(clientC_FD, buffer, sizeof(buffer), 0);

    int code = verifyPort(buffer);
    sprintf(server_response, "%d", code);

    // send code
    write(clientC_FD, server_response, sizeof(server_response));
    //printf("Connected with client\n");

    while (1)
    {
        bzero(buffer, MAX);
        recv(clientC_FD, buffer, sizeof(buffer), 0);

        // options on server
        if (buffer[0] == 'c' && buffer[1] == 'd')
        {
            char cmd[10];
            strcpy(cmd,buffer+3);
            if(chdir(cmd)<0)
            {
                strcpy(server_response,"501");
                write(clientC_FD,server_response,sizeof(server_response));
                break;
            }
            else
            {
                char dir[100];
                printf("%s\n",getcwd(dir,sizeof(dir)));
                strcpy(server_response,"200");
                write(clientC_FD,server_response,sizeof(server_response));
            }
            
        }
        else if (buffer[0] == 'g' && buffer[1] == 'e' && buffer[2] == 't')
        {
            char filename[20];
            //parse filename
            parseFileName(buffer,filename,4);
            printf("%s\n",filename);
            FILE* fp =fopen(filename,"rb");
            if(fp == NULL)
            {
                strcpy(server_response,"550");
                write(clientC_FD,server_response,sizeof(server_response));
                break;
            }
            else
            {
                strcpy(server_response,"201");
                write(clientC_FD,server_response,sizeof(server_response));
            }
            sleep(1); // wait for client to create data socket

            sendFileFunc(fp);
            fclose(fp);

        }
        else if (buffer[0] == 'p' && buffer[1] == 'u' && buffer[2] == 't')
        {

            char filename[20];
            //parse filename
            parseFileName(buffer,filename,4);

            sleep(1);

            getFile(filename);

        }
        else if (buffer[0] == 'q' && buffer[1] == 'u' && buffer[2] == 'i' && buffer[3] == 't')
        {
            strcpy(server_response,"421");

            write(clientC_FD, server_response, sizeof(server_response));
            break;
        }
        else
        {
            bzero(server_response, MAX);
            strcat(server_response, "502"); // invalid request

            write(clientC_FD, server_response, sizeof(server_response));
        }
    }
    return;
}

int main()
{
    int sockC_FD, clientC_FD, len;
    struct sockaddr_in server_addr, client_addr;

    // socket creation
    sockC_FD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockC_FD < 0)
    {
        printf("socket creation failed..\n");
        exit(0);
    }

    // allowing reuse of address and port
    if (setsockopt(sockC_FD, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    bzero(&server_addr, sizeof(server_addr));

    // assigning ip and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_CONTROL_PORT);

    // binding socket to ip
    if ((bind(sockC_FD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0))
    {
        printf("Bind failed");
        exit(0);
    }

    // listen for connections
    if (listen(sockC_FD, 10))
    {
        printf("Listening error..");
        exit(0);
    }
    printf("server listening... \n");

    len = sizeof(client_addr);
    while (clientC_FD = accept(sockC_FD, (struct sockaddr *)&client_addr, &len))
    {
        
        handleConnection(clientC_FD);
    }

    if (clientC_FD < 0)
    {
        printf("Error accepting client connection\n");
        exit(0);
    }

    close(sockC_FD);
}