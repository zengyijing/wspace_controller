#ifndef PKT_SCHEDULER_H_
#define PKT_SCHEDULER_H_

#include <pthread.h>
#include <string.h>
#include <cstdint>
#include <queue>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <unistd.h>

#include "pthread_wrapper.h"

using namespace std;

class PktQueue {
 public:
  PktQueue() : kMaxSize(0) {}
  PktQueue(size_t max_size);
  ~PktQueue();

  bool Enqueue(const char *pkt, uint16_t len);
  // Note: The caller needs to deallocate buf.
  void Dequeue(char **buf, uint16_t *len);
  // With lock. Return the size of the top packet in bytes.
  // Return 0 if the queue is empty.
  uint16_t PeekTopPktSize();
  int GetLength() { return q_.size(); }
 private:
  bool IsFull() { return q_.size() == kMaxSize; }
  bool IsEmpty() { return q_.empty(); }
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }
  void WaitEmpty() { Pthread_cond_wait(&empty_cond_, &lock_); }
  void SignalEmpty() { Pthread_cond_signal(&empty_cond_); }

  size_t kMaxSize;
  queue<pair<char*, uint16_t> > q_;  // <Packet buffer address, length>.
  pthread_mutex_t lock_;
  pthread_cond_t empty_cond_;
};

// Track clients with non-empty queues.
class ActiveList {
 public:
  ActiveList();
  ~ActiveList();

  // Append id to the end when the id doesn't exist.
  void Append(int id);
  // Remove the oldest id from the list.
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
    kEqualTime = 0,
    kProportionalThroughput = 1,
    kEqualThroughput = 2,
  };

  struct Status {
    double throughput;
    double quantum;  // time slot to send packets in microseconds.
    double counter;
    //unsigned long pkt_count;
    Status() : throughput(0.0), quantum(0.0), counter(0.0)/*, pkt_count(0)*/ {} 
    Status(double throughput, double quantum, double counter) :
        throughput(throughput), quantum(quantum), counter(counter)/*, pkt_count(0)*/ {}
    friend bool operator==(const Status &l, const Status &r);
  }; 

  PktScheduler(double min_throughput, 
         const vector<int> &client_ids, 
         uint32_t pkt_queue_size,
         uint32_t round_interval, 
         const FairnessMode &fairness_mode);
  ~PktScheduler();

  void Enqueue(const char *pkt, uint16_t len, int client_id);
  void Dequeue(vector<pair<char*, uint16_t> > *pkts, int *client_id);
  // throughputs: <client_id, throughput>.
  void ComputeQuantum(const unordered_map<int, double> &throughputs);

  void set_fairness_mode(const FairnessMode &mode) { fairness_mode_ = mode; }
  FairnessMode fairness_mode() const { return fairness_mode_; }
  
  unordered_map<int, Status> stats();
  void PrintStats();

 private:
  // No lock.
  void ComputeQuantumEqualTime();
  void ComputeQuantumProportionalThroughput();
  void ComputeQuantumEqualThroughput();
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }

  const double kMinThroughput;
  vector<int> client_ids_;
  unordered_map<int, PktQueue*> queues_;   // <client_id, pkt_queue>.
  unordered_map<int, Status>    stats_;    // <client_id, status>. 
  ActiveList active_list_;
  // Duration of scheduling all the clients per round in microseconds.
  uint32_t round_interval_;
  FairnessMode fairness_mode_;
  pthread_mutex_t lock_;  // For stats.
}; 

#endif
