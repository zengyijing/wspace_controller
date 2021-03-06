#ifndef WSPACE_CONTROLLER_H_
#define WSPACE_CONTROLLER_H_ 

#include <stdlib.h>
#include <time.h>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <iostream>
#include <fstream>

#include "wspace_asym_util.h"
#include "tun.h"
#include "packet_scheduler.h"

#define MIN_THROUGHPUT 1
#define PKT_QUEUE_SIZE 1000 

class BSStatsTable {
 public:
  BSStatsTable();    
  ~BSStatsTable();    

  void Update(int client_id, int bs_id, double throughput);
  // Only update existing values in stats_.
  void GetStats(unordered_map<int, unordered_map<int, double> > *stats);
  bool GetThroughput(int client_id, int bs_id, double* throughput);
  void Clear();
  void PrintStats();

 private:
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }

  unordered_map<int, unordered_map<int, double> > stats_; // <client_id, <bs_id, throughput>>.
  pthread_mutex_t lock_;
};

struct BSInfo {
  char ip_eth[16];
  char ip_tun[16];
  int port;
  int socket_id;
};

enum FairnessMode {
  kEqualTime = 0,
  kEqualThroughput = 1,
  kProportionalThroughput = 2,
};

enum SchedulingMode {
  kMaxThroughput = 0,
  kOptimizer = 1,
  kDuplication = 2,
  kRoundRobin = 3,
  kExhaustiveSearch = 4,
};

class RoutingTable {
 public:
  RoutingTable();
  ~RoutingTable();
  
  void Init(const Tun &tun, const vector<int> &bs_ids,
            const vector<int> &client_ids, 
            const FairnessMode &mode,
            unordered_map<int, int> &conflict_graph, 
            string f_stats, string f_conflict, 
            string f_route, string f_executable, 
            string f_route_log);

  // throughputs - throughput of the selected bs for each client.
  void UpdateRoutes(BSStatsTable &bs_stats_tbl, 
                    unordered_map<int, double> &throughputs, 
                    SchedulingMode scheduling_mode);
  bool FindRoute(int dest_id, int* bs_id, BSInfo *info);
  void PrintConflictGraph(const string &filename);
  // With Lock.
  void PrintRoutes();

 private:
  // With lock.
  void UpdateRoutesMaxThroughput(BSStatsTable &bs_stats_tbl, 
                                 unordered_map<int, double> &throughputs);
  // With lock.
  void UpdateRoutesOptimizer(BSStatsTable &bs_stats_tbl,
                             unordered_map<int, double> &throughputs,
                             SchedulingMode scheduling_mode);
  // With lock.
  void UpdateRoutesRoundRobin(BSStatsTable &bs_stats_tbl,
                              unordered_map<int, double> &throughputs);

  bool FindMaxThroughputBS(int client_id, int *bs_id, double *throughput);
  void PrintStats(const string &filename);
  void ParseRoutingTable(const string &filename);

  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }

  unordered_map<int, int> route_;  // <client_id, bs_id>.
  unordered_map<int, BSInfo> bs_tbl_;
  vector<int> bs_ids_;
  vector<int> client_ids_;
  unordered_map<int, int> conflict_graph_;
  unordered_map<int, unordered_map<int, double> > stats_; // <client_id, <bs_id, throughput>>.
  unordered_map<int, double> throughputs_;
  string f_stats_, f_conflict_, f_route_, f_executable_;
  FairnessMode fairness_mode_;
  ofstream route_file_;
  pthread_mutex_t lock_;
  double duration_;  // in milliseconds.
};

class WspaceController {
 public:

  WspaceController(int argc, char *argv[], const char *optstring);
  ~WspaceController();
  
  void* RecvFromBS(void *arg);
  void* ComputeRoutes(void *arg);
  void* ReadTun(void *arg);
  void* ForwardToBS(void *arg);

  void Init();
  void ParseIP(const vector<int> &ids, unordered_map<int, char [16]> &ip_table);
  void ParseConflictGraph(const string &filename);
  int ExtractClientID(const char *pkt);
  
// Data member
  pthread_t p_recv_from_bs_, p_compute_route_, p_read_tun_;
  unordered_map<int, pthread_t> p_forward_to_bs_tbl_;

  BSStatsTable bs_stats_tbl_;
  RoutingTable routing_tbl_;  
  Tun tun_;
  unordered_map<int, PktScheduler*> packet_scheduler_tbl_;
  FairnessMode fairness_mode_;
  uint32 round_interval_;        // in microseconds to schedule cilents in a round.
  uint32 update_route_interval_; // in microseconds.
  unordered_map<int, uint32> client_original_seq_tbl_;  // <client_id, original_seq>.
  vector<int> bs_ids_;
  vector<int> client_ids_;
  unordered_map<int, int> conflict_graph_;
  string f_stats_, f_conflict_, f_route_, f_executable_, f_route_log_;
  SchedulingMode scheduling_mode_;
};

/** Wrapper function for pthread_create. */
void* LaunchRecvFromBS(void* arg);
void* LaunchComputeRoutes(void* arg);
void* LaunchReadTun(void* arg);
void* LaunchForwardToBS(void* arg);

#endif
