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

  void Update(int client, int bs, double throughput);
  double FindStats(int client, int bs);

  int FindMaxThroughputBS(int client);
  vector<int> GetClients();

 private:
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }

  unordered_map< int, unordered_map<int, double> > stats_; //form: <client_id, <bs_id, throughput>>
  pthread_mutex_t lock_;
  pthread_t p_recv_;
};

struct BSInfo {
	char ip_eth[16];
	char ip_tun[16];
	int port;
	int socket_id;
//	pthread_t p_forward_;
};

class RoutingTable {
 public:
  RoutingTable();
  ~RoutingTable();

  void Init(Tun &tun_);
  void UpdateRoute(BSStatsTable &bs_stats_tbl, Tun &tun_);
  BSInfo* GetRoute(int dest_id);
  bool IsBS(int dest_id);

 private:
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }

  unordered_map<int, int> route_; //form: <client_id, bs_id>
  unordered_map<int, BSInfo> bs_tbl_;
  pthread_mutex_t lock_;
};

class WspaceController
{
public:
	WspaceController(int argc, char *argv[], const char *optstring);
	~WspaceController();
	
	void* RecvStats(void *arg);
	void* ComputeRoute(void *arg);
	void* Forward(void *arg);

// Data member
	pthread_t p_recv_stats_, p_compute_route_, p_forward_;

	BSStatsTable bs_stats_tbl_;
	RoutingTable routing_tbl_;	
	Tun tun_;
};

/** Wrapper function for pthread_create. */
void* LaunchRecvStats(void* arg);
void* LaunchComputeRoute(void* arg);
void* LaunchForward(void* arg);
