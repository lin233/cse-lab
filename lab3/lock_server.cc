// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server()
{
  pthread_mutex_init(&mutex, NULL);
}

lock_server::~lock_server(){
  pthread_mutex_destroy(&mutex);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  pthread_mutex_lock(&mutex);
  pthread_mutex_unlock(&mutex); 
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  pthread_mutex_lock(&mutex);
  if(lock_map.find(lid)!=lock_map.end()){
    while(lock_map[lid].is_locked){
      pthread_mutex_unlock(&mutex);
      pthread_mutex_lock(&mutex);
    }
    lock_map[lid].is_locked = true;
    lock_map[lid].clt = clt;
    lock_map[lid].nacquire++;
  }
  else{
    lock_map[lid] = lock_state(true, clt, 1);
  }
  pthread_mutex_unlock(&mutex);
 
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
   pthread_mutex_lock(&mutex);
   if(lock_map.find(lid)!=lock_map.end()&&lock_map[lid].clt==clt&&lock_map[lid].is_locked){
      lock_map[lid].is_locked = false;
      lock_map[lid].nacquire--; 
   }
   pthread_mutex_unlock(&mutex);
   return ret;
}
