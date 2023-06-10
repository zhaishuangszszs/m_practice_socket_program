#ifndef _THREADPOLL_
#define _THREADPOLL_
#include <pthread.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
// 任务结构体
typedef struct Task
{
    void (*function)(void *arg);
    void *arg;
} Task;

// 线程池结构体
struct ThreadPool
{
    // 任务队列
    Task *taskQ;
    int queueCapacity; // 容量
    int queueSize;     // 当前任务个数
    int queueFront;    // 队头->取数据
    int queueRear;     // 队尾->放数据

    pthread_t managerID;       // 管理者线程
    pthread_t *threadIDs;      // 工作的线程
    int minNum;                // 最小线程数
    int maxNum;                // 最大线程数
    int busyNum;               // 忙的线程数
    int liveNum;               // 存活的线程数
    int exitNum;               // 要销毁的线程数
    pthread_mutex_t mutexPool; // 锁整个的线程池
    pthread_mutex_t mutexBusy; // 锁busyNum变量
    pthread_cond_t Full;       // 任务队列是不是满了
    pthread_cond_t Empty;      // 任务队列是不是空了

    int shutdown; // 是否销毁线程池，销毁为1，否为0
};
typedef struct ThreadPool ThreadPool;



//创建线程池并初始化
ThreadPool* threadPoolCreate(int min,int max,int queueSize);

// 给线程池添加任务
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);


// 获取线程池中工作的线程的个数
int threadPoolBusyNum(ThreadPool* pool);
// 获取线程池中活着的线程的个数
int threadPoolAliveNum(ThreadPool* pool);
// 销毁线程池
int threadPoolDestroy(ThreadPool* pool);



//////////////////////
// 工作的线程(消费者线程)任务函数
void* worker(void* arg);
// 管理者线程任务函数
void* manager(void* arg);
// 单个线程退出
void threadExit(ThreadPool* pool);

#endif