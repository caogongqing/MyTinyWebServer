#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
	//1.创建套接字
	int fd = -1;
	int ret = -1;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket");
		return 1;
	}

	//2.连接
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);
	ret = inet_pton(AF_INET, "222.24.10.44", &addr.sin_addr.s_addr);
	if (-1 == ret) {
		perror("inet_pton");
		return 1;
	}
	ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
	if (-1 == ret) {
		perror("connect");
		return 1;	
	}
	
	//3.读写
	while (1) {
		char buf[1500] = "";
		int n = read(STDIN_FILENO, buf, sizeof(buf));
		write(fd, buf, n);
		n = read(fd, buf, sizeof(buf));
		if (n <= 0) {
			break;
		} else {
			printf("%s", buf);
		}
	}

	//4.关闭	
	close(fd);
	
	return 0;
} 
