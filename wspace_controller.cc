#include <vector>
#include <cmath>
#include "wspace_controller.h"
#include "base_rate.h"
#include "monotonic_timer.h"

using namespace std;

WspaceController *wspace_controller;

int main(int argc, char **argv) {
  const char* opts = "C:i:S:s:t:b:c:r:d:m:f:o:F:R:";
  wspace_controller = new WspaceController(argc, argv, opts);
  wspace_controller->Init();

  Pthread_create(&wspace_controller->p_recv_from_bs_, NULL, LaunchRecvFromBS, NULL);
  Pthread_create(&wspace_controller->p_compute_route_, NULL, LaunchComputeRoutes, NULL);
  Pthread_create(&wspace_controller->p_read_tun_, NULL, LaunchReadTun, NULL);
  for (auto it = wspace_controller->conflict_graph_.begin(); it != wspace_controller->conflict_graph_.end(); ++it) {
    Pthread_create(&wspace_controller->p_forward_to_bs_tbl_[it->second], NULL, LaunchForwardToBS, &(it->second));
  }

  Pthread_join(wspace_controller->p_recv_from_bs_, NULL);
  Pthread_join(wspace_controller->p_compute_route_, NULL);
  Pthread_join(wspace_controller->p_read_tun_, NULL);
  for (auto it = wspace_controller->conflict_graph_.begin(); it != wspace_controller->conflict_graph_.end(); ++it) {
    Pthread_join(wspace_controller->p_forward_to_bs_tbl_[it->second], NULL);
  }
  delete wspace_controller;
  return 0;
}

void* LaunchRecvFromBS(void* arg) {
  wspace_controller->RecvFromBS(arg);
}

void* LaunchComputeRoutes(void* arg) {
  wspace_controller->ComputeRoutes(arg);
}

void* LaunchReadTun(void* arg) {
  wspace_controller->ReadTun(arg);
}

void* LaunchForwardToBS(void* arg) {
  wspace_controller->ForwardToBS(arg);
}

BSStatsTable::BSStatsTable() {
  Pthread_mutex_init(&lock_, NULL);
}

BSStatsTable::~BSStatsTable() {
  Pthread_mutex_destroy(&lock_);
}

void BSStatsTable::Update(int client_id, int bs_id, double throughput) {
  Lock();
  stats_[client_id][bs_id] = throughput;
  UnLock();
}

void BSStatsTable::GetStats(unordered_map<int, unordered_map<int, double> > *stats) {
  Lock();
  *stats = stats_;
  UnLock();
}

bool BSStatsTable::GetThroughput(int client_id, int bs_id, double* throughput) {
  bool throughput_found = false;
  Lock();
  if (stats_.count(client_id) && stats_[client_id].count(bs_id) > 0 && stats_[client_id][bs_id] > MIN_THROUGHPUT) {
    *throughput = stats_[client_id][bs_id];
    throughput_found = true;
  }
  UnLock();
  return throughput_found;
}

void BSStatsTable::Clear() {
  Lock();
  stats_.clear();
  UnLock();
}

void BSStatsTable::PrintStats() {
  Lock();
  for (auto it_client = stats_.begin(); it_client != stats_.end(); ++it_client) {
    cout << it_client->first << " ";
    for (auto it_bs = it_client->second.begin(); it_bs != it_client->second.end(); ++it_bs) {
      cout << "(" << it_bs->first << ", " << it_bs->second << "), "; 
    }
    cout << endl;
  }
  UnLock();
}

RoutingTable::RoutingTable() {
  Pthread_mutex_init(&lock_, NULL);
}

RoutingTable::~RoutingTable() {
  Pthread_mutex_destroy(&lock_);
  if (route_file_.is_open()) {
    route_file_.close();
  }
}

