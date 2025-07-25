#ifndef LOG_H
#define LOG_H

#include "../buffer/buffer.h"
#include "blockqueue.h"

#include <memory>
#include <string>
#include <mutex>
#include <thread>
#include <cassert>
#include <sys/time.h>
#include <sys/stat.h>   // mkdir
#include <sys/types.h>  
#include <stdarg.h>     // va_list

class Log {
public:
    static Log* Instance();
    // 初始化日志实例（阻塞队列最大容量、日志保存路径、日志文件后缀）
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);
    void write(int level, const char *format, ...);  // 将输出内容按照标准格式整理

    static void FlushLogThread();   // 异步写日志公有方法，调用私有方法asyncWrite
    void flush();
    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }

private:
    Log();   // 构造函数私有化
    Log(const Log&) = delete;            // 禁止拷贝构造
    Log& operator=(const Log&) = delete; // 禁止拷贝赋值
    virtual ~Log();

    void AppendLogLevelTitle_(int level);
    void AsyncWrite_(); // 异步写日志方法

private:
    // 类共享的静态成员，不属于具体对象
    static const int LOG_PATH_LEN = 256;    // 日志文件最长文件名
    static const int LOG_NAME_LEN = 256;    // 日志最长名字
    static const int MAX_LINES = 50000;     // 日志文件内的最长日志条数
    
private:
    FILE* fp_;                                       //打开log的文件指针
    Buffer buff_;       // 输出的内容，缓冲区
    std::mutex mtx_;                                 //同步日志必需的互斥量

    bool isAsync_;      // 是否开启异步日志
    std::unique_ptr<BlockQueue<std::string>> deque_; //阻塞队列
    std::unique_ptr<std::thread> writeThread_;       //写线程的指针
    
    bool isOpen_;   
    const char* path_;          //路径名
    const char* suffix_;        //后缀名
    int level_;                 // 日志等级

    int lineCount_;             // 日志行数记录
    int toDay_;                 // 按当天日期区分文件
};

// 多语句宏封装
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

// 四个宏定义，主要用于不同类型的日志输出，也是外部使用日志的接口
// ...表示可变参数，__VA_ARGS__就是将...的值复制到这里
// 前面加上##的作用是：当可变参数的个数为0时，这里的##可以把把前面多余的","去掉,否则会编译出错。
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);


#endif