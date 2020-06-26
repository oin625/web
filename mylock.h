//#ifdef _MYLOCK_H
//#define _MYLOCK_H

#include <iostream>
#include <list>
#include <cstdio>
#include <semaphore.h>
#include <exception>
#include <pthread.h>
#include "myhttp_coon.h"
using namespace std;

//封装信号量
class sem{
	private:
		sem_t m_sem;
	public:
		sem();
		~sem();
		bool wait();
		bool post();
};

sem :: sem(){
	if(sem_init(&m_sem, 0, 0)!=0){
		throw std:: exception();
	}
}

sem :: ~sem(){
	sem_destroy(&m_sem);
}

bool sem:: wait(){
	return sem_wait(&m_sem) == 0;
}

bool sem::post(){
	return sem_post(&m_sem) == 0;
}

//封装互斥锁

class mylocker{
	private :
		pthread_mutex_t m_mutex;
	public:
		mylocker();
		~mylocker();
		bool lock();
		bool unlock();
};

mylocker :: mylocker(){
	if(pthread_mutex_init(&m_mutex, NULL)!= 0){
		throw std:: exception();
	}
}

mylocker:: ~mylocker(){
	pthread_mutex_destroy(&m_mutex);
}

bool mylocker:: lock(){
	return pthread_mutex_lock(&m_mutex) == 0;
}

bool mylocker:: unlock(){
	return pthread_mutex_unlock(&m_mutex) == 0;
}

//条件变量就暂时不用了
