#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional>   // function
#include <assert.h> 
#include <chrono>
#include "../log/log.h"


using Clock = std::chrono::high_resolution_clock;
using MS = std::chrono::milliseconds;
using TimeStamp = Clock::time_point; // Clock::time_point 是 high_resolution_clock 的“时间点”类型
using TimeoutCallBack = std::function<void()>;

// 定时器节点结构
struct TimerNode {
    int id;
    TimeStamp expires;  // 超时时间点
    TimeoutCallBack cb; // 到期回调函数
    bool operator<(const TimerNode& t) {    // 重载比较运算符
        return expires < t.expires;
    }
    bool operator>(const TimerNode& t) {    // 重载比较运算符
        return expires > t.expires;
    }
};
// 小根堆定时器管理类
class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }  // 保留（扩充）容量
    ~HeapTimer() { clear(); }
    void clear();
    // 接口
    void add(int id, int timeOut, const TimeoutCallBack& cb);
    void adjust(int id, int newExpires); // 调整指定id的结点（只设计了调大），需要调小需要add
    void doWork(int id);    // 删除指定id，并触发回调函数
    void tick();            // 清除超时结点
    void pop();
    int GetNextTick();      // 返回下一个定时器的剩余时间（单位：毫秒）

private:
    void siftup_(size_t i);             // 向上调整
    bool siftdown_(size_t i, size_t n); // 向下调整,若不能向下则返回false
    void SwapNode_(size_t i, size_t j); // 交换两个结点位置
    void del_(size_t i);                // 删除指定定时器

private:
    std::vector<TimerNode> heap_;
    // key:id value:vector的下标
    std::unordered_map<int, size_t> ref_;   // id对应的在heap_中的下标，方便用heap_的时候查找
};

#endif //HEAP_TIMER_H


