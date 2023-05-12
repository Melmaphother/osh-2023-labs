#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BIND_IP_ADDR "0.0.0.0"
#define BIND_PORT 8000
#define MAX_RECV_LEN 1048576
#define MAX_SEND_LEN 1048576
#define MAX_PATH_LEN 4096
#define MAX_CONN 50

#define handle_error(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define HTTP_STATUS_200 "200 OK"
#define HTTP_STATUS_404 "404 Not Found"
#define HTTP_STATUS_500 "500 Internal Server Error"


int parse_request(int clnt_sock,char *req,ssize_t *req_len,struct stat *fstatus)
{
    req[0] = '\0';
    char* buf = (char*) malloc(MAX_RECV_LEN * sizeof(char));
    ssize_t read_len;
    while(1)
    {
        read_len = read(clnt_sock, buf, MAX_RECV_LEN - 1);
        if (read_len < 0) handle_error("failed to read clnt_sock");
        buf[read_len] = '\0';
        strcat(req, buf);
        if(buf[read_len - 4] == '\r' && buf[read_len - 3] == '\n' && buf[read_len - 2] == '\r' && buf[read_len - 1] == '\n') break;
    }
    *req_len = strlen(req);
    if(*req_len <= 0) handle_error("failed to read clnt_sock");
    if(*req_len < 5) return -2;// 500 没有'GET /'
    if(req[0]!='G'||req[1]!='E'||req[2]!='T'||req[3]!=' '||req[4]!= '/') return -2;
    ssize_t s1 = 3;
    req[s1] = '.';
    ssize_t s2 = 5;
    int k = 0;
    while((s2-s1)<= MAX_PATH_LEN && req[s2] != ' ')
    {
        if(req[s2] == '/')
        {
            if(req[s2-1] == '.' && req[s2-2] == '.' && req[s2-3] == '/') k--;
            else k++;
            if(k < 0) return -2;//跳出当前路径 500
        }
        s2++;
    }
    if(s2 - s1 > MAX_PATH_LEN) return -2;//路径超过最大路径长度
    req[s2] = '\0';
    int fd = open(req+s1,O_RDONLY);
    if(fd < 0) return -1;//打开失败 404
    if(stat(req+s1,fstatus) == -1) handle_error("failed to stat");
    if(!S_ISREG(fstatus->st_mode)) return -2;//不是常规文件 500
    return fd;
}


void handle_clnt(int clnt_sock){
    char *req_buf = (char *)malloc(MAX_RECV_LEN * sizeof(char));
    if (req_buf == NULL)
        handle_error("failed to malloc req_buf\n");
    char *response = (char *)malloc(MAX_SEND_LEN * sizeof(char));
    if (response == NULL)
        handle_error("failed to malloc response\n");
    
    ssize_t req_len;
    struct stat fstatus;
    int file_fd = parse_request(clnt_sock, req_buf, &req_len, &fstatus);
    
    // 构造要返回的数据
    // 注意，响应头部后需要有一个多余换行（\r\n\r\n），然后才是响应内容
    if (file_fd == -1)
    {// 404
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_404, (size_t)0);
        size_t response_len = strlen(response);
        if (write(clnt_sock, response, response_len) == -1)
            handle_error("failed to write response when 404");
    }
    else if (file_fd == -2)
    {// 500
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_500, (size_t)0);
        size_t response_len = strlen(response);
        if (write(clnt_sock, response, response_len) == -1)
            handle_error("failed to write response when 500");
    }
    else
    {// 200
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_200, (size_t)(fstatus.st_size));
        size_t response_len = strlen(response);
        if (write(clnt_sock, response, response_len) == -1)
            handle_error("failed to write response when 200");
        
        //读取文件中的内容并返回
        while (response_len = read(file_fd, response, MAX_SEND_LEN)){
            if (response_len == -1)
                handle_error("failed to read file");
            if (write(clnt_sock, response, response_len) == -1)
                handle_error("failed to write file to clnt_sock");
        }
    }
    // 关闭客户端套接字
    // 释放内存
    close(clnt_sock);
    close(file_fd);
    free(req_buf);
    free(response);
}

struct job
{
    int clnt_sock;
    struct job *next;
};

struct threadpool
{
    int thread_num;                   //线程池中开启线程的个数
    struct job *head;                 //指向job的头指针
    struct job *tail;                 //指向job的尾指针
    pthread_t *pthreads;              //线程池中所有线程的pthread_t
    pthread_mutex_t mutex;            //互斥信号量
    pthread_cond_t queue_empty;       //队列为空的条件变量
    pthread_cond_t queue_not_empty;   //队列不为空的条件变量
    int queue_cur_num;                //队列当前的job个数
    int pool_close;                   //线程池是否已经关闭
};

