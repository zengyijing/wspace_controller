#include <algorithm>
#include "packet_scheduler.h"

PktQueue::PktQueue(size_t max_size) : kMaxSize(max_size) {
  Pthread_mutex_init(&lock_, NULL);
  Pthread_cond_init(&empty_cond_, NULL);
  Pthread_cond_init(&fill_cond_, NULL);
}

PktQueue::~PktQueue() {
  Lock();
  while (!q_.empty()) {
    delete[] q_.front().second.first;
    q_.pop();
  }
  UnLock();
  Pthread_mutex_destroy(&lock_);
  Pthread_cond_destroy(&empty_cond_);
  Pthread_cond_destroy(&fill_cond_);
}

bool PktQueue::Enqueue(const char *pkt, uint16_t len, int client_id) {
  if (len <= 0) {
    perror("PktQueue::Enqueue invalid len\n");
    exit(0);
  }
  Lock();
  if (IsFull()) {
    UnLock();
    return false;
  } else {
    char *buf = new char[len];
    memcpy(buf, pkt, len);
    q_.push(make_pair(client_id, make_pair(buf, len)));
    SignalFill();
    UnLock();
    return true;
  }
}

void PktQueue::Dequeue(char *&buf, uint16_t *len, int *client_id) {
  Lock();
  if(IsEmpty()) {
    WaitFill();
  }
  *client_id = q_.front().first;
  buf  = q_.front().second.first;
  *len = q_.front().second.second; 
  q_.pop();
  SignalEmpty();
  UnLock();
}

uint16_t PktQueue::PeekTopPktSize() {
  uint16_t len = 0;
  Lock();
  if (!IsEmpty()) {
    len = q_.front().second.second;
  }
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

void ActiveList::Append(int id) {
  Lock();
  if (exist_.count(id) == 0) {
    ids_.push(id);
    exist_.insert(id);
    SignalFill();
  }
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

bool operator==(const PktScheduler::Status &l, const PktScheduler::Status &r) {
  return l.throughput == r.throughput && l.quantum == r.quantum && 
         l.counter == r.counter;
}

PktScheduler::PktScheduler(double min_throughput, 
                           const vector<int> &client_ids, 
                           uint32_t pkt_queue_size,
                           uint32_t round_interval, 
                           const FairnessMode &fairness_mode) 
    : kMinThroughput(min_throughput), client_ids_(client_ids), sending_queue_(pkt_queue_size),
      round_interval_(round_interval), fairness_mode_(fairness_mode) {
  for (auto client_id : client_ids_) {
    queues_[client_id] = new PktQueue(pkt_queue_size);
    stats_[client_id].quantum = round_interval_/client_ids_.size(); 
    stats_[client_id].throughput = kMinThroughput;  // At least 1Mbps.
  }
  Pthread_mutex_init(&lock_, NULL);
}

PktScheduler::~PktScheduler() {
  for (auto client_id : client_ids_) {
    delete queues_[client_id];
  }
  Pthread_mutex_destroy(&lock_);
}

void PktScheduler::EnqueueToBuf(const char *pkt, uint16_t len, int client_id) {
  if (queues_[client_id]->Enqueue(pkt, len, client_id)) {
    //printf("enqueue for client:%d\n", client_id);
    active_list_.Append(client_id);
  }
}

void PktScheduler::DequeueFromBuf() {
  int client_id = active_list_.Remove();
  Lock();
  stats_[client_id].counter += stats_[client_id].quantum;
  char *pkt = NULL;
  uint16_t len = 0;
  while ((len = queues_[client_id]->PeekTopPktSize()) > 0) {
    int pkt_duration = len * 8.0 / stats_[client_id].throughput;
    // Not enough time to send this packet.
    if (stats_[client_id].counter < pkt_duration) break;
    int client = 0;  
    queues_[client_id]->Dequeue(pkt, &len, &client);
    assert(client == client_id);
    EnqueueToSend(pkt, len, client_id);
    delete[] pkt;
    stats_[client_id].counter -= pkt_duration;
    usleep(pkt_duration);
    //printf("Dequeue: %d len: %u pkt_dur: %u cnt: %u pkt_count: %u queue_size: %d\n",
    //       client_id, len, pkt_duration, stats_[client_id].counter, ++stats_[client_id].pkt_count, queues_[client_id]->GetLength());
  } 
  if (len == 0) {  // Empty queue.
    stats_[client_id].counter = 0;
  } else {
    active_list_.Append(client_id);
  }
  UnLock();
}

void PktScheduler::EnqueueToSend(const char *pkt, uint16_t len, int client_id) {
  if (sending_queue_.Enqueue(pkt, len, client_id) == false)
    Perror("Sending queue is full!\n");
}

void PktScheduler::DequeueToSend(char *&pkt, uint16_t *len, int *client_id) {
  sending_queue_.Dequeue(pkt, len, client_id);
}

void PktScheduler::ComputeQuantum(const unordered_map<int, double> &throughputs) {
  Lock();
  for (const auto &p : throughputs) {
    stats_[p.first].throughput = max(kMinThroughput, p.second);
  }
  switch(fairness_mode()) {
    case kEqualTime:
      ComputeQuantumEqual();
      break;

    case kProportionalThroughput:
      ComputeQuantumThroughputFair();
      break;

    case kEqualThroughput:
      ComputeQuantumEqualThroughput();
      break;

    default:
      assert(false);
  }
  UnLock();
}

void PktScheduler::ComputeQuantumThroughputFair() {
  double total_throughput = 0;
  for (auto client_id : client_ids_) {
    total_throughput += stats_[client_id].throughput;
  }
  for (auto client_id : client_ids_) {
    stats_[client_id].quantum = stats_[client_id].throughput / total_throughput * round_interval_;
  }
}

void PktScheduler::ComputeQuantumEqualThroughput() {
  double total_throughput = 0;
  for (auto client_id : client_ids_) {
    total_throughput += stats_[client_id].throughput;
  }
  double *proportion = new double[client_ids_.size()];
  double sum = 0;
  for (int i = 0; i < client_ids_.size(); ++i) {
    proportion[i] = total_throughput / stats_[client_ids_[i]].throughput;
    sum += proportion[i];
  }
  for (int i = 0; i < client_ids_.size(); ++i) {
    stats_[client_ids_[i]].quantum = proportion[i] / sum * round_interval_;
  }
  delete[] proportion; 
}

void PktScheduler::ComputeQuantumEqual() {
  for (auto client_id : client_ids_) {
    stats_[client_id].quantum = round_interval_/client_ids_.size(); 
  }
}

unordered_map<int, PktScheduler::Status> PktScheduler::stats() {
  Lock();
  unordered_map<int, Status> tmp = stats_;
  UnLock();
  return tmp;
}

void PktScheduler::PrintStats() {
  Lock();
  printf("===PktScheduler: client: throughput quantum counter===\n");
  for (auto client_id: client_ids_) {
    printf("%d: %3f %d %d\n", client_id, stats_[client_id].throughput, 
                           stats_[client_id].quantum, stats_[client_id].counter);
  }
  UnLock();
}
