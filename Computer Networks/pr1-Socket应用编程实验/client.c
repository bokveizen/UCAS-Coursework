/* client application */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAXSOCKNUM 10
#define MAXLINELEN 20

int main(int argc, char *argv[])
{
    int sock[MAXSOCKNUM];
    struct sockaddr_in server[MAXSOCKNUM];
    
    //Count socket & Get ip
    FILE* fp = fopen("./workers.conf", "r");
    char ip[MAXSOCKNUM][MAXLINELEN];
    int socket_count = 0;
    for (int i = 0; i < MAXSOCKNUM; ++i)
    {
        if(fgets(ip[i], MAXLINELEN, fp) == NULL)
        {
            socket_count = i;
            break;
        }
        printf("ip[%d] = %s", i, ip[i]);
    }
    fclose(fp);
    printf("Socket counting finished\nSocket number is %d\n", socket_count);

    //Create socket
    for (int i = 0; i < socket_count; ++i)
    {
        sock[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (sock[i] == -1)
        {
            printf("Could not create socket%d\n", i);
        }
        printf("Socket%d created\n", i);
        server[i].sin_addr.s_addr = inet_addr(ip[i]);
        server[i].sin_family = AF_INET;
        server[i].sin_port = htons(12345);
    }

    //Connect to remote server
    for (int i = 0; i < socket_count; ++i)
    {
        if (connect(sock[i], (struct sockaddr *)&server[i], sizeof(struct sockaddr_in)) < 0)
        {
            perror("connect failed. Error\n");
            return 1;
        }
    }
     
    printf("All connected\n");
    
    //Task set
    char file_path[MAXLINELEN] = "./";
    strcat(file_path,argv[1]);
    FILE* fpp = fopen(file_path, "r");
    fseek(fpp,0,SEEK_END);
    int file_len = ftell(fpp);
    printf("The length of the task is %d\n", file_len);
    fclose(fpp);
    int checkpoint[socket_count + 1];
    for (int i = 0; i <= socket_count; ++i)
    {
        checkpoint[i] = htonl(i * file_len / socket_count);
    }

    //Send data
    for (int i = 0; i < socket_count; ++i)
    {
        if(send(sock[i], &checkpoint[i], sizeof(int), 0) < 0)
        {
            printf("Socket%d start point send failed\n", i);
            return -1;
        }
        if(send(sock[i], &checkpoint[i+1], sizeof(int), 0) < 0)
        {
            printf("Socket%d end point send failed\n", i);
            return -1;
        }
        if(send(sock[i], file_path, MAXLINELEN * sizeof(char), 0) < 0)
        {
            printf("Socket%d path send failed\n", i);
            return -1;
        }
        printf("Socket%d all send done!\n", i);
    }

    printf("All sockets send done!\n");

    //Receive data & Result computing
    int buf[26];
    int res[26];
    for (int i = 0; i < 26; ++i)
    {
        res[i] = 0;
    }
    for (int i = 0; i < socket_count; ++i)
    {
        recv(sock[i], buf, 26 * sizeof(int), 0);
        for (int i = 0; i < 26; ++i)
        {
            res[i] += ntohl(buf[i]);
        }
        printf("Received from socket%d\n", i);
    }
    printf("All received\n");
    for (int i = 0; i < 26; ++i)
    {
        printf("%c %d\n", i+'a', res[i]);
    }

    for (int i = 0; i < socket_count; ++i)
    {
        close(sock[i]);
    }     
    return 0;
}