void* threadpool_function(void* arg)
{
    struct threadpool *pool = (struct threadpool*)arg;
    struct job *pjob = NULL;
    while (1)  //死循环
    {
        pthread_mutex_lock(&(pool->mutex));
        while ((pool->queue_cur_num == 0) && !pool->pool_close)   //队列为空时，就等待队列非空
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->mutex));
        if (pool->pool_close)   //线程池关闭，线程就退出
        {
            pthread_mutex_unlock(&(pool->mutex));
            pthread_exit(NULL);
        }
        pool->queue_cur_num--;
        pjob = pool->head;
        if (pool->queue_cur_num == 0)
            pool->head = pool->tail = NULL;
        else
            pool->head = pjob->next;
        if (pool->queue_cur_num == 0)
            pthread_cond_signal(&(pool->queue_empty));        //队列为空，就可以通知threadpool_destroy函数，销毁线程函数
        pthread_mutex_unlock(&(pool->mutex));
        handle_clnt(pjob->clnt_sock);   //线程真正要做的工作，回调函数的调用
        free(pjob);
        pjob = NULL;
    }
}

struct threadpool* threadpool_init(int thread_num)
{
    struct threadpool *pool = NULL;
    pool = malloc(sizeof(struct threadpool));
    if (NULL == pool)
        handle_error("failed to malloc threadpool");
    pool->thread_num = thread_num;
    pool->queue_cur_num = 0;
    pool->head = NULL;
    pool->tail = NULL;
    if (pthread_mutex_init(&(pool->mutex), NULL))
        handle_error("failed to init mutex");
    if (pthread_cond_init(&(pool->queue_empty), NULL))
        handle_error("failed to init queue_empty");
    if (pthread_cond_init(&(pool->queue_not_empty), NULL))
        handle_error("failed to init queue_not_empty");
    pool->pthreads = malloc(sizeof(pthread_t) * thread_num);
    if (NULL == pool->pthreads)
        handle_error("failed to malloc pthreads");
    pool->pool_close = 0;
    for (int i = 0; i < pool->thread_num; i++)
    {
        pthread_create(&(pool->pthreads[i]), NULL, threadpool_function, (void *)pool);
    }
    return pool;
}

int threadpool_add_job(struct threadpool* pool, int clnt_sock)
{
    pthread_mutex_lock(&(pool->mutex));
    if (pool->pool_close)    //线程池关闭就退出
    {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    struct job *pjob =(struct job*) malloc(sizeof(struct job));
    if (NULL == pjob)
    {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    pjob->clnt_sock = clnt_sock;
    pjob->next = NULL;
    if (pool->head == NULL)
    {
        pool->head = pool->tail = pjob;
        pthread_cond_broadcast(&(pool->queue_not_empty));  //队列空的时候，有任务来时就通知线程池中的线程：队列非空
    }
    else
    {
        pool->tail->next = pjob;
        pool->tail = pjob;
    }
    pool->queue_cur_num++;
    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}


int threadpool_destroy(struct threadpool *pool)
{
    pthread_mutex_lock(&(pool->mutex));
    if (pool->pool_close)   //线程池已经退出了，就直接返回
    {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    
    while (pool->queue_cur_num != 0)
    {
        pthread_cond_wait(&(pool->queue_empty), &(pool->mutex));  //等待队列为空
    }
    
    pool->pool_close = 1;      //置线程池关闭标志
    pthread_mutex_unlock(&(pool->mutex));
    pthread_cond_broadcast(&(pool->queue_not_empty));  //唤醒线程池中正在阻塞的线程
    for (int i = 0; i < pool->thread_num; ++i)
    {
        pthread_join(pool->pthreads[i], NULL);    //等待线程池的所有线程执行完毕
    }
    
    pthread_mutex_destroy(&(pool->mutex));          //清理资源
    pthread_cond_destroy(&(pool->queue_empty));
    pthread_cond_destroy(&(pool->queue_not_empty));
    free(pool->pthreads);
    struct job *p;
    while (pool->head != NULL)
    {
        p = pool->head;
        pool->head = p->next;
        free(p);
    }
    free(pool);
    return 0;
}



int main(){
    // 创建套接字，参数说明：
    //   AF_INET: 使用 IPv4
    //   SOCK_STREAM: 面向连接的数据传输方式
    //   IPPROTO_TCP: 使用 TCP 协议
    int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serv_sock == -1)
        handle_error("socket failed");
    
    // 将套接字和指定的 IP、端口绑定
    //   用 0 填充 serv_addr （它是一个 sockaddr_in 结构体）
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    
    //   设置 IPv4
    //   设置 IP 地址
    //   设置端口
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(BIND_IP_ADDR);
    serv_addr.sin_port = htons(BIND_PORT);
    //   绑定
    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
        handle_error("failed to bind");
    
    // 使得 serv_sock 套接字进入监听状态，开始等待客户端发起请求
    if (listen(serv_sock, SOMAXCONN)==-1)
        handle_error("failed to listen");
    
    // 接收客户端请求，获得一个可以与客户端通信的新的生成的套接字 clnt_sock
    struct sockaddr_in clnt_addr;
    socklen_t clnt_size = sizeof(clnt_addr);
    
    //创建线程池
    struct threadpool *pool = threadpool_init(20);
    
    while (1) {
        // 当没有客户端连接时， accept() 会阻塞程序执行，直到有客户端连接进来
        int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_size);
        // 处理客户端的请求
        if (clnt_sock != -1){
            threadpool_add_job(pool, clnt_sock);
            }
        }
    
    // 实际上这里的代码不可到达，可以在 while 循环中收到 SIGINT 信号时主动 break
    // 关闭套接字
    // 销毁线程池
    close(serv_sock);
    threadpool_destroy(pool);
    return 0;
}
