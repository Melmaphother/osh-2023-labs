// server.c
#include "server_epoll.h"

int parse_request(Connection *connect, int epoll_fd, int client_socket) {
	/*
	  for (int i = 0; i < req_len; i++) { printf("%c", req[i]); }
	  printf("\n");
	*/
	char *req = (char *)malloc(MAX_RECV_LEN * sizeof(char));
	if (req == NULL) Error("req_buf malloc failed\n");
	req[0]				 = '\0';
	ssize_t		 req_len = 0;
	struct stat *file_type;
	/*
	  将 clnt_sock 作为一个文件描述符，读取最多 MAX_RECV_LEN 个字符
	  但一次读取并不保证已经将整个请求读取完整，需要不断尝试
	  所以需要循环读入，一旦到达MAX_RECV_LEN
	*/
	char   *buf		= (char *)malloc(MAX_RECV_LEN * sizeof(char));
	ssize_t buf_len = 0;
	while (1) {
		/*
		  这里只能read最多MAX_RECV_LEN - *req_len - 1位
		  因为最后一位要赋值'\0'保证接下来的strcat有效
		*/
		buf_len = read(client_socket, buf, MAX_RECV_LEN - 1);
		if (buf_len < 0) Error("failed to read client_socket\n");
		buf[buf_len] = '\0'; // 最后一位清零保证之后strcat有效
		strcat(req, buf);
		req_len = strlen(req); // 更新req的长度
		if (buf[buf_len - 4] == '\r' && buf[buf_len - 3] == '\n' &&
			buf[buf_len - 2] == '\r' && buf[buf_len - 1] == '\n') {
			break; // 读到"\r\n\r\n"停止读取
		}
	}
	if (req_len <= 0) { // 没有读取，直接退出终端
		close(client_socket);
		Error("failed to read client_socket\n");
	} else if (req_len < 5) { // 没有'GET /'，返回500
		connect->response.status = 500;
		// epoll_write(connect, epoll_fd);
		return -2;
	} else { // 需要判断是否是正常文件和文件类型，
		if (req[0] != 'G' && req[1] != 'E' && req[2] != 'T' && req[3] != ' ' &&
			req[4] != '/') {
			connect->response.status = 500;
			return -2; // 开头不是'GET /'
		}
		/*
		  此时req[3] = ' ', req[4] = '/', 从req[5]开始为具体内容
		*/
		ssize_t begin = 3;
		req[begin]	  = '.'; // 构造相对路径最开始的'.'
		ssize_t end	  = 5;
		int		floor = 0; // 文件夹层级
		while (end - begin <= MAX_PATH_LEN &&
			   req[end] != ' ') { // 找到第一个空格停止
			if (req[end] == '/') {
				if (req[end - 1] == '.' && req[end - 2] == '.' &&
					req[end - 3] == '/') {
					floor--; // 说明到了上级目录
				} else {
					floor++;
				}
				printf("%d\n", floor);
				if (floor < 0) {
					connect->response.status = 500;
					// epoll_write(connect, epoll_fd);
					return -2;
				} // 已经在当前路径的上级{
			}
			end++;
		}
		if (end - begin > MAX_PATH_LEN) {
			// epoll_write(connect, epoll_fd);
			return -2; // 超过最长路径
		}
		req[end] = '\0';						// 将空格改为'\0'
		int fd	 = open(req + begin, O_RDONLY); // 以只读方式打开
		if (fd < 0) {
			connect->response.status = 404;
			// epoll_write(connect, epoll_fd);
			return -1; // 打开文件失败，返回404
		}
		if (stat(req + begin, file_type) == -1) { Error("stat failed\n"); }
		if (S_ISDIR(file_type->st_mode)) {
			connect->response.status = 500;
			// epoll_write(connect, epoll_fd);
			return -2; // 是目录文件，返回500
		}
		if (!S_ISREG(file_type->st_mode)) {
			connect->response.status = 500;
			// epoll_write(connect, epoll_fd);
			return -2; // 不是普通文件，返回500
		}
		connect->response.fd	 = fd;
		connect->response.status = 200;
		connect->response.size	 = file_type->st_size;
		epoll_write(connect, epoll_fd);
		return fd;
	}
}

void handle_clnt(Connection *connect, int epoll_fd, int client_socket) {
	// 读取客户端发送来的数据，并解析

	// memset(req, '\0', MAX_RECV_LEN * sizeof(char));
	char *response = (char *)malloc(MAX_SEND_LEN * sizeof(char));
	if (response == NULL) Error("response malloc failed\n");

	/*
	  构造要返回的数据
	  其中write()函数通过
	  clnt_sock 向客户端发送信息，将 clnt_sock 作为文件描述符写内容
	*/
	if (connect->response.status == 500) {
		// 500
		sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n",
				HTTP_STATUS_500, (size_t)0);
		size_t response_len = strlen(response);
		if (write(client_socket, response, response_len) == -1) {
			close(client_socket);
			Error("write response failed, 500 Internal Server Error");
		}
	} else if (connect->response.status == 404) {
		// 404
		sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n",
				HTTP_STATUS_404, (size_t)0);
		size_t response_len = strlen(response);
		if (write(client_socket, response, response_len) == -1) {
			close(client_socket);
			Error("write response failed, 404 Not Found");
		}
	} else {
		// 200
		// file_type.st_size以字节为单位，保存了文件的大小
		sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n",
				HTTP_STATUS_200, connect->response.size);

		size_t response_len = strlen(response);
		if (write(client_socket, response, response_len) == -1) {
			close(client_socket);
			Error("write response failed");
		}
		// printf("%ld\n", connect->response.size);
		// int size = sendfile(client_socket, connect->response.fd, NULL,
		// 					connect->response.size);
		// if (size < 0) { Error("sendfile failed"); }
        /* 注意这里使用sendfile也要循环写入，不能假定其一次写成功 */
		ssize_t size = connect->response.size;
		ssize_t writen;
		while (size > 0) {
			writen = sendfile(client_socket, connect->response.fd, NULL, size);
			if (writen < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
				perror("sendfile");
				break;
			}
			size -= writen;
		}
		// 关闭文件描述符
		close(connect->response.fd);
	}
	// 关闭客户端socket
	close(client_socket);
	free(connect);
	free(response);
}

