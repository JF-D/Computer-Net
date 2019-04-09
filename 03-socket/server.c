#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

struct message_t {
    int filename_len;
    char path[200];
    unsigned int st, ed;
} message;

int main(int argc, char *argv[])
{
    int s, cs;
    struct sockaddr_in server, client;
    
    if((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("create socket failed!\n");
        return -1;
    }
    printf("socket created.\n");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(12345);

    if(bind(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("bind failed.\n");
        return -1;
    }
    printf("bind done!\n");

    listen(s, 3);
    printf("waiting for incomming connections...\n");

    int c = sizeof(struct sockaddr_in);
    if((cs = accept(s, (struct sockaddr*)&client, (socklen_t *)&c)) < 0)
    {
        perror("accept failed.\n");
        return -1;
    }
    printf("connection accepted.\n");
    while(1)
    {
        int msg_len = recv(cs, &message, sizeof(message), 0);
        message.filename_len = ntohl(message.filename_len);
        message.st = ntohl(message.st);
        message.ed = ntohl(message.ed);
        FILE *fd = fopen(message.path, "r");
        unsigned int num[26];
        memset(num, 0, sizeof(num));
        int i; char ch;
        fseek(fd, message.st, SEEK_SET);
        for(i = message.st; i <= message.ed; i++)
        {
            fscanf(fd, "%c", &ch);
            if(ch >= 'A' && ch <= 'Z')
                num[ch-'A']++;
            else if(ch >= 'a' && ch <= 'z')
                num[ch-'a']++;
        }
        for(i = 0; i < 26; i++)
            num[i] = htonl(num[i]);
        write(cs, num, sizeof(num));
    }
    return 0;
}