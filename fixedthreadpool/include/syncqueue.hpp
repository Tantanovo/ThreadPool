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
#include"fixed_threadpool.hpp"
using namespace std;
template<typename T>
class syncqueue{
private:
    list<T> tasks;
    mutable mutex t_mtx;
    condition_variable notfull;//通知存
    condition_variable notempty;//通知取
    int tmaxsize;
    bool isstop;
    bool isfull()const{
        return tasks.size()>=tmaxsize;
    }
    bool isempty()const{
        return tasks.empty();
    }
    template<typename Y>
    bool add(Y&&task,RejectPolicy policy=RejectPolicy::abort){
        unique_lock<mutex>locker(t_mtx);
        if(!notfull.wait_for(locker,chrono::microseconds(100),[this]{return isstop||!isfull();})){
            switch (policy){
            case RejectPolicy::abort:
                throw runtime_error("任务队列已满");
            case RejectPolicy::discard:
                return false;//静默丢弃
            case RejectPolicy::caller_runs://调用者线程执行
                locker.unlock();
                task();
                return true;
            case RejectPolicy::discard_oldest:
                if(!tasks.empty()){
                    tasks.pop_front();
                    tasks.push_back(forward<Y>(task));
                    notempty.notify_one();
                    return true;
                }
                return false;
            default:
                throw runtime_error("未知的拒绝策略");
            }
        }
        if(isstop)return false;
        tasks.push_back(forward<Y>(task));
        notempty.notify_one();
        return true;
    }
    // 基础版
    // void add(Y&&task){
    //     unique_lock<mutex>locker(t_mtx);
    //     notfull.wait(locker,[this]{return isstop||!isfull();});
    //     if(isstop)return;
    //     tasks.push_back(forward(task));
    //     notempty.notify_one();
    // }

    
public:
    syncqueue(int size=100):tmaxsize(size),isstop(false){}
    ~syncqueue(){
        stop();
    }
    void put(T&&task){
        add(forward(task));
    }
    void take(list<T>&list){//全部取出
        unique_lock<mutex>locker(t_mtx);
        notempty.wait(locker,[this]{return isstop||!isfull();});
        if(isstop)return;
        list=move(tasks);
        notfull.notify_one();
    }
    void take(T&task){
        unique_lock<mutex>locker(t_mtx);
        notempty.wait(locker,[this]{return isstop||!isfull();});
        if(isstop)return ;
        task=tasks.front();
        tasks.pop_front();
        notfull.notify_one();
    }
    void stop(){
        lock_guard<mutex>locker(t_mtx);
        isstop=true;
        notempty.notify_all();
        notfull.notify_all();
    }
    bool empty()const{
        lock_guard<mutex>locker(t_mtx);
        return tasks.empty();
    }
    bool full()const{
        lock_guard<mutex>locker(t_mtx);
        return tasks.size()>=tmaxsize;
    }
    size_t size()const{
        lock_guard<mutex>locker(t_mtx);
        return tasks.size();
    }
    int maxsize()const{
        return tmaxsize;
    }
};
#endif