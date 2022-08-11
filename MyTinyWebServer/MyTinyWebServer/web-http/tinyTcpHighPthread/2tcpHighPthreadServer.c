#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct ciifo {
	int fd;
	struct sockaddr_in addr;
}CINFO;

void* fun(void* arg) {
	CINFO* info = (CINFO*)arg;
	char ip[16] = "";
	printf("new cilent ip:%s, port:%d\n", inet_ntop(AF_INET, &(info->addr.sin_addr.s_addr), ip, 16), ntohs(info->addr.sin_port));
	while (1) {
		char buffer[1024] = "";
		int n = read(info->fd, buffer, sizeof(buffer));
		if (n > 0) {
			printf("%s", buffer);
			write(info->fd, buffer, n);
		} else if (n == 0) {
			printf("client closed\n");
			close(info->fd);
			free(info);
			break;
		} else {
			perror("read");
			close(info->fd);
			free(info);
			break;
		}
	}
	return NULL;
}

int main() {
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	//1.创建套接字
	int ret = -1;
	int lfd = -1;
	lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == lfd) {
		perror("socket");
		return 1;
	}	

	//2.绑定
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = ntohs(8080);
	ret = inet_pton(AF_INET, "222.24.10.212", &addr.sin_addr.s_addr);
	if (-1 == ret) {
		perror("inet_pton");
		return 1;
	}
	ret = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));
	if (-1 == ret) {
		perror("bind");
		return 1;
	}	

	//3.监听
	ret = listen(lfd, 128);
	if (-1 == ret) {
		perror("bind");
		return 1;
	}		

	//4.提取
	CINFO* info;
	while (1) {
		struct sockaddr_in caddr;
		int len = sizeof(caddr);
		int cfd = accept(lfd, (struct sockaddr*)&caddr, &len);
		if (-1 == cfd) {
			perror("accept");
			return 1;
		}
		pthread_t tid = -1;
		info = malloc(sizeof(CINFO));
		info->fd = cfd;
		info->addr = caddr;
		pthread_create(&tid, &attr, fun, info);
	}
	
	//7.关闭
	close(lfd);	

	return 0;
}
