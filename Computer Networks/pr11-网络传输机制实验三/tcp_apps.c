#include "tcp_sock.h"

#include "log.h"

#include <unistd.h>

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

	FILE* fp;
	fp = fopen("server-output.dat", "w");

	char rbuf[4097];
	int rlen = 0;
	while (1) {
		rlen = tcp_sock_read(csk, rbuf, 1000);
		if (rlen < 0) {
			log(DEBUG, "tcp_sock_read return negative value, finish transmission.");
			break;
		}
		else if (rlen > 0) {
			rbuf[rlen] = '\0';
			fwrite(rbuf, 1, rlen, fp);
			fflush(fp);			
		}
	}

	log(DEBUG, "close this connection after 10 sec.(server)");
	sleep(10);
	tcp_sock_close(csk);
	
	return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data 
void *tcp_client(void *arg)
{
	int done = 0;

	struct sock_addr *skaddr = arg;

	struct tcp_sock *tsk = alloc_tcp_sock();

	if (tcp_sock_connect(tsk, skaddr) < 0) {
		log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
				NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}

	char rbuf[4097];

	FILE* fp;
	fp = fopen("./1MB.dat", "r");
	fseek(fp, 0, SEEK_END);  
	int file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	while(!feof(fp)) {
		int size = fread(rbuf, 1, 4096, fp);
		done += size;
		printf("sending at %d of %d\n", done, file_size);
		tcp_sock_write(tsk, rbuf, size);
	}

	log(DEBUG, "close this connection after 10 sec.(client)");
	sleep(10);
	tcp_sock_close(tsk);

	return NULL;
}


