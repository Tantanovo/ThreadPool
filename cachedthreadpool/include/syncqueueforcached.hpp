#ifndef SYNCQUEUEFORCACHED.HPP
#define SYNCQUEUEFORCACHED.HPP
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
class syncqueue{
private:
    queue<T>tasks;
    condition_variable noempty;
    mutable mutex m_mtx;
    bool isstop;
    size_t maxsize;


public:
    syncqueue(int maxsize=100):maxsize(100),isstop(false){}
    bool put(const T&task){//放任务
        lock_guard<mutex>lock(m_mtx);
        if(isstop)return false;
        tasks.push(task);
        noempty.notify_one();
        return true;
    }
    bool take(T&task){
        unique_lock<mutex>lock(m_mtx);
        noempty.wait(lock,[this](){return isstop||!tasks.empty();});
        if(isstop&&tasks.empty())return false;
        task=tasks.front();
        tasks.pop();
        return true;
    }
    bool trytake(T&task){
        lock_guard<mutex>lock(m_mtx);
        if(tasks.empty())return false;
        task=tasks.front();
        tasks.pop();
        return true;
    }
    bool notaskforseconds(int seconds){//检查队列是否长时间无任务
        unique_lock<mutex>lock(m_mtx);
        return noempty.wait_for(lock,chrono::seconds(seconds))==cv_status::timeout;
    }
    ~syncqueue(){
        stop();
    }
    void stop(){
        lock_guard<mutex>lock(m_mtx);
        isstop=true;
        noempty.notify_all();
    }
    size_t size()const{
        lock_guard<mutex>lock(m_mtx);
        return tasks.size();
    }
    bool empty()const{
        lock_guard<mutex>lock(m_mtx);
        return tasks.empty();
    }
    bool Isstop()const{
        lock_guard<mutex>lock(m_mtx);
        return isstop;
    }
};


#endif