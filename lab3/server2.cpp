#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

bool send_404(int fd) {
	static const std::string str =
		"HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
	return write(fd, (const void *)str.c_str(), str.length()) > 0;
}

bool send_500(int fd) {
	static const std::string str =
		"HTTP/1.1 500 Internal Server Error\r\nContent-Length: 25\r\n\r\n500 "
		"Internal Server Error";
	return write(fd, (const void *)str.c_str(), str.length()) > 0;
}

class Request {
public:
	Request();
	int				   feed(const char *buffer, size_t size);
	void			   clear();
	const std::string &get_path() { return path; }
	const std::map<std::string, std::string> &get_headers() { return headers; }
	const int								  get_state() { return state; }

private:
	std::string						   path;
	std::map<std::string, std::string> headers;
	std::ostringstream				   parser_buffer;
	std::string						   header_key;
	int								   state;
	void							   parse_path(const std::string &url);
	std::string						   simplify_path(const std::string &path);
};

Request::Request() { state = 1; }

int Request::feed(const char *buffer, size_t size) {
	for (size_t i = 0; i < size; ++i) {
		if (state == 0 || state == -1) { return state; }
		char c = buffer[i];
		if (state >= 1 && state <= 3) {
			if (c == "GET"[state - 1]) {
				++state;
			} else {
				state = -1;
			}
		} else if (state == 4) {
			if (c == ' ') {
				state = 5;
			} else {
				state = -1;
			}
		} else if (state == 5) {
			if (c != ' ') {
				state = 6;
				parser_buffer << c;
			}
		} else if (state == 6) {
			if (c == ' ') {
				state = 7;
				parse_path(parser_buffer.str());
			} else {
				parser_buffer << c;
			}
		} else if (state >= 7 && state <= 13) {
			if (c == "HTTP/1."[state - 7]) {
				++state;
			} else {
				state = -1;
			}
		} else if (state == 14) {
			if (c == '0' | c == '1') {
				state = 15;
			} else {
				state = -1;
			}
		} else if (state >= 15 && state <= 16) {
			if (c == "\r\n"[state - 15]) {
				++state;
			} else {
				state = -1;
			}
		} else if (state == 17) {
			if (c != '\r') {
				state = 18;
				parser_buffer.str("");
				parser_buffer << c;
			} else {
				state = 22;
			}
		} else if (state == 18) {
			if (c == ':') {
				state	   = 19;
				header_key = parser_buffer.str();
				parser_buffer.str("");
			} else {
				parser_buffer << c;
			}
		} else if (state == 19) {
			if (c != ' ') {
				state = 20;
				parser_buffer << c;
			}
		} else if (state == 20) {
			if (c == '\r') {
				state				= 21;
				headers[header_key] = parser_buffer.str();
			} else {
				parser_buffer << c;
			}
		} else if (state == 21) {
			if (c == '\n') {
				state = 17;
			} else {
				state = -1;
			}
		} else if (state == 22) {
			if (c == '\n') {
				state = 0;
			} else {
				state = -1;
			}
		}
	}
	return state;
}

void Request::clear() {
	state = 1;
	path.clear();
	headers.clear();
	parser_buffer.str("");
}

void Request::parse_path(const std::string &url) {
	path = simplify_path(url.substr(0, url.find('?')));
	if (path.empty() || path[0] != '/') { state = -1; }
}

std::string Request::simplify_path(const std::string &path) {
	size_t					 i = 0, j = path.find('/');
	std::vector<std::string> parts;
	while (true) {
		if (j == std::string::npos) {
			const std::string &part = path.substr(i);
			if (part == "..") {
				if (!parts.empty()) { parts.pop_back(); }
			} else {
				parts.push_back(part);
			}
			break;
		} else if (j > i) {
			const std::string &part = path.substr(i, j - i);
			if (part == "..") {
				if (!parts.empty()) { parts.pop_back(); }
			} else {
				parts.push_back(part);
			}
		}
		i = j + 1;
		if (i == path.length()) { break; }
		j = path.find('/', i);
	}
	std::ostringstream ss;
	for (auto &&part : parts) { ss << '/' << part; }
	return ss.str();
}

struct Response {
	bool   has_sent_header;
	int	   status_code;
	int	   fd;
	size_t pos;
	size_t size;
	Response() {
		has_sent_header = false;
		status_code		= 200;
		fd				= -1;
		pos				= 0;
		size			= 0;
	}
	~Response() {
		if (fd != -1) { close(fd); }
	}
	void clear() {
		has_sent_header = false;
		status_code		= 200;
		fd				= -1;
		pos				= 0;
		size			= 0;
	}
};

struct Connection {
	int		 fd;
	Request	 request;
	Response response;
};

bool set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL);
	if (flags == -1) { return false; }
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

void epoll_read(Connection *conn, int epoll_fd) {
	epoll_event event;
	event.data.ptr = (void *)conn;
	event.events   = EPOLLIN | EPOLLET;
	epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &event);
}

void epoll_write(Connection *conn, int epoll_fd) {
	epoll_event event;
	event.data.ptr = (void *)conn;
	event.events   = EPOLLOUT | EPOLLET;
	epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &event);
}