void RoutingTable::Init(const Tun &tun, const vector<int> &bs_ids,
                        const vector<int> &client_ids,
                        const FairnessMode &mode, 
                        unordered_map<int, int> &conflict_graph,
                        string f_stats, string f_conflict, 
                        string f_route, string f_executable, 
                        string f_route_log) {
  BSInfo info;
  printf("routing table init, tun.bs_ip_tbl_.size():%d\n", tun.bs_ip_tbl_.size());
  Lock();
  for (auto it = tun.bs_ip_tbl_.begin(); it !=tun.bs_ip_tbl_.end(); ++it) {
    strncpy(info.ip_eth, it->second, 16);
    char bs_ip_tun[16] = "10.0.0.";
    string last_octet = to_string(it->first);;
    strcat(bs_ip_tun, last_octet.c_str());
    strncpy(info.ip_tun, bs_ip_tun, 16);
    printf("eth ip:%s, tun ip:%s \n", info.ip_eth, info.ip_tun);
    info.port = PORT_ETH;
    info.socket_id = tun.sock_fd_eth_;
    printf("bs_id: %d\n", it->first);
    bs_tbl_[it->first] = info;
    route_[it->first] = it->first;    //route to bs 
    printf("route_[%d]: %d\n", it->first, route_[it->first]);
  }
  bs_ids_ = bs_ids;
  client_ids_ = client_ids;
  fairness_mode_ = mode; 
  conflict_graph_ = conflict_graph;
  f_stats_ = f_stats;
  f_conflict_ = f_conflict;
  f_route_ = f_route;
  f_executable_ = f_executable;
  route_file_.open(f_route_log);
  UnLock();
}

void RoutingTable::UpdateRoutes(BSStatsTable &bs_stats_tbl, 
                                unordered_map<int, double> &throughputs,
                                SchedulingMode scheduling_mode) {
  TIME start;
  switch (scheduling_mode) {
    case kMaxThroughput:
 
    case kDuplication:
      UpdateRoutesMaxThroughput(bs_stats_tbl, throughputs);
      break;

    case kOptimizer:
      UpdateRoutesOptimizer(bs_stats_tbl, throughputs, scheduling_mode);
      break;
    
    case kRoundRobin:
      UpdateRoutesRoundRobin(bs_stats_tbl, throughputs);
      break;
    
    default:
      assert(false);
  }
  TIME end;
  duration_ = (end - start) / 1000.0;
}

void RoutingTable::UpdateRoutesOptimizer(BSStatsTable &bs_stats_tbl, 
                                         unordered_map<int, double> &throughputs,
                                         SchedulingMode scheduling_mode) {
  bs_stats_tbl.GetStats(&stats_);
  // Fill invalid entries.
  for (auto client_id : client_ids_) {
    stats_[client_id];
    for (auto bs_id : bs_ids_) {
      if (stats_[client_id].count(bs_id) == 0) {
        stats_[client_id][bs_id] = MIN_THROUGHPUT;
      }
    }
  }
  PrintStats(f_stats_);
  usleep(100000);
  string cmd = "Rscript " + f_executable_ + " " + to_string(int(scheduling_mode)) +
               " " + to_string(int(fairness_mode_)) + " " + f_conflict_ + " " + f_stats_ +
               " " + f_route_;
  printf("Execute cmd: %s\n", cmd.c_str());
  system(cmd.c_str());
  throughputs.clear();
  Lock();
  ParseRoutingTable(f_route_);
  for (auto client_id : client_ids_) {
    if (route_.count(client_id) == 0) {
      printf("Warining: UpdateRouteOptimizer: route not found client:%d\n", client_id);
      continue;
    }
    int bs_id = route_[client_id];
    if (bs_id == -1) {
      route_[client_id] = bs_ids_[0];
      throughputs[client_id] = MIN_THROUGHPUT;
    } else {
      throughputs[client_id] = stats_[client_id][bs_id];
    }
  }
  UnLock();
}

void RoutingTable::UpdateRoutesMaxThroughput(BSStatsTable &bs_stats_tbl, 
                                             unordered_map<int, double> &throughputs) {
  bs_stats_tbl.GetStats(&stats_);
  cout << "UpdateRoutes: bs_stats_tbl:" << endl;
  bs_stats_tbl.PrintStats();
  throughputs.clear();
  Lock();
  route_.clear();
  for (auto client_id : client_ids_) {
    int bs_id = 0;
    double max_throughput = -1.0;
    bool is_route_available = FindMaxThroughputBS(client_id, &bs_id, &max_throughput); 
    if (is_route_available && max_throughput > 0) {
      route_[client_id] = bs_id;
      throughputs[client_id] = max_throughput;
      printf("route to %d is through %d\n", client_id, route_[client_id]);
    } else {
      printf("no route to %d currently, set route to default bs %d\n", client_id, bs_ids_[0]);
      route_[client_id] = bs_ids_[0];
      throughputs[client_id] = MIN_THROUGHPUT;
    }
  }
  UnLock();
}

