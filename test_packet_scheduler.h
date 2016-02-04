#ifndef test_pkt_scheduler_H_
#define test_pkt_scheduler_H_

#include <stdio.h>
#include <string>
#include <iostream>
#include <cassert>
#include <queue>
#include <utility>
#include <unistd.h>

#include "packet_scheduler.h"

void TestPktQueue();
void* LaunchEnqueue(void* arg);
void* LaunchDequeue(void* arg);

void TestActiveList();
void* LaunchAppend(void* arg);
void* LaunchRemove(void* arg);

struct QuantumTest {
  PktScheduler *scheduler;
  unordered_map<int, PktScheduler::Status> stats;
  unordered_map<int, double> throughputs;
};
void TestComputeQuantum();

struct SchedulerQueueTest {
  PktScheduler *scheduler;
  vector<pair<int, string> > tx_pkts;    // <client_id, pkt>.
  vector<pair<int, string> > rx_pkts;    // <client_id, pkt>.
  vector<pair<int, string> > gold_pkts;  // <client_id, pkt>.
  bool go;
  pthread_mutex_t lock;
  pthread_cond_t go_cond;
  pthread_t p_enqueue, p_dequeue;

  SchedulerQueueTest() : go(false) {
    Pthread_mutex_init(&lock, NULL);
    Pthread_cond_init(&go_cond, NULL);
  }

  ~SchedulerQueueTest() {
    Pthread_mutex_destroy(&lock);
    Pthread_cond_destroy(&go_cond);
  }

  void Lock() { Pthread_mutex_lock(&lock); }
  void UnLock() { Pthread_mutex_unlock(&lock); }
  void WaitGo() { Pthread_cond_wait(&go_cond, &lock); }
  void SignalGo() { Pthread_cond_signal(&go_cond); }
};

void TestSchedulerQueue();
void* LaunchSchedulerEnqueue(void* arg);
void* LaunchSchedulerDequeue(void* arg);

#endif
