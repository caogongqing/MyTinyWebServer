#include <stdio.h>
#include "wrap.h"
#include "pub.h"
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>

#define PORT 8888

void send_header(int cfd, int code, char* info, char* filetype, int length) {
	//发送状态行
	char buf[1024] = "";
	int len = sprintf(buf, "HTTP/1.1 %d %s\r\n", code, info);
	send(cfd, buf, len, 0);


	//发送消息头
	len = sprintf(buf, "Content-Type:%s\r\n", filetype);
	send(cfd, buf, len, 0);
	if (length > 0) {
		//发送消息头
		len = sprintf(buf, "Content-Length:%d\r\n", length);
		send(cfd, buf, len, 0);
	}
	//空行
	send(cfd, "\r\n", 2, 0);
}

void send_file(int cfd, int epfd, char* path, struct epoll_event* ev, int flag) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return;
	}
	char buf[1024] = "";
	while (1) {
		int n = read(fd, buf, sizeof(buf));
		if (n < 0) {
			perror("read");
			break;
		} else if (n == 0) {
			break;
		} else {
			int len = 0;
			len = send(cfd, buf, n, 0);
			//写缓冲区可能满写不进去，此时监听EPOLLOUT，将没有发送的数据保存等写事件触发
			printf("len = %d\n", len);
		}
	}
	close(fd);
	//关闭cfd, 下树
	if (flag == 1) {
		close(cfd);
		epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, ev);
	}
}

void read_client_request(int epfd, struct epoll_event* ev) {
	//读取请求 先读取一行 再把其他行读取 扔掉
	char buf[1024] = "";
	char tmp[1024] = "";
	int n = Readline(ev->data.fd, buf, sizeof(buf));
	//将第一行读取保存至buf
	if (n <= 0) {
		printf("close or err\n");
		epoll_ctl(epfd, EPOLL_CTL_DEL, ev->data.fd, ev);
		close(ev->data.fd);
		return;
	}
	printf("%s\n", buf);
	int ret = 0;
	//剩下的行扔掉
	while ((ret = Readline(ev->data.fd, tmp, sizeof(tmp))) > 0);
	//printf("read ok\n"); 

	//解析请求 判断是否为get请求 get请求菜处理
	char method[256] = "";
	char content[256] = "";
	char protocol[256] = "";
	sscanf(buf, "%[^ ] %[^ ] %[^ \r\n]", method, content, protocol);
	printf("[%s] [%s] [%s]\n", method, content, protocol);
	//忽略大小写	
	if (strcasecmp(method, "get") == 0) {
		char *strfile = content + 1;
		strdecode(strfile, strfile);	
 		

		//判断请求的文件在不在
		if (*strfile == 0) {//如果没有请求文件，默认请求当前目录
			strfile = "./";
		} 
		struct stat s;
		if (stat(strfile, &s) < 0) {//文件不存在
			printf("file not found\n");
			//下发送 报头
			send_header(ev->data.fd, 404, "NOT FOUND", get_mime_type("*.html"), 0);
			//发送文件error.html
			send_file(ev->data.fd, epfd, "error.html", ev, 1);
			
		} else {
			//请求一个普通文件
			if (S_ISREG(s.st_mode)) {
				printf("file\n");	
				//先发送 报头（状态行 消息头 空行）
				send_header(ev->data.fd, 200, "OK", get_mime_type(strfile), s.st_size);
				//发送文件
				send_file(ev->data.fd, epfd, strfile, ev, 1);
			
			} else if (S_ISDIR(s.st_mode)) { //请求一个目录文件
				printf("dir\n");
				//发送一个列表
				send_header(ev->data.fd, 200, "OK", get_mime_type("*.html"), 0);
				//发送header.html
				send_file(ev->data.fd, epfd, "dir_header.html", ev, 0);
				
				//组html中的列表
				struct dirent **mylist = NULL;
				char buf[1024] = "";
				int len = 0;
				int n = scandir(strfile, &mylist, NULL, alphasort);
				for (int i = 0; i < n; i++) {
					//printf("%s\n", mylist[i]->d_name);
					if (mylist[i]->d_type == DT_DIR) {
						len = sprintf(buf, "<li> <a href=%s/>%s</a></li>",mylist[i]->d_name, mylist[i]->d_name);
					} else {
						len = sprintf(buf, "<li> <a href=%s>%s</a></li>",mylist[i]->d_name, mylist[i]->d_name);
					} 
					send(ev->data.fd, buf, len, 0);
					free(mylist[i]);
				}
				free(mylist);	 
				send_file(ev->data.fd, epfd, "dir_tail.html", ev, 1);
			
			}
		}
	}

}

//void sighandler(int signo) {
//	printf("signo == [%d]\n", signo);
//}

int main() {
	pid_t pid = -1;
	int ret = -1;
	//创建子进程父进程退出
	pid = fork();
	if (-1 == pid) {
		perror("fork");
		return 1;
	}

	if (pid > 0) {
		return 1;
	}

	//创建新的会话 完全脱离控制终端
	pid = setsid();
	if (-1 == pid) {
		perror("setsid");
		return 1;
	}

	//改变当前工作目录为根目录,并设置权限掩码
	ret = chdir("/");
	if (-1 == ret) {
		perror("chdir");
		return 1;
	}

	umask(0);

	//关闭文件描述符
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	//类似于管道文件，因此当socket中读端关闭可能导致另外一端直接退出,从而发出SIGPIPE信号
	//因此直接忽略该信号
	//signal(SIGPIPE, SIG_IGN);
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);

	//切换工作目录
	//获取当前目录的工作路径
	char pwd_path[256] = "";
	char* path = getenv("PWD");
	strcpy(pwd_path, path);
	strcat(pwd_path, "/web-http");
	chdir(pwd_path);

	//创建绑定
	int lfd = tcp4bind(PORT, NULL);

	//监听
	Listen(lfd, 128);	

	//创建树
	int epfd = epoll_create(1);

	//上树
	struct epoll_event ev,evs[1024];
	ev.data.fd = lfd;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);

	//循环监听
	while (1) {
		int n = epoll_wait(epfd, evs, 1024, -1);
		if (n < 0) {
			perror("epoll_wait");
			break;
		} else {
			for (int i = 0; i < n; i++) {
				if (evs[i].data.fd == lfd && evs[i].events & EPOLLIN) {
					struct sockaddr_in cliaddr;
					char ip[16] = "";
					socklen_t len = sizeof(cliaddr);
					int cfd = Accept(lfd, (struct sockaddr*)&cliaddr, &len);
					printf("new client ip:%s port:%d\n", inet_ntop(AF_INET, &cliaddr.sin_addr.s_addr, ip, 16), ntohs(cliaddr.sin_port));
					//设置cfd为非阻塞
					int flag = fcntl(cfd, F_GETFL);
					flag |= O_NONBLOCK;
					fcntl(cfd, F_SETFL, flag);			
					//上树
					ev.data.fd = cfd;
					ev.events = EPOLLIN;
					epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);	
				} else if (evs[i].events & EPOLLIN){//cfd变化
					read_client_request(epfd, &evs[i]);
					
					
				}
			}
		}
	}

	//关闭	

	return 0;
}


