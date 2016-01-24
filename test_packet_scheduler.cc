#include "test_packet_scheduler.h"

PktQueue *pkt_queue;
vector<string> rx_pkts;

void* LaunchEnqueue(void* arg) {
  vector<string> *pkts = (vector<string> *)arg;
  for (auto pkt : *pkts) {
	pkt_queue->Enqueue(pkt.c_str(), pkt.size());
	cout << "Push pkt: " << pkt << endl;
	sleep(1);
  }
}

void* LaunchDequeue(void* arg) {
  sleep(10);
  char buf[1500] = {0};
  size_t len = 0;
  vector<string> *tx_pkts = (vector<string> *)arg;
  while (rx_pkts.size() < tx_pkts->size()) {
    bool found = pkt_queue->Dequeue(buf, (uint16_t*)&len);
    cout << "Found: " << found << " pkt: " << string(buf, len) << endl;
    if (found) {
      rx_pkts.push_back(string(buf, len));
    }
    usleep(100000);
  }      
}

void TestPktQueue() {
  pkt_queue = new PktQueue(5);
  //vector<string> tx_pkts = {"a", "bc", "bbaa", "a12156", "151a2", "16a21", "aw714#", "haha", "try"};
  vector<string> tx_pkts = {"1", "2", "30", "4", "55", "666", "77", "88888"};
  pthread_t p_enqueue_, p_dequeue_;
  Pthread_create(&p_enqueue_, NULL, LaunchEnqueue, &tx_pkts);
  Pthread_create(&p_dequeue_, NULL, LaunchDequeue, &tx_pkts);
  
  Pthread_join(p_enqueue_, NULL);
  Pthread_join(p_dequeue_, NULL);

  delete pkt_queue;
  assert(tx_pkts == rx_pkts);
  cout << "TestPktQueue good!" << endl;
}

int main(int argc, char **argv) {
  TestPktQueue();
  return 0;
}
