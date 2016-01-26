#include "test_packet_scheduler.h"

PktQueue *pkt_queue;
vector<string> rx_pkts;

ActiveList *active_l;
vector<int> rx_ids;

void* LaunchEnqueue(void* arg) {
  //sleep(3);
  vector<string> *pkts = (vector<string> *)arg;
  for (auto pkt : *pkts) {
	pkt_queue->Enqueue(pkt.c_str(), pkt.size());
	cout << "Push pkt: " << pkt << endl;
	sleep(1);
  }
}

void* LaunchDequeue(void* arg) {
  sleep(3);
  char *buf = NULL;
  size_t len = 0, len1 = 0;
  size_t cnt = *(size_t*)arg;
  while (rx_pkts.size() < cnt) {
    len = pkt_queue->PeekTopPktSize();
    if (len) {
	  pkt_queue->Dequeue(&buf, (uint16_t*)&len1);
      assert(len == len1);
      string s(buf, len);
	  cout << "Pop pkt: " << s << endl;
	  rx_pkts.push_back(s);
      delete[] buf;
    };
    usleep(100000);
  }      
}

void TestPktQueue() {
  cout << "Launch TestPktQueue" << endl;
  pkt_queue = new PktQueue(5);
  //vector<string> tx_pkts = {"a", "bc", "bbaa", "a12156", "151a2", "16a21", "aw714#", "haha", "try"};
  vector<string> tx_pkts = {"1", "2", "30", "4", "55", "666", "77", "88888"};
  size_t num_pkts = tx_pkts.size();
  pthread_t p_enqueue, p_dequeue;
  Pthread_create(&p_enqueue, NULL, LaunchEnqueue, &tx_pkts);
  Pthread_create(&p_dequeue, NULL, LaunchDequeue, &num_pkts);
  
  Pthread_join(p_enqueue, NULL);
  Pthread_join(p_dequeue, NULL);

  assert(tx_pkts == rx_pkts);
  assert(pkt_queue->PeekTopPktSize() == 0);
  cout << "TestPktQueue good!" << endl;
  delete pkt_queue;
}

void* LaunchAppend(void* arg) {
  sleep(3);
  vector<int> *ids = (vector<int> *)arg;
  for (auto id : *ids) {
    active_l->Append(id);
	cout << "Push id: " << id << endl;
    usleep(100000);
    if (id == 50) {
      sleep(2);
    }
  }
}

void* LaunchRemove(void* arg) {
  //sleep(3);
  size_t cnt = *(size_t*)arg;
  while (rx_ids.size() < cnt) {
    int id = active_l->Remove(); 
    rx_ids.push_back(id);
    cout << "Pop id: " << id << endl;
	sleep(1);
  }      
}

void TestActiveList() {
  cout << "Launch TestActiveList" << endl;
  active_l = new ActiveList();
  //vector<int> tx_ids = {10, 20, 30, 40, 50, 60}; 
  vector<int> tx_ids = {10, 20, 20, 30, 40, 30, 50, 60, 20, 30, 20, 100}; 
  vector<int> rx_gold = {10, 20, 30, 40, 50, 60, 20, 30, 100};
  size_t count = rx_gold.size();
  pthread_t p_append, p_remove;
  Pthread_create(&p_append, NULL, LaunchAppend, &tx_ids);
  Pthread_create(&p_remove, NULL, LaunchRemove, &count);
  
  Pthread_join(p_append, NULL);
  Pthread_join(p_remove, NULL);

  assert(rx_ids == rx_gold);
  cout << "TestActiveList good!" << endl;
  delete active_l;
}

void TestComputeQuantum() {
  cout << "Launch TestComputeQuantum" << endl;
  QuantumTest tests[3];
  tests[0].scheduler = new PktScheduler(1.0, {1, 2, 3}, 5, 1000, PktScheduler::kThroughputFair);
  tests[0].stats[1] = {1.5, 333, 0};
  tests[0].stats[2] = {2, 444, 0};
  tests[0].stats[3] = {1, 222, 0};
  tests[0].throughputs[1] = 1.5;
  tests[0].throughputs[2] = 2;
  tests[0].throughputs[3] = 0.5;

  tests[1].scheduler = new PktScheduler(1.0, {1, 2, 3}, 5, 1000, PktScheduler::kThroughputFair);
  tests[1].stats[1] = {5, 625, 0};
  tests[1].stats[2] = {2, 250, 0};
  tests[1].stats[3] = {1, 125, 0};
  tests[1].throughputs[1] = 5;
  tests[1].throughputs[2] = 2;
  tests[1].throughputs[3] = 1;

  tests[2].scheduler = new PktScheduler(1.0, {1, 2, 3}, 5, 1000, PktScheduler::kEqualQuantum);
  tests[2].stats[1] = {5, 333, 0};
  tests[2].stats[2] = {2, 333, 0};
  tests[2].stats[3] = {1, 333, 0};
  tests[2].throughputs[1] = 5;
  tests[2].throughputs[2] = 2;
  tests[2].throughputs[3] = 1;

  for (auto &test : tests) {
	test.scheduler->ComputeQuantum(test.throughputs);
	auto stats = test.scheduler->stats();
    cout << "Mode: " << test.scheduler->fairness_mode() << endl;
	test.scheduler->PrintStats();
	assert(stats == test.stats);
    delete test.scheduler;
  } 
  cout << "TestComputeQuantum good!" << endl;
}

