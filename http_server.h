#ifndef _EPOLL_SERVER_HTTP_H
#define _EPOLL_SERVER_HTTP_H

/*服务器开始运行*/
void epoll_run(int port);

/*初始化监听套接字*/
int init_listen_fd(int port,int epfd);

void do_accept(int epfd,int lfd);

void do_read(int cfd,int epfd);

int get_line(int sock,char* buf,int size);

void disconnected(int cfd,int epfd);

void http_request(const char* request,int cfd);

void send_respond_head(int cfd,int num,const char* status,const char* type,long len);

const char* get_file_type(const char* name);

void send_file(int cfd,const char* filename);

void send_dir(int cfd,const char* dirname);

void encode_str(char* to, int tosize, const char* from);

void decode_str(char *to, char *from);

#endif