void event_loop(int fd, int max_fd) {
	char buffer[8192];
	int	 epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		std::cerr << "Cannot create epoll fd.\n";
		return;
	}
	epoll_event event;
	event.data.fd = fd;
	event.events  = EPOLLIN | EPOLLET;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
		std::cerr << "Cannot epoll_ctl EPOLL_CTL_ADD.\n";
		return;
	}
	epoll_event *events = new epoll_event[max_fd];
	while (true) {
		int ready_num = epoll_wait(epoll_fd, events, max_fd, -1);
		for (int i = 0; i < ready_num; ++i) {
			if (events[i].events & EPOLLERR) { continue; }
			if (events[i].data.fd == fd) {
				while (true) {
					sockaddr_in addr;
					socklen_t	len	   = sizeof(addr);
					int			new_fd = accept(fd, (sockaddr *)&addr, &len);
					if (new_fd == -1) {
						if (errno = EAGAIN) { break; }
						std::cerr << "Cannot accept: " << strerror(errno)
								  << "\n";
						return;
					}
					if (!set_nonblocking(new_fd)) {
						std::cerr << "Cannot set non blocking.\n";
						return;
					}
					Connection *conn = new Connection;
					conn->fd		 = new_fd;
					event.data.ptr	 = (void *)conn;
					event.events	 = EPOLLIN | EPOLLET;
					epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &event);
				}
			} else if (events[i].events & EPOLLIN) {
				Connection *conn	= (Connection *)events[i].data.ptr;
				int			sock_fd = conn->fd;
				if (events[i].events & EPOLLRDHUP) {
					delete conn;
					close(sock_fd);
					continue;
				}
				while (true) {
					ssize_t size = read(sock_fd, (void *)buffer, 8192);
					if (size > 0) {
						int state = conn->request.feed(buffer, size);
						if (state == -1) {
							conn->response.status_code = 500;
							epoll_write(conn, epoll_fd);
							break;
						} else if (state == 0) {
							std::string path = "." + conn->request.get_path();
							if (path[path.length() - 1] == '/') {
								path = path.substr(0, path.length() - 1);
							}
							int file_fd = open(path.c_str(), O_RDONLY);
							if (file_fd == -1) {
								conn->response.status_code = 404;
							} else {
								struct stat st;
								fstat(file_fd, &st);
								if (S_ISREG(st.st_mode)) {
									conn->response.fd		   = file_fd;
									conn->response.size		   = st.st_size;
									conn->response.status_code = 200;
								} else {
									conn->response.status_code = 500;
								}
								// conn->response.size = lseek(file_fd, 0,
								// SEEK_END); lseek(file_fd, 0, SEEK_SET);
							}
							epoll_write(conn, epoll_fd);
							break;
						}
					} else if (size == -1 && errno == EAGAIN) {
						// epoll_read(conn, epoll_fd);
						break;
					} else {
						delete conn;
						close(sock_fd);
						break;
					}
				}
			} else if (events[i].events & EPOLLOUT) {
				Connection *conn		= (Connection *)events[i].data.ptr;
				int			sock_fd		= conn->fd;
				bool		is_finished = false;
				if (conn->response.status_code == 200) {
					if (!conn->response.has_sent_header) {
						std::ostringstream ss;
						ss << "HTTP/1.1 200 OK\r\nContent-Length: "
						   << conn->response.size << "\r\n\r\n";
						const std::string &header = ss.str();
						ssize_t			   size =
							write(sock_fd, (const void *)header.c_str(),
								  header.length());
						if (size > 0) {
							conn->response.has_sent_header = true;
						} else if (size == -1 && errno == EAGAIN) {
							// epoll_write(conn, epoll_fd);
							continue;
						} else {
							delete conn;
							close(sock_fd);
							continue;
						}
					}
					sendfile(sock_fd, conn->response.fd, NULL,
							 conn->response.size);
					// while (true) {
					//     if (conn->response.pos >= conn->response.size) {
					//         is_finished = 1;
					//         break;
					//     }
					//     ssize_t size = sendfile(sock_fd, conn->response.fd,
					//     NULL, 8192); if (size > 0) {
					//         conn->response.pos += size;
					//         continue;
					//     } else if (size == -1 && errno == EAGAIN) {
					//         //epoll_write(conn, epoll_fd);
					//         break;
					//     } else {
					//         delete conn;
					//         close(sock_fd);
					//         break;
					//     }
					// }
				} else if (conn->response.status_code == 500) {
					if (!send_500(sock_fd)) {
						delete conn;
						close(sock_fd);
						continue;
					} else {
						is_finished = true;
					}
				} else if (conn->response.status_code == 404) {
					if (!send_404(sock_fd)) {
						delete conn;
						close(sock_fd);
						continue;
					} else {
						is_finished = true;
					}
				}
				if (is_finished) {
					/*
					delete conn;
					close(sock_fd);
					*/
					auto it = conn->request.get_headers().find("Keep-Alive");
					if (it == conn->request.get_headers().end()) {
						delete conn;
						close(sock_fd);
						// std::cerr << "close" << sock_fd << " " <<
						// close(sock_fd) << "\n";
					} else {
						conn->request.clear();
						conn->response.clear();
						epoll_read(conn, epoll_fd);
					}
				}
			}
		}
	}
}

int main() {
	const char *ip			= "0.0.0.0";
	int			port		= 8000;
	int			threads_num = 4;
	int			max_fd		= 1024 * 1024;
	signal(SIGPIPE, SIG_IGN);
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		std::cerr << "Cannot open socket.\n";
		return 0;
	}
	int opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1 ||
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
		std::cerr << "Cannot setsockopt.\n";
		return 0;
	}
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	inet_aton(ip, &server_addr.sin_addr);
	server_addr.sin_port = htons(port);
	if (bind(sockfd, (sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		std::cerr << "Cannot bind.\n";
		return 0;
	}
	if (listen(sockfd, SOMAXCONN) == -1) {
		std::cerr << "Cannot listen.\n";
		return 0;
	}
	if (!set_nonblocking(sockfd)) {
		std::cerr << "Cannot set non blocking.\n";
		return 0;
	}
	std::vector<std::thread> threads;
	for (int i = 0; i < threads_num; ++i) {
		threads.emplace_back(event_loop, sockfd, max_fd);
	}
	for (auto &&thread : threads) { thread.join(); }
	return 0;
}