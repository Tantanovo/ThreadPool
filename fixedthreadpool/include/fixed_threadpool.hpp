#ifndef FIXED_THREAD_POOL_HPP
#define FIXED_THREAD_POOL_HPP
#include<iostream>
#include"syncqueueforfixed.hpp"
#include<atomic>
#include <chrono>
using namespace std;

class fixedthreadpool{
public:
    using task=function<void()>;
private:
    vector<thread>workers;
    FixedSyncQueue<task>queue_;
    atomic<bool>running;//原子变量 无需加锁
    once_flag stop_flag;//只允许改变一次

    RejectPolicy policy;

    void workerthread(){
        while(running){
            task t;
            queue_.take(t);
            if(t&&running)t();
        }
    }
    void stopworker(){
        queue_.stop();
        running=false;
        for(auto&thread : workers){
            if(thread.joinable())thread.join();
        }
        workers.clear();
    }
public:
    fixedthreadpool(int numthreads=thread::hardware_concurrency()):running(true),policy(policy){//当前系统硬件并发数 cpu核心数
        if(numthreads<=0)numthreads=thread::hardware_concurrency();
        for(int i=0;i<numthreads;i++){
            workers.emplace_back(&fixedthreadpool::workerthread,this);
        }
    }
    ~fixedthreadpool(){
        stop();
    }
    void stop(){
        call_once(stop_flag,[this]{stopworker();});//保证只调用一次
    }
    // setrejectpolicy: 设置拒绝策略
    void setrejectpolicy(RejectPolicy policy){
        this->policy=policy;
    }
    // execute: 执行任务
    template<typename Y>
    void execute(Y&&task){
        queue_.put(forward<Y>(task));
    }
    // threadcount: 获取线程数量
    size_t threadcount()const{
        return workers.size();
    }

};



#endif