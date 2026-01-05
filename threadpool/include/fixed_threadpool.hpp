#ifndef FIXED_THREAD_POOL_HPP
#define FIXED_THREAD_POOL_HPP
#include<iostream>
#include"syncqueue.hpp"
#include<atomic>
#include <chrono>
using namespace std;
class fixedthreadpool{
public:
    using task=function<void()>;
private:
    vector<thread>m_threads;
    syncqueue<task>m_queue;
    atomic<bool>m_running;//原子变量 无需加锁
    once_flag m_stopflag;//只允许改变一次

    void runthread(){
        while(m_running){
            task t;
            m_queue.take(t);
            if(t&&m_running)task();
        }
    }
    void stopthreads(){
        m_queue.stop();
        m_running=false;
        for(auto&thread : m_threads){
            if(thread.joinable())thread.join();
        }
        m_threads.clear();
    }
public:
    fixedthreadpool(int numthreads=thread::hardware_concurrency()):m_running(true){//当前系统硬件并发数 cpu核心数
        if(numthreads<=0)numthreads=thread::hardware_concurrency();
        for(int i=0;i<numthreads;i++){
            m_threads.emplace_back(&fixedthreadpool::runthread,this);
        }
    }
    ~fixedthreadpool(){
        stop();
    }
    void stop(){
        call_once(m_stopflag,[this]{stopthreads();});
    }
    template<typename Y>
    void addtask(Y&&task){
        m_queue.put(forward<Y>(task));
    }
    size_t threadcount()const{
        return m_threads.size();
    }

};



#endif