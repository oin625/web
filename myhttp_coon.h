#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/stat.h> //用到了stat
#include <sys/types.h>
using namespace std;

#define READ_BUF 2000

class http_coon{
	public :
		enum HTTP_CODE{
			NO_REQUESTION, //代表请求不完整，需要客户端继续输入
			GET_REQUESTION, //代表获得并解析了一个正确的HTTP请求
			BAD_REQUESTION, //HTTP的请求语法不正确 400
			FORBIDDEN_REQUESTION, //代表访问资源的权限有问题403
			FILE_REQUESTION, //代表GET方法资源请求
			INTERNAL_ERROR, //代表服务器自身的问题
			NOT_FOUND, //代表请求的资源文件不存在 404
			DYNAMIC_FILE, //表示这是一个动态请求
			POST_FILE //表示获得一个POST方式请求的HTTP请求
		};
		enum CHECK_STATUS{
			HEAD,  //头部信息
			REQUESTION //请求行
		};

	private:
		char request_head_buf[1000]; //响应头
		char post_buf[1000]; //POST请求的都缓冲区
		char read_buf[READ_BUF]; //读取的客户端的HTTP请求
		char filename[250]; //文件总目录
		int file_size;// 文件大小
		int check_index; //目前检测到的位置
		int read_buf_len; // 读取缓冲区的大小
		char *method; //请求方法
		char *url; //文件名称
		char *version; // 协议版本
		char *argv; // 动态请求参数
		bool m_linger; // 是否保持连接
		int m_http_count; //http长度
		char *m_host; // 主机名记录
		char path_400[17]; //出错码400打开的文件名缓冲区
		char path_403[23]; //出错码403打开返回的文件名缓冲区
		char path_404[40]; //404 别的同上 
		char message[1000]; // 响应消息体缓冲区
		char body[2000]; //post响应消息体缓冲区
		CHECK_STATUS status; //状态转移
		bool m_flag; //true表示动态 反之静态
	public:
		int epfd;
		int client_fd;
		int read_count;
		http_coon();
		~http_coon();
		void init(int e_fd, int c_fd); //初始化
		int myread(); // 读取请求
		bool mywrite(); // 响应发送
		void doit(); // 线程接口
		void close_coon(); // 关闭客户端连接
	private:
		void modfd(int epfd, int sock, int ev); //改变监听状态
		void dynamic(char *filename, char *argv); //通过get方法进入的动态请求处理
		HTTP_CODE analyse(); //解析http请求
		bool judge_line(int &check_index, int &read_buf_len); //以/r/n分离
		HTTP_CODE head_analyse(char *temp); //请求头解析
		HTTP_CODE requestion_analyse(char *temp); //请求行解析
		HTTP_CODE do_get(); //对get请求中的url、版本协议进行分离
		HTTP_CODE do_post(); //对post请求中的参数进行解析
		void post_respond(); //POST请求相应填充
		bool bad_respond(); // 语法错误相应填充 400
		bool forbidden_respond(); //403
		bool successful_respond();//200 ok
		bool not_found_request(); //404
		 	
		 
};

http_coon :: http_coon(){
		
}

http_coon :: ~http_coon(){
	
}

void http_coon :: init(int e_fd, int c_fd){
	epfd = e_fd;
	client_fd = c_fd;
	read_count = 0;
	m_flag = false;
}

void http_coon:: close_coon(){
	epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, 0);
	close(client_fd);
	client_fd = -1;
}

void http_coon::modfd(int epfd, int client_fd, int ev){
	epoll_event event;
	event.data.fd = client_fd;
	event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epfd, EPOLL_CTL_MOD, client_fd, &event);
}

int http_coon :: myread(){
	bzero(&read_buf, sizeof(read_buf));
	while(true){
		int ret = recv(client_fd, read_buf+read_count, READ_BUF-read_count, 0);
		if(ret == -1){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				break;
			}
			return 0;
			
		}
		else if(ret == 0){
			return 0;
		}
		read_count += ret;
	}
	strcpy(post_buf, read_buf);
	return 1;

}