void RoutingTable::UpdateRoutesRoundRobin(BSStatsTable &bs_stats_tbl,
                                          unordered_map<int, double> &throughputs) {
  static uint32 bs_index = 0;
  int bs_id = bs_ids_[bs_index++ % bs_ids_.size()];
  bs_stats_tbl.GetStats(&stats_);
  throughputs.clear();
  Lock();
  route_.clear();
  for (auto client_id : client_ids_) {
    if(!stats_.count(client_id) || !stats_[client_id].count(bs_id) || stats_[client_id][bs_id] < MIN_THROUGHPUT) {
      throughputs[client_id] = MIN_THROUGHPUT;
    } else {
      throughputs[client_id] = stats_[client_id][bs_id];
    }
    route_[client_id] = bs_id;
  }
  UnLock();
  printf("currently route is set to bs %d for all clients\n", bs_id);
}

bool RoutingTable::FindMaxThroughputBS(int client_id, int *bs_id, double *throughput) {
  auto max_p = make_pair(-1, -1.0);
  bool found = stats_.count(client_id);
  if (found) {
    for (const auto &p : stats_[client_id]) {
      if (p.second > max_p.second) {
        max_p = p;
      }
    }
  }
  *bs_id = max_p.first;
  *throughput = max_p.second;
  return found;
}

void RoutingTable::PrintStats(const string &filename) {
  ofstream ofs(filename.c_str());
  for (auto client_id : client_ids_) {
    for (auto bs_id : bs_ids_) {
      ofs << stats_[client_id][bs_id] << " ";
    }
    ofs << endl;
  }
  ofs.flush();
  ofs.close();
}

void RoutingTable::PrintConflictGraph(const string &filename) {
  ofstream ofs(filename.c_str());
  for (auto bs_id : bs_ids_) { 
    ofs << bs_id << " " << conflict_graph_[bs_id] << endl;
  }
  ofs.flush();
  ofs.close();
}

void RoutingTable::PrintRoutes() {
  Lock();
  TIME timer;
  for (auto client_id : client_ids_) {
    int bs_id = route_.count(client_id) ? route_[client_id] : -1;
    route_file_ << bs_id << " ";
  }
  UnLock();
  route_file_ << duration_ << " \"" << timer.CvtCurrTime() << "\"" << endl;
  route_file_.flush();
}

void RoutingTable::ParseRoutingTable(const string &filename) {
  ifstream ifs(filename.c_str());
  string line;
  int i = 0;
  route_.clear();
  while (getline(ifs, line)) {
    int bs_id = atoi(line.c_str());
    route_[client_ids_[i++]] = bs_id;
  }
  ifs.close();
}

bool RoutingTable::FindRoute(int dest_id, int* bs_id, BSInfo *info) {
  bool found = false;
  Lock();
  found = route_.count(dest_id);
  if (found) {
    *bs_id = route_[dest_id];
    *info = bs_tbl_[*bs_id];
  }
  UnLock();
  return found;
}

