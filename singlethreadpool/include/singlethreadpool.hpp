#ifndef SINGLETHREADPOOL_HPP
#define SINGLETHREADPOOL_HPP
#include<iostream>
#include<mutex>
#include<thread>
#include<condition_variable>
#include<vector>
#include<queue>
#include<chrono>
#include<future>
#include<atomic>
#include<functional>
#include"singlesyncqueue.hpp"
using namespace std;
class singlethreadpool{
public:
    using task=function<void()>;
private:
    thread worker;
    SingleSyncQueue<task> queue_;
    atomic<bool>running;
    once_flag stop_flag;

    void workerthread(){
        while(running){
            task t;
            if(queue_.take(t)){
                try{t();}
                catch(const exception&e){
                    cerr<<"任务执行异常"<<e.what()<<endl;
                }
            }
        }
    }
    void stopworker(){
        running=false;
        queue_.stop();
        if(worker.joinable())worker.join();
    }

public:
    singlethreadpool():running(true){
        worker=thread(&singlethreadpool::workerthread,this);
        cout<<"pool启动"<<endl;
    }
    ~singlethreadpool(){
        stop();
    }
    singlethreadpool(const singlethreadpool&)=delete;
    singlethreadpool& operator=(const singlethreadpool&)=delete;

    void stop(){
        call_once(stop_flag,[this](){stopworker();cout<<"pool停止"<<endl;});
    }

    template<typename Func,typename...Args>
    void execute(Func&&func,Args&&...args){
        auto task=bind(forward<Func>(func),forward<Args>(args)...);//：绑定函数和参数，forward保持参数的左值/右值属性
        queue_.put(task);
    }
    template<typename Func,typename...Args>
    auto submit(Func&&func,Args&&...args)->future<decltype(func(args...))>{
        using returntype=decltype(func(args...));
        auto task=make_shared<packaged_task<returntype()>>(bind(forward<Func>(func),forward<Args>(args)...));
        future<returntype>result=task->get_future();
        queue_.put([task](){(*task)();});
        return result;
    };

    
    size_t queuesize()const{
        return queue_.getsize();
    }
    bool queueempty()const{
        return queue_.empty();
    }
    bool isrunning()const{
        return running;
    }
    thread::id getthreadid()const{
        return worker.get_id();
    }
};
#endif
