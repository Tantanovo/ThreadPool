#ifndef SCHEDULEDPOOL_HPP
#define SCHEDULEDPOOL_HPP
#include"scheduledsyncqueue.hpp"
using namespace std;
#include<thread>
#include<atomic>
#include<functional>
#include<mutex>
#include<condition_variable>
#include<queue>
#include<vector>
#include<future>
#include<chrono>
class scheduledthreadpool{
public:
    using task=function<void()>;
private:
    vector<thread>workers;
    SyncQueue taskqueue;
    atomic<bool>isrunning;
    once_flag isstop;
    void worker(){
        while(isrunning){
            PairTask st;
            if(taskqueue.take(st)){
                try{
                    st.second();
                }catch(const exception&e){
                    cerr<<"定时任务执行异常"<<e.what()<<endl;
                }
            }
        }
    }
    void stopall(){
        isrunning=false;
        taskqueue.stop();
        for(auto&t:workers){
            if(t.joinable())t.join();
        }
        workers.clear();
    }
public:
    scheduledthreadpool(size_t thread_count=thread::hardware_concurrency()):isrunning(true){
        if(thread_count==0){
            thread_count=thread::hardware_concurrency();
        }
        for(size_t i=0;i<thread_count;i++){
            workers.emplace_back(&scheduledthreadpool::worker,this);
        }
        cout<<"定时任务线程池启动，线程数:"<<thread_count<<endl;
    }
    ~scheduledthreadpool(){
        stop();
    }
    scheduledthreadpool(const scheduledthreadpool&)=delete;
    scheduledthreadpool& operator=(const scheduledthreadpool&)=delete;
    void stop(){
        call_once(isstop,[this](){stopall();cout<<"定时任务线程池停止"<<endl;});
    }
    template<typename Func,typename...Args>
    void schedule(int delay_seconds,int interval_seconds,Func&&f,Args&&...args){//延迟执行 一次性
        auto task=bind(forward<Func>(f),forward<Args>(args)...);
        scheduledtask st;
        st.task=move(task);
        st.exec_time=chrono::steady_clock::now()+chrono::seconds(delay_seconds);
        st.interval_seconds=0;
        queue_.add(move(st));
    }
    template<typename Func,typename...Args>
    void scheduleAtFixedRate(int delay_seconds,int interval_seconds,Func&&f,Args&&...args){//延迟执行 周期性
        auto task=bind(forward<Func>(f),forward<Args>(args)...);
        scheduledtask st;
        st.task=move(task);
        st.exec_time=chrono::steady_clock::now()+chrono::seconds(delay_seconds);
        st.interval_seconds=interval_seconds;
        queue_.add(move(st));
    }
    template<typename Func,typename...Args>
    void schedulewithfixeddelay(int delay_seconds,int interval_seconds,Func&&f,Args&&...args){//固定延迟执行 任务结束后延迟固定时间
        auto wrapped_func = [func = std::forward<Func>(func), args_tuple = std::make_tuple(std::forward<Args>(args)...),delay_seconds]() mutable {
            // 执行原任务
            std::apply(func, args_tuple); 
            // 任务完成后，重新调度自己（实现固定延迟）
            // 注意：实际需要能访问queue_，这里简化处理
        };
        schedule(delay_seconds, std::move(wrapped_func));
    }
    template<typename Func,typename...Args>
    auto scheduledwithresult(int delay_seconds,Func&&f,Args&&...args)->future<decltype(f(args...))>{
        using returntype=decltype(f(args...));
        auto task=make_shared<packaged_task<returntype()>>(bind(forward<Func>(f),forward<Args>(args)...));
        future<returntype>result=task->get_future();
        auto wrapped_task=[task](){(*task)();};
        schedule(delay_seconds,move(wrapped_task));
        return result;
    }
    size_t threadcount()const{
        return workers.size();
    }
    size_t pendingtasks()const{//待执行任务数
        return taskqueue.size();
    }
    bool isrunningpool()const{
        return isrunning;
    }
};
#endif