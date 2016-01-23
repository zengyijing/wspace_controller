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

void PktQueue::PushPkt(const char *pkt, uint16_t len) {
  if (len <= 0) {
    perror("PktQueue::PushPkt invalid len\n");
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

// Return false if the queue is empty.
bool PktQueue::PopPkt(char *buf, uint16_t *len) {
  Lock();
  bool found = !IsEmpty();
  if (found) {
    char *pkt = q_.front().first;
    *len = q_.front().second; 
    memcpy(buf, pkt, *len); 
    delete[] pkt;
    q_.pop();
    SignalEmpty();
  }
  UnLock();
  return found;
}
