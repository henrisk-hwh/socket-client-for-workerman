#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h> 
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>

#define ROLE_DEVICE 1
#define ROLE_WEBSEVER 0

#define TYPE_CONNCET  3000
#define TYPE_UPDATE  3001
#define TYPE_PUSH  3002

#define MSG_AUTH_REQ  4000
#define MSG_AUTH_RESP  4001
#define MSG_AUTH_VEFY  4002
#define MSG_AUTH_FIAL  4003

static char *ServerIp = "114.215.158.131";
static unsigned short PortNum = 6666;
static int cfd;

int recbytes;
char buffer[1024]={0};
int command = 0;
char* defualt_device_id = "testid";
char* DeviceId;
struct _thread {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_t tid;
	void* status;
} read_thread,write_thread;

struct _thread input_thread;

#define log_dbg(x,arg...) printf("[debug] "x,##arg)
#define log_err(x,arg...) printf("\033[42;31m[error]\033[0m "x,##arg)


#include "cJSON.h"

int get_command(){
	pthread_mutex_lock(&write_thread.mutex);
	int cmd = command;
	pthread_mutex_unlock(&write_thread.mutex);
	return cmd;
}
void set_command(int cmd){
	pthread_mutex_lock(&write_thread.mutex);
	command =  cmd;
	pthread_mutex_unlock(&write_thread.mutex);
}
int get_set_command(int cmd){
	pthread_mutex_lock(&write_thread.mutex);
	int cur_cmd = command;
	command =  cmd;
	pthread_mutex_unlock(&write_thread.mutex);
	return cur_cmd;
}
void set_command_signal(int cmd){
	set_command(cmd);
	pthread_cond_signal(&write_thread.cond);
}
/* Parse text to JSON, then render back to text, and print! */
void doit(char *text)
{
	char *out;cJSON *json;
	
	json=cJSON_Parse(text);
	if (!json) {printf("Error before: [%s]\n",cJSON_GetErrorPtr());}
	else
	{
		out=cJSON_Print(json);
		cJSON_Delete(json);
		printf("%s\n",out);
		free(out);
	}
}
int handler_read_msg(char *text)
{	
	cJSON *json;

	doit(text);
	json = cJSON_Parse(text);
	if (!json) {
		printf("Error before: [%s]\n",cJSON_GetErrorPtr());
		return -1;
	}

	int type = cJSON_GetObjectItem(json,"type")->valueint;
	int msg = cJSON_GetObjectItem(json,"msg")->valueint;
	cJSON_Delete(json);
	log_dbg("type: %d\n",type);

	switch(type){
		case TYPE_CONNCET:
			if(msg == MSG_AUTH_REQ){
				set_command_signal(MSG_AUTH_REQ);
			}else if (msg == MSG_AUTH_VEFY){
				log_dbg("devide connected to server!!\n");
			}			
			break;
		case TYPE_UPDATE:
			break;
		case TYPE_PUSH:
			break;
	}
	return 0;

}
void create_package(char *p,int *len,int type,int msg,char* data,int data_len,char* device_id)
{
	cJSON *root;
	char *out;	/* declare a few. */
	int size = 0;
	/* Here we construct some JSON standards, from the JSON site. */	

	root=cJSON_CreateObject();
	cJSON_AddNumberToObject(root,"role",ROLE_DEVICE);
	cJSON_AddNumberToObject(root,"type",type);
	cJSON_AddNumberToObject(root,"msg",msg);
	cJSON_AddStringToObject(root,"id",device_id);
	if(data != NULL && data_len > 0){

	}
	out = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	printf("%s\n",out);
	while(out[size++] != '\0');
	if(size < *len){
		*len = size;
		memcpy(p,out,size);
		p[(*len)++] = '\n';
	}else {
		log_err("json size overflow!");
		*len = 0;
	};
	free(out);
}

