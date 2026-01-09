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

struct PairTask {
    size_t first;      // 延迟时间（秒）
    Task second;       // 任务函数
    PairTask(size_t delay = 0, Task task = nullptr) 
        : first(delay), second(task) {}
    // 用于priority_queue排序
    bool operator>(const PairTask& other) const {
        return first > other.first;
    }
};

class SyncQueue {
private:
    priority_queue<PairTask, vector<PairTask>, greater<PairTask>> tasks_;//最早的在堆顶
    mutable mutex mutex_;
    condition_variable not_empty_;
    condition_variable not_full_;
    size_t max_size_;
    bool need_stop_;
    
    bool isFull() const {
        return tasks_.size() >= max_size_;
    }
    bool isEmpty() const {
        return tasks_.empty();
    }
    
public:
    SyncQueue(size_t max_size = 100) 
        : max_size_(max_size), need_stop_(false) {}
    
    // 放入任务
    bool put(const PairTask& task) {
        unique_lock<mutex> lock(mutex_);   
        // 等待队列不满
        not_full_.wait(lock, [this]() { 
            return need_stop_ || !isFull(); 
        });
        if (need_stop_) return false;
        tasks_.push(task);
        not_empty_.notify_one();
        return true;
    }
    // 取出任务（会等待到任务时间）
    bool take(PairTask& task) {
        unique_lock<mutex> lock(mutex_);   
        // 等待队列不空
        while (!need_stop_ && isEmpty()) {
            not_empty_.wait(lock);
        }
        if (need_stop_ && isEmpty()) return false;
        // 取出堆顶任务（时间最早的任务）
        task = tasks_.top();
        tasks_.pop();   
        // 等待到任务执行时间
        if (task.first > 0) {
            not_empty_.wait_for(lock, chrono::seconds(task.first));
        }
        not_full_.notify_one();
        return true;
    }
    
    // 停止队列
    void stop() {
        {
            lock_guard<mutex> lock(mutex_);
            need_stop_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    
    // 状态查询
    size_t size() const {
        lock_guard<mutex> lock(mutex_);
        return tasks_.size();
    }
    bool empty() const {
        lock_guard<mutex> lock(mutex_);
        return tasks_.empty();
    }
    bool stopped() const {
        lock_guard<mutex> lock(mutex_);
        return need_stop_;
    }
};

#endif