#include "webserver.h"

using namespace std;

/*
    参数        含义
    port        初始化列表port_(port)，端口
    trigMode    设置listenEvent_和connEvent_的监听事件
    timeoutMS   初始化列表timeoutMS_(timeoutMS)，定时器的默认过期时间，单位MS
    sqlPort
    sqlUser
    sqlPwd
    dbName
    connPoolNum SqlConnPool的允许最大数量
    threadNum   threadpool_(new ThreadPool(threadNum))
    openLog     是否打开日志的标志
    logLevel    用于日志单例化单线程的参数，日志等级
    logQueSize  用于日志单例化单线程的参数，0为同步日志，>0为异步日志
*/
WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd, const char* dbName, 
            int connPoolNum, int threadNum, bool openLog, int logLevel, int logQueSize):
            port_(port), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {

    // 是否打开日志标志
    if(openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }

    // getcwd：获取当前工作目录（current working directory）。
    // 参数 nullptr, 256 表示由 getcwd 自动 malloc 一块 256 字节的空间返回给你。
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    // 在 srcDir_ 后追加字符串 "/resources/"。
    strcat(srcDir_, "/resources/");
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_; // ：HTTP 服务器的资源目录路径

    // 初始化操作
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);  // 连接池单例的初始化
    // 初始化事件和初始化socket(监听)
    InitEventMode_(trigMode);
    if(!InitSocket_()) { isClose_ = true;}
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;    // 检测socket关闭
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;     // EPOLLONESHOT由一个线程处理
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

// 初始化监听 sockFd（listenFd_），执行成功返回 true，失败返回 false
bool WebServer::InitSocket_() {
    // 1. 创建套接字：socket()
    // AF_INET为IPV4协议、SOCK_STREAM为TCP流式套接字、0为默认协议
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    int ret;
    int optval = 1;
    /* 端口复用 */
    /*
        SO_REUSEADDR表示**“允许端口快速重用”**。
        防止服务器意外退出或重启时，端口可能处于 TIME_WAIT 状态，无法立刻重新 bind 相同端口
        否则你可能遇到：bind: Address already in use

        SO_REUSEADDR 并不意味着多个服务“同时”接收数据。
        如果多个 socket 使用 SO_REUSEADDR 绑定同一个地址端口，只有最后一个成功 bind 的 socket 能收到数据。
    */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    // 2. 绑定地址端口：bind()
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 3. 开始监听：listen()
    // ret = listen(listenFd_, 8);      //（原版）
    ret = listen(listenFd_, SOMAXCONN); // (改动)
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 4. 注册listenFd_到epoller
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);  // 将监听套接字加入epoller
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }

    // 非阻塞是 IO 多路复用（如 epoll）配套使用的关键点
    // 否则一次 accept 或 recv 就可能挂住整个线程。
    SetFdNonblock(listenFd_);       // 设置非阻塞，epoll一般和非阻塞一起用
    LOG_INFO("Server port:%d", port_);
    return true;
}

void WebServer::Start() {
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {
        /*
        1. 计算下一次超时时间（用于 epoll_wait 的超时时间）
        2. 等待事件：epoll_wait
        3. 遍历返回的事件数组，依次处理
        */
        if(timeoutMS_ > 0) {
            // 获取下一次的超时等待事件(至少这个时间才会有用户过期，每次关闭超时连接则需要有新的请求进来)
            // 第一次运行时，事件堆为空，返回-1
            timeMS = timer_->GetNextTick();     
        }
        // timeMS为-1时，永久阻塞，直到有事件
        // timeMS>0时，指定MS内阻塞，超时返回
        int eventCnt = epoller_->Wait(timeMS);
        for(int i = 0; i < eventCnt; i++) {
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            // fd等于listenFd_，代表是连接事件
            if(fd == listenFd_) {
                DealListen_();              
            }
            // fd等于connFd，代表服务器和客户端之间的事务事件
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 客户端关闭或异常
                assert(users_.count(fd) > 0);
                // 不要直接手动关闭（原版）
                // CloseConn_(&users_[fd]);
                // 只调用定时器的doWork，由定时器复杂触发CloseConn_回调
                timer_->doWork(fd); // 已内部清除user_
            }
            else if(events & EPOLLIN) {
                // 读事件
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);     
            }
            else if(events & EPOLLOUT) {
                //写事件
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);    
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

/*
接收连接，EPOLLET 模式需循环 accept()
    若HttpConn::userCount >= MAX_FD，运行SendError_
    否则，connFd加入时间堆timer、反应堆epoller
*/
/*
    在 EPOLLET 模式下，一次 epoll_wait() 唤醒后，可能有 多个客户端同时连接；
    你必须用循环 accept() 把他们一次性全部接收完，否则后续连接会被“饿死”。
*/
void WebServer::DealListen_() {
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);

    while (listenEvent_ & EPOLLET) {
        int connFd = accept(listenFd_, (struct sockaddr *)&clientAddr, &len);
        if (connFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // EAGAIN此处代表，操作资源暂时不可用（如 socket 当前无连接可 accept）
                // EWOULDBLOCK	操作会阻塞（在非阻塞模式下不能阻塞）
                // 在大多数系统中，它们是等价的，即：#define EWOULDBLOCK EAGAIN
                /* 此次唤醒，所有连接处理完毕，退出循环 */
                break;
            }
            // 其他错误
            LOG_ERROR("Accept error: %d (%s)", errno, strerror(errno));
            break;
        }
        if(HttpConn::userCount >= MAX_FD) {
            SendError_(connFd, "Server busy!");
            LOG_WARN("Clients is full!");
            continue;
        }

        AddClient_(connFd, clientAddr); // 将connFd加入epoll管理
    }
}

