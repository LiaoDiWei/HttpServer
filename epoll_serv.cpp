/*
*   By LiaoDiWei
*   2020.5.30
*/

#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <string>
#include <iostream>
#include <fstream>
#include <fcntl.h>

using std::string;
using std::cout;
using std::endl;
using std::fstream;


//处理url
void handle_url(int clntSock, const string& url);
//发送响应消息体
void send_data(FILE* &fp, const string& ct, const string& filename);
//获取响应文件类型
string content_type(const string& file);  
//响应错误处理 
void send_error(FILE* &fp);
//获取文件长度
std::streampos getLen(fstream &fp);  
//套接字错误处理   
void error_handling(const std::string& message);

#define BUF_SIZE 2048
#define SMALL_BUF 200
#define EPOLL_SIZE 100




int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_size;
    char buf[BUF_SIZE];
    int str_len, i;
    string str;     //用于错误处理

    struct epoll_event *ep_events;      //用于epoll_wait地址值
    struct epoll_event event;           //用于epoll_ctl
    int epfd;                           //用于epoll_create
    int event_cnt;                      //用于epoll_wait返回值
    

    if (argc != 2)
    {
        cout << ("Usage : %s <port>\n", argv[0]) << endl;
        exit(1);    //exit(0)表示程序成功终止，exit(1)表示程序非正常终止
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serv_sock == -1)
    {
        str = "socket() error";
        error_handling(str);
    }
        
    
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;                 //AF_INET表示TCP/IP协议
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);  //将主机网卡转换为网络字节序,INADDR_ANY表示自动获取IP地址
    serv_adr.sin_port = htons(atoi(argv[1]));


    if (bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1)
    {
        str = "bind() error";
        error_handling(str);
    }  

    if (listen(serv_sock, 5) == -1)
    {
        str = "listen() error";
        error_handling(str);
    }

    //向OS申请文件描述符保存空间(epoll例程)
    epfd = epoll_create(EPOLL_SIZE);
    //动态分配内存，用于保存epoll_wait函数调用后发生事件的所有文件描述符
    ep_events = (struct epoll_event*)malloc(sizeof(struct epoll_event)*EPOLL_SIZE);
    
    event.events = EPOLLIN;
    event.data.fd = serv_sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);  //注册serv_sock的读取事件


    while (true)
    {
        event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
        if (event_cnt == -1)
        {
            str = "epoll_wait() error";
            error_handling(str);
        }

        cout << "此时有 " << event_cnt << " 个请求" << endl;
        //处理发生读取事件的套接字
        for (i = 0; i < event_cnt; ++i)
        {
            if (ep_events[i].data.fd == serv_sock)  //连接请求
            {
                clnt_adr_size = sizeof(clnt_adr);
                clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_adr, &clnt_adr_size);
                if (clnt_sock == -1)
                {
                    cout << "连接请求失败" << endl;
                    continue;
                }
                else
                {
                    event.events = EPOLLIN;
                    event.data.fd = clnt_sock;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                    cout << "客户端 " << clnt_sock << " 已连接: " << inet_ntoa(clnt_adr.sin_addr) 
                         << ": " << ntohs(clnt_adr.sin_port) << endl;
                }
            }
            else    //读取数据
            {
                int clntSock = ep_events[i].data.fd;
                str_len = read(clntSock, buf, BUF_SIZE);
                if (str_len == 0)   //断开连接请求,接收FIN包
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clntSock, NULL);
                    close(clntSock);
                    cout << "客户端: " << clntSock << " 断开连接..." << endl;
                }
                else
                {
                    string recv_header(buf);
                    auto index = recv_header.find('\n');
    
                    //获取"GET /index.html HTTP/1.1" 
                    string urlStr;
                    if (index != string::npos)
                        urlStr = recv_header.substr(0, index);
                    
                    handle_url(clntSock, urlStr);
                }
            }

        }

        
    }

    close(serv_sock);
    return 0;
}


//处理url
void handle_url(int clntSock, const string& url)
{
    //url情况：
    //GET /dist/index.html HTTP/1.1
    //GET /favicon.ico HTTP/1.1
    //GET /api/seller?id=undefined HTTP/1.1
    string method, ct, file_name;
    method = url.substr(0, 3);
    cout << "请求方法：" << method << endl;
    if (method.compare("GET") != 0)
    {
        cout << "不是GET请求\n";
        return;
    }
    
    auto pos1 = url.find('/');
    auto pos2 = url.find('?');
    
    if (pos2 == string::npos)
        pos2 = url.rfind(' ');

    file_name = url.substr(pos1+1, pos2-pos1-1);
    cout << "文件名：" << file_name << endl;

    ct = content_type(file_name);
    cout << "文件类型：" << ct << endl;

    FILE* clnt_write = fdopen(clntSock, "w");

    send_data(clnt_write, ct, file_name);
    // close(clntSock);    //此时不需要关闭文件描述符，关闭文件指针就行
}



