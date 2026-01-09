#ifndef SYNCQUEUEFORCACHED_HPP
#define SYNCQUEUEFORCACHED_HPP
#include<iostream>
#include<mutex>
#include<atomic>
#include<thread>
#include<vector>
#include<queue>
#include<chrono>
#include<condition_variable>
#include<functional>
using namespace std;
template<typename T>
class CachedSyncQueue{
private:
    queue<T>tasks;
    condition_variable notempty;
    condition_variable notfull;
    mutable mutex mtx;
    bool isstop;
    size_t maxsize;


public:
public:
    CachedSyncQueue(int maxsize=100):maxsize(maxsize),isstop(false){}
    // put: 放入任务
    bool put(const T&task){//放任务
        lock_guard<mutex>lock(mtx);
        if(isstop)return false;
        tasks.push(task);
        notempty.notify_one();
        return true;
    }
    // take: 取出任务
    bool take(T&task){
        unique_lock<mutex>lock(mtx);
        notempty.wait(lock,[this](){return isstop||!tasks.empty();});
        if(isstop&&tasks.empty())return false;
        task=tasks.front();
        tasks.pop();
        return true;
    }
    // trytake: 尝试取出任务
    bool trytake(T&task){
        lock_guard<mutex>lock(mtx);
        if(tasks.empty())return false;
        task=tasks.front();
        tasks.pop();
        return true;
    }
    bool notaskforseconds(int seconds){//检查队列是否长时间无任务
        unique_lock<mutex>lock(mtx);
        return notempty.wait_for(lock,chrono::seconds(seconds))==cv_status::timeout;
    }
    ~CachedSyncQueue(){
        stop();
    }
    void stop(){
        lock_guard<mutex>lock(mtx);
        isstop=true;
        notempty.notify_all();
        notfull.notify_all();
    }
    size_t getsize()const{
        lock_guard<mutex>lock(mtx);
        return tasks.size();
    }
    bool empty()const{
        lock_guard<mutex>lock(mtx);
        return tasks.empty();
    }
    bool stopped()const{
        lock_guard<mutex>lock(mtx);
        return isstop;
    }
};


#endif