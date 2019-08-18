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
#include "http.h"


/*断开连接*/
void Http::disconnected(int cfd, int epfd)
{
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if (ret == -1)
    {
        perror("epoll_ctl error\n");
        exit(1);
    }
    close(cfd);
    m_user_count++;
}

/*读数据*/
void Http::do_read(int cfd, int epfd)
{
    char line[1024] = { 0 };
    //读取浏览器发过来的请求行数据
    int len = get_line(cfd, line, sizeof(line));
    if (len == 0)
    {
        printf("client disconnected!\n");
        disconnected(cfd, epfd);
    }
    else
    {
        printf("浏览器请求行数据:%s", line);
        printf("浏览器请求头-----------\n");
        while (len)
        {
            char buf[1024] = { 0 };
            len = get_line(cfd, buf, sizeof(buf));
            printf("----:%s", buf);
        }
        printf("---------End---------\n");
    }
    if (strncasecmp("get", line, 3) == 0)
    {
        http_request(line, cfd);
        disconnected(cfd, epfd);
    }
}

void Http::setnonblocking(int fd)
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
}

void Http::addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;

    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);

}

int Http::m_user_count = 0;
int Http::m_epollfd = -1;

void Http::init(int sockfd, const sockaddr_in& addr)
{
    int error = 0;
    sockaddr_in client = addr;
    socklen_t len = sizeof(error);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
    /*如下两行是为了避免TIME_WAIT状态，仅用于调试，实际使用时应该去掉*/
    //int reuse = 1;
    //setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    char ipbuf[64] = { 0 };
    printf("client: ip:%s   port:%d   sockfd:%d\n",
           inet_ntop(AF_INET, &client.sin_addr.s_addr, ipbuf, sizeof(ipbuf)),
           ntohs(client.sin_port), sockfd);
    addfd(m_epollfd, sockfd, true);
    m_user_count++;
}

//解析浏览器请求的数据
int Http::get_line(int sock, char* buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i<size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        if (n>0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n>0) && (c == '\n'))
                {
                    recv(sock, &c, 1, 0);
                }
                else
                {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }
        else
        {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

//http 请求处理
void Http::http_request(const char* request, int cfd)
{
    char method[12], path[1024], protocol[12];
    sscanf(request, "%[^ ] %[^ ] %[^ ]", method, path, protocol);
    printf("method:%s  path:%s  protocol:%s\n", method, path, protocol);

    decode_str(path, path);
    //处理path
    char *file = path + 1;
    //默认访问资源
    if (strcmp(path, "/") == 0)
    {
        char files[3]="./";
        file =files;
    }
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1)
    {
        send_respond_head(cfd, 404, "File Not Found!", ".html", -1);
        send_file(cfd, "404.html");
    }
    //判断是目录还是文件
    if (S_ISDIR(st.st_mode))
    {
        send_respond_head(cfd, 200, "OK", get_file_type(".html"), -1);
        send_dir(cfd, file);
    }
    else if (S_ISREG(st.st_mode))
    {
        send_respond_head(cfd, 200, "OK", get_file_type(file), st.st_size);
        send_file(cfd, file);
    }
}

/*发送响应头*/
void Http::send_respond_head(int cfd, int num, const char* status, const char* type, long len)
{
    char buf[1024] = { 0 };
    //响应状态行
    sprintf(buf, "http/1.1 %d %s\r\n", num, status);
    send(cfd, buf, strlen(buf), 0);
    //消息报头
    sprintf(buf, "Content-Type:%s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length:%ld\r\n", len);
    send(cfd, buf, strlen(buf), 0);
    //空行
    send(cfd, "\r\n", 2, 0);
}

/*发送文件*/
void Http::send_file(int cfd, const char* filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        //404
        return;
    }
    char buf[4096] = { 0 };
    int len = 0;
    while ((len = read(fd, buf, sizeof(buf)))>0)
    {
        send(cfd, buf, len, 0);
        memset(buf, 0, len);
    }
    if (len == -1)
    {
        perror("read error");
        exit(1);
    }
    close(fd);
}

/*发送目录内容*/
void Http::send_dir(int cfd, const char* dirname)
{
    //html页面
    char buf[4094] = { 0 };
    sprintf(buf, "<html><head><title>Directory:%s</title></head>", dirname);
    sprintf(buf + strlen(buf), "<body><h1>Current Directory:%s</h1><table>", dirname);

    char path[1024] = { 0 };
    char enstr[1024] = { 0 };

    struct dirent** ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);
    //遍历
    for (int i = 0; i<num; ++i)
    {
        char* name = ptr[i]->d_name;
        sprintf(path, "%s%s", dirname, name);//不是 buf
        printf("path:---------%s\n", path);
        struct stat st;
        stat(path, &st);

        encode_str(enstr, sizeof(enstr), name);
        //如果是目录
        if (S_ISDIR(st.st_mode))
        {
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }
        //如果是文件
        else if (S_ISREG(st.st_mode))
        {
            sprintf(buf + strlen(buf),
                    "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                    enstr, name, (long)st.st_size);
        }
        send(cfd, buf, strlen(buf), 0);
        memset(buf, 0, sizeof(buf));
    }
    sprintf(buf + strlen(buf), "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);
    printf("dirname send ok!\n");
}

//获取文件类型
const char* Http::get_file_type(const char* name)
{
    char names[32];
    strcpy(names,name);
    char* dot;
    // 自右向左查找‘.’字符, 如不存在返回NULL
    dot = strrchr(names, '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";
    return "text/plain; charset=utf-8";
}

// 16进制数转化为10进制
int Http::hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}
/*解码过程*/
void Http::encode_str(char* to, int tosize, const char* from)
{
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from)
    {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0)
        {
            *to = *from;
            ++to;
            ++tolen;
        }
        else
        {
            sprintf(to, "%%%02x", (int)*from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}

void Http::decode_str(char *to, char *from)
{
    for (; *from != '\0'; ++to, ++from)
    {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            *to = hexit(from[1]) * 16 + hexit(from[2]);
            from += 2;
        }
        else
        {
            *to = *from;
        }
    }
    *to = '\0';
}
