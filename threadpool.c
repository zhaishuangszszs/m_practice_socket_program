#include "threadpool.h"
const int NUMBER = 2;

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

ThreadPool *threadPoolCreate(int min, int max, int queueSize)
{
    ThreadPool *pool = (ThreadPool *)malloc(sizeof(ThreadPool)); // 线程池pool申请空间
    do
    {
        if (pool == NULL)
        {
            printf("malloc threadpool fail...\n");
            break;
        }

        pool->threadIDs = (pthread_t *)malloc(sizeof(pthread_t) * max); // 工作的线程申请空间
        if (pool->threadIDs == NULL)
        {
            printf("malloc threadIDs fail...\n");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max); // 工作的线程空间初始化为0
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min; // 和最小个数相等
        pool->exitNum = 0;

        if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
            pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
            pthread_cond_init(&pool->Empty, NULL) != 0 ||
            pthread_cond_init(&pool->Full, NULL) != 0) // 线程互斥锁条件变量初始化
        {
            printf("mutex or condition init fail...\n");
            break;
        }

        // 任务队列
        pool->taskQ = (Task *)malloc(sizeof(Task) * queueSize); // 任务队列申请空间
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;

        // 创建线程
        pthread_create(&pool->managerID, NULL, manager, pool);
        for (int i = 0; i < min; ++i)//liveNum的线程
        {
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
            printf("create init thread %ld\n",pool->threadIDs[i]);
        }
        return pool;
    } while (0);

    // 释放资源
    if (pool && pool->threadIDs)
        free(pool->threadIDs);
    if (pool && pool->taskQ)
        free(pool->taskQ);
    if (pool)
        free(pool);

    return NULL;
}

// 工作线程（生产者）
void *worker(void *arg)
{
    ThreadPool *pool = (ThreadPool *)arg;

    while (1)
    {
        pthread_mutex_lock(&pool->mutexPool);//锁住线程池
        // 当前任务队列是否为空
        while (pool->queueSize == 0 && !pool->shutdown)
        {
            // 任务队列为空阻塞工作线程
            pthread_cond_wait(&pool->Empty, &pool->mutexPool);

            // 判断是不是要销毁线程
            if (pool->exitNum > 0)
            {
                pool->exitNum--;                  // 先减，避免存活线程要小于最小线程
                if (pool->liveNum > pool->minNum) // 存活线程要大于最小线程
                {
                    pool->liveNum--;
                    threadExit(pool); // 将退出的线程id设为0
                    pthread_mutex_unlock(&pool->mutexPool);//解锁线程池
                    pthread_exit(NULL);//退出线程
                }
            }
        }

        // 判断线程池是否被关闭了
        if (pool->shutdown)
        {
            threadExit(pool); // 将退出的线程id设为0
            pthread_mutex_unlock(&pool->mutexPool);//解锁线程池
            pthread_exit(NULL);//退出线程
        }

        // 从任务队列中取出一个任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        // 移动头结点
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;
        // 解锁
        pthread_cond_signal(&pool->Full);       //唤醒一个因任务满的生产者
        pthread_mutex_unlock(&pool->mutexPool); //解锁线程池

        printf("thread %ld start working...\n", pthread_self());
        //busyNum修改需要加锁（修改busyNum）不需要锁住整个线程池
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);
        
        
        //任务工作并发执行
        task.function(task.arg);
        free(task.arg);
        task.arg = NULL;
        printf("thread %ld end working...\n", pthread_self());

        //busyNum修改需要加锁
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}

// 管理线程
void *manager(void *arg)
{
    ThreadPool *pool = (ThreadPool *)arg;
    while (!pool->shutdown)//当线程池不关闭时
    {
        // 每隔3s检测一次（是否需要添加线程或者销毁线程）
        usleep(3);

        // 取出线程池中任务的数量和当前线程的数量
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        // 取出忙的线程的数量
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        // 添加线程
        // 任务的个数>存活的线程个数 && 存活的线程数<最大线程数
        if (queueSize > liveNum && liveNum < pool->maxNum)
        {
            pthread_mutex_lock(&pool->mutexPool); // 加锁
            int counter = 0;
            for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; ++i)
            {
                if (pool->threadIDs[i] == 0) // memset初始化为0和释放的线程id为0
                {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    printf("create new thread %ld\n",pool->threadIDs[i]);
                    counter++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool); // 解锁
        }
        // 销毁线程
        // 忙的线程*2 < 存活的线程数 && 存活的线程>最小线程数
        if (busyNum * 2 < liveNum && liveNum > pool->minNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            // 让工作的线程自杀
            for (int i = 0; i < NUMBER; ++i)
            {
                pthread_cond_signal(&pool->Empty);//唤醒一个因任务空的线程自杀
            }
        }
    }
    return NULL;
}

void threadPoolAdd(ThreadPool *pool, void (*func)(void *), void *arg)
{
    pthread_mutex_lock(&pool->mutexPool);//加锁
    while (pool->queueSize == pool->queueCapacity && !pool->shutdown)
    {
        // 任务队列满了，阻塞生产者线程
        pthread_cond_wait(&pool->Full, &pool->mutexPool);
    }
    if (pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }
    // 添加任务
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;

    pthread_cond_signal(&pool->Empty);//唤醒一个因任务空等待线程
    pthread_mutex_unlock(&pool->mutexPool);//解锁
}

void threadExit(ThreadPool *pool) // 将退出的线程id设为0
{
    pthread_t tid = pthread_self();
    for (int i = 0; i < pool->maxNum; ++i)
    {
        if (pool->threadIDs[i] == tid)
        {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
}

int threadPoolBusyNum(ThreadPool *pool)
{
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolAliveNum(ThreadPool *pool)
{
    pthread_mutex_lock(&pool->mutexPool);
    int aliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return aliveNum;
}
int threadPoolDestroy(ThreadPool *pool)
{
    if (pool == NULL)
    {
        return -1;
    }

    // 关闭线程池
    pthread_mutex_lock(&pool->mutexPool);
    pool->shutdown = 1;
    pthread_mutex_unlock(&pool->mutexPool);
    // 唤醒阻塞的消费者线程让其退出
    for (int i = 0; i < pool->liveNum; ++i)
    {
        pthread_cond_signal(&pool->Empty);
    }
    // 阻塞回收管理者线程
    pthread_join(pool->managerID, NULL);
    // 释放堆内存
    if (pool->taskQ)
    {
        free(pool->taskQ);
    }
    if (pool->threadIDs)
    {
        free(pool->threadIDs);
    }

    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->Empty);
    pthread_cond_destroy(&pool->Full);

    free(pool);
    pool = NULL;

    return 0;
}