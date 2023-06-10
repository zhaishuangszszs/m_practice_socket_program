#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>

int main()
{
    //1. 创建监听的套接字
    int listenfd=socket(AF_INET,SOCK_STREAM,0);
    if(listenfd==-1)//创建套接字是否成功
    {
        perror("socket error\n");
        return -1;
    }



    //2. 绑定本地的 ip port
    struct sockaddr_in severaddr;//监听的套接字地址信息
    //初始化severaddr
    severaddr.sin_family=AF_INET;
    severaddr.sin_port=htons(9999);//short2字节转换为网络字节序
    severaddr.sin_addr.s_addr=INADDR_ANY;//0=0.0.0.0 会自动读取网卡实际ip地址
    //severaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //severaddr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址
    int ret=bind(listenfd,(struct sockaddr*)&severaddr,sizeof(severaddr));
    if(ret==-1)
    {
        perror("bind error\n");
        return -1;
    }

    //3. 设置监听
    ret=listen(listenfd,10);//10表示最大同时连接数
    if(ret==-1)
    {
        perror("listen error\n");
        return -1;
    }

    //4. 阻塞并等待客户端连接 
    struct sockaddr_in connaddr;//连接套接字地址信息
    int connlen=sizeof(connaddr);
    int connfd=accept(listenfd,(struct sockaddr*)&connaddr,&connlen);//连接套接字
    if(connfd==-1)
    {
        perror("accept error\n");
        return -1;
    }
    //连接成功，打印客户端的IP和端口信息
    char ip[32];
    printf("客户端的IP:%s,端口：%d\n",inet_ntop(AF_INET,&connaddr.sin_addr.s_addr,ip,sizeof(ip)),ntohs(connaddr.sin_port));


    //5. 和客户端通信
    char buf[1024];
    while(1)
    {
        memset(buf,0,sizeof(buf));
        int len=recv(connfd,buf,sizeof(buf),0);//0等价于read
        if(len>0)
        {
            printf("client say:%s\n",buf);
            send(connfd,buf,len,0);//0等价于write(回显)
        }
        else if(len==0)
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
    //6. 关闭文件描述符
    close(listenfd);
    close(connfd);

    return 0;
}