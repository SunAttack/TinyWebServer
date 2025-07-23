#include "../code/log/log.h"            // 日志模块头文件
#include "../code/pool/threadpool.h"    // 线程池模块头文件
#include <features.h>   //  GNU C 的内部系统头文件，允许我们访问 __GLIBC__ 等宏，用来判断 glibc 版本

#include <iostream>

/*
如果你的系统 glibc 版本小于 2.30（即不支持 std::this_thread::get_id() 打印真实线程 ID），则手动定义 gettid()。
gettid() 获取线程 ID，用于日志记录线程编号。
*/
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif

/*
    测试日志系统在同步/异步、不同等级、不同滚动方式下的性能与输出。
*/
void TestLog() {
    int cnt = 0, level = 0;
    
    std::cout << "同步日志" << std::endl;
    // 同步日志
    // 1W个error
    // 1W个warn  和 1W个error交错
    // 1W个info  和 1W个warn 和 1W个error交错
    // 1W个Debug 和 1W个info 和 1W个warn 和 1W个error交错
    Log::Instance()->init(level, "./log1", ".log", 0);
    for(level = 3; level >= 0; level--) {
        Log::Instance()->SetLevel(level);
        for(int j = 0; j < 10000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s 111111111 %d ============= ", "Test", cnt++);
            }
        }
    }

    std::cout << "异步日志" << std::endl;
    // 异步日志
    // 1W个Debug 和 1W个info 和 1W个warn 和 1W个error交错
    // 1W个info  和 1W个warn 和 1W个error交错
    // 1W个warn  和 1W个error交错
    // 1W个error
    cnt = 0;
    Log::Instance()->init(level, "./log2", ".log", 5000);
    for(level = 0; level < 4; level++) {
        Log::Instance()->SetLevel(level);
        for(int j = 0; j < 10000; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s 222222222 %d ============= ", "Test", cnt++);
            }
        }
    }
}

void ThreadLogTask(int i, int cnt) {
    for(int j = 0; j < 10000; j++ ){
        LOG_BASE(i,"PID:[%04d]======= %05d ========= ", gettid(), cnt++);
    }
}

void TestThreadPool() {
    Log::Instance()->init(0, "./testThreadpool", ".log", 5000);
    ThreadPool threadpool(6);
    for(int i = 0; i < 18; i++) {
        threadpool.AddTask(std::bind(ThreadLogTask, i % 4, i * 10000));
    }
    getchar();
}

int main() {
    TestLog();
    std::cout << "TestLog出来" << std::endl;
    // TestThreadPool();
    return 0;
}
