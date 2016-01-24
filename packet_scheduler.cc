#include <algorithm>

#include "packet_scheduler.h"

PktQueue::PktQueue(size_t max_size) : kMaxSize(max_size) {
  Pthread_mutex_init(&lock_, NULL);
  Pthread_cond_init(&empty_cond_, NULL);
}

PktQueue::~PktQueue() {
  Lock();
  while (!q_.empty()) {
	  delete[] q_.front().first;
	  q_.pop();
  }
  UnLock();
  Pthread_mutex_destroy(&lock_);
  Pthread_cond_destroy(&empty_cond_);
}

void PktQueue::Enqueue(const char *pkt, uint16_t len) {
  if (len <= 0) {
    perror("PktQueue::Enqueue invalid len\n");
    exit(0);
  }
  Lock();
  while (IsFull()) {
	  WaitEmpty();
  }	
  char *buf = new char[len];
  memcpy(buf, pkt, len);
  q_.push(make_pair(buf, len));
  UnLock();
}

void PktQueue::Dequeue(char **buf, uint16_t *len) {
  Lock();
  assert(!q_.empty());
  *buf  = q_.front().first;
  *len = q_.front().second; 
  q_.pop();
  SignalEmpty();
  UnLock();
}

bool PktQueue::IsEmpty() { 
  Lock();
  bool is_empty = q_.empty(); 
  UnLock();
  return is_empty;
}

uint16_t PktQueue::PeekHeadSize() {
  Lock();
  uint16_t len = q_.front().second;
  UnLock();
  return len;
}

ActiveList::ActiveList() {
  Pthread_mutex_init(&lock_, NULL);
  Pthread_cond_init(&fill_cond_, NULL);
}

ActiveList::~ActiveList() {
  Pthread_mutex_destroy(&lock_);
  Pthread_cond_destroy(&fill_cond_);
}

void ActiveList::Add(int id) {
  Lock();
  ids_.push(id);
  exist_.insert(id);
  SignalFill();
  UnLock();
}

int ActiveList::Remove() {
  Lock();
  while (IsEmpty()) {
    WaitFill();
  }
  int id = ids_.front();
  ids_.pop();
  exist_.erase(id);
  UnLock();
  return id;
}

PktScheduler::PktScheduler(double min_throughput, 
                           const vector<int> &client_ids, 
                           uint32_t pkt_queue_size,
                           uint32_t round_interval, 
                           const FairnessMode &fairness_mode) 
    : kMinThroughput(min_throughput), client_ids_(client_ids), 
      round_interval_(round_interval), fairness_mode_(fairness_mode) {
  for (auto client_id : client_ids) {
    queues_[client_id] = PktQueue(pkt_queue_size);
    stats_[client_id].quantum = round_interval_/client_ids_.size(); 
    stats_[client_id].throughputs = kMinThroughput;  // At least 1Mbps.
  }
  Pthread_mutex_init(&lock_, NULL);
}

PktScheduler::~PktScheduler() {
  Pthread_mutex_destroy(&lock_);
}

void PktScheduler::Enqueue(const char *pkt, uint16_t len, int client_id) {
  queues_[client_id].Enqueue(pkt, len); 
  active_list_.Add(client_id);
}

void PktScheduler::Dequeue(vector<pair<char*, uint16_t> > &pkts, int *client_id) {
  *client_id = active_list_.Remove();
  pkts.clear();
  Lock();
  stats_[*client_id].counter += stats_[*client_id].quantum;
  char *pkt = NULL;
  uint16_t len = 0;
  while (!queues_[*client_id].IsEmpty() && 
          queues_[*client_id].PeekHeadSize() < stats_[*client_id].counter) {
    queues_.Dequeue(&pkt, &len);
    pkts.push_back({pkt, len});
    stats_[*client_id].counter -= queues_[*client_id].PeekHeadSize();
  } 
  if (queues_[*client_id].IsEmpty()) {
    stats_[*client_id].counter = 0;
  } else {
    active_list_.Add(*client_id);
  }
  UnLock();
}

void PktScheduler::UpdateStatus(const unordered_map<int, double> &throughputs) {
  Lock();
  for (const auto &p : throughputs) {
    stats_[p.first] = max(kMinThroughput, p.second);
  }
  ComputeQuantum();
  UnLock();
}

void PktScheduler::ComputeQuantum() {
  switch(fairness_mode()) {
    case kThroughputFair:
      ComputeQuantumThroughputFair();
      break;

    default:
      assert(false);
  }
}

void PktScheduler::ComputeQuantumThroughputFair() {
  double total_throughput = 0;
  for (auto client_id : client_ids_) {
    total_throughput += stats_[client_id].throughput;
  }
  for (auto client_id : client_ids_) {
    stats_[client_id].quantum = (int)stat_[client_id].throughput / total_throughput * round_interval_;
  }
}
