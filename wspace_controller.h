#include <cmath>
#include <time.h>
#include <utility>
#include <unordered_map>
//#include <algorithm>
//#include <iostream>

#include "wspace_asym_util.h"
#include "tun.h"

class BSStatsTable {
 public:
  BSStatsTable();    
  ~BSStatsTable();    

  void Update(int client_id, int radio_id, int bs_id, double throughput);
  //double FindStats(int client, int bs);

  int FindMaxThroughputBS(const int client_id, const int radio_id);

 private:
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }

  unordered_map<int, unordered_map<int, unordered_map<int, double> > > stats_; //form: <client_id, <radio_id, <bs_id, throughput>>>
  pthread_mutex_t lock_;
};

struct BSInfo {
  char ip_eth[16];
  char ip_tun[16];
  int port;
  int socket_id;
//  pthread_t p_forward_;
};

class RoutingTable {
 public:
  RoutingTable();
  ~RoutingTable();
  
  void Init(const Tun &tun);
  void UpdateRoutes(const Tun &tun, BSStatsTable &bs_stats_tbl);
  bool FindRoute(int dest_id, BSInfo *info);

 private:
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }

  unordered_map<int, int> route_;  // <client_id, bs_id>.
  unordered_map<int, BSInfo> bs_tbl_;
  pthread_mutex_t lock_;
};

class WspaceController
{
 public:
  WspaceController(int argc, char *argv[], const char *optstring);
  ~WspaceController() {}
  
  void* RecvFromBS(void *arg);
  void* ComputeRoutes(void *arg);
  void* ForwardToBS(void *arg);

// Data member
  pthread_t p_recv_from_bs_, p_compute_route_, p_forward_to_bs_;

  BSStatsTable bs_stats_tbl_;
  RoutingTable routing_tbl_;  
  Tun tun_;
  uint32 update_route_interval_;
};

/** Wrapper function for pthread_create. */
void* LaunchRecvFromBS(void* arg);
void* LaunchComputeRoutes(void* arg);
void* LaunchForwardToBS(void* arg);
