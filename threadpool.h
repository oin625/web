//#ifdef _THREADPOOL_H
//#define _THREADPOOL_H
#include <iostream>
#include <list>
#include <cstdio>
#include <semaphore.h>
#include <exception>
#include <pthread.h>
//#include "myhttp_coon.h"
#include "mylock.h"

template<typename T>
class threadpool{
	private:
		int max_thread;
		int max_job;
		pthread_t *pthread_pool;
		list<T*> m_myworkqueue;
		mylocker m_queuelocker;
		sem m_queuestat;
		bool m_stop; //是否结束进程

		static void * worker(void *arg);
		void run();
	public:
		threadpool();
		~threadpool();
		bool addjob(T* request);
		
};
template<typename T>
threadpool<T> :: threadpool(){
	max_thread = 8;
	max_job = 1000;
	m_stop = false;
	pthread_pool = new pthread_t[max_thread];
	if(!pthread_pool){
		throw std:: exception();
	}
	for(int i =0; i<max_thread; ++i){
		std::cout << "create the pthread:"<<i<<std::endl;
		if(pthread_create(pthread_pool+i, NULL, worker, this)!=0){//返回0说明创建成功。
			delete []pthread_pool;
			throw std::exception();
		}
		if(pthread_detach(pthread_pool[i])){
			delete []pthread_pool;
			throw std::exception();
		}
	}
}

template<typename T>
threadpool<T> :: ~threadpool(){
	delete []pthread_pool;
	m_stop = true;
}

template<typename T>
bool threadpool<T> :: addjob(T* request){
	m_queuelocker.lock();
	//临界区
	if(m_myworkqueue.size() > max_job){
		m_queuelocker.unlock();
		return false;
	}
	m_myworkqueue.push_back(request);
	m_queuelocker.unlock();
	m_queuestat.post();//信号量+1
	return true;
}

template<typename T>
void * threadpool<T> :: worker(void *arg){
	threadpool *pool = (threadpool*) arg;
	pool -> run();
	return pool;
}

template<typename T>
void threadpool<T> :: run(){
	while(!m_stop){
		m_queuestat.wait();//信号量-1.到0线程挂起等待
		m_queuelocker.lock();
		if(m_myworkqueue.empty()){
			m_queuelocker.unlock();
			continue;
		}
		T* request = m_myworkqueue.front();
		m_myworkqueue.pop_front();
		m_queuelocker.unlock();
		if(!request) {
			continue;
		}
		request -> doit();
	}
}

//#endif


