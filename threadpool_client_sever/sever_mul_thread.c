#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include"threadpool.h"

// 任务函数（返回值类型为void）
void working(void *arg);
void acceptCoon(void *);

// 信息结构体（传给working函数的）
struct SockInfo
{
    struct sockaddr_in addr;
    int fd;
};

//信息结构体（传给acceptCoon函数的
struct PoolInfo
{
    ThreadPool* pool;
    int listenfd;
};

int main()
{
    // 1. 创建监听的套接字
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) //创建套接字是否成功
    {
        perror("socket error\n");
        return -1;
    }

    // 2. 绑定本地的 ip port
    struct sockaddr_in severaddr; //监听的套接字地址信息
    // 初始化severaddr
    severaddr.sin_family = AF_INET;
    severaddr.sin_port = htons(9999);       // port s代表2字节转换为网络字节序
    severaddr.sin_addr.s_addr = INADDR_ANY; // 0=0.0.0.0 会自动读取网卡实际ip地址
    // severaddr.sin_addr.s_addr = htonl(INADDR_ANY); //l代表四字节
    // severaddr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址
    int ret = bind(listenfd, (struct sockaddr *)&severaddr, sizeof(severaddr));
    if (ret == -1)
    {
        perror("bind error\n");
        return -1;
    }

    // 3. 设置监听
    ret = listen(listenfd, 10); // 10表示最大同时连接数
    if (ret == -1)
    {
        perror("listen error\n");
        return -1;
    }

    ThreadPool* pool =threadPoolCreate(3,8,10);//创建线程池
    struct PoolInfo* accpInfo=(struct PoolInfo*)malloc(sizeof(struct PoolInfo));
    accpInfo->pool=pool;
    accpInfo->listenfd=listenfd;
    printf("theadPoolAdd 启动\n");
    threadPoolAdd(pool,acceptCoon,accpInfo);//添加任务
    pthread_exit(NULL);//主进程退出，子进程继续
    return 0;//会调用exit，导致子进程退出
}


// 工作函数用于接受客户端连接
void acceptCoon(void *arg)
{
    struct PoolInfo* poolInfo=(struct PoolInfo*)arg;
    // 4. 阻塞并等待客户端连接
    int connlen = sizeof(struct sockaddr_in);
    while (1)
    {
        struct SockInfo *pinfo; //套接字信息结构体用于获取连接套接字地址信息
        pinfo=(struct SockInfo*)malloc(sizeof(struct SockInfo));
        printf("accepttCoon queueSize is %d\n",poolInfo->pool->queueSize);

        int connfd = accept(poolInfo->listenfd, (struct sockaddr *)&pinfo->addr, &connlen); // 连接套接字（获得客户端地址信息和文件描述符）
        if (connfd == -1)
        {
            perror("accept error\n");
            break;
        }
        pinfo->fd = connfd; //套接字信息结构体储存连接套接字，用于传参

        //添加通信的任务
        printf("theadPoolAdd 启动\n");
        threadPoolAdd(poolInfo->pool,working,pinfo);//因为工作线程一直在工作（工作时任务队列取出一个减一busyNum+1）所以第三第四客户端通信无法回应
    }
    close(poolInfo->listenfd); // 关闭监听描述符
    return;
}


// 工作函数用于和客户端通信
void working(void *arg)
{
    struct SockInfo *pinfo = (struct SockInfo *)arg;
    // 连接成功，打印客户端的IP和端口信息
    char ip[32];
    printf("客户端的IP:%s,端口：%d\n", inet_ntop(AF_INET, &pinfo->addr.sin_addr.s_addr, ip, sizeof(ip)), ntohs(pinfo->addr.sin_port));

    // 5. 和客户端通信
    char buf[1024];
    while (1)
    {
        memset(buf, 0, sizeof(buf));
        int len = recv(pinfo->fd, buf, sizeof(buf), 0); // 0等价于read
        if (len > 0)
        {
            printf("client say:%s\n", buf);
            send(pinfo->fd, buf, len, 0); // 0等价于write(回显)
        }
        else if (len == 0)
        {
            printf("客户端断开连接...\n");
            break;
        }
        else
        {
            perror("recv error\n");
            break;
        }
    }
    // 6. 关闭文件描述符
    close(pinfo->fd);
}