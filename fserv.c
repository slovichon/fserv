/* $Id$ */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip_ipsp.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>

#define K_SEND 1
#define K_RECV 2
#define K_INV  3

#define PORT 2134
#define SERVNAME "fserv"

#define PORT_MAX 65535

typedef u_int16_t port_t;

void usage(void) __attribute__((__noreturn__));
void server(const union sockaddr_union *, const char *);
void client(const union sockaddr_union *, const char *);
void fillsun(union sockaddr_union *, const char *, port_t);

int
main(int argc, char *argv[])
{
	union sockaddr_union sun;
	char *file, *cmd, *addr;
	port_t port = PORT;
	long l;
	int c;

	while ((c = getopt(argc, argv, "p:")) != -1) {
		switch (c) {
		case 'p':
			if ((l = strtoul(optarg, (char **)NULL,
			     10)) < 0 || l > PORT_MAX)
				err(1, "%s: bad port", optarg);
			port = (port_t)l;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 3)
		usage();
	cmd  = argv[0];
	file = argv[1];
	addr = argv[2];

	fillsun(&sun, addr, port);

	switch (lookup(cmd)) {
	case K_SEND:
		client(&sun, file);
		break;
	case K_RECV:
		server(&sun, file);
		break;
	case K_INV:
		err(1, "%s: unknown command", argv[1]);
		/* NOTREACHED */
	}
	exit(0);
}

void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-p port] send|recv file addr\n", __progname);
	exit(1);
}

int
lookup(const char *cmd)
{
	if (cmd == NULL)
		return (K_INV);
	else if (strcmp(cmd, "send") == 0)
		return (K_SEND);
	else if (strcmp(cmd, "recv") == 0)
		return (K_RECV);
	return (K_INV);
}

void
server(const union sockaddr_union *sun, const char *fil)
{
	union sockaddr_union clisun;
	int s, clifd, fd, n;
	char buf[BUFSIZ];
	socklen_t len;

	if ((fd = open(fil, O_EXCL | O_CREAT | O_WRONLY)) == -1)
		err(1, "%s", fil);
	if ((s = socket(sun->sa.sa_family, SOCK_DGRAM, 0)) == -1)
		err(1, "socket");
	if (bind(s, &sun->sa, sun->sa.sa_len) == -1)
		err(1, "bind");
	if (listen(s, 1) == -1)
		err(1, "listen");
	len = sun->sa.sa_len;
	if ((clifd = accept(s, &clisun.sa, &len)) == -1)
		err(1, "accept");
	while ((n = read(clifd, buf, sizeof(buf))) != -1 && n != 0)
		if (write(fd, buf, n) != n)
			err(1, "write");
	if (n == -1)
		err(1, "read");
	(void)close(clifd);
	(void)close(fd);
}

void
client(const union sockaddr_union *sun, const char *fil)
{
	int s, servfd, fd;
	char buf[BUFSIZ];
	ssize_t n;

	if ((fd = open(fil, O_RDONLY)) == -1)
		err(1, "%s", fil);
	if ((s = socket(sun->sa.sa_family, SOCK_DGRAM, 0)) == -1)
		err(1, "socket");
	if ((servfd = connect(s, &sun->sa, sun->sa.sa_len)) == -1)
		err(1, "connect");
	while ((n = read(fd, buf, sizeof(buf))) != -1 && n != 0)
		if (write(servfd, buf, n) != n)
			err(1, "write");
	if (n == -1)
		err(1, "read");
	(void)close(servfd);
	(void)close(fd);
}

void
fillsun(union sockaddr_union *sun, const char *addr, port_t port)
{
	struct addrinfo hints, *res;
	int error, s;

	memset(sun, 0, sizeof(*sun));
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	if ((error = getaddrinfo(addr, SERVNAME, &hints, &res)) != 0)
		errx(1, "%s: %s", addr, gai_strerror(error));
	switch (res->ai_family) {
	case AF_INET:
		sun->sin.sin_len = sizeof(struct sockaddr_in);
		sun->sin.sin_family = res->ai_family;
		sun->sin.sin_port = port;
		sun->sin.sin_addr.s_addr = ((struct sockaddr_in *)
		    &res->ai_addr)->sin_addr.s_addr;
		break;
	case AF_INET6:
		sun->sin6.sin6_len = sizeof(struct sockaddr_in6);
		sun->sin6.sin6_family = res->ai_family;
		sun->sin6.sin6_port = port;
		memcpy(&sun->sin6.sin6_addr,
		    &((struct sockaddr_in6 *)&res->ai_addr)->sin6_addr,
		    sizeof(sun->sin6.sin6_addr));
		break;
	default:
		errx(1, "%d: unknown address family", res->ai_family);
		/* NOTREACHED */
	}
	freeaddrinfo(res);
}
