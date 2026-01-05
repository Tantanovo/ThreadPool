//同步队列
#ifndef SYNCQUEUE_HPP
#define SYNCQUEUE_HPP
#include<iostream>
#include<queue>
#include<list>
#include<vector>
#include<thread>
#include<mutex>
#include<functional>
#include<condition_variable>
#include <chrono>
using namespace std;
template<typename T>
class syncqueue{
private:
    list<T>m_queue;
    mutable mutex m_mutex;
    condition_variable m_notempty;//队列非空（通知取数据）
    condition_variable m_notfull;//非满（通知存数据）
    int m_maxsize;  //队列最大容量
    bool m_needstop;

    bool isfull()const{
        return m_queue.size()>=m_maxsize;
    }
    bool isempty()const{
        return m_queue.empty();
    }
    template<typename Y>
    void add(Y&& task){
        unique_lock<mutex>locker(m_mutex);
        m_notfull.wait(locker,[this]{return m_needstop||!isfull();});
        if(m_needstop)return;

        m_queue.push_back(forward<Y>(task));//完美转发插入任务
        m_notempty.notify_one();//通知等待的线程
    }
public:
    syncqueue(int maxsize=100):m_maxsize(maxsize),m_needstop(false){}
    ~syncqueue(){
        stop();
    }
    void put(const T&task){//存入任务
        add(task);
    }
    void put(T&&task){//右值版本
        add(forward<T>(task));
    }
    void take(list<T>&list){//一次性全取出
        unique_lock<mutex>locker(m_mutex);
        m_notempty.wait(locker,[this]{return m_needstop||!isempty();});
        if(m_needstop)return;
        list=move(m_queue);
        m_notfull.notify_one();
    }
    void take(T&task){//取出头部一个
        unique_lock<mutex>locker(m_mutex);
        m_notempty.wait(locker,[this]{return m_needstop||!isempty();});
        if(m_needstop)return;

        task=m_queue.front();
        m_queue.pop_front();
        m_notfull.notify_one();
    }
    void stop(){
        lock_guard<mutex>locker(m_mutex);
        m_needstop=true;
        m_notempty.notify_all();
        m_notfull.notify_all();
    }
    bool empty()const{
        lock_guard<mutex>locker(m_mutex);
        return m_queue.empty();
    }
    bool full()const{
        lock_guard<mutex>locker(m_mutex);
        return m_queue.size()>=m_maxsize;
    }
    size_t size()const{
        lock_guard<mutex>locker(m_mutex);
        return m_queue.size();
    }
    int maxsize()const{
        return m_maxsize;
    }
};

#endif