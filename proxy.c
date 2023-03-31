/* proxy.c: Really dumb Minecraft proxy.
   Written 13 March 2023 Nekosha (github.com/Nekosha)
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>

#include "varnum.h"
#include "debug.h"
#define ARRAYLEN(a) (sizeof(a)/sizeof((a)[0]))

struct ForwardRule {
	char const * const domain;
	char const * const address;
	uint16_t port;
} const servers[];
#include "config.h"
struct sockaddr_in server_addrs[ARRAYLEN(servers)];

#define MAX_CLIENTS 20
struct ClientConnection {
	int clientfd;
	int serverfd;
} clients[MAX_CLIENTS];
int nclients;

int proxy;
struct pollfd fds[1+2*MAX_CLIENTS];

static int
resolve(struct sockaddr_in *server, struct ForwardRule const *rule) {
	int ret;
	struct addrinfo *res, hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	ret = getaddrinfo(rule->address, NULL, &hints, &res);
	if (ret < 0) {
		server->sin_family = AF_UNSPEC;
		return -1;
	}
	server->sin_family = res->ai_family;
	server->sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
	server->sin_port = htons(rule->port);
	freeaddrinfo(res);
	return 0;
}
void
cleanup() {
	close(proxy);
	for (int i = 0; i < nclients; ++i) {
		close(clients[i].clientfd);
		shutdown(clients[i].serverfd, SHUT_RDWR);
		close(clients[i].serverfd);
	}
}
void
setup() {
	int ret;
	/* Parse server IP strings into address structures */
	for (size_t i = 0; i < ARRAYLEN(servers); ++i)
		resolve(&server_addrs[i], &servers[i]);
	/* Prepares the listening socket for the proxy */
	proxy = socket(AF_INET, SOCK_STREAM, 0);
	int one = 1;
	setsockopt(proxy, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	setsockopt(proxy, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
	atexit(cleanup);
	struct sockaddr_in sa = {0};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_port = htons(proxy_port);
	ret = bind(proxy, (struct sockaddr *)&sa, sizeof(sa));
	if (ret < 0)
		die("setup(): bind:");
	ret = listen(proxy, MAX_CLIENTS);
	if (ret < 0)
		die("setup(): listen:");
	/* Start monitoring for incoming connections */
	fds[0].fd = proxy;
	fds[0].events = POLLIN;
	/* Don't die when trying to send to a closed connection */
	signal(SIGPIPE, SIG_IGN);
}
int
accept_connection(int fd) {
	struct sockaddr_in sa;
	unsigned int sa_len = sizeof(sa);
	int ret;
	for (ret = accept(fd, (struct sockaddr *)&sa, &sa_len);
	     ret < 0;
	    ) {
		if (errno == EINTR || errno == ECONNABORTED) continue;
		return -1;
	}
	listen(proxy, MAX_CLIENTS - nclients);
	char source_ip[17] = {0};
	uint16_t source_port = ntohs(sa.sin_port);
	if (inet_ntop(AF_INET, &sa.sin_addr, source_ip, sizeof(source_ip)))
		_log("New connection from %s:%hu", source_ip, source_port);
	return ret;
}
int
relay_data(int sender, int receiver) {
	static unsigned char buffer[8192];
	int received;
	int ret;
	unsigned char *sendp = buffer;
	received = recv(sender, buffer, sizeof(buffer), 0);
	if (received <= 0)
		return -1;
	while (received > 0) {
		ret = send(receiver, sendp, received, 0);
		if (ret <= 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		received -= ret;
		sendp += ret;
	}
	return 0;
}
int
setup_proxy_connection(int conn) {
	/* Handshake packet format
	 * +----------------------------+
	 * | TotalLength       (VarInt) |
	 * | ID                (VarInt) |
	 * | Proto             (VarInt) |
	 * | SrvAddr  - Length (VarInt) | <----/
	 * |          - Bytes  (0-255)  | <----/
	 * | ...                        |
	 * +----------------------------+
	 */
	static unsigned char buffer[4*4+255+1];
	int ret;
	int addrlen = 0;
	unsigned char *addr = buffer;

	memset(buffer, 0, 16);
	ret = recv(conn, buffer, sizeof(buffer), MSG_PEEK);
	if (ret < 0) return -1;
	/* Extract the domain name from the packet */
	addr += from_varint(addr, NULL);
	addr += from_varint(addr, NULL);
	addr += from_varint(addr, NULL);
	addr += from_varint(addr, &addrlen);
	if (addrlen > 255) addrlen = 255;
	addr[addrlen] = '\0';

	for (size_t i = 0; i < ARRAYLEN(servers); ++i) {
		if (!strcmp((char*)addr, servers[i].domain)) {
			if (server_addrs[i].sin_family == AF_UNSPEC) {
				/* Try to resolve the address one more time.
				 * This will choke connecting clients.
				   TODO: Not choke.
				 */
				if (resolve(&server_addrs[i], &servers[i]) < 0) {
					_log("Cannot find address of server %s", addr);
					return -1;
				}
			}
			conn = socket(AF_INET, SOCK_STREAM, 0);
			ret = connect(conn, (struct sockaddr *)&server_addrs[i], sizeof(server_addrs[i]));
			if (ret < 0) {
				_log("Server %s is unreachable", addr);
				return -1;
			}
			_log("Will relay connection to server %s", addr);
			return conn;
		}
	}
	_log("No such server: %s", addr);
	return -1;
}
const char *argv0;
static void
usage() {
	die("Usage: %s [-h] [-v] [-p port]", argv0);
}
int
main(int argc, const char *argv[]) {
	int ret;
	for (--argc, argv0 = *argv++; *argv && (*argv)[0] == '-'; --argc, ++argv) {
		switch((*argv)[1]) {
			case 'v': vflag = 1; break;
			case 'p':
				if (--argc < 1) usage();
				proxy_port = atoi(*++argv);
				break;
			default: usage();
		}
	}
	if (argc > 0) usage();
	setup();
	while (1) {
		ret = poll(fds, 1+2*nclients, -1);
		if (ret < 0) {
			if (errno == EINTR) continue;
			die("run(): poll:");
		}
		if (fds[0].revents) ret -= 1;
		if (nclients < MAX_CLIENTS) {
			if (fds[0].revents & POLLIN) {
				int clientfd = accept_connection(proxy);
				int serverfd = setup_proxy_connection(clientfd);
				if (serverfd < 0) {
					shutdown(clientfd, SHUT_RDWR);
					close(clientfd);
				} else {
					clients[nclients].clientfd = fds[1+2*nclients].fd = clientfd;
					clients[nclients].serverfd = fds[1+2*nclients+1].fd = serverfd;
					fds[1+2*nclients].events = fds[1+2*nclients+1].events = POLLIN;
					nclients += 1;
				}
			}
		}
		for (int i = 0, n = 0; ret > 0 && i < nclients; ++i) {
			/* Client -> server */
			if (fds[1+2*i].revents) ret -= 1;
			if (fds[1+2*i].revents & POLLIN) {
				ret = relay_data(clients[i].clientfd, clients[i].serverfd);
				if (ret < 0) {
					shutdown(clients[i].clientfd, SHUT_RDWR);
					shutdown(clients[i].serverfd, SHUT_RDWR);
					close(clients[i].clientfd);
					close(clients[i].serverfd);
				}
			}
			/* Server -> client */
			if (fds[1+2*i+1].revents) ret -= 1;
			if (fds[1+2*i+1].revents & POLLIN) {
				ret = relay_data(clients[i].serverfd, clients[i].clientfd);
				if (ret < 0) {
					shutdown(clients[i].clientfd, SHUT_RDWR);
					shutdown(clients[i].serverfd, SHUT_RDWR);
					close(clients[i].clientfd);
					close(clients[i].serverfd);
				}
			}
			/* Skip closed connections */
			if (fds[1+2*i].revents & POLLNVAL) {
				/* Truncate the list if there are no more */
				if (i == nclients - 1) nclients = n;
				continue;
			}
			fds[1+2*n].fd   = fds[1+2*i].fd;
			fds[1+2*n+1].fd = fds[1+2*i+1].fd;
			clients[n]      = clients[i];
			fds[1+2*n+1].fd = fds[1+2*i+1].fd;
			n += 1;
			/* TODO: Rotate fds to balance requests */
		}
	}
	return 0;
}
