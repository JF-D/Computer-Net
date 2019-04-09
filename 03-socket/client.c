//client
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define MAX_SERVER 100

struct message_t {
    int filename_len;
    char path[200];
    unsigned int st, ed;
} message[200];

int main(int argc, char *argv[])
{
    //command line input
    if(argc == 1)
    {
        printf("No input text.\n");
        return 0;
    }
    //IP conf file
    FILE *fd1 = fopen("workers.conf", "r");
    if(fd1 == NULL)
    {
        printf("No worker conf file!\n");
        return -1;
    }
    //work text
    FILE *fd2 = fopen(argv[1], "r");
    if(fd2 == NULL)
    {
        printf("%s file does not exist!\n", argv[1]);
        return -1;
    }
    //length of the text
    char c;
    unsigned int len = 0;
    while(fscanf(fd2, "%c", &c) != EOF)
        len++;
    fclose(fd2);

    int i, sock[MAX_SERVER], server_num = 3; //server numbers
    struct sockaddr_in server;

    if(argc == 3)
    {
        server_num = atoi(argv[2]);
        if(server_num == 0)
        {
            printf("Server number %s is wrong!\n", argv[2]);
            return 0;
        }
    }
    printf("argc:%d server number: %d\n", argc, server_num);

    for(i = 0; i < server_num; i++)
    {
        sock[i] = socket(AF_INET, SOCK_STREAM, 0);
        if(sock[i] == -1)
        {
            printf("create socket failed.\n");
            return -1;
        }
    }
    
    for(i = 0; i < server_num; i++)
    {
        char ip[20];
        fscanf(fd1, "%s", ip);
        server.sin_addr.s_addr = inet_addr(ip);
        server.sin_family = AF_INET;
        server.sin_port = htons(12345);

        // connect to server
        if(connect(sock[i], (struct sockaddr *)&server, sizeof(server)) < 0) 
        {
            perror("connect failed");
            return 1;
        }
    }
    fclose(fd1);

    int flen = strlen(argv[1]);
    while(flen > 0 && argv[1][flen-1] != '/')
        flen--;
    flen = strlen(argv[1]) - flen;
    unsigned int part = len / server_num, bg = 0;
    for(i = 0; i < server_num; i++)
    {
        message[i].filename_len = htonl(flen);
        strcpy(message[i].path, argv[1]);
        message[i].st = htonl(bg);
        if(i == server_num - 1)
            bg = len;
        else
            bg += part;
        message[i].ed = htonl(bg-1);

        if(send(sock[i], &(message[i]), sizeof(message[i]), 0) < 0)
        {
            printf("send failed\n");
            return 1;
        }
    }

    unsigned int num[26], tnum[26];
    memset(tnum, 0, sizeof(tnum));
    for(i = 0; i < server_num; i++)
    {
        int len = recv(sock[i], num, sizeof(num), 0);
        if(len < 0)
        {
            printf("recv failed\n");
            break;
        }
        int j;
        for(j = 0; j < 26; j++)
            tnum[j] += ntohl(num[j]);
    }

    for(i = 0; i < server_num; i++)
        close(sock[i]);
    for(i = 0; i < 26; i++)
    {
        printf("%c: %u\n", 'a'+i, tnum[i]);
    }
    return 0;
}