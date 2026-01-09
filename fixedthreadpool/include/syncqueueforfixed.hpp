//同步队列
#ifndef SYNCQUEUEFORFIXED_HPP
#define SYNCQUEUEFORFIXED_HPP
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
//拒绝策略
enum class RejectPolicy {
    abort,           // 直接抛异常（默认）
    caller_runs,     // 调用者线程执行
    discard,         // 静默丢弃
    discard_oldest   // 丢弃最老任务
};
template<typename T>
class FixedSyncQueue{
private:
    list<T> tasks;
    mutable mutex mtx;
    condition_variable notfull;//通知存
    condition_variable notempty;//通知取
    int max_size;
    bool isstop;
    bool isfull()const{
        return tasks.size()>=max_size;
    }
    bool isempty()const{
        return tasks.empty();
    }
    template<typename Y>
    bool add(Y&&task,RejectPolicy policy=RejectPolicy::abort){
        unique_lock<mutex>locker(mtx);
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
    FixedSyncQueue(int size=100):max_size(size),isstop(false){}
    ~FixedSyncQueue(){
        stop();
    }
    // put: 放入任务
    void put(T&&task){
        add(forward(task));
    }
    // take: 取出所有任务
    void take(list<T>&list){//全部取出
        unique_lock<mutex>locker(mtx);
        notempty.wait(locker,[this]{return isstop||!isfull();});
        if(isstop)return;
        list=move(tasks);
        notfull.notify_one();
    }
    // take: 取出单个任务
    void take(T&task){
        unique_lock<mutex>locker(mtx);
        notempty.wait(locker,[this]{return isstop||!isfull();});
        if(isstop)return ;
        task=tasks.front();
        tasks.pop_front();
        notfull.notify_one();
    }
    void stop(){
        lock_guard<mutex>locker(mtx);
        isstop=true;
        notempty.notify_all();
        notfull.notify_all();
    }
    bool empty()const{
        lock_guard<mutex>locker(mtx);
        return tasks.empty();
    }
    bool full()const{
        lock_guard<mutex>locker(mtx);
        return tasks.size()>=maxsize;
    }
    size_t getsize()const{
        lock_guard<mutex>locker(mtx);
        return tasks.size();
    }
    int maxsize()const{
        return max_size;
    }
};
#endif