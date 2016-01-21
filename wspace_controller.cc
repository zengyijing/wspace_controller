#include <vector>
#include <cmath>
#include "wspace_controller.h"
#include "base_rate.h"
#include "monotonic_timer.h"

using namespace std;

WspaceController *wspace_controller;

int main(int argc, char **argv) {
  const char* opts = "C:i:S:s:t:b:c:r:";
  wspace_controller = new WspaceController(argc, argv, opts);

  wspace_controller->tun_.InitSock();
  wspace_controller->routing_tbl_.Init(wspace_controller->tun_);

  Pthread_create(&wspace_controller->p_recv_from_bs_, NULL, LaunchRecvFromBS, NULL);
  Pthread_create(&wspace_controller->p_compute_route_, NULL, LaunchComputeRoutes, NULL);
  Pthread_create(&wspace_controller->p_forward_to_bs_, NULL, LaunchForwardToBS, NULL);

  Pthread_join(wspace_controller->p_recv_from_bs_, NULL);
  Pthread_join(wspace_controller->p_compute_route_, NULL);
  Pthread_join(wspace_controller->p_forward_to_bs_, NULL);

  delete wspace_controller;
  return 0;
}

void* LaunchRecvFromBS(void* arg) {
  wspace_controller->RecvFromBS(arg);
}

void* LaunchComputeRoutes(void* arg) {
  wspace_controller->ComputeRoutes(arg);
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

void BSStatsTable::Update(int client_id, int radio_id, int bs_id, double throughput) {
  Lock();
  stats_[client_id][radio_id][bs_id]= throughput;
/*
  stats_[client_id][radio_id][bs_id+1]= throughput + 1; //For test
  stats_[client_id][radio_id][bs_id+2]= throughput + 2; //For test
  stats_[client_id][radio_id][bs_id+3]= throughput + 3; //For test
  stats_[client_id][radio_id][bs_id+4]= throughput + 4; //For test
*/
  UnLock();
/*
  unordered_map< int, unordered_map<int, double> >::iterator it_1;
  unordered_map<int, double>::iterator it_2;
  for (it_1 = stats_.begin(); it_1 != stats_.end(); it_1++) {
    for(it_2 = it_1->second.begin(); it_2 != it_1->second.end(); it_2++) {
      printf("%d %d %f\n", it_1->first, it_2->first, it_2->second);
    }
  }
  printf("\n");
*/

}
/*
double BSStatsTable::FindStats(int client, int bs) {
  Lock();
  double throughput = stats_[client][bs];
  UnLock();
  return throughput;
}
*/
int BSStatsTable::FindMaxThroughputBS(const int client_id, const int radio_id) {
  Lock();
  int bs_id = 0;
  if (stats_.count(client_id) != 0)
    bs_id = max_element(stats_[client_id][radio_id].begin(),stats_[client_id][radio_id].end())->first;
  UnLock();
  return bs_id;
}

RoutingTable::RoutingTable() {
  Pthread_mutex_init(&lock_, NULL);
}

RoutingTable::~RoutingTable() {
  Pthread_mutex_destroy(&lock_);
}

void RoutingTable::Init(const Tun &tun) {
  BSInfo info;
  int bs_id;
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
    //Pthread_create(&info1.p_forward_, NULL, LaunchForward, NULL);
    //Pthread_join(info1.p_forward_, NULL);
    bs_id = it->first;
    printf("bs_id: %d\n", bs_id);
    bs_tbl_[bs_id] = info;
    route_[bs_id] = bs_id;    //route to bs 
    printf("route_[%d]: %d\n", bs_id, route_[bs_id]);
  }

  UnLock();
}

void RoutingTable::UpdateRoutes(const Tun &tun, BSStatsTable &bs_stats_tbl) {
  Lock();
  for (auto it = tun.client_ip_tbl_.begin(); it != tun.client_ip_tbl_.end(); ++it) {
    route_[it->first] = bs_stats_tbl.FindMaxThroughputBS(it->first, Laptop::kFront); //TODO:right now hard code radio_id
    printf("%d %d\n", it->first, route_[it->first]);
  }
  UnLock();
}

bool RoutingTable::FindRoute(int dest_id, BSInfo *info) {
  bool found = false;
  Lock();
  found = route_.count(dest_id);
  if (found) {
    int bs_id = route_[dest_id];
    if (bs_id == 0)
      found = false;
    else
      *info = bs_tbl_[bs_id];
  }
  UnLock();
  return found;
}

