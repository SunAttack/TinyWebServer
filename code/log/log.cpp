#include "log.h"

Log::Log() {
    fp_ = nullptr;
    deque_ = nullptr;
    writeThread_ = nullptr;

    lineCount_ = 0;
    toDay_ = 0;
    isAsync_ = false;
}

Log::~Log() {
    while(!deque_->empty()) {
        deque_->flush();    // 唤醒消费者，处理掉剩下的任务
    }
    deque_->Close();    // 关闭队列
    writeThread_->join();   // 等待当前线程完成手中的任务
    if(fp_) {       // 冲洗文件缓冲区，关闭文件描述符
        lock_guard<mutex> locker(mtx_);
        flush();        // 清空缓冲区中的数据
        fclose(fp_);    // 关闭日志文件
    }
}

// 唤醒阻塞队列消费者，开始写日志
void Log::flush() {
    // 只有异步日志，才会用到日志队列deque_
    if (isAsync_) deque_->flush();  
    fflush(fp_);    // 清空输入缓冲区
}

// 懒汉式：局部静态变量法（最简单）
Log* Log::Instance() {
    static Log log;
    return &log;
}

// 异步日志的写线程函数
void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}

// 写线程真正的执行函数
void Log::AsyncWrite_() {
    string str = "";
    while (deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

// 初始化日志实例
void Log::init(int level, const char* path, const char* suffix, int maxQueCapacity) {
    isOpen_ = true;
    level_ = level;
    path_ = path;
    suffix_ = suffix;
    if (maxQueCapacity) {
        // 异步方式
        isAsync_ = true;

        deque_ = std::make_unique<BlockQueue<std::string>>();
        writeThread_ = std::make_unique<std::thread>(FlushLogThread);
    }
    else isAsync_ = false;

    lineCount_ = 0;
    time_t timer = time(nullptr);           // 该函数会返回从1970年1月1日0时0秒算起到现在所经过的秒数
    struct tm* systime = localtime(&timer); // 参数timer所指的当前秒数，转换成真实世界所使用的时间日期
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, systime->tm_year + 1900, systime->tm_mon + 1, systime->tm_mday, suffix_);
    toDay_ = systime->tm_mday;

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();
        if (fp_) {// 重新打开
            flush();
            fclose(fp_);
        }
        
        struct stat st = {0};
        if (stat(path_, &st) == -1) {
            mkdir(path_, 0777);
        }
        fp_ = fopen(fileName, "a"); // a追加写入，文件不存在则创建，存在则在末尾添加
        assert(fp_ != nullptr);
    }
}

void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};    // 结构体含秒sec，微妙usec
    gettimeofday(&now, nullptr);    // 获取当前时间，为UTC时间，1970年以来的秒+微秒（timeval-now）、时区信息(timezone-nullptr)
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;

    va_list vaList;
    // 日志日期：期不一样，说明跨天了，需要切换到当天新的日志文件
    // 日志行数：判断当前写的行数是否刚好是MAX_LINES的整数倍
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        // 1. 准备新文件名，非临界区，无需加锁
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday) {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
        } else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_ / MAX_LINES), suffix_);
        }
        // 2. 临界区：加锁更新共享状态，关闭旧文件，打开新文件
        {
            unique_lock<mutex> locker(mtx_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
            flush();
            fclose(fp_);
            fp_ = fopen(newFile, "a");
            assert(fp_ != nullptr);
        }
    }

    // 在buffer内生成一条对应的日志信息
    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        buff_.HasWritten(n);
        
        AppendLogLevelTitle_(level);    
        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);
        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        if(isAsync_ && deque_ && !deque_->full()) { // 异步方式（加入阻塞队列中，等待写线程读取日志信息）
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {    // 同步方式（直接向文件中写入日志信息）
            fputs(buff_.Peek(), fp_);   // 同步就直接写入文件
        }
        buff_.RetrieveAll();    // 清空buff
    }
}

// 添加日志等级
void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

