#ifndef WORKSTEALSYNCQUEUE_HPP
#define WORKSTEALSYNCQUEUE_HPP
#include<iostream>
using namespace std;
#include<mutex>
#include<queue>
#include<vector>
#include<condition_variable>
#include<atomic>
#include<thread>
#include<functional>
#include<list>
template<typename T>
class WorkStealSyncQueue{    
private:
    deque<T>tasks;
    mutable mutex mtx;
    condition_variable notempty;
    condition_variable notfull;
  
public:
    WorkStealSyncQueue()=default;
    WorkStealSyncQueue(const WorkStealSyncQueue&)=delete;
    WorkStealSyncQueue& operator=(const WorkStealSyncQueue&)=delete;
    //本地线程用
    void pushback(T task){//从尾部放入任务
        lock_guard<mutex>lock(mtx);
        tasks.push_back(move(task));
        notempty.notify_one();
    }
    bool popback(T&task){//从尾部取出任务
        lock_guard<mutex>lock(mtx);
        if(tasks.empty()){
            return false;
        }
        task=move(tasks.back());
        tasks.pop_back();
        return true;
    }
    //窃取操作(其他线程用)
    bool stealfront(T&task){
        lock_guard<mutex>lock(mtx);
        if(tasks.empty()){
            return false;
        }
        task=move(tasks.front());
        tasks.pop_front();
        return true;
    }
    
    bool trystealfront(T&task){//尝试从头部窃取 非阻塞
        lock_guard<mutex>lock(mtx);
        if(tasks.empty()){
            return false;
        }
        task=move(tasks.front());
        tasks.pop_front();
        return true;
    }

    bool empty()const{
        lock_guard<mutex>lock(mtx);
        return tasks.empty();
    }
    size_t getsize()const{
        lock_guard<mutex>lock(mtx);
        return tasks.size();
    }
    void waitfortask(){
        unique_lock<mutex>lock(mtx);
        notempty.wait(lock,[this](){return !tasks.empty();});
    }
    void stop(){
        // 工作窃取队列不需要停止，但为了规范加一个
        notempty.notify_all();
        notfull.notify_all();
    }
};
#endif