void WebServer::SendError_(int connFd, const char*info) {
    assert(connFd > 0);
    int ret = send(connFd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", connFd);
    }
    close(connFd);
}

void WebServer::AddClient_(int connFd, sockaddr_in clientAddr) {
    assert(connFd > 0);
    users_[connFd].init(connFd, clientAddr);
    if(timeoutMS_ > 0) {
        timer_->add(connFd, timeoutMS_, [this, connFd]() {
            if (users_.count(connFd)) {
                CloseConn_(&users_[connFd]);
            }
        });
    }
    epoller_->AddFd(connFd, EPOLLIN | connEvent_);
    SetFdNonblock(connFd);  // // 设置非阻塞
    LOG_INFO("Client[%d] in!", users_[connFd].GetFd());
}
// 不要直接手动关闭
// 只调用定时器的doWork，由定时器复杂触发CloseConn_回调
void WebServer::CloseConn_(HttpConn* client) {
    assert(client);

    int connFd = client->GetFd();
    LOG_INFO("Client[%d] quit!", connFd);
    epoller_->DelFd(connFd);   // 从epoll中删除
    client->Close();
    users_.erase(connFd);   // 内部清除users_中的HttpConn
}


/*
在events& EPOLLIN 或events & EPOLLOUT为真时，需要进行读写的处理。分别调用 DealRead_(&users_[fd])和DealWrite_(&users_[fd]) 函数。
这里需要说明：DealListen_()函数并没有调用线程池中的线程，而DealRead_(&users_[fd])和DealWrite_(&users_[fd]) 则都交由线程池中的线程进行处理了。

这就是Reactor，读写事件交给了工作线程处理。
*/

// 处理读事件，主要逻辑是将OnRead加入线程池的任务队列中
void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client)); // 这是一个右值，bind将参数和函数绑定
}

// 处理写事件，主要逻辑是将OnWrite加入线程池的任务队列中
void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}
// 读/写事件，标志此connFd不需要定时销毁===>刷新对应的时间堆
void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);         // 将fd的内容读到httpconn的readBuff_缓存区
    if(ret <= 0 && readErrno != EAGAIN) {   // 读异常就关闭客户端(EAGAIN标志暂时无数据可读/写)
        // 只调用定时器的doWork，由定时器复杂触发CloseConn_回调
        timer_->doWork(client->GetFd()); // 已内部清除user_
        return;
    }
    // 业务逻辑的处理（先读后处理）
    OnProcess(client);
}

/* 处理读（请求）数据的函数 */
void WebServer::OnProcess(HttpConn* client) {
    // 首先调用process()进行逻辑处理
    if(client->process()) { // 根据返回的信息重新将fd置为EPOLLOUT（写）或EPOLLIN（读）
    //读完事件就跟内核说可以写了
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);    // 响应成功，修改监听事件为写,等待OnWrite_()发送
    } else {
    //写完事件就跟内核说可以读了
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            // 保持连接，改成监听读事件，等待下一次请求
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN); // 回归换成监测读事件
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) { 
            // 写缓冲区满了，继续监听写事件，等待可写时续传
            // 只要再规定时间内再次继续写，就不会触发关闭connfd
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    // 只调用定时器的doWork，由定时器复杂触发CloseConn_回调
    timer_->doWork(client->GetFd()); // 已内部清除user_
}

// 设置非阻塞
int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

