//实现简易fixedpool
#include<iostream>
#include<queue>
#include<vector>
#include<thread>
#include<mutex>
#include<functional>
#include<condition_variable>
#include <chrono>
using namespace std;
class fixedthreadpool{
private:
    vector<thread>workers;
    queue<function<void()>>tasks;//任务队列  存放要执行的函数
    //同步工具
    mutex queue_mutex;
    condition_variable condition;
    bool stop=false;
    void worker_function(){
        while(true){
            function<void()>task;
            //等待任务
            unique_lock<mutex>lock(queue_mutex);
            condition.wait(lock,[this](){return stop||!tasks.empty();});//有任务或要停止
            if(stop&&tasks.empty()){//要停止且没任务 就退出
                return;
            }//取任务
            task=tasks.front();
            tasks.pop();
            task();
        }
    }
public:
    fixedthreadpool(int num_threads){
        cout<<"创建线程池 线程数："<<num_threads<<endl;
        for(int i=0;i<num_threads;i++){
            workers.push_back(thread([this](){ this->worker_function(); }));
        }
    }
    void add_task(function<void()>task){
        lock_guard<mutex>lock(queue_mutex);
        tasks.push(task);
        condition.notify_one();
    }
    void shutdown(){
        lock_guard<mutex>lock(queue_mutex);
        stop=true;
        condition.notify_all();
        for(auto& t:workers){
            if(t.joinable()){
                t.join();
            }
        }
        cout<<"thread pool已经停止"<<endl;
    }
    ~fixedthreadpool(){
        shutdown();
    }
};
//全局锁
mutex print_mtx;
void printMessage(int id) {
    lock_guard<mutex> lock(print_mtx);
    cout << "任务 " << id << " 被执行\n";
}
int main() {
    fixedthreadpool pool(2);
    cout << "\n添加5个任务:\n";
    for (int i = 1; i <= 5; i++) {
        pool.add_task([i]() { printMessage(i);});
    }
    cout << "\n等待2秒让任务执行...\n";
    this_thread::sleep_for(chrono::seconds(2));
    cout << "\n再添加3个任务:\n";
    for (int i = 6; i <= 8; i++) {
        pool.add_task([i]() { printMessage(i); });
    }
    this_thread::sleep_for(chrono::seconds(3));
    cout << "\n 结束 \n";
    // pool会在main结束时自动调用stop()
    return 0;
}