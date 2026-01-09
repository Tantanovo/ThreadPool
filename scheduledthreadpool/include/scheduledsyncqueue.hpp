#ifndef SYNCQUEUE5_HPP
#define SYNCQUEUE5_HPP
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <vector>
#include <iostream>
using namespace std;

using Task = function<void()>;

struct scheduledtask {
    Task task;
    chrono::steady_clock::time_point exec_time;
    int interval_seconds;
    // 用于priority_queue排序
    bool operator>(const scheduledtask& other) const {
        return exec_time > other.exec_time;
    }
};

class ScheduledSyncQueue {
private:
    priority_queue<scheduledtask, vector<scheduledtask>, greater<scheduledtask>> tasks;//最早的在堆顶
    mutable mutex mtx;
    condition_variable notempty;
    condition_variable notfull;
    size_t maxsize;
    bool isstop;
    
    bool isfull() const {
        return tasks.size() >= maxsize;
    }
    bool isempty() const {
        return tasks.empty();
    }
    
public:
    ScheduledSyncQueue(size_t max_size = 100) 
        : maxsize(max_size), isstop(false) {}
    
    // put: 放入任务
    bool put(const scheduledtask& task) {
        unique_lock<mutex> lock(mtx);   
        // 等待队列不满
        notfull.wait(lock, [this]() { 
            return isstop || !isfull(); 
        });
        if (isstop) return false;
        tasks.push(task);
        notempty.notify_one();
        return true;
    }
    // take: 取出任务
    bool take(scheduledtask& task) {
        unique_lock<mutex> lock(mtx);   
        // 等待队列不空
        while (!isstop && isempty()) {
            notempty.wait(lock);
        }
        if (isstop && isempty()) return false;
        // 取出堆顶任务（时间最早的任务）
        task = tasks.top();
        tasks.pop();   
        // 等待到任务执行时间
        if (task.exec_time > chrono::steady_clock::now()) {
            auto delay = task.exec_time - chrono::steady_clock::now();
            notempty.wait_for(lock, delay);
        }
        notfull.notify_one();
        return true;
    }
    
    // 停止队列
    void stop() {
        {
            lock_guard<mutex> lock(mtx);
            isstop = true;
        }
        notempty.notify_all();
        notfull.notify_all();
    }
    
    // 状态查询
    size_t getsize() const {
        lock_guard<mutex> lock(mtx);
        return tasks.size();
    }
    bool empty() const {
        lock_guard<mutex> lock(mtx);
        return tasks.empty();
    }
    bool stopped() const {
        lock_guard<mutex> lock(mtx);
        return isstop;
    }
};

#endif