bool http_coon::successful_respond(){ // 200ok
	 m_flag = false;
	 bzero(request_head_buf, sizeof(request_head_buf));
	 sprintf(request_head_buf,"HTTP/1.1 200 ok\r\nConnection: close\r\ncontent-length:%d\r\n\r\n", file_size);
}

bool http_coon:: bad_respond(){ //400
	bzero(url, strlen(url));
	strcpy(path_400, "bad_respond.html");
	url = path_400;
	bzero(filename, sizeof(filename));
	sprintf(filename, "%s", url);
	struct stat my_file;
	if(stat(filename, &my_file) < 0){
		cout << "文件不存在！！\n";
	}
	file_size = my_file.st_size;
	bzero(request_head_buf, sizeof(request_head_buf));
	sprintf(request_head_buf, "HTTP/1.1 400 BAD_REQUESTION\r\nConnection: close\r\nconnect-legth:%d\r\n\r\n", file_size);
}
bool http_coon :: forbidden_respond(){ //403
	bzero(url, strlen(url));
	strcpy(path_403, "forbiden_respond.html");
	url = path_403;
	bzero(filename, sizeof(filename));
	sprintf(filename, "%s", url);
	struct stat my_file;
	if(stat(filename, &my_file)<0){
		cout << "失败\n" ; 
	}	
	file_size = my_file.st_size;
	bzero(request_head_buf, sizeof(request_head_buf));
	sprintf(request_head_buf, "HTTP/1.1 403 FORBIDDEN\r\nConnection: close/r/ncontent-length:%d\r\n\r\n", file_size);
}


bool http_coon :: not_found_request(){ // 404
	bzero(url, strlen(url));
	strcpy(url, "not_found_request.html");
	bzero(filename, sizeof(filename));
	sprintf(filename, "%s", url);
	struct stat my_file;
	if(stat(filename, &my_file) < 0){
		cout << "404!!" <<endl;
	}
	file_size = my_file.st_size;
	bzero(request_head_buf, sizeof(request_head_buf));
	sprintf(request_head_buf, "HTTP/1.1 404 NOT_FOUND\r\nConnection: close\r\ncontent-length:%d\r\n\r\n", file_size);
}


void http_coon :: dynamic(char *filename, char *argv){
	int sum = 0 ;
	m_flag = true;
	int number[2];
	bzero(request_head_buf, sizeof(request_head_buf));
	bzero(body, sizeof(body));
	sscanf(argv, "a=%d&b=%d", &number[0], &number[1]);
	if(strcmp(filename, "/add") == 0){

		cout << "\t\t\t\tadd\n\n";
		sum = number[0] + number[1];
		sprintf(body, "<html><body>\r\n<p>%d + %d = %d </p><hr>\r\n</body></html>\r\n",number[0], number[1], sum );
		sprintf(request_head_buf, "HTTP/1.1 200 ok\r\nConnection: close\r\ncontent-length: %d\r\n\r\n", strlen(body));
	}
	else if(strcmp(filename, "/multiplication")==0){
		cout << "\t\t\t\tmultiplication\n\n";
		sum = number[0] * number[1];
		sprintf(body, "<html><body>/r/n<p>%d * %d = %d </p></hr>\r\n</body></html>\r\n", number[0], number[1], sum);
		sprintf(request_head_buf, "HTTP/1.1 200 ok\r\nConnection: close\r\ncontent-length: %d\r\n\r\n", strlen(body));
	}
}

void http_coon::post_respond(){ //POST请求处理
	if(fork() == 0){
		dup2(client_fd, STDOUT_FILENO);
		execl(filename, argv, NULL);
	}
	wait(NULL);
}

