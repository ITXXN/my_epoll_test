#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <netinet/in.h>

#define IP_ADDR "127.0.0.1"
#define PORT 9999
#define MAX_SIZE 1024
#define EVENT_SIZE 100

static int server_socket_init(void);
static int do_epoll(int server_fd);
static void add_event(int epollfd, int fd, int state);
static void handle_event(int epollfd, struct epoll_event *events, int num, int fd, char *buf);
static void do_write(int epollfd, int fd, char *buf);
static void do_accept(int epollfd, int server_fd);
static void do_read(int epollfd, int fd, char *buf);
static void delete_event(int epollfd, int fd, int state);
static void change_event(int epollfd, int fd, int state);

int main()
{
	int fd;

	fd = server_socket_init();
	do_epoll(fd);
	return 0;
}

static int server_socket_init(void)
{
	int fd;
	int ret;
	struct sockaddr_in addr;

	ret = socket(AF_INET, SOCK_STREAM, 0);
	if (ret == -1) {
		printf("create server socket failed.\n");
		exit(0);
	}
	fd = ret;

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, IP_ADDR, &addr.sin_addr);
	addr.sin_port = htons(PORT);

	ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret == -1) {
		printf("server bind failed.\n");
		exit(0);
	}

	ret = listen(fd, 5);
	if (ret == -1) {
		printf("listen failed.\n");
		exit(0);
	}

	return fd;
}

static int do_epoll(int server_fd)
{
	int epollfd;
	struct epoll_event events[EVENT_SIZE];
	char buffer[MAX_SIZE];
	int ret;

	memset(buffer, 0, MAX_SIZE);
	epollfd = epoll_create(MAX_SIZE);

	add_event(epollfd, server_fd, EPOLLIN);

	while (1) {
		ret = epoll_wait(epollfd, events, EVENT_SIZE, 5000);
		handle_event(epollfd, events, ret, server_fd, buffer);
	}
	close(epollfd);
}


static void add_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;

	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

static void handle_event(int epollfd, struct epoll_event *events, int num, int server_fd, char *buf)
{
	int i = 0;
	int fd;
	if (0 == num) {
		printf("-----timeout-----\n");
		return;
	}

	for ( ; i < num; ++i) {
		fd = events[i].data.fd;
		if (fd == server_fd) {
			do_accept(epollfd, fd);
		} else if (events[i].events & EPOLLIN) {
			do_read(epollfd, fd, buf);
		} else if (events[i].events & EPOLLOUT) {
			do_write(epollfd, fd, buf);
		}
	}
}

static void do_write(int epollfd, int fd, char *buf)
{
	int size;
	size = write(fd, buf, strlen(buf));
	if (-1 == size) {
		printf("write error\n");
		close(fd);
		delete_event(epollfd, fd, EPOLLOUT);
		return;
	}
	change_event(epollfd, fd, EPOLLIN);
	memset(buf, 0, MAX_SIZE);
}

static void do_accept(int epollfd, int server_fd)
{
	int client_fd;
	struct sockaddr_in addr;
	char ip[32];
	socklen_t addr_len;
	client_fd = accept(server_fd, (struct sockaddr *) &addr,
			&addr_len);

	if (-1 == client_fd) {
		printf("accept failed\n");
		return;
	}

	inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
	printf("accept a new client: %s:%d\n", ip, addr.sin_port);
	add_event(epollfd, client_fd, EPOLLIN);
}

static void do_read(int epollfd, int fd, char *buf)
{
	int size;

	size = read(fd, buf, MAX_SIZE);
	if (-1 == size) {
		printf("read error\n");
		close(fd);
		delete_event(epollfd, fd, EPOLLIN);
	} else if (0 == size) {
		printf("client close\n");
		close(fd);
		delete_event(epollfd, fd, EPOLLIN);
	} else {
		printf("recive message : %s\n", buf);
		change_event(epollfd, fd, EPOLLOUT);
	}
}

static void delete_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;

	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
}

static void change_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;

	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}
