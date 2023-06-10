#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>

int main()
{
    //1. 创建通信的套接字
    int connfd=socket(AF_INET,SOCK_STREAM,0);//connect会自动将connfd绑定一个端口和地址
    if(connfd==-1)//创建套接字是否成功
    {
        perror("socket error\n");
        return -1;
    }



    //2. 连接服务器 ip port
    struct sockaddr_in severaddr;//服务器的套接字地址信息
    //初始化severaddr
    severaddr.sin_family=AF_INET;
    severaddr.sin_port=htons(9999);//short2字节转换为网络字节序
    inet_pton(AF_INET,"127.0.0.1",&severaddr.sin_addr.s_addr);
    //severaddr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址
    int ret=connect(connfd,(struct sockaddr*)&severaddr,sizeof(severaddr));//连接服务器
    if(ret==-1)
    {
        perror("connect error\n");
        return -1;
    }


    //3. 和服务器通信
    int number=0;
    char buf[1024];
    while(1)
    {
        int n=snprintf(buf,1024,"hello world,%d...\n",number++);
        send(connfd,buf,n+1,0);
        memset(buf,0,sizeof(buf));
        int len=recv(connfd,buf,sizeof(buf),0);
        if(len>0)
        {
            printf("sever say:%s\n",buf);
        }
        else if(len==0)
        {
            printf("服务器端已经断开连接...\n");
            break;
        }
        else
        {
            perror("recv error\n");
            break;
        }
        sleep(2);
    }


    //4. 关闭文件描述符
    close(connfd);

    return 0;
}