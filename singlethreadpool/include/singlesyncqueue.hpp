#ifndef SINGLESYNCQUEUE_HPP
#define SINGLESYNCQUEUE_HPP
#include<iostream>
#include<mutex>
#include<thread>
#include<condition_variable>
#include<vector>
#include<queue>
#include<chrono>
#include<atomic>
using namespace std;
template<typename T>
class SingleSyncQueue{
private:
    queue<T>tasks;
    condition_variable notempty;
    condition_variable notfull;
    mutable mutex mtx;
    bool isstop;
    
public:
    SingleSyncQueue():isstop(false){}
    //禁止拷贝
    SingleSyncQueue(const SingleSyncQueue&)=delete;
    SingleSyncQueue& operator=(const SingleSyncQueue&)=delete;

    void put(const T&task){
        lock_guard<mutex>lock(mtx);
        if(isstop){
            throw runtime_error("队列已停止，不能添加任务");
        }
        tasks.push(task);
        notempty.notify_one();
    }
    
    void put(T&&task){
        lock_guard<mutex>lock(mtx);
        if(isstop){
            throw runtime_error("队列已停止，不能添加任务");
        }
        tasks.push(task);
        notempty.notify_one();
    }

    bool take(T& task){//取出任务(阻塞)
        unique_lock<mutex>lock(mtx);
        notempty.wait(lock,[this](){return isstop||!tasks.empty();});
        if(isstop&&tasks.empty()){
            return false;
        }
        task=move(tasks.front());
        tasks.pop();
        return true;
    }
    ~SingleSyncQueue(){
        stop();
    }
    void stop(){
        lock_guard<mutex>lock(mtx);
        isstop=true;
        notempty.notify_all();
        notfull.notify_all();
    }
    bool empty()const{
        lock_guard<mutex>lock(mtx);
        return tasks.empty();
    }
    size_t getsize()const{
        lock_guard<mutex>lock(mtx);
        return tasks.size();
    }
    bool stopped()const{
        lock_guard<mutex>lock(mtx);
        return isstop;
    }
};
#endif
