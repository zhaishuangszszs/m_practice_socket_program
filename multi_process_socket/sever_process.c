#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include<string.h>
int main()
{
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket");
        exit(-1);
    }
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(9999);
    saddr.sin_addr.s_addr = INADDR_ANY;
    // 绑定
    int ret = bind(lfd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret == -1)
    {
        perror("bind");
        exit(-1);
    }
    // 监听
    ret = listen(lfd, 128);
    if (ret == -1)
    {
        perror("listen");
        exit(-1);
    }
    // 接受连接
    while (1)
    {
        struct sockaddr_in cliaddr;
        int len = sizeof(cliaddr);
        int confd = accept(lfd, (struct sockaddr *)&cliaddr, &len);
        if (confd == -1)
        {
            perror("accept");
            exit(-1);
        }
        //每个连接进来就创建子进程与客户端通信
        pid_t pid=fork();//waitpid回收（只能有一个新连接回收一次accept阻塞）
        if(pid==0)
        {
            //子进程
            //获取客户端信息
            char cliIP[16];
            inet_ntop(AF_INET,&cliaddr.sin_addr.s_addr,cliIP,sizeof(cliIP));
            unsigned short cliPort=ntohs(cliaddr.sin_port);
            printf("client ip is: %s,port is %d\n",cliIP,cliPort);
            //接收客户端发来的信息
            char buf[1024]={0};
            while(1)
            {
                int len=read(confd,buf,sizeof(buf));
                if(len==-1)
                {
                    perror("read");
                    exit(-1);
                }
                else if(len==0)
                {
                    printf("客户端断开连接\n");
                    break;
                }
                else
                {
                    printf("recv client data: %s\n",buf);
                }
                write(confd,buf,strlen(buf));
            }
            close(confd);
            exit(0);//退出子进程
        }
    }
    close(lfd);
    return 0;
}