bool http_coon::judge_line(int &check_index, int &read_buf_len){
	cout << read_buf << endl;
	char ch;
	for( ; check_index < read_buf_len; ++check_index){
		ch = read_buf[check_index];
		if(ch == '\r'){
			if(check_index == read_buf_len){
				return false;
			}
			if(read_buf[check_index+1] == '\n'){
				return true;
			}
		}
		if(ch == '\n') {
			return false;
		
		}

      /*  if(ch == '\r' && check_index+1<read_buf_len && read_buf[check_index+1]=='\n')
        {
            read_buf[check_index++] = '\0';
            read_buf[check_index++] = '\0';
            return 1;//完整读入一行
        }
        if(ch == '\r' && check_index+1==read_buf_len)
        {
            return 0;
        }
        if(ch == '\n')
        {
            if(check_index>1 && read_buf[check_index-1]=='\r')
            {
                read_buf[check_index-1] = '\0';
                read_buf[check_index++] = '\0';
                return 1;
            }
            else{
                return 0;
			}
		}*/
	}
	return false;	
}


http_coon:: HTTP_CODE http_coon::requestion_analyse(char *temp){
	char *p = temp;
	cout << "p = " << p << endl;
	method = p;
	while((*p != ' ') && (*p != '\r')){
		p ++;
	}
	p[0] = '\0';
	p ++;
	cout << "method: "<< method <<endl;
	url = p;
	while((*p!=' ') && (*p!='\r')){
		p ++;
	}
	p[0] = '\0';
	p ++;

	cout << "url: "<< url << endl;

	version = p;
	while(*p!='\r' && *p!='\0' && *p!=' ') {
		p ++;	
	}
	cout << "version: " << version << endl;
	p[0] = '\0';
	p ++;
	p[0] = '\0';
	p ++;
	if(strcmp(method, "GET")!=0 && strcmp(method, "POST") != 0){
		return BAD_REQUESTION;
	}
	if(!url && url[0] != '/'){
		return BAD_REQUESTION;
	}
	if(strcmp(version, "HTTP/1.1") !=0 ){
		return BAD_REQUESTION;
	}
	status = HEAD;
	return NO_REQUESTION;
}


http_coon::HTTP_CODE http_coon::head_analyse(char *temp){
	cout << "mmp" << endl;
	if(temp[0] =='\0'){
		return GET_REQUESTION;
	}
	else if(strncasecmp(temp, "Connection:",11) == 0){
		temp =  temp + 11;
		while(*temp == ' '){
			temp ++;
		}
		if(strcasecmp(temp, "keep-alive") == 0){ // 只需特别判断长连接，默认为短链接
			m_linger = true;
		}
	}
	else if(strncasecmp(temp, "content-length:", 15) == 0){
		temp = temp + 15;
		while(*temp == ' ') {
			cout << *temp << endl;
			temp ++;
		}
		m_http_count = atoi(temp); // 填充消息体的长度
	}
	else if(strncasecmp(temp, "Host:", 5) == 0){
		temp = temp + 5;
		while(*temp = ' '){
			temp ++;
		}
		m_host = temp;
	}
	else{
		cout << "can't handle it's hand\n";
	}
	cout << "nommp" << endl;
	return NO_REQUESTION;
}

http_coon :: HTTP_CODE http_coon:: do_get(){ // GET方法请求 ，分析该请求
	char *ch;
	if(ch = strchr(url, '?')){
		argv = ch + 1;
		*ch = '\0';
		strcpy(filename, url);
		return DYNAMIC_FILE;
	}
	else {
		sprintf(filename, ".%s", url);
		struct stat my_file_stat;
		if(stat(filename, &my_file_stat) < 0) { //文件打不开
			return NOT_FOUND; // 404
		}
		if(!(my_file_stat.st_mode & S_IROTH)){ // 403 权限问题
			return FORBIDDEN_REQUESTION;
		}
		if(S_ISDIR(my_file_stat.st_mode)){
			return BAD_REQUESTION;
		}
		file_size = my_file_stat.st_size;
		return FILE_REQUESTION;
	}
}


