#include "heaptimer.h"

// 向时间堆中添加一个定时器
// 如果该 id 已存在，则更新其到期时间（调大/调小）与回调函数；否则新建一个定时器加入小根堆。
void HeapTimer::add(int id, int timeOut, const TimeoutCallBack& cb) {
    assert(id >= 0);
    // 如果有，则调整
    if(ref_.count(id)) {
        int tmp = ref_[id];
        heap_[tmp].expires = Clock::now() + MS(timeOut);
        heap_[tmp].cb = cb;
        if(!siftdown_(tmp, heap_.size())) {
            siftup_(tmp);
        }
    } 
    else {
        size_t n = heap_.size();
        ref_[id] = n;

        heap_.push_back({id, Clock::now() + MS(timeOut), cb});
        // 结构体 TimerNode 的列表初始化
        // 等价于
        // TimerNode node(id, expires, cb);  // 构造
        // heap_.push_back(node);

        // 推荐用（前提是你必须定义对应的构造函数 TimerNode(int, TimePoint, function<void()>)）
        // heap_.emplace_back(id, Clock::now() + MS(timeOut), cb);
        siftup_(n);
    }
}

// 用于插入时维护小根堆结构
// 沿着当前节点向上父节点冒泡，直到满足堆性质
void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if(heap_[parent] > heap_[i]) {  // 会调用定时器节点结构的比较运算符
            SwapNode_(i, parent);
            i = parent;
        } else break;
    }
}

// 用于堆顶或中间节点的调整（如 del_() 后）
// 维护小根堆局部有序性
// false：不需要下滑  true：下滑成功
bool HeapTimer::siftdown_(size_t i, size_t n) {
    assert(i >= 0 && i < heap_.size());
    assert(n >= 0 && n <= heap_.size());    // n:共几个结点
    auto index = i;         // 父节点
    auto child = 2*index+1; // 左子节点
    while(child < n) {
        // 用比较小的子节点===>确保交换后符合小根堆(父<子)
        if(child+1 < n && heap_[child+1] < heap_[child]) {
            child++;
        }
        // 如果子节点更小，交换并继续向下
        if(heap_[child] < heap_[index]) {
            SwapNode_(index, child);
            index = child;
            child = 2*child+1;
        }
    }
    return index > i; // 是否发生了下沉
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i <heap_.size());
    assert(j >= 0 && j <heap_.size());
    swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;    // 结点内部id所在索引位置也要变化
    ref_[heap_[j].id] = j;    
}

// 删除指定位置的结点
void HeapTimer::del_(size_t index) {
    assert(index >= 0 && index < heap_.size());
    // 将要删除的结点换到队尾，然后调整堆
    size_t tmp = index;
    size_t n = heap_.size() - 1;
    assert(tmp <= n);
    // 如果就在队尾，就不用移动了
    if(index < heap_.size()-1) {
        SwapNode_(tmp, heap_.size()-1); //和队尾的交换
        if(!siftdown_(tmp, n)) {    // 队尾元素来到tmp位置，判断是否需要下沉/上浮
            siftup_(tmp);
        }
    }
    ref_.erase(heap_.back().id);    // 先读再弹出
    heap_.pop_back();
}


// 调整指定id的结点（只设计了调大），需要调小需要add
void HeapTimer::adjust(int id, int newExpires) {
    assert(!heap_.empty() && ref_.count(id));
    heap_[ref_[id]].expires = Clock::now() + MS(newExpires);
    siftdown_(ref_[id], heap_.size());
}

// 删除指定id，并触发回调函数
void HeapTimer::doWork(int id) {
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    auto node = heap_[i];
    node.cb();  // 触发回调函数
    del_(i);
}

void HeapTimer::tick() {
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break; 
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

int HeapTimer::GetNextTick() {
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}


