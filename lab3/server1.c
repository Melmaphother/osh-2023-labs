#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BIND_IP_ADDR "127.0.0.1"
#define BIND_PORT 8000
#define MAX_RECV_LEN 1048576
#define MAX_SEND_LEN 1048576
#define MAX_PATH_LEN 1024
#define MAX_HOST_LEN 1024
#define MAX_CONN SOMAXCONN
#define MAXEVENTS 1024

#define HTTP_STATUS_200 "200 OK"
#define HTTP_STATUS_404 "404 File Not Found"
#define HTTP_STATUS_500 "500 Internal Server Error"

char path[MAX_CONN][MAX_PATH_LEN];
ssize_t path_len[MAX_CONN];

int file_size (char *filename) {
    struct stat statbuf;
    stat (filename, &statbuf);
    int size = statbuf.st_size;
    return size;
}

void parse_request (char* request, ssize_t req_len, int i) {
    char* req = request;
    ssize_t s1 = 0;
    while(s1 < req_len && req[s1] != ' ') s1++;
    ssize_t s2 = s1 + 1;
    while(s2 < req_len && req[s2] != ' ') s2++;

    memcpy(path[i], req + s1 + 1, (s2 - s1 - 1) * sizeof(char));
    path[i][s2 - s1 - 1] = '\0';
    path_len[i] = (s2 - s1 - 1);
}

void handle_clnt_write (int clnt_sock, int i) {
    // 构造要返回的数据
    // 这里没有去读取文件内容，而是以返回请求资源路径作为示例，并且永远返回 200
    // 注意，响应头部后需要有一个多余换行（\r\n\r\n），然后才是响应内容
    char* response = (char*) malloc(MAX_SEND_LEN * sizeof(char)) ;
    int len = strlen (path[i]);
    if (path[i][len - 1] == '/') {
        //请求的资源为目录
        sprintf(response, 
            "HTTP/1.0 %s\r\nContent-Length: %d\r\n\r\n",
            HTTP_STATUS_500, 0);
        size_t response_len = strlen(response);
        write(clnt_sock, response, response_len);
    } else {
        //不是目录，则看是否存在该文件
        char relative_path[MAX_PATH_LEN] = ".";
        strcat (relative_path, path[i]);
        int fd = open (relative_path, O_RDONLY);
        if (fd == -1) {
            //不存在
            sprintf(response, 
                "HTTP/1.0 %s\r\nContent-Length: %d\r\n\r\n",
                HTTP_STATUS_404, 0);
            size_t response_len = strlen(response);
            write(clnt_sock, response, response_len);
        } else {
            //该文件存在
            //先获取文件长度
            //再读取并发送
            int content_len = file_size (relative_path);
            sprintf(response, 
                "HTTP/1.0 %s\r\nContent-Length: %d\r\n\r\n",
                HTTP_STATUS_200, content_len);
            size_t response_len = strlen(response);
            write(clnt_sock, response, response_len);
            //读取并发送文件内容
            //int write_len = 0, read_len = 0;
            //while (read_len < content_len) {
                //int read_count = read (fd, response, MAX_SEND_LEN);

				//write(clnt_sock, response, read_count);
                //read_len += read_count;

            //}
			int read_count;
			while((read_count = read (fd, response, MAX_SEND_LEN)) > 0) {
				write(clnt_sock, response, read_count);
			}
        }
		close (fd);
	}
	close (clnt_sock);
	free (response);
}

static int create_and_bind (char *port) {
 	struct addrinfo hints;
 	struct addrinfo *result, *rp;
 	int s, serv_sock;

  	memset (&hints, 0, sizeof (struct addrinfo));
  	hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
  	hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
  	hints.ai_flags = AI_PASSIVE;     /* All interfaces */

  	s = getaddrinfo (NULL, port, &hints, &result);
  	if (s != 0) {
      	fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
      	return -1;
    }

  	for (rp = result; rp != NULL; rp = rp->ai_next) {
      	serv_sock = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      	if (serv_sock == -1)
        	continue;

      	s = bind (serv_sock, rp->ai_addr, rp->ai_addrlen);
      	if (s == 0)
        {
			//bind成功
          	break;
        }

      	close (serv_sock);
    }

  	if (rp == NULL) {
      	fprintf (stderr, "Could not bind\n");
      	return -1;
    }

  	freeaddrinfo (result);
  	return serv_sock;
}

