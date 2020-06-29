/* server application */
 
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAXLINELEN 20

int main(int argc, const char *argv[])
{
    int s, cs;
    struct sockaddr_in server, client;
     
    // Create socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket");
		return -1;
    }
    printf("Socket created\n");
     
    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(12345);
     
    // Bind
    if (bind(s,(struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed. Error");
        return -1;
    }
    printf("bind done\n");
     
    // Listen
    listen(s, 3);
     
    // Accept and incoming connection
    printf("Waiting for incoming connections...\n");
     
    // accept connection from an incoming client
    int c = sizeof(struct sockaddr_in);
    if ((cs = accept(s, (struct sockaddr *)&client, (socklen_t *)&c)) < 0) {
        perror("accept failed");
        return 1;
    }
    printf("Connection accepted\n");
    
    int start, end;
    char path[MAXLINELEN];
    recv(cs, &start, sizeof(int), 0);
    recv(cs, &end, sizeof(int), 0);
    recv(cs, path, MAXLINELEN * sizeof(char), 0);
    printf("All received!\nStart = %d\nEnd = %d\nPath = %s\n", ntohl(start), ntohl(end), path);
    FILE* fp;
    fp = fopen(path, "r");
    fseek(fp, ntohl(start), 0);
    int ptr = 0;
    int count[26];
    for (int i = 0; i < 26; ++i)
    {
    	count[i] = 0;
    }
    while(ptr != ntohl(end))
    {
    	int buf = fgetc(fp);
    	if(buf >= 'a' && buf <= 'z') count[buf - 'a']++;
    	if(buf >= 'A' && buf <= 'Z') count[buf - 'A']++;
    	ptr++;
    }

    int count_upload[26];
    for (int i = 0; i < 26; ++i)
    {
    	count_upload[i] = htonl(count[i]);
    }
    
    write(cs, count_upload, 26 * sizeof(int));
    return 0;
}