WspaceController::WspaceController(int argc, char *argv[], const char *optstring): 
  update_route_interval_(100000), round_interval_(10), fairness_mode_(kEqualTime),
  scheduling_mode_(kMaxThroughput), f_stats_("stats.dat"), f_conflict_("conflict.dat"), 
  f_route_("route.dat"), f_executable_("get_assignment.R"), f_route_log_("") {
  int option;
  int is_same_channel = 0;  // 0 for all bs on different channels, 1 for all on same channel, else for read file to parse
  while ((option = getopt(argc, argv, optstring)) > 0) {
    switch(option) {
      case 'C': {
        strncpy(tun_.controller_ip_eth_,optarg,16);
        printf("controller_ip_eth: %s\n", tun_.controller_ip_eth_);
        break;
      }
      case 'b': {
        string s;
        stringstream ss(optarg);
        while(getline(ss, s, ',')) {
          int bs_id = atoi(s.c_str());
          if(bs_id == 1)
              Perror("id 1 is reserved by controller\n");
          bs_ids_.push_back(bs_id);
        }
        break;
      }
      case 'S': {
        ParseIP(bs_ids_, tun_.bs_ip_tbl_);
        break;
      }
      case 'c': {
        string s;
        stringstream ss(optarg);
        while(getline(ss, s, ',')) {
          int client_id = atoi(s.c_str());
          if(client_id == 1)
              Perror("id 1 is reserved by controller\n");
          client_ids_.push_back(client_id);
          client_original_seq_tbl_[client_id] = 0;
        }
        break;
      }
      case 's': {
        ParseIP(client_ids_, tun_.client_ip_tbl_);
        break;
      }
      case 'i': {
        strncpy(tun_.if_name_, optarg, IFNAMSIZ-1);
        tun_.tun_type_ = IFF_TUN;
        break;
      }
      case 't': {
        update_route_interval_ = atoi(optarg);
        break;
      }
      case 'd': {
        round_interval_ = atoi(optarg);
        break;
      }
      case 'm': {
        fairness_mode_ = FairnessMode(atoi(optarg));
        break;
      }
      case 'f': {
        // For initializing confict graph.
        is_same_channel = atoi(optarg);
        break;
      }
      case 'o': {
        scheduling_mode_ = (SchedulingMode) atoi(optarg);
        break;
      }
      case 'F': {
        f_executable_ = string(optarg);
        break;
      }
      // Log base station assignments over time.
      case 'R': {
        f_route_log_ = string(optarg);
        break;
      }
      default: {
        Perror("Usage: %s -i tun0/tap0 -C controller_ip_eth -b bs_ids \
                -S bs_ip_eth -c client_ids -s client_ip_eth -t update_interval \
                -d round_interval -m fairness_mode[1/2]\n", argv[0]);
      }
    }
  }
  assert(tun_.if_name_[0] && tun_.controller_ip_eth_[0] && tun_.bs_ip_tbl_.size() && tun_.client_ip_tbl_.size());
  for (auto it = tun_.bs_ip_tbl_.begin(); it != tun_.bs_ip_tbl_.end(); ++it) {
    assert(it->second[0]);
  }
  for (auto it = tun_.client_ip_tbl_.begin(); it != tun_.client_ip_tbl_.end(); ++it) {
    assert(it->second[0]);
  }
  // Construct conflict graph for the route optimizer. 
  if (is_same_channel == 1) {
    for (auto bs_id : bs_ids_) {
      conflict_graph_[bs_id] = 1;
    }
  } else if (is_same_channel == 0) {
    for (auto bs_id : bs_ids_) {
      conflict_graph_[bs_id] = bs_id;
    }
  } else {
    ParseConflictGraph("conflict_config.dat");
    for (auto bs_id : bs_ids_) {
      printf("%d %d\n", bs_id, conflict_graph_[bs_id]);
    }
  }
  for (auto it = conflict_graph_.begin(); it != conflict_graph_.end(); ++it) {
    if(packet_scheduler_tbl_.count(it->second) == 0)
      packet_scheduler_tbl_[it->second] = new PktScheduler(MIN_THROUGHPUT, client_ids_, PKT_QUEUE_SIZE, 
                                            round_interval_, PktScheduler::FairnessMode(fairness_mode_));
  }
}

WspaceController::~WspaceController() {
  for (auto it = packet_scheduler_tbl_.begin(); it != packet_scheduler_tbl_.end(); ++it) {
    delete it->second;
  }
}


void WspaceController::ParseConflictGraph(const string &filename) {
  ifstream ifs(filename.c_str());
  string line;
  int i = 0;
  conflict_graph_.clear();
  while (getline(ifs, line)) {
    int bs_id = atoi(line.c_str());
    char tmp[16] = {0};
    strcpy(tmp, line.c_str());
    int contention_id = atoi(strchr(tmp, ' ') + 1);
    conflict_graph_[bs_id] = contention_id;
  }
  ifs.close();
}


void WspaceController::ParseIP(const vector<int> &ids, unordered_map<int, char [16]> &ip_table) {
  if (ids.empty()) {
    Perror("WspaceController::ParseIP: missing ids!\n");
  }
  auto it = ids.begin();
  string addr;
  stringstream ss(optarg);
  while(getline(ss, addr, ',')) {
    if (it == ids.end())
      Perror("WspaceController::ParseIP: Too many input addresses\n");
    strncpy(ip_table[*it], addr.c_str(), 16);
    ++it;
  }
}

