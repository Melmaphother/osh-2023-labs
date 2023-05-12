// server.c
#include "server.h"
#include "thread.h"

#define BIND_IP_ADDR "127.0.0.1"
#define BIND_PORT 8000
#define MAX_RECV_LEN 1048576
#define MAX_SEND_LEN 1048576
#define MAX_PATH_LEN 1024
#define MAX_HOST_LEN 1024
#define MAX_CONN 20
#define MAX_THREAD 50
#define MAX_QUEUE_SIZE 1024

#define HTTP_STATUS_200 "200 OK"
#define HTTP_STATUS_404 "404 Not Found"
#define HTTP_STATUS_500 "500 Internal Server Error"

int parse_request(int client_socket, ssize_t *req_len, char *req,
				  struct stat *file_type) {
	/*
	  将 clnt_sock 作为一个文件描述符，读取最多 MAX_RECV_LEN 个字符
	  但一次读取并不保证已经将整个请求读取完整，需要不断尝试
	  所以需要循环读入，一旦到达MAX_RECV_LEN(1048576 = 1024 * 1024 = 1MB)
	*/
	char   *buf		= (char *)malloc(MAX_RECV_LEN * sizeof(char));
	ssize_t buf_len = 0;
	while (1) {
		/*
		  这里只能read最多MAX_RECV_LEN - *req_len - 1位
		  因为最后一位要赋值'\0'保证接下来的strcat有效
		*/
		buf_len = read(client_socket, buf, MAX_RECV_LEN - *req_len - 1);
		if (buf_len < 0) Error("failed to read client_socket\n");
		buf[buf_len] = '\0'; // 最后一位清零保证之后strcat有效
		strcat(req, buf);
		*req_len = strlen(req); // 更新req的长度
		if (buf[buf_len - 4] == '\r' && buf[buf_len - 3] == '\n' &&
			buf[buf_len - 2] == '\r' && buf[buf_len - 1] == '\n') {
			break; // 读到"\r\n\r\n"停止读取
		}
	}

	if (*req_len <= 0) { // 没有读取，直接退出终端
		Error("failed to read client_socket\n");
	} else if (*req_len < 5) { // 没有'GET /'，返回500
		return -2;
	} else { // 需要判断是否是正常文件和文件类型，
		if (req[0] != 'G' && req[1] != 'E' && req[2] != 'T' && req[3] != ' ' &&
			req[4] != '/') {
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
				if (floor < 0) { return -2; } // 已经在当前路径的上级
			}
			end++;
		}
		if (end - begin > MAX_PATH_LEN) return -2; // 超过最长路径
		req[end] = '\0';						   // 将空格改为'\0'
		int fd	 = open(req + begin, O_RDONLY);	   // 以只读方式打开
		if (fd < 0) return -1; // 打开文件失败，返回404
		if (stat(req + begin, file_type) == -1) { Error("stat failed\n"); }
		if (S_ISDIR(file_type->st_mode)) return -2; // 是目录文件，返回500
		if (!S_ISREG(file_type->st_mode)) return -2; // 不是普通文件，返回500
		return fd;
	}
}

void handle_clnt(int client_socket) {
	// 读取客户端发送来的数据，并解析
	char *req = (char *)malloc(MAX_RECV_LEN * sizeof(char));
	if (req == NULL) Error("req_buf malloc failed\n");
	req[0] = '\0';
	// memset(req, '\0', MAX_RECV_LEN * sizeof(char));
	char *response = (char *)malloc(MAX_SEND_LEN * sizeof(char));
	if (response == NULL) Error("response malloc failed\n");
	ssize_t		req_len = 0;
	struct stat file_type;
	int			fd = parse_request(client_socket, &req_len, req, &file_type);

	/*
	  构造要返回的数据
	  其中write()函数通过
	  clnt_sock 向客户端发送信息，将 clnt_sock 作为文件描述符写内容
	*/
	if (fd == -2) {
		// 500
		sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n",
				HTTP_STATUS_500, (size_t)0);
		size_t response_len = strlen(response);
		if (write(client_socket, response, response_len) == -1) {
			Error("write response failed, 500 Internal Server Error");
		}
	} else if (fd == -1) {
		// 404
		sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n",
				HTTP_STATUS_404, (size_t)0);
		size_t response_len = strlen(response);
		if (write(client_socket, response, response_len) == -1) {
			Error("write response failed, 404 Not Found");
		}
	} else {
		// 200
		// file_type.st_size以字节为单位，保存了文件的大小
		sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n",
				HTTP_STATUS_200, (size_t)(file_type.st_size));

		size_t response_len = strlen(response);
		if (write(client_socket, response, response_len) == -1) {
			Error("write response failed, 200 OK");
		}

		// 循环读取文件内容并返回文件内容
		while (response_len = read(fd, response, MAX_SEND_LEN)) {
			if (response_len == -1) Error("read file failed");
			if (write(client_socket, response, response_len) == -1) {
				Error("write file failed");
			}
		}
	}

	// 关闭客户端socket和文件
	close(fd);
	close(client_socket);

	// 释放内存
	free(req);
	free(response);
}

int main() {

	// 创建套接字，参数说明：
	//   AF_INET: 使用 IPv4
	//   SOCK_STREAM: 面向连接的数据传输方式
	//   IPPROTO_TCP: 使用 TCP 协议
	int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// 设置 SO_REUSEADDR 选项，避免server无法重启
	int opt = 1;
	setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// 将套接字和指定的 IP、端口绑定
	//   用 0 填充 serv_addr （它是一个 sockaddr_in 结构体）
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	//   设置 IPv4
	//   设置 IP 地址
	//   设置端口
	serv_addr.sin_family	  = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(BIND_IP_ADDR);
	serv_addr.sin_port		  = htons(BIND_PORT);
	//   绑定
	bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	// 使得 serv_sock 套接字进入监听状态，开始等待客户端发起请求
	listen(serv_sock, MAX_CONN);

	// 接收客户端请求，获得一个可以与客户端通信的新的生成的套接字 clnt_sock
	struct sockaddr_in client_addr;
	socklen_t		   client_addr_size = sizeof(client_addr);

	threadpool_t *pool = threadpool_create(MAX_THREAD, MAX_QUEUE_SIZE);
	while (1) // 一直循环
	{
		// 当没有客户端连接时， accept() 会阻塞程序执行，直到有客户端连接进来
		int client_socket = accept(serv_sock, (struct sockaddr *)&client_addr,
								   &client_addr_size);
		// 处理客户端的请求
		if (client_socket != -1) { threadpool_add(pool, client_socket); }
	}

	// 实际上这里的代码不可到达，可以在 while 循环中收到 SIGINT 信号时主动 break
	// 关闭套接字
	close(serv_sock);
	return 0;
}