WspaceController::WspaceController(int argc, char *argv[], const char *optstring): update_route_interval_(100000) {
  int option;
  vector<int> bs_ids;
  vector<int> client_ids;
  while ((option = getopt(argc, argv, optstring)) > 0) {
    switch(option) {
      case 'C': {
        strncpy(tun_.controller_ip_eth_,optarg,16);
        printf("controller_ip_eth: %s\n", tun_.controller_ip_eth_);
        break;
      }
      case 'b': {
        string addr;
        stringstream ss(optarg);
        while(getline(ss, addr, ',')) {
          if(atoi(addr.c_str()) == 1)
              Perror("id 1 is reserved by controller\n");
          bs_ids.push_back(atoi(addr.c_str()));
          //strcpy(tun_.bs_ip_tbl_[atoi(addr.c_str())],"");
        }
        break;
      }
      case 'S': {
        if (bs_ids.size() > 0) {
          auto it = bs_ids.begin();
          string addr;
          stringstream ss(optarg);
          while(getline(ss, addr, ',')) {
            if (it == bs_ids.end())
              Perror("Too many input bs addresses\n");
            strncpy(tun_.bs_ip_tbl_[*it], addr.c_str(), 16);
            printf("tun_.bs_ip_tbl_[%d]: %s\n", *it, tun_.bs_ip_tbl_[*it]);
            ++it;
          }
        } else {
          Perror("Need to indicate bs_id first!\n");
        }
        break;
      }
      case 'c': {
        string addr;
        stringstream ss(optarg);
        while(getline(ss, addr, ',')) {
          if(atoi(addr.c_str()) == 1)
              Perror("id 1 is reserved by controller\n");
          client_ids.push_back(atoi(addr.c_str()));
          //strcpy(tun_.client_ip_tbl_[atoi(addr.c_str())],""); 
        }
        break;
      }
      case 's': {
        if (client_ids.size() > 0) {
          auto it = client_ids.begin();
          string addr;
          stringstream ss(optarg);
          while(getline(ss, addr, ',')) {
            if (it == client_ids.end())
              Perror("Too many input client addresses\n");
            strncpy(tun_.client_ip_tbl_[*it], addr.c_str(), 16);
            printf("tun_.client_ip_tbl_[%d]: %s\n", *it, tun_.client_ip_tbl_[*it]);
            ++it;
          }
        } else {
          Perror("Need to indicate bs_id first!\n");
        }
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
      default: {
        Perror("Usage: %s  -i tun0/tap0 -C controller_ip_eth -b bs_ids -S bs_ip_eth -c client_ids -s client_ip_eth -t update_interval \n", argv[0]);
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
}

void* WspaceController::RecvFromBS(void* arg) {
  uint16 nread=0;
  char *pkt = new char[PKT_SIZE];
  //static uint32 current_seq = 0;
  static unordered_map<int, uint32> current_seq;
  for(auto it = tun_.bs_ip_tbl_.begin(); it != tun_.bs_ip_tbl_.end(); ++it) {
    current_seq[it->first] = 0;
  }
  while (1) {
    nread = tun_.Read(Tun::kControl, pkt, PKT_SIZE);
    //printf("pkt header: %d\n", (int)pkt[0]);
    if (pkt[0] == STAT_DATA && nread == sizeof(BSStatsPkt)) {
      BSStatsPkt* stats_pkt;
      stats_pkt = (BSStatsPkt*) pkt;
      uint32 seq;
      int bs_id;
      int client_id;
      int radio_id;
      double throughput;
      stats_pkt->ParsePkt(&seq, &bs_id, &client_id, &radio_id, &throughput);
      if(tun_.bs_ip_tbl_.count(bs_id) && tun_.client_ip_tbl_.count(client_id) && current_seq[bs_id] < seq) {
        current_seq[bs_id] = seq;
        bs_stats_tbl_.Update(client_id, radio_id, bs_id, throughput);  //@Tan: radio_id needs to be stored in BSStatsTable?
      }
    }

    else if (pkt[0] == CELL_DATA) {
      //printf("received uplink data message.\n");
      tun_.Write(Tun::kTun, pkt + 1, nread - 1, NULL);
    }
  }
  delete[] pkt;
  return (void*)NULL;
}

void* WspaceController::ComputeRoutes(void* arg) {
  while(1) {
    routing_tbl_.UpdateRoutes(tun_, bs_stats_tbl_);
    usleep(update_route_interval_);  
  }
  return (void*)NULL;
}

void* WspaceController::ForwardToBS(void* arg) {
  printf("forward to bs start\n");

  uint16 len = 0;
  char *pkt = new char[PKT_SIZE];
  char ip_tun[16] = {0};
  int dest_id = 0;
  struct in_addr bs_addr;
  struct sockaddr_in bs_addr_eth;
  BSInfo info;

  while (1) {
    len = tun_.Read(Tun::kTun, pkt + 1 + sizeof(int), PKT_SIZE - 1 - sizeof(int));
    memcpy(&bs_addr.s_addr, pkt + 1 + sizeof(int) + 16, sizeof(long));//suppose it's a IP packet and get the destination inner IP address like 10.0.0.2
    strncpy(ip_tun, inet_ntoa(bs_addr), 16);
    //printf("read %d bytes from tun to %s\n", len, ip_tun);
    dest_id = atoi(strrchr(ip_tun,'.') + 1);
    printf("dest_id: %d\n", dest_id);
    memcpy(pkt + 1, dest_id, sizeof(int));
    if(tun_.client_ip_tbl_.count(dest_id) == 0)
      Perror("Traffic to a client not specified!\n");
    bool is_route_available = routing_tbl_.FindRoute(dest_id, &info);
    if (is_route_available) {
      tun_.CreateAddr(info.ip_eth, info.port, &bs_addr_eth);
      //printf("convert address to %s\n", info.ip_eth);
      *pkt = FORWARD_DATA;
      tun_.Write(Tun::kControl, pkt, len + 1 + sizeof(int), &bs_addr_eth);
    } else {
      printf("No route to the client[%d]\n", dest_id);
      for(auto it = tun_.bs_ip_tbl_.begin(); it != tun_.bs_ip_tbl_.end(); ++it) {
        printf("broadcast through bs %d/%s\n", it->first, it->second);
        tun_.CreateAddr(it->second, PORT_ETH, &bs_addr_eth);
        *pkt = FORWARD_DATA;
        tun_.Write(Tun::kControl, pkt, len + 1 + sizeof(int), &bs_addr_eth);
      }
    }
  }
  delete[] pkt;
  return (void*)NULL;
}
