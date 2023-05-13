#include <arpa/inet.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

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
#define Error(error)                                                           \
	do {                                                                       \
		perror(error);                                                         \
		exit(EXIT_FAILURE);                                                    \
	} while (0)

void handle_clnt(int client_socket);
int	 parse_request(int client_socket, ssize_t *req_len, char *req,
				   struct stat *file_type);