void WspaceController::Init() {
  tun_.InitSock();
  routing_tbl_.Init(tun_, bs_ids_, client_ids_, fairness_mode_, conflict_graph_,
                    f_stats_, f_conflict_, f_route_, f_executable_, f_route_log_);
  if (scheduling_mode_ == kOptimizer) {
    routing_tbl_.PrintConflictGraph(f_conflict_);
  }
}

void* WspaceController::RecvFromBS(void* arg) {
  uint16 nread=0;
  char *pkt = new char[PKT_SIZE];
  static unordered_map<int, unordered_map<int, uint32> > current_seq;
  for(auto it_1 = bs_ids_.begin(); it_1 != bs_ids_.end(); ++it_1) {
    for(auto it_2 = client_ids_.begin(); it_2 != client_ids_.end(); ++it_2) {
      current_seq[*it_1][*it_2] = 0;
    }
  }
  while (1) {
    nread = tun_.Read(Tun::kControl, pkt, PKT_SIZE);
    //printf("pkt header: %d\n", (int)pkt[0]);
    if (pkt[0] == BS_STATS) {
      BSStatsPkt* stats_pkt = (BSStatsPkt*) pkt;
      uint32 seq;
      int bs_id;
      int client_id;
      double throughput;
      stats_pkt->ParsePkt(&seq, &bs_id, &client_id, &throughput);
      //stats_pkt->Print();
      if(tun_.bs_ip_tbl_.count(bs_id) && tun_.client_ip_tbl_.count(client_id) && current_seq[bs_id][client_id] < seq) {
        current_seq[bs_id][client_id] = seq;
        // Assume bs_id = radio_id for simplicity.
        bs_stats_tbl_.Update(client_id, bs_id, throughput);
      } else {
        printf("WspaceController::RecvFromBS: Received invalid BSStatsPkt\n");
      }
    } else if (pkt[0] == CELL_DATA) {
      //printf("received uplink data message.\n");
      tun_.Write(Tun::kTun, pkt + CELL_DATA_HEADER_SIZE, nread - CELL_DATA_HEADER_SIZE, NULL);
    } else if (pkt[0] == DATA_ACK || pkt[0] == RAW_ACK) {
      // Send Ack to corresponding bs.
      AckHeader *hdr = (AckHeader*)pkt;
      struct sockaddr_in bs_addr;
      tun_.CreateAddr(tun_.bs_ip_tbl_[hdr->bs_id()], PORT_ETH, &bs_addr);
      tun_.Write(Tun::kControl, pkt, nread, &bs_addr);
    } else if (pkt[0] == GPS) {
      // Send gps message to default bs.
      GPSHeader *hdr = (GPSHeader*)pkt;
      struct sockaddr_in bs_addr;
      tun_.CreateAddr(tun_.bs_ip_tbl_.begin()->second, PORT_ETH, &bs_addr);
      tun_.Write(Tun::kControl, pkt, nread, &bs_addr);
    } else if (pkt[0] == ATH_CODE) {
      AthHeader *hdr = (AthHeader*)pkt;
      struct sockaddr_in client_addr;
      tun_.CreateAddr(tun_.client_ip_tbl_[hdr->client_id()], PORT_ETH, &client_addr);
      tun_.Write(Tun::kControl, pkt, nread, &client_addr);
    }
  }
  delete[] pkt;
  return (void*)NULL;
}

void* WspaceController::ComputeRoutes(void* arg) {
  unordered_map<int, double> throughputs; 
  while(1) {
    routing_tbl_.UpdateRoutes(bs_stats_tbl_, throughputs, scheduling_mode_);
    bs_stats_tbl_.Clear();
    unordered_map<int, unordered_map<int, double> > splited_throughputs; // <contention id, <client_id, throughput> >.
    for(auto it = throughputs.begin(); it != throughputs.end(); ++it) {
      int client_id = it->first;
      int bs_id = 0;
      BSInfo info;
      bool is_route_available = routing_tbl_.FindRoute(client_id, &bs_id, &info);
      if (is_route_available) {
        int contention_id = conflict_graph_[bs_id];
        splited_throughputs[contention_id][client_id] = it->second;
      } else {
        printf("WspaceController::ComputeRoutes did not find route\n");
        assert(false);
      }
    }
    for(auto it = packet_scheduler_tbl_.begin(); it != packet_scheduler_tbl_.end(); ++it) {
      if (splited_throughputs[it->first].size() > 0) {
        printf("contention id: %d\n", it->first);
        it->second->ComputeQuantum(splited_throughputs[it->first]);
        it->second->PrintStats();
      }
    }
    if (f_route_log_ != "") {
      routing_tbl_.PrintRoutes();
    }
    usleep(update_route_interval_);  
  }
  return (void*)NULL;
}

