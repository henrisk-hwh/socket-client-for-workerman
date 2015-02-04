#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
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

static char *ServerIp = "127.0.0.1";
static unsigned short PortNum = 6666;
static int cfd;

int recbytes;
char buffer[1024]={0};
int command = 0;
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
	}
	return 0;

}
void create_objects(char *p,int *len,char* device_id)
{
	cJSON *root;
	char *out;	/* declare a few. */
	int size = 0;
	/* Here we construct some JSON standards, from the JSON site. */
	
	/* Our "Video" datatype: */
	root=cJSON_CreateObject();
	cJSON_AddNumberToObject(root,"role",ROLE_DEVICE);
	cJSON_AddNumberToObject(root,"type",TYPE_CONNCET);
	cJSON_AddNumberToObject(root,"msg",MSG_AUTH_RESP);
	cJSON_AddStringToObject(root,"id",device_id);
	//cJSON_AddFalseToObject (root,"interlace");
	//cJSON_AddNumberToObject(root,"frame rate",	24);
	

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
			create_objects(data,&len,"1234");			
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
//main...............................................................................
int main()
{

	log_dbg("Hello,welcome to client !\r\n");
	if(init_socket(&cfd) == -1 || cfd == -1)
		goto failed;
	log_dbg("connect ok !\r\n");

	pthread_mutex_init(&write_thread.mutex,NULL);
	pthread_cond_init(&write_thread.cond,NULL);

	start_read_thread();
	start_write_thread();
	start_input_thread();

	pthread_join(read_thread.tid,read_thread.status);
	pthread_join(write_thread.tid,write_thread.status);
	pthread_join(input_thread.tid,input_thread.status);
failed:
	close(cfd);
	return 0;
}