void* LaunchSchedulerEnqueue(void* arg) {
  SchedulerQueueTest *test = (SchedulerQueueTest*)arg;
  for (const auto &p : test->tx_pkts) {
    if (p.second == "STOP") {
      cout << "===Wait Push===" << endl;
      test->Lock(); 
      while (!test->go) {
        test->WaitGo();
      }
      test->UnLock();
    }
    const char *pkt = p.second.c_str();
    uint16_t len = p.second.size(); 
    test->scheduler->Enqueue(pkt, len, p.first);
    cout << "Push: id: " << p.first << " pkt: " << p.second << endl;
  }
}

void* LaunchSchedulerDequeue(void* arg) {
  SchedulerQueueTest *test = (SchedulerQueueTest*)arg;
  sleep(5);
  cout << "Start pop!" << endl;
  vector<pair<char*, uint16_t> > pkts;
  int client_id = 0;
  while (true) {
    test->scheduler->Dequeue(&pkts, &client_id);
    for (auto &p : pkts) {
      string s(p.first, p.second);
      delete[] p.first;
      if (s == "goahead ") {
        cout << "===Signal Push===" << endl;
        test->Lock();
        test->go = true;
        test->SignalGo();
        test->UnLock();
      }
      cout << "Pop: id: " << client_id << " pkt: " << s << endl;
      test->rx_pkts.push_back({client_id, s});
      if (s == "EX") return NULL;
      usleep(100000);
    }
  }
}

void TestSchedulerQueue() {
  cout << "Launch TestSchedulerQueue" << endl;
  SchedulerQueueTest test;
  vector<int> clients = {1, 2, 3};
  uint32_t per_client_interval = 50; 
  uint32_t interval = per_client_interval * clients.size();
  test.scheduler = new PktScheduler(2.0, clients, 5, interval, PktScheduler::kEqualQuantum);
  // Round 1.
  test.tx_pkts.push_back({1, "ab"});
  test.tx_pkts.push_back({3, "abcd"});
  test.tx_pkts.push_back({2, "abcdef"});
  test.tx_pkts.push_back({1, "abcd"});
  test.tx_pkts.push_back({1, "abcdef"});
  test.tx_pkts.push_back({2, "dcba"});
  test.tx_pkts.push_back({3, "XL"});

  test.gold_pkts.push_back({1, "ab"});
  test.gold_pkts.push_back({1, "abcd"});
  test.gold_pkts.push_back({1, "abcdef"});
  test.gold_pkts.push_back({3, "abcd"});
  test.gold_pkts.push_back({3, "XL"});
  test.gold_pkts.push_back({2, "abcdef"});
  test.gold_pkts.push_back({2, "dcba"});

  // Round 2.
  test.tx_pkts.push_back({2, "abcdef"});
  test.tx_pkts.push_back({1, "fedcba"});
  test.tx_pkts.push_back({1, "ab"});
  test.tx_pkts.push_back({1, "abcd"});
  test.tx_pkts.push_back({2, "goahead "});
  test.tx_pkts.push_back({1, "a"});

  test.gold_pkts.push_back({1, "fedcba"});
  test.gold_pkts.push_back({1, "ab"});
  test.gold_pkts.push_back({1, "abcd"});
  test.gold_pkts.push_back({1, "a"});
  test.gold_pkts.push_back({2, "abcdef"});
  test.gold_pkts.push_back({2, "goahead "});

  // Round 3
  test.tx_pkts.push_back({2, "abcdef"});
  test.tx_pkts.push_back({1, "abcd"});
  test.tx_pkts.push_back({1, "cd"});
  test.tx_pkts.push_back({2, "ab"});
  test.tx_pkts.push_back({2, "abcd"});
  test.tx_pkts.push_back({2, "d"});
  test.tx_pkts.push_back({3, "STOP"});
  test.tx_pkts.push_back({3, "ab"});
  test.tx_pkts.push_back({3, "cd"});
  
  test.gold_pkts.push_back({1, "abcd"});
  test.gold_pkts.push_back({1, "cd"});
  test.gold_pkts.push_back({2, "abcdef"});
  test.gold_pkts.push_back({2, "ab"});
  test.gold_pkts.push_back({2, "abcd"});
  test.gold_pkts.push_back({2, "d"});
  test.gold_pkts.push_back({3, "STOP"});
  test.gold_pkts.push_back({3, "ab"});
  test.gold_pkts.push_back({3, "cd"});

  // Round 4
  test.tx_pkts.push_back({3, "abcdef"});
  test.tx_pkts.push_back({3, "EX"});

  test.gold_pkts.push_back({3, "abcdef"});
  test.gold_pkts.push_back({3, "EX"});

  // Start threads.
  Pthread_create(&test.p_enqueue, NULL, LaunchSchedulerEnqueue, &test);
  Pthread_create(&test.p_dequeue, NULL, LaunchSchedulerDequeue, &test);
  
  Pthread_join(test.p_enqueue, NULL);
  Pthread_join(test.p_dequeue, NULL);

  assert(test.rx_pkts == test.gold_pkts);
  cout << "TestSchedulerQueue good!" << endl;
  
  delete test.scheduler;
}

int main(int argc, char **argv) {
  //TestPktQueue();
  //TestActiveList();
  //TestComputeQuantum();
  TestSchedulerQueue();
  return 0;
}
