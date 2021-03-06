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

#define MSG_PUSH_ALL 5000
#define MSG_PUSH_ONE 5001

#define MSG_UPDATE_IMAGE  6000

static char *ServerIp = "114.215.158.131";
static unsigned short PortNum = 2345;
static int cfd;

int recbytes;
char buffer[1024]={0};
int command = 0;
char* defualt_device_id = "0123";
char* DeviceId;
enum device_status
{
	ON = 1,
	OFF = 0,
} status;
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
#include "base64.h"

#ifdef ANDROID_ENV
#define LOG_TAG "bell-socket-network"
#include <cutils/log.h>
#include <android/log.h>
#undef log_dbg
#undef log_err
#define log_dbg ALOGD
#define log_err ALOGE
int android_system(const char * cmdstring)
{
        pid_t pid;
        int status;
        if(cmdstring == NULL){
                return (1); //如果cmdstring为空，返回非零值，一般为1
        }

        if((pid = fork())<0){
                status = -1; //fork失败，返回-1
        }
        else if(pid == 0){
                execl("/system/bin/sh", "sh", "-c", cmdstring, (char *)0);
                _exit(127); // exec执行失败返回127，注意exec只在失败时才返回现在的进程，成功的话现在的进程就不存在啦~~
        }
        else //父进程
        {
                while(waitpid(pid, &status, 0) < 0){
                        if(errno != EINTR){
                                status = -1; //如果waitpid被信号中断，则返回-1
                                break;
                        }
                }
        }
        return status; //如果waitpid成功，则返回子进程的返回状态
}
#endif
int system_shell(const char * cmdstring)
{
        #ifdef ANDROID_ENV
        return android_system(cmdstring);
        #else
        return system(cmdstring);
        #endif
}

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
int device_on(int face){
#ifdef ANDROID_ENV
        int ret;char *cmd;
	device_off();
	if(face == 1)
		cmd = "echo 146:1:0:800:600# > data/camera/command";
	else
		cmd = "echo 146:0:0:3264:2448# > data/camera/command";
        ret = system_shell(cmd);
        if(ret < 0){
                log_err("%s ret is %d,%s\n",cmd,ret,strerror(errno));
                return ret;
        }
        cmd = "/data/camera/hawkview &";
        ret = system_shell(cmd);
        if(ret < 0){
                log_err("%s ret is %d,%s\n",cmd,ret,strerror(errno));
                return ret;
        }
        return ret;
#else
        log_dbg("device on!");
        return 0;
#endif
}

int device_off()
{
#ifdef ANDROID_ENV
        int ret;
        char *cmd = "ps | busybox grep 'hawkview' | kill `busybox awk '{print $2}'`";
        ret = system_shell(cmd);
        if(ret < 0){
                log_err("%s ret is %d,%s\n",cmd,ret,strerror(errno));                
        }
        return ret;
#else
        log_dbg("device off!");
        return 0;
#endif
}
int device_get()
{
#ifdef ANDROID_ENV
        int ret = 0;
/*
        char *cmd = "rm /data/camera/command";
        ret = system_shell(cmd);
        if(ret < 0){
                log_err("%s ret is %d,%s\n",cmd,ret,strerror(errno));
                return ret;
        }
*/
        char *cmd = "echo 149:test.jpg# > data/camera/command";
        ret = system_shell(cmd);
        if(ret < 0){
                log_err("%s ret is %d,%s\n",cmd,ret,strerror(errno));
                return ret;
        }
        return ret;
#else
        log_dbg("device off!");
        return 0;
#endif
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
        char *data = "test";
        
	
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
			if(msg == MSG_PUSH_ONE){

			}else if(msg == MSG_PUSH_ALL){
				set_command_signal(100);
			}

			data = cJSON_GetObjectItem(json,"data")->valuestring;
			log_dbg("data: %s\n",data);
			if(!strcmp(data, "on")){
				device_on(0);
			}
			if(!strcmp(data, "onn")){
				device_on(1);
			}
			if(!strcmp(data, "off")){
				device_off();
			}
			if(!strcmp(data, "get")){
				device_get();
				sleep(1);
				set_command_signal(MSG_UPDATE_IMAGE);
			}
			break;
	}       

        cJSON_Delete(json);
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
		cJSON_AddStringToObject(root,"data",data);
	}
	out = cJSON_PrintUnformatted(root);
	cJSON_Delete(root);
	log_dbg("%s\n",out);
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
	char data[1000*1000];
	int len = sizeof(data);
	int base64_buf_len;
	char* malloc_data = NULL;
	switch(cmd){
		case MSG_AUTH_REQ:
			create_package(data,&len,TYPE_CONNCET,MSG_AUTH_RESP,NULL,0,DeviceId);
			break;
		case MSG_UPDATE_IMAGE:
			malloc_data = base64_encode_file("data/camera/test.jpg",&base64_buf_len);
			//malloc_data = base64_encode_file("data/camera/command",&base64_buf_len);
			log_dbg("file base64 encoded len = %d\n",base64_buf_len);
			create_package(data,&len,TYPE_UPDATE,MSG_UPDATE_IMAGE,malloc_data,base64_buf_len,DeviceId);
			log_dbg("json buffer len = %d\n",len);
			break;
		case 100:
			create_package(data,&len,TYPE_PUSH,100,NULL,0,DeviceId);
			break;

	}
	if(len > 0){
		if(-1 == write(cfd,data,len)){
			log_err("write fail!\r\n");
			return -1;
		}
		memset(data,0,sizeof(data));
		if(malloc_data != NULL) free(malloc_data);
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
		log_err("connect fail! %s.\r\n",strerror(errno));
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
//应该建立命令队列缓存
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
	//start_input_thread();

	//挂起work主进程
	pthread_join(read_thread.tid,read_thread.status);

failed:
	close(cfd);
	exit(0);
}
void init_deamon(void) 
{ 
	int pid; 
	int i;

	/* 处理SIGCHLD信号。处理SIGCHLD信号并不是必须的。但对于某些进程，
	   特别是服务器进程往往在请求到来时生成子进程处理请求。如果父进
	   程不等待子进程结束，子进程将成为僵尸进程（zombie）从而占用系统资源。*/
	if(signal(SIGCHLD,SIG_IGN) == SIG_ERR){
		printf("Cant signal in init_daemon.");
		exit(1);
	}
	if(pid=fork()) 
		exit(0);//是父进程，结束父进程 
	else if(pid< 0){ 
		perror("fail to fork1");
		exit(1);//fork失败，退出
	}
	//是第一子进程，后台继续执行
	setsid();//第一子进程成为新的会话组长和进程组长

	//并与控制终端分离 
	if(pid=fork()) 
		exit(0);//是第一子进程，结束第一子进程 
	else if(pid< 0) 
		exit(1);//fork失败，退出 
	//是第二子进程，继续 
	//第二子进程不再是会话组长 

	umask(0);//重设文件创建掩模
	log_dbg("init daemon successfully!!\n");
	return; 
}
//创建守护进程，检测任务进程是否退出，并重新创建任务进程，保证socket连接
int main(int argc,char* argv[])
{
	log_dbg("Hello,welcome to socket device daemon !\r\n");
	DeviceId = defualt_device_id;
	if(argc == 2) DeviceId = argv[1];
	if(argc == 4){
		ServerIp = argv[1];
		PortNum = atoi(argv[2]);
		DeviceId = argv[3];
	}
	//init_deamon();
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
			device_off();
			sleep(5);//5秒后重新创建连接进程
		}
	}

	return 0;
}
