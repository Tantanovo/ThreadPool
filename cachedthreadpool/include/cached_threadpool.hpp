#ifndef CACHED_THREADPOOL_HPP
#define CACHED_THREADPOOL_HPP
#include<iostream>
#include<atomic>
#include<mutex>
#include<queue>
#include<vector>
#include<functional>
#include<condition_variable>
#include<unordered_map>
#include<thread>
#include<future>
#include"syncqueueforcached.hpp"
using namespace std;

class cachedthreadpool{
public:
    using task=function<void()>;
private:
    vector<thread>workers;
    atomic<int>idlecount;//空闲线程数
    atomic<int>totalcount;

    const int corethreadsize;//核心线程数（最少保留）
    const int maxthreadsize;
    const int keepalivetime;//空闲存活时间

    CachedSyncQueue<task>queue_;
    atomic<bool>running;
    once_flag stop_flag;
    void createnewthread(){
        if(totalcount>=maxthreadsize)return ;
        workers.emplace_back(&cachedthreadpool::worker,this);
        totalcount++;
        idlecount++;
    }
    void stopallthreads(){
        running=false;
        queue_.stop();
        for(auto& thread:workers){
            if(thread.joinable())thread.join();
        }
        workers.clear();
        totalcount=0;
        idlecount=0;
    }
    void worker(){
        auto lastworktime=chrono::steady_clock::now();
        while(running){
            task t;
            bool hastask=false;
            if(queue_.trytake(t)){
                hastask=true;
            }
            else{
                idlecount++;
                bool timeout=queue_.notaskforseconds(keepalivetime);
                idlecount--;
                if(!timeout&&queue_.take(t)){
                    hastask=true;
                }
            }
            if(hastask){
                t();
                lastworktime=chrono::steady_clock::now();
            }
            else{
                auto now=chrono::steady_clock::now();
                auto idleduration=chrono::duration_cast<chrono::seconds>(now-lastworktime);
                if( idleduration.count() >= keepalivetime && totalcount > corethreadsize){
                    totalcount--;
                    return;
                }
            }
        }
    }

public:
    cachedthreadpool(int coresize=4,int maxsize=20,int keepaliveseconds=60,int queuesize=100)
                                                    :corethreadsize(coresize),
                                                    maxthreadsize(maxsize),
                                                    keepalivetime(keepaliveseconds),
                                                    queue_(queuesize),
                                                    idlecount(0),
                                                    totalcount(0),
                                                    running(true){
        if(coresize<=0)coresize=1;
        if(maxsize<coresize)maxsize=coresize*2;
        for(int i=0;i<corethreadsize;i++){
            createnewthread();
        }
        cout<< "[CachedThreadPool] 启动: " 
            << "核心" << corethreadsize << "线程, "
            << "最大" << maxthreadsize << "线程, "
            << "空闲超时" << keepalivetime << "秒" 
            << endl;
    }
    //typename... args：定义一个类型参数包，args可以代表 0 个、1 个或多个不同的类型。
    //args&&...a：定义一个值参数包，a可以代表 0 个,1 个或多个对应类型的参数（这里用&&是万能引用,支持左值/右值传递）
    template<typename func,typename...args>
    void execute(func&&f,args&&...a){//提交任务 无返回值
        if(!running){
            throw runtime_error("线程池已停止");
        }
        auto task=bind(forward<func>(f),forward<args>(a)...);//：绑定函数和参数，forward保持参数的左值/右值属性
        if(idlecount==0&&totalcount<maxthreadsize){
            createnewthread();
        }
        queue_.put(task);
    }
    template<typename func,typename...args>//提交任务 有返回值
    auto submit(func&&f,args&&...a)->future<decltype(func(a...))>{//// 返回值：future<返回值类型>，用->指定返回值（尾置返回类型，因为要推导返回值）
        using returntype=decltype(func(a...));// 推导任务函数的返回值类型，给类型起别名returntype
        // 1. packaged_task：把函数包装成“可异步执行的任务”
        // 模板参数returntype()表示“无参数、返回returntype的可调用对象”
        // bind把函数f和参数a...绑定，变成无参数的可调用对象
        packaged_task<returntype()>task(bind(forward<func>(f),forward<args>(a)...));
        future<returntype>result=task.get_future();// 2. 获取这个任务的future对象（提货单），后续用它拿返回值
        if(idlecount==0&&totalcount<maxthreadsize){// 3. 线程池逻辑：没空闲线程且没到上限，就创建新线程
            createnewthread();
        }
        // 4. 把任务放入队列：用lambda封装，move转移task所有权（避免拷贝）
        //mutable：允许lambda内部修改捕获的task（因为task()是执行任务，需要修改状态）
        queue_.put([task = move(task)]() mutable {task();});// // 执行任务，结果会自动存到对应的future里
        return result;
    }
    //packaged_task 和 future 是一对 “搭档”
    // packaged_task：负责包装任务，把函数和 “结果存储” 绑定在一起。
    // future：负责获取结果，是packaged_task的 “结果出口”。
    // 关系：packaged_task执行时，会把返回值存起来，你通过future.get()就能拿到这个值（如果任务没执行完，get()会阻塞，直到拿到结果）。
    ~cachedthreadpool(){
        stop();
    }
    void stop(){
        call_once(stop_flag,[this]{stopallthreads();cout<<"pool停止"<<endl;});
    }
    int getidlecount()const{
        return idlecount;
    }
    int gettotalcount()const{
        return totalcount;
    }
    size_t queuesize()const{
        return queue_.getsize();
    }
    bool isrunning()const{
        return running;
    }

};

#endif