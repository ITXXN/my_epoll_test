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

static int connect_server(void);
static void do_epoll(int client_fd);
static void handle_event(int epollfd, struct epoll_event *events,int num, int client_fd, char *buf);
static void do_read(int epollfd, int client_fd, int fd, char *buf);
static void do_write(int epollfd, int client_fd, int fd, char *buf);
static void add_event(int epollfd, int fd, int state);
static void delete_event(int epollfd, int fd, int state);
static void change_event(int epollfd, int fd, int state);

int main()
{
	int fd;

	fd = connect_server();
	do_epoll(fd);
	close(fd);
}

static int connect_server(void)
{
	int fd;
	struct sockaddr_in addr;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, IP_ADDR, &addr.sin_addr);
	addr.sin_port = htons(PORT);

	connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	return fd;
}

static void do_epoll(int client_fd)
{
	int epollfd;
	struct epoll_event events[EVENT_SIZE];
	char buffer[MAX_SIZE];
	int number;

	epollfd = epoll_create(MAX_SIZE);
	add_event(epollfd, STDIN_FILENO, EPOLLIN);
	while (1) {
		number = epoll_wait(epollfd, events, EVENT_SIZE, 5000);
		handle_event(epollfd, events, number, client_fd, buffer);
	}
	close(epollfd);
}

static void handle_event(int epollfd, struct epoll_event *events,int num, int client_fd, char *buf)
{
	int i = 0;
	int fd;
	if (0 == num) {
		printf("-----timeout-----\n");
		return;
	}
	
	for ( ; i < num; ++i) {
		fd = events[i].data.fd;
		if (events[i].events & EPOLLIN) {
			do_read(epollfd, client_fd, fd, buf);
		} else if (events[i].events & EPOLLOUT) {
			do_write(epollfd, client_fd, fd, buf);
		}
	}
}

static void do_read(int epollfd, int client_fd, int fd, char *buf)
{
	int size;
	size = read(fd, buf, MAX_SIZE);
	if (-1 == size) {
		printf("read error\n");
		close(fd);
		exit(0);
	} else if (0 == size) {
		printf("server close\n");
		close(fd);
		exit(0);
	} else {
		if (fd == STDIN_FILENO) {
			add_event(epollfd, client_fd, EPOLLOUT);
		} else {
			delete_event(epollfd, client_fd, EPOLLIN);
			add_event(epollfd, STDOUT_FILENO, EPOLLOUT);
		}
	}
}

static void do_write(int epollfd, int client_fd, int fd, char *buf)
{
	int size;
	size = write(fd, buf, strlen(buf));
	if (-1 == size) {
		printf("write error\n");
		close(fd);
	} else {
		if (fd == STDOUT_FILENO) {
			delete_event(epollfd, fd, EPOLLOUT);
		} else {
			change_event(epollfd, client_fd, EPOLLIN);
		}
	}
	memset(buf, 0, MAX_SIZE);
}

static void add_event(int epollfd, int fd, int state)
{
	struct epoll_event ev;
	ev.events = state;
	ev.data.fd = fd;

	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
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
