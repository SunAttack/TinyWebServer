#ifndef BOLCKQUEUE_H
#define BLOCKQUEUE_H

#include <deque>
#include <condition_variable>
#include <mutex>
#include <sys/time.h>
#include <cassert>

using namespace std;

template <typename T>
class BlockQueue {
public:
    explicit BlockQueue(size_t maxsize = 1000);
    ~BlockQueue();

    bool empty();
    bool full();
    void push_back(const T& item);
    void push_front(const T& item); 
    bool pop(T& item);  // 弹出的任务放入item
    bool pop(T& item, int timeout);  // 等待时间
    T front();
    T back();
    size_t capacity();
    size_t size();
    void clear();

    void flush();                       // 唤醒消费者
    void Close();

private:
    deque<T> deq_;                      // 底层数据结构
    size_t capacity_;                   // 容量
    bool isClose_;                      // 关闭标志
    
    mutex mtx_;
    condition_variable condConsumer_;   // 消费者条件变量
    condition_variable condProducer_;   // 生产者条件变量
};

template <typename T>
BlockQueue<T>::BlockQueue(size_t maxsize) : capacity_(maxsize) {
    assert(maxsize > 0);
    isClose_ = false;
}

template <typename T>
BlockQueue<T>::~BlockQueue() {
    Close();
}

template <typename T>
void BlockQueue<T>::Close() {
    clear();
    isClose_ = true;
    condConsumer_.notify_all();
    condProducer_.notify_all();
}

template <typename T>
void BlockQueue<T>::clear() {
    lock_guard<mutex> locker(mtx_);
    deq_.clear();
}

template <typename T>
bool BlockQueue<T>::empty() {
    lock_guard<mutex> locker(mtx_);
    return deq_.empty();
}

template <typename T>
bool BlockQueue<T>::full() {
    lock_guard<mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template <typename T>
void BlockQueue<T>::push_back(const T& item) {
    unique_lock<mutex> locker(mtx_);
    while (deq_.size() >= capacity_) {        // 队列满了，需要等待
        condProducer_.wait(locker); // 暂停生产，等待消费者唤醒生产条件变量
    }
    deq_.push_back(item);
    condConsumer_.notify_one();     // 唤醒消费者
}

template <typename T>
void BlockQueue<T>::push_front(const T& item) {
    unique_lock<mutex> locker(mtx_);
    while (deq_.size() >= capacity_) {        // 队列满了，需要等待
        condProducer_.wait(locker); // 暂停生产，等待消费者唤醒生产条件变量
    }
    deq_.push_front(item);
    condConsumer_.notify_one();     // 唤醒消费者
}

template <typename T>
bool BlockQueue<T>::pop(T& item) {
    unique_lock<mutex> locker(mtx_);
    while(deq_.empty()) {
        condConsumer_.wait(locker); // 队列空了，需要等待
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();     // 唤醒生产者
    return true;
}

template <typename T>
bool BlockQueue<T>::pop(T& item, int timeout) {
    unique_lock<mutex> locker(mtx_);
    while(deq_.empty()) {
        // 队列空了，需要等待
        if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout))
            == std::cv_status::timeout) {
            return false;
        }
        if (isClose_) return false;
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();     // 唤醒生产者
    return true;
}

template <typename T>
T BlockQueue<T>::front() {
    unique_lock<mutex> locker(mtx_);
    return deq_.front();
}

template <typename T>
T BlockQueue<T>::back() {
    unique_lock<mutex> locker(mtx_);
    return deq_.back();
}

template <typename T>
size_t BlockQueue<T>::capacity() {
    unique_lock<mutex> locker(mtx_);
    return capacity_;
}

template <typename T>
size_t BlockQueue<T>::size() {
    unique_lock<mutex> locker(mtx_);
    return deq_.size();
}

template<typename T>
void BlockQueue<T>::flush() {
    condConsumer_.notify_one();
}


#endif