/* 其实这里用不到epoll_read函数，但是写出来可供参考 */
void epoll_read(Connection *connect, int epoll_fd) {
	struct epoll_event event;
	event.data.ptr = (void *)connect;
	event.events   = EPOLLIN | EPOLLET;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, connect->fd, &event) == -1) {
		Error("Read epoll failed");
	}
}

void epoll_write(Connection *connect, int epoll_fd) {
	struct epoll_event event;
	event.data.ptr = (void *)connect;
	event.events   = EPOLLOUT | EPOLLET;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, connect->fd, &event) == -1) {
		Error("Write epoll failed");
	}
}

int make_socket_non_blocking(int fd) {
	int flags, s;
	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		perror("fcntl failed 1");
		return -1;
	}
	flags |= O_NONBLOCK;
	s = fcntl(fd, F_SETFL, flags);
	if (s == -1) {
		perror("fcntl failed 2");
		return -1;
	}
	return 0;
}

int main() {
	signal(SIGPIPE, SIG_IGN);
	// 创建套接字，参数说明：
	//   AF_INET: 使用 IPv4
	//   SOCK_STREAM: 面向连接的数据传输方式
	//   IPPROTO_TCP: 使用 TCP 协议
	int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_socket == -1) { Error("Socket failed"); }

	// 设置 SO_REUSEADDR 选项，避免server无法快速重启
	int opt = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// 将套接字和指定的 IP、端口绑定
	//   用 0 填充 server_addr （它是一个 sockaddr_in 结构体）
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	//   设置 IPv4
	//   设置 IP 地址
	//   设置端口
	server_addr.sin_family		= AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(BIND_IP_ADDR);
	server_addr.sin_port		= htons(BIND_PORT);
	//   绑定
	bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));

	// 使得 server_socket 套接字进入监听状态，开始等待客户端发起请求
	listen(server_socket, MAX_CONN);
	//
	if (make_socket_non_blocking(server_socket) == -1) {
		Error("Set non blocking failed");
	};
	/*
	  参考https://github.com/zhenbianshu/tinyServer/blob/master/server.c
	*/
	/* 创建epoll */
	int epoll_fd = epoll_create(FD_SIZE);
	if (epoll_fd == -1) { Error("create epoll failed"); }
	/*
	  定义事件组
	  注册socketEPOLL事件为ET(垂直触发)模式
	*/
	struct epoll_event events[MAX_EVENTS];
	struct epoll_event event;
	event.events  = EPOLLIN | EPOLLET;
	event.data.fd = server_socket;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1) {
		Error("Register epoll failed");
	};
	int event_num = 0;
	while (1) {
		event_num = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (event_num == -1) { Error("Wait epoll failed"); }
		for (int i = 0; i < event_num; i++) {
			if ((events[i].events == EPOLLERR) ||
				(events[i].events == EPOLLHUP)) { // 连接出错
				fprintf(stderr, "epoll error\n");
				close(events[i].data.fd);
				continue;
			}
			if (events[i].data.fd == server_socket) { // 有新的连接
				while (1) {
					struct sockaddr_in client_addr;
					socklen_t		   client_addr_size = sizeof(client_addr);
					int				   client_socket =
						accept(server_socket, (struct sockaddr *)&client_addr,
							   &client_addr_size);
					if (client_socket == -1) {
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
							break;
						} else
							Error("Accept failed");
					}
					if (make_socket_non_blocking(client_socket) == -1) {
						Error("Set non blocking failed");
					}
					Connection *connect =
						(Connection *)malloc(sizeof(Connection));
					connect->fd	   = client_socket;
					event.data.ptr = (void *)connect;
					event.events   = EPOLLIN | EPOLLET;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket,
								  &event) == -1) {
						Error("Register epoll failed");
					};
				}
			} else if (events[i].events == EPOLLIN) { // 文件描述符可读
				Connection *connect		  = (Connection *)events[i].data.ptr;
				int			client_socket = connect->fd;
				parse_request(connect, epoll_fd, client_socket);
			} else if (events[i].events == EPOLLOUT) { // 文件描述符可写
				Connection *connect		  = (Connection *)events[i].data.ptr;
				int			client_socket = connect->fd;
				handle_clnt(connect, epoll_fd, client_socket);
			}
		}
	}
	// 实际上这里的代码不可到达，可以在 while 循环中收到 SIGINT 信号时主动
	// break 关闭套接字
	close(server_socket);
	return 0;
}
