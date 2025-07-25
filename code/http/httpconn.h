#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // iovec/readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>      

#include "../log/log.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"
/*
进行读写数据并调用httprequest 来解析数据以及httpresponse来生成响应
*/
class HttpConn {
public:
    static bool isET;                   // ET模式
    static const char* srcDir;
    static std::atomic<int> userCount;  // 用户连接数，原子操作

public:
    HttpConn();
    ~HttpConn();
    void Close();
    
    void init(int sockFd, const sockaddr_in& addr);
    /*
        将fd的内容读到readBuff_缓冲区
    */
    ssize_t read(int* saveErrno); 
    /*
        request_.Init(); (感觉多余了)
        将readBuff_缓冲区给request_.parse解析，解析出：
            首行（方法为method_，URL为path_，版本号为version_）
            协议头HEADER（字段名: 值）
            \r\n
            正文BODY
        根据上面的URL，初始化response_.Init

        生成响应报文(首行、协议头、\r\n)放入writeBuff_缓冲区中，封装进iov_[0]
        若无误则存在正文，通过mmap的mmFile_和mmFileStat_封装进iov_[1]
    */
    bool process(); 
    /*
        处理iov_，全部（分批次）写入fd(socket)
    */
    ssize_t write(int* saveErrno);


    // 简单接口
    int GetFd() const;
    int GetPort() const;
    const char* GetIP() const;
    sockaddr_in GetAddr() const;
    // 写的总长度
    int ToWriteBytes() { 
        return iov_[0].iov_len + iov_[1].iov_len; 
    }
    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

private:
    HttpRequest request_;
    HttpResponse response_;

    int fd_;
    struct  sockaddr_in addr_;
    Buffer readBuff_;       // 读缓冲区
    Buffer writeBuff_;      // 写缓冲区
    bool isClose_;
    
    int iovCnt_;
    struct iovec iov_[2];
};

#endif

/*
HttpConn::HttpConn
HttpConn::init
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
HttpConn::read
HttpConn::process
    LOG_DEBUG("%s", request_.path().c_str());
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
HttpConn::write
HttpConn::~HttpConn
    LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
*/
