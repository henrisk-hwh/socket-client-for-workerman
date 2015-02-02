#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
static char *ServerIp = "127.0.0.1";
static unsigned short PortNum = 6666;
static int cfd;

int recbytes;
char buffer[1024]={0};

struct _thread {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_t tid;
	void* status;
} read_thread,write_thread;


#define log_dbg(x,arg...) printf("[debug] "x,##arg)
#define log_err(x,arg...) printf("\033[42;31m[error]\033[0m "x,##arg)

//初始化socket连接
int init_socket(int *socketfd){
	struct sockaddr_in s_add,c_add;
	*socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == *socketfd){
    		log_err("socket fail ! \r\n");
    		return -1;
	}
	log_dbg("socket ok ! socketfd=%d\r\n",*socketfd);

	bzero(&s_add,sizeof(struct sockaddr_in));
	s_add.sin_family=AF_INET;
	s_add.sin_addr.s_addr= inet_addr(ServerIp);
	s_add.sin_port=htons(PortNum);
	log_dbg("s_addr = %#x ,port : %#x\r\n",s_add.sin_addr.s_addr,s_add.sin_port);

	if(-1 == connect(*socketfd,(struct sockaddr *)(&s_add), sizeof(struct sockaddr))){
		log_err("connect fail !\r\n");
		return -1;
	}	
}

//读线程...............................................................................
void* read_form_server_thread(void* arg){
	int ret;
	while(1){
		recbytes = read(cfd,buffer,1024);
		//log_dbg("recbytes is %d\n",recbytes);
		if(-1 == recbytes){
			log_err("read data fail !\r\n");
			break;
		}else if(recbytes > 0){
			printf("read ok\r\nREC:\r\n");
			buffer[recbytes]='\0';
			printf("%s\r\n",buffer);
		}
	}
}

int start_read_thread(){
	int ret;
	ret = pthread_create(&read_thread.tid,NULL,read_form_server_thread,NULL);
	if(ret == -1) log_err("can't create read thread(%s)\n",strerror(errno));
	return ret;
}

//写线程...............................................................................
void* write_to_server_thread(void* arg){
	int ret;
	while(1){
		if(-1 == write(cfd,"hello,trying to connected to server \r\n",50)){
			log_err("write fail!\r\n");
			return NULL;
		}
		sleep(3);
	}
}

int start_write_thread(){
	int ret;
	ret = pthread_create(&write_thread.tid,NULL,write_to_server_thread,NULL);
	if(ret == -1) log_err("can't create write thread(%s)\n",strerror(errno));	
	return ret;
}

//main...............................................................................
int main()
{
	int is_exit = 0;



	log_dbg("Hello,welcome to client !\r\n");
	if(init_socket(&cfd) == -1 || cfd == -1)
		goto failed;
	log_dbg("connect ok !\r\n");

	start_read_thread();
	start_write_thread();
	
	pthread_join(read_thread.tid,read_thread.status);
	pthread_join(write_thread.tid,write_thread.status);
failed:
	close(cfd);
	return 0;
}
