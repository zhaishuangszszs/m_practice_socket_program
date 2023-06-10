#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
void* working(void*arg);


//信息结构体（传给working函数的）
struct SockInfo
{
    struct sockaddr_in addr;
    int fd;
};
struct SockInfo infos[512];

int main()
{
    // 1. 创建监听的套接字
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) // 创建套接字是否成功
    {
        perror("socket error\n");
        return -1;
    }

    // 2. 绑定本地的 ip port
    struct sockaddr_in severaddr; // 监听的套接字地址信息
    // 初始化severaddr
    severaddr.sin_family = AF_INET;
    severaddr.sin_port = htons(9999);       // short2字节转换为网络字节序
    severaddr.sin_addr.s_addr = INADDR_ANY; // 0=0.0.0.0 会自动读取网卡实际ip地址
    // severaddr.sin_addr.s_addr = htonl(INADDR_ANY);
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



    // 4. 阻塞并等待客户端连接

    //初始化信息结构体数组(用于表示连接套接字地址信息)
    int max=sizeof(infos)/sizeof(infos[0]);
    for(int i=0;i<max;i++)
    {
        bzero(&infos[i],sizeof(infos[i]));
        infos[i].fd=-1;//-1表明空闲
    }
    int connlen = sizeof(struct sockaddr_in);


    while (1)
    {
        struct SockInfo* pinfo;//用于获取连接套接字地址信息

        for(int i=0;i<max;i++)
        {
            if(infos[i].fd==-1)//该结构体空闲
            {
                pinfo=&infos[i];
                break;
            }
        }

        int connfd = accept(listenfd, (struct sockaddr *)&pinfo->addr, &connlen); // 连接套接字（获得客户端地址信息和文件描述符）
        if (connfd == -1)
        {
            perror("accept error\n");
            break;//直接退出主线程
            //连接失败仍然可以连接用continue,但要回收信息结构体
        }
        pinfo->fd=connfd;//信息结构体储存连接套接字，用于传参

        pthread_t tid;
        pthread_create(&tid,NULL,working,pinfo);//创建子线程传入信息结构体
        pthread_detach(tid);
    }
    close(listenfd);//关闭监听描述符
    return 0;
}
void *working(void *arg)
{
    struct SockInfo* pinfo=(struct SockInfo*)arg;
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
    pinfo->fd=-1;//置为空
    return NULL;
}