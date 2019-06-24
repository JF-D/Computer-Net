#include "tcp_sock.h"

#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int myread(char a[], FILE *fd)
{
    int len = 0;
    char c;
    while(len < 10000 && (c = fgetc(fd)) != EOF)
        a[len++] = c;
    a[len] = '\0';
    return len;
}

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

	FILE *fd = fopen("server-output.dat", "wb");
    char rbuf[10050];
	int rlen = 0;
	while (1) {
		rlen = tcp_sock_read(csk, rbuf, 10000);
		if (rlen == 0) {
			log(DEBUG, "tcp_sock_read return 0, finish transmission.");
			break;
		} 
		else if (rlen > 0) {
			rbuf[rlen] = '\0';
			fprintf(fd, "%s", rbuf);
		}
		else {
			log(DEBUG, "tcp_sock_read return negative value, something goes wrong.");
			exit(1);
		}
	}

	log(DEBUG, "close this connection.");

	tcp_sock_close(csk);
    fclose(fd);
	
	return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data 
void *tcp_client(void *arg)
{
	struct sock_addr *skaddr = arg;

	struct tcp_sock *tsk = alloc_tcp_sock();

	sleep(1);

	if (tcp_sock_connect(tsk, skaddr) < 0) {
		log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
				NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}
	printf("connect!\n");

    char wbuf[10050];
    FILE *fd = fopen("client-input.dat", "rb");
    int wlen;
    while((wlen = myread(wbuf, fd)) != 0)
    {
        if (tcp_sock_write(tsk, wbuf, wlen) < 0)
			break;
    }

	tcp_sock_close(tsk);
    fclose(fd);

	return NULL;
}
