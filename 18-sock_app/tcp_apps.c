#include "tcp_sock.h"

#include "log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#define MAX_SERVER 100

struct message_t {
    int filename_len;
    char path[200];
    unsigned int st, ed;
};
// tcp server application, listens to port (specified by arg) and serves only one
// connection request
void *tcp_server(void *arg)
{
	u16 port = *(u16 *)arg;
	struct tcp_sock *tsk = alloc_tcp_sock();

	struct sock_addr addr;
	addr.ip = htonl(0);
	addr.port = port;
	if (tcp_sock_bind(tsk, &addr) < 0) {
		log(ERROR, "tcp_sock bind to port %hu failed", ntohs(port));
		exit(1);
	}

	if (tcp_sock_listen(tsk, 3) < 0) {
		log(ERROR, "tcp_sock listen failed");
		exit(1);
	}

	log(DEBUG, "listen to port %hu.", ntohs(port));

	struct tcp_sock *csk = tcp_sock_accept(tsk);

	log(DEBUG, "accept a connection.");

	struct message_t *message;
	char s[100];
	tcp_sock_read(csk, s, sizeof(struct message_t));
	message = (struct message_t*)s;
	message->filename_len = ntohl(message->filename_len);
    message->st = ntohl(message->st);
    message->ed = ntohl(message->ed);
    FILE *fd = fopen(message->path, "r");
    unsigned int num[26];
    memset(num, 0, sizeof(num));
    int i; char ch;
    fseek(fd, message->st, SEEK_SET);
    for(i = message->st; i <= message->ed; i++)
    {
        fscanf(fd, "%c", &ch);
        if(ch >= 'A' && ch <= 'Z')
            num[ch-'A']++;
		else if(ch >= 'a' && ch <= 'z')
            num[ch-'a']++;
    }
	fclose(fd);
    for(i = 0; i < 26; i++)
        num[i] = htonl(num[i]);
    tcp_sock_write(csk, (char *)num, sizeof(num));

	log(DEBUG, "close this connection.");
	sleep(2);

	tcp_sock_close(csk);
	
	return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data 
void *tcp_client(void *arg)
{
	struct sock_addr *skaddr;
	skaddr = (struct sock_addr *)arg;

	int server_num = 2;
	struct tcp_sock *tsk[2];
	for(int i = 0; i < server_num; i++)
	{
		tsk[i] = alloc_tcp_sock();
		if (tcp_sock_connect(tsk[i], &(skaddr[i])) < 0) {
			log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
					NET_IP_FMT_STR(skaddr[i].ip), ntohs(skaddr[i].port));
			exit(1);
		}

		sleep(1);
	}
	printf("connect!\n");

	//work text
    FILE *fd2 = fopen("war_and_peace.txt", "r");
    if(fd2 == NULL)
    {
        printf("%s file does not exist!\n", "war_and_peace.txt");
        return NULL;
    }
    //length of the text
    char c;
    unsigned int len = 0;
    while(fscanf(fd2, "%c", &c) != EOF)
        len++;
    fclose(fd2);

	struct message_t message[200];
	char path[50] = "war_and_peace.txt";
	int flen = strlen(path);
    while(flen > 0 && path[flen-1] != '/')
        flen--;
    flen = strlen(path) - flen;
    unsigned int part = len / server_num, bg = 0;
    for(int i = 0; i < server_num; i++)
    {
        message[i].filename_len = htonl(flen);
        strcpy(message[i].path, path);
        message[i].st = htonl(bg);
        if(i == server_num - 1)
            bg = len;
        else
            bg += part;
        message[i].ed = htonl(bg-1);

		if(tcp_sock_write(tsk[i], (char *)&(message[i]), sizeof(message[i])) < 0)
		{
			printf("send failed\n");
            return NULL;
		}
    }

    unsigned int num[26], tnum[26];
    memset(tnum, 0, sizeof(tnum));
    for(int i = 0; i < server_num; i++)
    {
		int len = tcp_sock_read(tsk[i], (char *)num, sizeof(num));
        if(len < 0)
        {
            printf("recv failed\n");
            break;
        }
        int j;
        for(j = 0; j < 26; j++)
            tnum[j] += ntohl(num[j]);
    }

    for(int i = 0; i < server_num; i++)
        tcp_sock_close(tsk[i]);
    for(int i = 0; i < 26; i++)
    {
        printf("%c: %u\n", 'a'+i, tnum[i]);
    }

	return NULL;
}
