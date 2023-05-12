#include <pthread.h>

typedef struct threadpool_s {
	pthread_mutex_t lock; /*用于锁住本结构体*/
	pthread_cond_t
		queue_empty; /*当队列任务满时，添加任务的线程阻塞，等待此条件变量*/
	pthread_cond_t queue_not_empty; /*任务队列不为空时，通知等待任务的线程*/

	pthread_t *threads;	   /*存放线程池中每个线程的tid，数组*/
	int		  *task_queue; /*任务队列*/

	int thread_num; /*线程池线程数*/

	int queue_front;	/*task_queue队头下表*/
	int queue_rear;		/*task_queue队尾下表*/
	int queue_size;		/*task_queue队列中实际任务数*/
	int queue_max_size; /*task_queue队列可容纳任务上限*/

	int shutdown; /*标志位，线程池使用状态，true或者false*/
} threadpool_t;

threadpool_t *threadpool_create(int thread_num, int queue_max_size);
void		 *threadpool_thread(void *threadpool);
int			  threadpool_add(threadpool_t *pool, int client_socket);
int			  threadpool_distory(threadpool_t *pool);
int			  threadpool_free(threadpool_t *pool);