http_coon :: HTTP_CODE http_coon :: do_post(){
	sprintf(filename, "%s", url);
	int star = read_buf_len - m_http_count;
	argv = post_buf + star;
	argv[strlen(argv) +1 ] = '\0';
	if(filename!=NULL && argv!=NULL){
		return POST_FILE;
	}
	return BAD_REQUESTION;
}

http_coon :: HTTP_CODE http_coon :: analyse(){
	status = REQUESTION;
	char *temp = read_buf;
	int star_line = 0;
	check_index = 0;
	read_buf_len = strlen(read_buf);
	int len = read_buf_len;
	while(judge_line(check_index, len) == 1){
		temp = read_buf + star_line;
		star_line  = check_index;
		switch(status){
			case REQUESTION:{
				cout << "requestion\n";
				int ret = requestion_analyse(temp);
				if(ret == BAD_REQUESTION){ //400
					cout << "ret = BAD_REQUESTION\n";
					return BAD_REQUESTION;
				}
				break;
			}
			case HEAD:{
				int ret = head_analyse(temp);
				if(ret == GET_REQUESTION){
					if(strcmp(method, "GET") == 0){
						return do_get();
					}
					else if(strcmp(method, "POST") == 0){
						return do_post();
					}
					else {
						return BAD_REQUESTION; //400
					}
				}
				break;
			}
			defautl:{
				return INTERNAL_ERROR; //服务器自身问题			
			}
		}
	}
	return NO_REQUESTION; //请求不完整
}


void http_coon:: doit(){
	HTTP_CODE choice = analyse(); // 得到请求头解析结果
	switch(choice){
		case NO_REQUESTION:{// 请求不完整那个
			cout << "NO_REQUESTION" << endl;
			modfd(epfd, client_fd, EPOLLIN);
			return ;
		}
		case BAD_REQUESTION: { // 400
			cout << "BAD_REQUESTION 400" <<endl;
			bad_respond();
			modfd(epfd, client_fd, EPOLLOUT);
			break;
		}
		case FORBIDDEN_REQUESTION:{ // 403
			cout << "FORBIDDEN_REQUESTION" << endl;
			forbidden_respond();
			modfd(epfd, client_fd, EPOLLOUT);
			break; 
		}
		case NOT_FOUND:{ //404
			cout << "NOT_FOUND_REQUEST 404" <<endl;
			not_found_request();
			modfd(epfd, client_fd, EPOLLOUT);
			break;
		}
		case FILE_REQUESTION:{
			cout << "FILE_REQUESTION 200ok" <<endl;
			successful_respond();
			modfd(epfd, client_fd, EPOLLOUT);
			break;
		}
		case DYNAMIC_FILE:{ // 动态请求处理
			cout << "DYNAMIC_FILE" <<endl;
			cout << filename << " " << argv << endl;
			dynamic(filename, argv);
			modfd(epfd, client_fd, EPOLLOUT);
			break;
		}
		case POST_FILE:{ // POST方法处理
			cout << "post_respond\n" << endl;
			post_respond();
			break;
		}
		default:{
			close_coon();		
		}
	}
}


bool http_coon :: mywrite(){
	if(m_flag){
		int ret = send(client_fd, request_head_buf, strlen(request_head_buf), 0);
		int r = send(client_fd, body, strlen(body), 0);
		if(ret>0 && r>0){
			return true;
		} 
	}
	else {
		int fd = open(filename, O_RDONLY);
		cout << "***filename = " << filename << endl;
		assert(fd != -1);
		int ret = send(client_fd, request_head_buf, strlen(request_head_buf), 0);
		if(ret < 0){
			close(fd);
			return false;
		}
		ret = sendfile(client_fd, fd, NULL, file_size);
		if(ret < 0) {
			close(fd);
			return false;
		}
		close(fd);
		return true;
	}
	return false;
}

//#endif


