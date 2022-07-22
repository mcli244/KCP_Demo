#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "ikcp.h"

#include <string.h>

#include <sys/time.h>
#include <sys/wait.h>
#include <arpa/inet.h>


//=====================================================================
//
// kcp demo
//
// 说明：
// ikcp_input、ikcp_send返回值0 为正常
// 前期通信异常原因：
// 1.sendto(send->sockfd, buf, len, 0, (struct sockaddr *)&send->CientAddr,sizeof(struct sockaddr_in));
	// 参数buf 写成send->buff
// 2.ret = ikcp_send(send->pkcp, send->buff,sizeof(send->buff) );//第三个参数 正确应该是strlen(send->buff)+1
//   send->buff实际长度14字节，却传输512字节，后面全是0，kcp对这512字节数据全部进行封包，由UDP发送，导致kcp  input处理出错
//  send中的 buff[488]最大长度为488，传输正常，（488+24=512）[实际长度488+24kcp头部]，感觉kcp_input处理超过512的数据就出错
//=====================================================================

static int number = 0;

 typedef struct {
	unsigned char *ipstr;
	int port;
	
	ikcpcb *pkcp;
	
	int sockfd;
	
	struct sockaddr_in addr;//存放服务器信息的结构体
	struct sockaddr_in CientAddr;//存放客户机信息的结构体
	
	char buff[488];//存放收发的消息
	
}kcpObj;


/* get system time */
void itimeofday(long *sec, long *usec)
{
	#if defined(__unix)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#else
	static long mode = 0, addsec = 0;
	BOOL retval;
	static IINT64 freq = 1;
	IINT64 qpc;
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0)? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif
}

/* get clock in millisecond 64 */
IINT64 iclock64(void)
{
	long s, u;
	IINT64 value;
	itimeofday(&s, &u);
	value = ((IINT64)s) * 1000 + (u / 1000);
	return value;
}

IUINT32 iclock()
{
	return (IUINT32)(iclock64() & 0xfffffffful);
}

/* sleep in millisecond */
void isleep(unsigned long millisecond)
{
	#ifdef __unix 	/* usleep( time * 1000 ); */
	struct timespec ts;
	ts.tv_sec = (time_t)(millisecond / 1000);
	ts.tv_nsec = (long)((millisecond % 1000) * 1000000);
	/*nanosleep(&ts, NULL);*/
	usleep((millisecond << 10) - (millisecond << 4) - (millisecond << 3));
	#elif defined(_WIN32)
	Sleep(millisecond);
	#endif
}


int udpOutPut(const char *buf, int len, ikcpcb *kcp, void *user){
   
    kcpObj *send = (kcpObj *)user;

	//发送信息
    int n = sendto(send->sockfd, buf, len, 0, (struct sockaddr *)&send->CientAddr,sizeof(struct sockaddr_in));
    if (n >= 0)       
   	{       
		//会重复发送，因此牺牲带宽
		// printf("udpOutPut-send: 字节 =%d bytes   内容=[%s]\n", n ,buf+24);//24字节的KCP头部
        return n;
    } 
	else 
	{
        printf("error: %d bytes send, error\n", n);
        return -1;
    }
}

char buf[2048]={0};
int init(kcpObj *send)
{	
	send->sockfd = socket(AF_INET,SOCK_DGRAM,0);
	
	if(send->sockfd<0)
	{
		perror("socket error！");
		exit(1);
	}
	
	bzero(&send->addr, sizeof(send->addr));
	
	send->addr.sin_family = AF_INET;
	send->addr.sin_addr.s_addr = htonl(INADDR_ANY);//INADDR_ANY
	send->addr.sin_port = htons(send->port);
		
	printf("服务器socket: %d  port:%d\n",send->sockfd,send->port);
	
	if(send->sockfd<0){
		perror("socket error！");
		exit(1);
	}
	
	if(bind(send->sockfd,(struct sockaddr *)&(send->addr),sizeof(struct sockaddr_in))<0)
	{
		perror("bind");
		exit(1);
	}
	
}

void loop(kcpObj *send)
{
	int len = sizeof(struct sockaddr_in);
	int n,ret;	
	int cnt = 0;
	//接收到第一个包就开始循环处理
	
	while(1)
	{
		isleep(1);		
		ikcp_update(send->pkcp,iclock());

		//处理收消息
		while(1)
		{
			memset(buf, 0, sizeof(buf));
			n = recvfrom(send->sockfd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&send->CientAddr, (socklen_t *)&len);			
			
			if(n < 0)//检测是否有UDP数据包: kcp头部+data
				break;
			//预接收数据:调用ikcp_input将裸数据交给KCP，这些数据有可能是KCP控制报文，并不是我们要的数据。 
			//kcp接收到下层协议UDP传进来的数据底层数据buffer转换成kcp的数据包格式
			// printf("recvfrom buf[%d]:[%s]\r\n", n, buf+24);
			ikcp_input(send->pkcp, buf, n);
		}
		

		while (1) {
            uint8_t kcpbuf[4096];
			int hr = ikcp_recv(send->pkcp, kcpbuf, sizeof(kcpbuf));
			if(hr < 0) break;

			// 校验数据
			int i = 0;
			for(i=0; i<hr; i++)
			{
				if(kcpbuf[i] != (i&0xff))
					break;
			}
			// 打印来自服务端发送来的数据
            printf("cnt:%d Recv From Server[%d] ",cnt++ , hr);
			if(i < hr)
			{
				printf("check failed kcpbuf[%d]:%d -- i&0xff:%d\r\n", i, kcpbuf[i], (i&0xff));
			}
			else
			{
				printf("check pass\r\n");
			}
        }
	}	
}

int main(int argc,char *argv[])
{
	printf("this is kcpServer\n");
	if(argc <2 )
	{
		printf("请输入服务器端口号\n");
		return -1;
	}
	
	kcpObj send;
	send.port = atoi(argv[1]);
	send.pkcp = NULL;
	ikcpcb *kcp = ikcp_create(0x1, (void *)&send);//创建kcp对象把send传给kcp的user变量
	kcp->output = udpOutPut;//设置kcp对象的回调函数
	ikcp_nodelay(kcp, 0, 10, 0, 0);//1, 10, 2, 1
	ikcp_wndsize(kcp, 128, 128);
	
	send.pkcp = kcp;

	init(&send);//服务器初始化套接字
	loop(&send);//循环处理
	
	return 0;	
}