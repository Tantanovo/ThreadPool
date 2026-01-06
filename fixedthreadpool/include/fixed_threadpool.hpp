#ifndef FIXED_THREAD_POOL_HPP
#define FIXED_THREAD_POOL_HPP
#include<iostream>
#include"syncqueueforfixed.hpp"
#include<atomic>
#include <chrono>
using namespace std;
//拒绝策略
enum class RejectPolicy {
    abort,           // 直接抛异常（默认）
    caller_runs,     // 调用者线程执行
    discard,         // 静默丢弃
    discard_oldest   // 丢弃最老任务
};

class fixedthreadpool{
public:
    using task=function<void()>;
private:
    vector<thread>m_threads;
    syncqueue<task>m_queue;
    atomic<bool>m_running;//原子变量 无需加锁
    once_flag m_stopflag;//只允许改变一次

    RejectPolicy m_policy;

    void runthread(){
        while(m_running){
            task t;
            m_queue.take(t);
            if(t&&m_running)t();
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
    fixedthreadpool(int numthreads=thread::hardware_concurrency()):m_running(true),m_policy(m_policy){//当前系统硬件并发数 cpu核心数
        if(numthreads<=0)numthreads=thread::hardware_concurrency();
        for(int i=0;i<numthreads;i++){
            m_threads.emplace_back(&fixedthreadpool::runthread,this);
        }
    }
    ~fixedthreadpool(){
        stop();
    }
    void stop(){
        call_once(m_stopflag,[this]{stopthreads();});//保证只调用一次
    }
    void setrejectpolicy(RejectPolicy policy){
        m_policy=policy;
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