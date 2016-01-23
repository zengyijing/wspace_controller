#ifndef PKT_SCHEDULER_H_
#define PKT_SCHEDULER_H_

#include <cstdint>
#include <pthread.h>
#include <queue>
#include <utility>
#include <unordered_map>

#include "pthread_wrapper.h"

using namespace std;

class PktQueue {
 public:
  PktQueue(size_t max_size);
  ~PktQueue();

  void PushPkt(const char *pkt, uint16_t len);
  bool PopPkt(char *buf, uint16_t *len);

 private:
  bool IsFull() { return q_.size() == kMaxSize; }
  bool IsEmpty() { return q_.empty(); }
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }
  void WaitEmpty() { Pthread_cond_wait(&empty_cond_, &lock_); }
  void SignalEmpty() { Pthread_cond_signal(&empty_cond_); }

  const size_t kMaxSize;
  queue<pair<char*, uint16_t> > q_;  // <Packet buffer address, length>.
  pthread_mutex_t lock_;
  pthread_cond_t empty_cond_;
};

class PktScheduler {
 public:
  PktScheduler() {}
  PktScheduler(uint32_t round_interval);
  ~PktScheduler();

  void Init();
  void UpdateQuantum(const unordered_map<int, double> &throughputs);
  void PushPkt(const char *pkt, uint16_t len, int client_id);
  void PopPkt(char *buf, uint16_t *len, int *client_id);

 private:
  class Quantum {
   public:
    Quantum();
    ~Quantum();
    
    void set_quantum();
    int quantum();

   private:
    int q_;
    pthread_mutex_t lock_;  
  }; 

  int ScheduleClient();

  vector<int> client_ids_;
  int ind_;  // index of the current client to be scheduled.
  unordered_map<int, PktQueue> queues_;   // <client_id, pkt_queue>.
  unordered_map<int, Quantum> quantums_;  // <client_id, quantum>. 
  unordered_map<int, int> counters_;      // <client_id, deficit counter>. 
  uint32_t round_interval_;                 // in milliseconds.
}; 

#endif
