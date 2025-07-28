#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()/getcwd/
#include <cassert>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../timer/heaptimer.h"

#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"

#include "../http/httpconn.h"

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        int sqlPort, const char* sqlUser, const  char* sqlPwd, const char* dbName, 
        int connPoolNum, int threadNum, bool openLog, int logLevel, int logQueSize);
    ~WebServer();

    void Start();

private:
    void InitEventMode_(int trigMode);
    bool InitSocket_(); 
  
    void DealListen_();
    void SendError_(int connFd, const char*info);
    void AddClient_(int connFd, sockaddr_in clientAddr);
    void CloseConn_(HttpConn* client);

    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);
    void ExtentTime_(HttpConn* client);
    void OnRead_(HttpConn* client);
    void OnProcess(HttpConn* client);
    void OnWrite_(HttpConn* client);
    
    static int SetFdNonblock(int fd);

private:
    static const int MAX_FD = 65536;

    int port_;          // 端口
    int timeoutMS_;     // 毫秒MS,定时器的默认过期时间
    bool isClose_;      // 服务启动标志
    int listenFd_;      // 用于监听客户端连接请求，fd是操作系统中的一个资源句柄
    bool openLinger_;   // 优雅关闭选项
    char* srcDir_;      // 需要获取的路径
    
    uint32_t listenEvent_;  // listenFd_的监听事件设置，InitEventMode_->InitSocket_
    uint32_t connEvent_;    // connFd的监听事件设置，InitEventMode_->DealListen_[AddClient_]->DealRead_或DealWrite_

    std::unique_ptr<HeapTimer> timer_;          // 时间堆
    std::unique_ptr<ThreadPool> threadpool_;    // 线程池
    std::unique_ptr<Epoller> epoller_;          // 反应堆
    std::unordered_map<int, HttpConn> users_;   // 连接队列
};


#endif //WEBSERVER_H