int WspaceController::ExtractClientID(const char *pkt) {
  char ip[16] = {0};
  struct in_addr addr;
  memcpy(&addr.s_addr, pkt + 16, sizeof(long));
  strncpy(ip, inet_ntoa(addr), 16);
  int id = atoi(strrchr(ip,'.') + 1);
  return id;
}

void* WspaceController::ReadTun(void *arg) {
  char *pkt = new char[PKT_SIZE];
  ((ControllerToClientHeader*)pkt)->set_type(CONTROLLER_TO_CLIENT);
  while (true) { 
    uint16 len = tun_.Read(Tun::kTun, pkt + sizeof(ControllerToClientHeader), PKT_SIZE - sizeof(ControllerToClientHeader));
    len += sizeof(ControllerToClientHeader);
    int client_id = ExtractClientID(pkt+ sizeof(ControllerToClientHeader));
    if (client_original_seq_tbl_.count(client_id) == 0) {
      printf("Traffic to an unknown client:%d, continue.\n", client_id);
      continue;
    }
    ((ControllerToClientHeader*)pkt)->set_client_id(client_id);
    ((ControllerToClientHeader*)pkt)->set_o_seq(++client_original_seq_tbl_[client_id]);
    int bs_id = 0;
    BSInfo info;
    bool is_route_available = routing_tbl_.FindRoute(client_id, &bs_id, &info);
    if (is_route_available) {
      int contention_id = conflict_graph_[bs_id];
      packet_scheduler_tbl_[contention_id]->Enqueue(pkt, len, client_id);
    } else {
      for(auto it = packet_scheduler_tbl_.begin(); it != packet_scheduler_tbl_.end(); ++it) {
        it->second->Enqueue(pkt, len, client_id);
      }
    }
  }
  delete[] pkt;
}

void* WspaceController::ForwardToBS(void* arg) {
  int *contention_id = (int*) arg;
  printf("contention_id %d start forwarding to BS.\n", *contention_id);
  int client_id = 0;
  char *buf = new char[PKT_SIZE];
  struct sockaddr_in bs_addr;
  BSInfo info;
  vector<pair<char*, uint16_t> > pkts;
  while (1) {
    packet_scheduler_tbl_[*contention_id]->Dequeue(&pkts, &client_id);
    if(!tun_.IsValidClient(client_id)) {
      Perror("WspaceController::ForwardToBS: Invalid client[%d]!\n", client_id);
    }
    // Send packets from a client in a round.
    for (auto &p : pkts) {
      memcpy(buf, p.first, p.second);
      delete[] p.first;
      uint16 len = p.second;
      int bs_id = 0;
      bool is_route_available = routing_tbl_.FindRoute(client_id, &bs_id, &info);
      int duration = len * 8;
      double throughput = 0;
      if (is_route_available && scheduling_mode_ != kDuplication) {
        tun_.CreateAddr(info.ip_eth, info.port, &bs_addr);
        //printf("convert address to %s\n", info.ip_eth);
        tun_.Write(Tun::kControl, buf, len, &bs_addr);
        if (bs_stats_tbl_.GetThroughput(client_id, bs_id, &throughput))
          duration =  len * 8.0 / throughput;
      } else {
        for(auto it = tun_.bs_ip_tbl_.begin(); it != tun_.bs_ip_tbl_.end(); ++it) {
          //printf("broadcast through bs %d/%s\n", it->first, it->second);
          tun_.CreateAddr(it->second, PORT_ETH, &bs_addr);
          tun_.Write(Tun::kControl, buf, len, &bs_addr);
          if (scheduling_mode_ == kDuplication && bs_stats_tbl_.GetThroughput(client_id, it->first, &throughput)) {   
            int dur = len * 8.0 / throughput;
            if (duration > dur)
              duration = dur;
          }
        }
      }
      usleep(duration);
    }
  }
  delete[] buf;
  return (void*)NULL;
}

