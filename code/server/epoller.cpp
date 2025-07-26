#include "epoller.h"

Epoller::Epoller(int maxEvent):epollFd_(epoll_create1(0)), events_(maxEvent){
    assert(epollFd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() {
    close(epollFd_);
}

bool Epoller::AddFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, 0);
}

// 返回事件数量
int Epoller::Wait(int timeoutMs) {
    // 第二个参数struct epoll_event* events，是输出参数（存放检测到事件的epoll_event数组）
    // &events_[0]      是struct epoll_event*      √
    // events_          它是一个容器，不是裸指针；   ×  
    // events_.data()   (更现代的写法)获取 std::vector 底层数组的首地址指针   √
    return epoll_wait(epollFd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
}

// 获取事件的fd
int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

// 获取事件属性
uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}