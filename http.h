#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <iostream>

class Http
{
public:
	Http(){};
	~Http(){};

public:
	/*所有socket上的事件都被注册到同一个epoll内核事件表中，所以将epoll文件描述符设置为静态*/
	static int m_epollfd;
	/*统计用户数量*/
	static int m_user_count;
private:
	int get_line(int sock, char* buf, int size);
  void setnonblocking(int fd);
  void addfd(int epollfd, int fd, bool one_shot);
	void http_request(const char* request, int cfd);
	void send_respond_head(int cfd, int num, const char* status, const char* type, long len);
	const char* get_file_type(const char* name);
	void send_file(int cfd, const char* filename);
	void send_dir(int cfd, const char* dirname);
	void encode_str(char* to, int tosize, const char* from);
	void decode_str(char *to, char *from);
  int hexit(char c);
public:
	/*服务器开始运行*/
	void process(int port);
	void disconnected(int cfd, int epfd);
	void init(int sockfd, const sockaddr_in& addr);
	void do_read(int cfd, int epfd);
};
