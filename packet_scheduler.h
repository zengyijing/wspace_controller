#ifndef PKT_SCHEDULER_H_
#define PKT_SCHEDULER_H_

#include <cstdint>
#include <pthread.h>
#include <queue>
#include <utility>
#include <unordered_map>
#include <unordered_set>

#include "pthread_wrapper.h"

#define MAX_PKT_SIZE 1500

using namespace std;

class PktQueue {
 public:
  PktQueue(size_t max_size);
  ~PktQueue();

  void Enqueue(const char *pkt, uint16_t len);
  // Note: Depend on the caller to deallocate buf.
  void Dequeue(char **buf, uint16_t *len);
  bool IsEmpty();
  uint16_t PeekHeadSize();

 private:
  bool IsFull() { return q_.size() == kMaxSize; }
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }
  void WaitEmpty() { Pthread_cond_wait(&empty_cond_, &lock_); }
  void SignalEmpty() { Pthread_cond_signal(&empty_cond_); }

  const size_t kMaxSize;
  queue<pair<char*, uint16_t> > q_;  // <Packet buffer address, length>.
  pthread_mutex_t lock_;
  pthread_cond_t empty_cond_;
};

// Track clients with non-empty queues.
class ActiveList {
 public:
  ActiveList();
  ~ActiveList();

  void Add(int id);
  // Remove oldest id from the list.
  int Remove(); 

 private:
  bool IsEmpty() { return ids_.empty(); }
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }
  void WaitFill() { Pthread_cond_wait(&fill_cond_, &lock_); }
  void SignalFill() { Pthread_cond_signal(&fill_cond_); }
  
  queue<int> ids_;  // Track the order of incoming active flows.
  unordered_set<int> exist_;
  pthread_mutex_t lock_;
  pthread_cond_t fill_cond_;
};

class PktScheduler {
 public:
  enum FairnessMode {
    kThroughputFair = 0,
  };

  PktScheduler(double min_throughput);
  ~PktScheduler();

  void Enqueue(const char *pkt, uint16_t len, int client_id);
  void Dequeue(char *buf, uint16_t *len, int *client_id);
  void UpdateStatus(const unordered_map<int, double> &throughputs);
  // No lock for stats.
  void ComputeQuantum();

  void set_fairness_mode(const FairnessMode &mode) { fairness_mode_ = mode; }
  FairnessMode fairness_mode() const { return fairness_mode_; }

 private:
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }

  struct Status {
    double throughput;
    uint32_t quantum:
    uint32_t counter:
    Status() : throughput(0.0), quantum(0), counter(0) {} 
  }; 

  void ComputeQuantumThroughputFair();

  const double kMinThroughput;
  vector<int> client_ids_;
  unordered_map<int, PktQueue> queues_;   // <client_id, pkt_queue>.
  unordered_map<int, Status>   stats_;    // <client_id, status>. 
  ActiveList active_list_;
  // Duration of scheduling all the clients per round in microseconds.
  uint32_t round_interval_;
  FairnessMode fairness_mode_;
  pthread_mutex_t lock_;  // For stats.
}; 

#endif