int handler_write_msg(int cmd){
	char data[1000];
	int len = sizeof(data);
	switch(cmd){
		case MSG_AUTH_REQ:
			create_package(data,&len,TYPE_CONNCET,MSG_AUTH_RESP,NULL,0,DeviceId);			
			break;
	}
	if(len > 0){
		if(-1 == write(cfd,data,len)){
			log_err("write fail!\r\n");
			return -1;
		}
		len = sizeof(data);
		memset(data,0,len);	
	}
	return 0;
}
//初始化socket连接
int init_socket(int *socketfd){
	struct sockaddr_in s_add;
	*socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == *socketfd){
    		log_err("socket fail ! \r\n");
    		return -1;
	}
	log_dbg("socket ok ! socketfd=%d\r\n",*socketfd);

	bzero(&s_add,sizeof(struct sockaddr_in));
	s_add.sin_family = AF_INET;
	s_add.sin_addr.s_addr = inet_addr(ServerIp);
	s_add.sin_port = htons(PortNum);
	log_dbg("s_addr = %#x ,port : %#x\r\n",s_add.sin_addr.s_addr,s_add.sin_port);

	if(-1 == connect(*socketfd,(struct sockaddr *)(&s_add), sizeof(struct sockaddr))){
		log_err("connect fail !\r\n");
		return -1;
	}

	int keepalive = 1;  
    int keepidle = 5;  
    int keepinterval = 3;  
    int keepcount = 2;  
    if(setsockopt(*socketfd,SOL_SOCKET,SO_KEEPALIVE,&keepalive,sizeof(keepalive))<0) return -3;  
    if(setsockopt(*socketfd,SOL_TCP,TCP_KEEPIDLE,&keepidle,sizeof(keepidle))<0) return -4;  
    if(setsockopt(*socketfd,SOL_TCP,TCP_KEEPINTVL,&keepinterval,sizeof(keepinterval))<0) return -5;  
    if(setsockopt(*socketfd,SOL_TCP,TCP_KEEPCNT,&keepcount,sizeof(keepcount))<0) return -6;  
	
	return 0;
}

//读线程...............................................................................
void* read_form_server_thread(void* arg){

	while(1){		
		recbytes = read(cfd,buffer,1024);
		//log_dbg("recbytes is %d\n",recbytes);
		if(-1 == recbytes){
			log_err("read data fail !\r\n");
			break;
		}else if(recbytes > 0){
			buffer[recbytes]='\0';
			handler_read_msg(buffer);
		}
		else if(0 == recbytes){
			log_err("socket connection broken!\r\n");
			exit(0);
		}
	}/**/

	
	return (void*)0;
}

int start_read_thread(){
	int ret;
	ret = pthread_create(&read_thread.tid,NULL,read_form_server_thread,NULL);
	if(ret == -1) log_err("can't create read thread(%s)\n",strerror(errno));
	return ret;
}



//写线程...............................................................................
void* write_to_server_thread(void* arg){

	while(1){
		if(get_command() == 0){
			pthread_mutex_lock(&write_thread.mutex);
			pthread_cond_wait(&write_thread.cond,&write_thread.mutex);
			pthread_mutex_unlock(&write_thread.mutex);
		}
		int cmd = get_set_command(0);
		handler_write_msg(cmd);
	}
	return (void*)0;
}

int start_write_thread(){
	int ret;
	ret = pthread_create(&write_thread.tid,NULL,write_to_server_thread,NULL);
	if(ret == -1) log_err("can't create write thread(%s)\n",strerror(errno));	
	return ret;
}

//调试键盘input线程...................................................................
void* keyboard_input_thread(void* arg){
	int a;
	while(scanf("%d",&a)){
		log_dbg("%d\n",a);
		set_command(1);
		pthread_cond_signal(&write_thread.cond);
	}
	return (void*)0;
}
int start_input_thread(){
	int ret;
	ret = pthread_create(&input_thread.tid,NULL,keyboard_input_thread,NULL);
	if(ret == -1) log_err("can't create input thread(%s)\n",strerror(errno));	
	return ret;
}
int work(){
	if(init_socket(&cfd) < 0 || cfd == -1)
		goto failed;
	log_dbg("connect ok !\r\n");
	pthread_mutex_init(&write_thread.mutex,NULL);
	pthread_cond_init(&write_thread.cond,NULL);

	start_read_thread();
	start_write_thread();
	start_input_thread();

	pthread_join(read_thread.tid,read_thread.status);
	log_dbg("read_thread return %d\r\n",*(int*)read_thread.status);
	pthread_join(write_thread.tid,write_thread.status);
	log_dbg("write_thread return %d\r\n",*(int*)write_thread.status);
	pthread_join(input_thread.tid,input_thread.status);
	log_dbg("input_thread return %d\r\n",*(int*)input_thread.status);
failed:
	close(cfd);
	exit(0);
}
//作为守护进程daemon，检测任务进程是否退出，并重新创建任务进程，保证socket连接.....
int main(int argc,char* argv[])
{
	log_dbg("Hello,welcome to socket device daemon !\r\n");
	DeviceId = defualt_device_id;
	if(argc == 2) DeviceId = argv[1];
	if(argc == 4){
		char *port;
		ServerIp = argv[1];
		port = argv[2];
		PortNum = atoi(port);
		DeviceId = argv[3];
	}
	while(1){
		//创建工作进程
		pid_t work_pid;
		work_pid = fork();
		if(work_pid < 0)
			log_err("fork work progress fail\n");
		else if(work_pid == 0){
			log_dbg("here is work progress\n");
			work();
		}
		else{
			log_dbg("here is daemon\n");
			wait(NULL);//工作进程异常退出后，唤醒守护进程
			sleep(5);//5秒后重新创建连接进程
		}
	}

	return 0;
}