//发送响应消息结构
void send_data(FILE* &fp, const string& ct, const string& file_name)
{
    cout << "请求文件名: " << file_name << endl;

    char protocol[] = "HTTP/1.1 200 OK\r\n";
    char server[] = "Server:Web Server \r\n";
    char cnt_len[SMALL_BUF];
    char connection[] = "Connection: keep-alive\r\n";
    char acceptRan[] = "Accept-Ranges:bytes\r\n";
    char cnt_type[SMALL_BUF];
    sprintf(cnt_type, "Content-type:%s\r\n\r\n", ct.c_str());

    string buf;             //传输数据
    

    fstream send_file;      //文件流    
    send_file.open(file_name, std::ios::in | std::ios::ate);
    if (!send_file.is_open())
    {
        cout << "文件打开失败" << endl;
        send_error(fp);
        return;
    }

    //获取文件长度，chrome浏览器要求文件实际长度与Content-length一致
    auto fileLen = getLen(send_file);
    sprintf(cnt_len, "Content-length:%ld\r\n", fileLen);

    //传输头部信息
    fputs(protocol, fp);
    fputs(server, fp);
    fputs(acceptRan, fp);
    fputs(cnt_len, fp);
    fputs(connection, fp);
    fputs(cnt_type, fp);
    fflush(fp);

    int str_len = 0;
    //传输请求的文件
    // cout << "要发送大小为 " << fileLen << " 字节的文件" << endl;
    // 第1种传输数据方式：文本文件
    buf.assign(fileLen, '\0');
    while (getline(send_file, buf)) //遇到'\n'结束读取
    {
        fputs(&buf[0], fp);
        str_len += buf.size();
        if (str_len < fileLen)
        {
            fputs("\n", fp);
            str_len += 1;
        }
        fflush(fp);  
    }
    fflush(fp);  
    cout << "发送了 " << str_len << " 字节" << endl;

    // 第2种传输数据方式：二进制文件
    // buf.assign(fileLen, '\0');
    // if(send_file.read(&buf[0], fileLen))
    //     cout << "发送了 " << buf.size() << " 字节" << endl;
    // fputs(buf.c_str(), fp);
    // fflush(fp);  

    send_file.close();  //关闭文件流

    fclose(fp);         //关闭 “写” 套接字
}


//获取文件长度
std::streampos getLen(fstream &fp)
{
    std::streampos fileLen = fp.tellp();    //获取文件长度
    cout << "文件长度: " << fileLen << endl;        
    fp.seekp(0, std::ios::beg);             //回到文件头

    return fileLen;
}



//设置Content-type信息
//str("index.html")
string content_type(const string& str)
{
    string extension;
    auto pos = str.rfind(".");

    
    //没有后缀名一律为json
    if (pos == string::npos)
    {   
        extension = "application/json";
        return extension;
    }
        
    extension = str.substr(pos+1, str.size()-pos-1);    //拿到扩展名,例如html、htm

    cout << "文件扩展名：" << extension << endl;

    if ((extension.compare("html") == 0) || (extension.compare("htm") == 0))
        extension = "text/html; charset=UTF-8";
    else if (extension.compare("css") == 0)
        extension = "text/css";
    else if (extension.compare("js") == 0)
        extension = "application/x-javascript";

    return extension;
}


//请求相关的错误处理
void send_error(FILE* &fp)
{
    char protocol[] = "HTTP/1.1 400 Bad Request\r\n";
    char server[] = "Server:Linux Web Server \r\n";
    char cnt_len[SMALL_BUF];
    char cnt_type[] = "Content-type:text/html\r\n\r\n";
    char content[BUF_SIZE];

    fstream err_file;
    err_file.open("./error_index.html", std::ios::in | std::ios::out | std::ios::ate);
    //获取文件长度，为了配合chrome浏览器
    auto errLen = getLen(err_file);
    sprintf(cnt_len, "Content-length:%ld\r\n", errLen);

    puts("请求错误");
    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);
    fflush(fp);
    
    while (err_file.getline(content, BUF_SIZE))
    {
        fputs(content, fp);
        fputs("\n", fp);
        fflush(fp);
        cout << "发送了 " << strlen(content) << "字节" << endl;
    }
    fflush(fp);


    err_file.close();
    fclose(fp);
}


//套接字相关的错误处理
void error_handling(const std::string& message)
{
    // fputs(message, stderr);
    // fputc('\n', stderr);
    std::cerr << message << endl;
    exit(1);
}