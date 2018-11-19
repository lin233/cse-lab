// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

class lock_state {
public:
    bool is_locked;  
    int clt;  
    int nacquire;  
    lock_state() {}
    lock_state(bool i, int c, int n): is_locked(i), clt(c), nacquire(n) {}
};


class lock_server {

 protected:
  //int nacquire;
  pthread_mutex_t mutex;
  std::map<lock_protocol::lockid_t, lock_state> lock_map;

 public:
  lock_server();
  ~lock_server();
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







