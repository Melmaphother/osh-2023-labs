#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BIND_IP_ADDR "127.0.0.1"
#define BIND_PORT 8000
#define MAX_RECV_LEN 1048576
#define MAX_SEND_LEN 1048576
#define MAX_PATH_LEN 1024
#define MAX_HOST_LEN 1024
#define MAX_CONN 20

#define HTTP_STATUS_200 "200 OK"
#define HTTP_STATUS_404 "404 Not Found"
#define HTTP_STATUS_500 "500 Internal Server Error"
#define Error(error)                                                           \
	do {                                                                       \
		perror(error);                                                         \
		exit(EXIT_FAILURE);                                                    \
	} while (0)