static int make_socket_non_blocking (int serv_sock) {
  	int flags, s;

  	if ((flags = fcntl (serv_sock, F_GETFL, 0)) == -1) {
      	perror ("fcntl");
      	return -1;
    }

  	flags |= O_NONBLOCK;
  	if ((s = fcntl (serv_sock, F_SETFL, flags)) == -1) {
      	perror ("fcntl");
      	return -1;
    }
  	return 0;
}

int main () {
	signal(SIGPIPE, SIG_IGN);
	int serv_sock, s;
	int efd;
  	struct epoll_event event;
  	struct epoll_event *events;

  	if ((serv_sock = create_and_bind ("8000")) == -1) {
		perror("socker: ");
	  	exit(EXIT_FAILURE);
 	}

  	if ((s = make_socket_non_blocking (serv_sock)) == -1) {
		perror ("make_non_blocking: ");
	  	exit(EXIT_FAILURE);
  	}

  	if((s = listen (serv_sock, SOMAXCONN)) == -1) {
      	perror ("listen");
	  	exit(EXIT_FAILURE);
    }

  	if((efd = epoll_create1 (0)) == -1) {
      	perror ("epoll_create");
	  	exit(EXIT_FAILURE);
    }

  	event.data.fd = serv_sock;
  	event.events = EPOLLIN | EPOLLET;
  	s = epoll_ctl (efd, EPOLL_CTL_ADD, serv_sock, &event);
  	if (s == -1) {
      	perror ("epoll_ctl");
		exit (EXIT_FAILURE);
    }

  	events = calloc (MAXEVENTS, sizeof (event));

  	while (1) {
      	int n, i;
      	n = epoll_wait (efd, events, MAXEVENTS, -1);
      	for (i = 0; i < n; i++) {
	  		if ((events[i].events & EPOLLERR) ||
              	(events[i].events & EPOLLHUP) ||
              	(!(events[i].events & EPOLLIN))) {
				//出错了
	      		fprintf (stderr, "epoll error\n");
	      		close (events[i].data.fd);
	      		continue;
	    	} else if (serv_sock == events[i].data.fd) {
				//有新的连接（一个或者多个）
              	while (1) {
                  	struct sockaddr in_addr;
                  	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                  	socklen_t in_len = sizeof (in_addr);
                  	int infd = accept (serv_sock, &in_addr, &in_len);
                  	if (infd == -1) {
                      	if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
							//已经处理完所有连接
                          	break;
                        } else {
                          	perror ("accept");
                          	break;
                        }
                    }

					//设置为非阻塞并加入监控列表
					if((s = make_socket_non_blocking (infd)) == -1) {
						perror ("make_non_blocking: ");
						exit (EXIT_FAILURE);
					}

                  	event.data.fd = infd;
                  	event.events = EPOLLIN | EPOLLET;
                  	s = epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event);
                  	if (s == -1) {
                      	perror ("epoll_ctl");
                      	exit (EXIT_FAILURE);
                    }
                }
             	continue;
            } else {
				// 必须一次全部读完
              	int done = 0;
                char req_buf[MAX_RECV_LEN];
				ssize_t req_len = 0;
				//读请求
              	while (1) {
                  	ssize_t count;

                  	count = read (events[i].data.fd, req_buf, sizeof (req_buf));
                  	if (count == -1) {
                      	if (errno != EAGAIN) {
							//读完了所有数据
                          	perror ("read");
                          	done = 1;
                        }
                      	break;
                    } else if (count == 0) {
                      	done = 1;
                      	break;
                    }
					req_len += count;
				}
				parse_request(req_buf, req_len, i);
				
				//回应请求
				handle_clnt_write (events[i].data.fd, i);
				
              	if (done) {
                  	//printf ("Closed connection on descriptor %d\n",
                    //      	events[i].data.fd);
                  	close (events[i].data.fd);
				  	epoll_ctl (efd, EPOLL_CTL_DEL, events[i].data.fd, &event);
                }
            }
        }
    }

  free (events);
  close (serv_sock);
  return 0;
}