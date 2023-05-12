#include "thread.h"
#include "server.h"

threadpool_t *threadpool_create(int thread_num, int queue_max_size) {
	int			  i;
	threadpool_t *pool = NULL;

	do {
		pool = (threadpool_t *)malloc(sizeof(threadpool_t));
		if (pool == NULL) {
			printf("malloc threadpool fail\n");
			break;
		}

		pool->thread_num	 = thread_num;
		pool->queue_size	 = 0;
		pool->queue_max_size = queue_max_size;
		pool->queue_front	 = 0;
		pool->queue_rear	 = 0;
		pool->shutdown		 = 0;

		/*根据最大线程上限数，给工作线程数据开辟空间，并清零*/
		pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_num);
		if (pool->threads == NULL) {
			printf("malloc threads fail\n");
			break;
		}
		memset(pool->threads, 0, sizeof(pthread_t) * thread_num);

		/* 队列开辟空间 */
		pool->task_queue = (int *)malloc(sizeof(int) * queue_max_size);
		if (pool->task_queue == NULL) {
			printf("malloc task_queue fail\n");
			break;
		}

		/* 初始化互斥锁，条件变量 */
		if (pthread_mutex_init(&(pool->lock), NULL) != 0) {
			printf("init the lock fail\n");
			break;
		}
		if (pthread_cond_init(&(pool->queue_not_empty), NULL) != 0) {
			printf("init the queue_not_empty fail\n");
			break;
		}
		if (pthread_cond_init(&(pool->queue_empty), NULL) != 0) {
			printf("init the queue_empty fail\n");
			break;
		}

		/* 启动 thread_num 个 work thread */
		for (i = 0; i < thread_num; i++) {
			pthread_create(&(pool->threads[i]), NULL, threadpool_thread,
						   (void *)pool); /*pool指向当前线程池*/
			// printf("start thread  0x%x...\n", (unsigned int)pool->threads[i]);
		}

		return pool;
	} while (0);

	threadpool_free(pool); /*前面代码调用失败，释放poll存储空间*/
	return NULL;
}

void *threadpool_thread(void *threadpool)
{
    threadpool_t *pool = (threadpool_t *)threadpool;
    int task;

    while(1) {
        /* Lock must be taken to wait on condition variable */
        /* 刚创建出线程，等待任务队列里面有任务，否则阻塞等待任务队列里有任务再唤醒
         * 接收任务
         */
        pthread_mutex_lock(&(pool->lock));
        
        /* queue_size == 0 说明没有任务，调wait 阻塞在条件变量上，若有任务，跳过该while */
        while((pool->queue_size == 0) && (!pool->shutdown)) {
            printf("thread 0x%x is waiting\n", (unsigned int)pthread_self());
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->lock));
            
            /* 清除指定数目的空闲线程，如果要结束的线程个数大于0，结束线程 */
            if (pool->wait_exit_thr_num > 0) {
                pool->wait_exit_thr_num--;
                
                /* 如果线程池里的线程个数大于最小值时可以结束当前线程 */
                if (pool->live_thr_num > pool->min_thr_num) {
                    printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
                    pool->live_thr_num--;
                    pthread_mutex_unlock(&(pool->lock));
                    pthread_exit(NULL);
                }
            }
        }
        
        /*如果关闭了线程池，自行退出处理*/
        if (pool->shutdown == 1) {
            pthread_mutex_unlock(&(pool->lock));
            printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
            pthread_exit(NULL);
        }
        
        /*从任务队列里获取任务，是一个出队操作*/
        task.function = pool->task_queue[pool->queue_front].function;
        task.arg = pool->task_queue[pool->queue_front].arg;

        /*出队，模拟环形队列*/
        pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;
        pool->queue_size--;
        
        /*通知可以有新的任务添加进来*/
        pthread_cond_broadcast(&(pool->queue_not_full));
        
        /*任务取出后，立即将线程池锁释放*/
        pthread_mutex_unlock(&(pool->lock));
        
        /*执行任务*/
        printf("thread 0x%x starts woking\n", (unsigned int)pthread_self());
        pthread_mutex_lock(&(pool->thread_counter));    /*忙状态线程数变量锁*/
        pool->busy_thr_num++;                           /*忙状态线程数+1*/
        pthread_mutex_unlock(&(pool->thread_counter));
        (*(task.function))(task.arg);                   /*执行回调函数*/
        
        /*任务结束处理*/
        printf("thread 0x%x ends woking\n", (unsigned int)pthread_self());
        pthread_mutex_lock(&(pool->thread_counter));
        pool->busy_thr_num--;
        pthread_mutex_unlock(&(pool->thread_counter));
    }
    
    return NULL;
}