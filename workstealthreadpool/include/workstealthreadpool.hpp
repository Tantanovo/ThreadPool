#ifndef WORKSTEALTHREADPOOL.HPP
#define WORKSTEALTHREADPOOL.HPP
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
#include<memory>
#include<random>
#include<future>
#include"workstealsyncqueue.hpp"
class workstealpool{
public:
using task=function<void()>;
private:
    vector<unique_ptr<syncqueue<task>>>threadqueues;//每个线程一个队列
    vector<thread>workers;
    atomic<bool>running;
    once_flag isstop;
    static thread_local std::mt19937 random_generator;//随机数生成器 （用于随机选择偷取目标)
    static thread_local size_t thread_index;

    void workermain(size_t index){
        thread_index=index;
        while(running){
            task t;//从自己的队列取任务
            if(threadqueues[index]->popback(t)){
                t();
                continue;
            }//自己队列空 尝试窃取
            bool stolen=false;
            size_t queue_count=threadqueues.size();
            vector<size_t>steal_order(queue_count);
            for(size_t i=0;i<queue_count;i++){
                steal_order[i]=i;
            }
            shuffle(steal_order.begin(),steal_order.end(),random_generator);
            for(size_t i:steal_order){
                if(i==index)continue;//不偷自己
                if(threadqueues[i]->stealfront(t)){
                    stolen=true;
                    break;
                }
            }
            if(stolen){//如果窃取成功，执行任务
                t();
                continue;
            }
            this_thread::sleep_for(chrono::milliseconds(10));//所有队列都空，等待一会
        }
    }

    void stopall(){
        running=false;
        for(auto& thread:workers){
            if(thread.joinable())thread.join();
        }
        workers.clear();
        threadqueues.clear();
    }

public:
    workstealpool(size_t thread_count=thread::hardware_concurrency()):running(true){
        if(thread_count==0){
            thread_count=thread::hardware_concurrency();
        }
        threadqueues.reserve(thread_count);
        for(size_t i=0;i<thread_count;i++){
            
        }
    }
    ~workstealpool(){
        stop();
    }
    void stop(){

    }
};
#endif
