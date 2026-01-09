#ifndef WORKSTEALTHREADPOOL_HPP  
#define WORKSTEALTHREADPOOL_HPP
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <future>
#include <memory>
#include <random>  
#include "workstealsyncqueue.hpp"  

class workstealpool {
public:
    using task = function<void()>;
    
private:
    vector<unique_ptr<syncqueue<task>>> threadqueues;  // 每个线程一个队列
    vector<thread> workers;
    atomic<bool> running;
    once_flag isstop;
    static thread_local size_t thread_index;  // 每个线程记住自己的索引

    void workermain(size_t index) {
        thread_index = index;
        while (running) {
            task t;
            // 1. 从自己的队列取任务
            if (threadqueues[index]->popback(t)) {
                t();
                continue;
            }
            // 2. 自己队列空，尝试窃取
            bool stolen = false;
            size_t queue_count = threadqueues.size();
            // 简单策略：按顺序尝试偷取（从下一个线程开始）
            for (size_t offset = 1; offset < queue_count; offset++) {
                size_t target = (index + offset) % queue_count;
                if (threadqueues[target]->stealfront(t)) {
                    stolen = true;
                    cout << "线程" << index << " 窃取了线程" << target << "的任务" << endl;
                    break;
                }
            }
            // 3. 如果窃取成功，执行任务
            if (stolen) {
                t();
                continue;
            }
            // 4. 所有队列都空，等待一会
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }
    void stopall() {
        running = false;   
        for (auto& thread : workers) {
            if (thread.joinable()) thread.join();
        }
        workers.clear();
        threadqueues.clear();
    }

public:
    workstealpool(size_t thread_count = thread::hardware_concurrency()) 
        : running(true) {
        if (thread_count == 0) {
            thread_count = thread::hardware_concurrency();
        }
        // 创建队列
        threadqueues.reserve(thread_count);
        for (size_t i = 0; i < thread_count; i++) {
            threadqueues.emplace_back(new syncqueue<task>());
        }
        // 创建工作线程
        workers.reserve(thread_count);
        for (size_t i = 0; i < thread_count; i++) {
            workers.emplace_back(&workstealpool::workermain, this, i);
        }
        cout << "工作窃取线程池启动，有 " << thread_count << " 个线程" << endl;
    }
    workstealpool(const workstealpool&) = delete;
    workstealpool& operator=(const workstealpool&) = delete;
    ~workstealpool() {
        stop();
    }
    void stop() {
        call_once(isstop, [this]() { 
            stopall(); 
            cout << "线程池已停止" << endl; 
        });
    }

    // 提交任务（随机选择队列）
    template<typename Func, typename... Args>
    void execute(Func&& func, Args&&... args) {
        if (!running) {
            throw runtime_error("线程池已停止");
        }        
        // 包装任务
        auto t = bind(forward<Func>(func), forward<Args>(args)...);
        // 随机选择一个队列（简单的随机）
        static random_device rd;
        static mt19937 gen(rd());
        uniform_int_distribution<size_t> dist(0, threadqueues.size() - 1);
        size_t index = dist(gen);
        threadqueues[index]->pushback(t);
    }
    // 提交任务到指定线程的队列
    template<typename Func, typename... Args>
    void executeToThread(size_t thread_index, Func&& func, Args&&... args) {
        if (!running) {
            throw runtime_error("线程池已停止");
        }
        if (thread_index >= threadqueues.size()) {
            throw out_of_range("线程索引超出范围");
        }
        auto t = bind(forward<Func>(func), forward<Args>(args)...);
        threadqueues[thread_index]->pushback(t);
    }
    // 提交任务（有返回值）
    template<typename Func, typename... Args>
    auto submit(Func&& func, Args&&... args) 
        -> future<decltype(func(args...))> {   
        using ReturnType = decltype(func(args...));
        // 创建packaged_task
        auto task = make_shared<packaged_task<ReturnType()>>(
            bind(forward<Func>(func), forward<Args>(args)...)
        );
        // 获取future
        future<ReturnType> result = task->get_future();
        // 随机选择队列
        static random_device rd;
        static mt19937 gen(rd());
        uniform_int_distribution<size_t> dist(0, threadqueues.size() - 1);
        size_t index = dist(gen);
        // 包装成void()函数
        threadqueues[index]->pushback([task]() {
            (*task)();
        });   
        return result;
    }
    // 获取线程数量
    size_t threadCount() const {
        return workers.size();
    }
    // 获取各队列大小
    vector<size_t> queueSizes() const {
        vector<size_t> sizes;
        sizes.reserve(threadqueues.size());
        for (const auto& queue : threadqueues) {
            sizes.push_back(queue->getsize());
        }
        return sizes;
    }
    
    // 获取总任务数量
    size_t totalTaskCount() const {
        size_t total = 0;    
        for (const auto& queue : threadqueues) {
            total += queue->getsize();
        }
        return total;
    }
    void printStatus() const {
        auto sizes = queueSizes();
        cout << "线程池状态：" << endl;      
        for (size_t i = 0; i < sizes.size(); i++) {
            cout << "  线程" << i << "队列: " << sizes[i] << " 个任务" << endl;
        }
    }
};
// 初始化线程局部变量
thread_local size_t workstealpool::thread_index = 0;

#endif