#include <cmath>
#include <time.h>
#include <utility>
#include "wspace_asym_util.h"
#include "tun.h"

class BSStatsTable {
 public:
  BSStatsTable();		
  ~BSStatsTable();		

  void Update(int bs, int client, double throughput);

  double FindStats(int bs, int client);

 private:
  void Lock() { Pthread_mutex_lock(&lock_); }
  void UnLock() { Pthread_mutex_unlock(&lock_); }

  //unordered_map<int, unordered_map<int, double> > stats_;
  vector<vector<double> > stats_;
  pthread_mutex_t lock_;
  pthread_t p_recv_;
};

struct BSInfo {
	string ip;
	int port;
	int socket_id;
	pthread_t p_forward_;
};

class RoutingTable {
 public:
  RoutingTable(int num_clients) {
    route_.resize(num_clients);
  }

  ~RoutingTable() {}

 private:
  vector<int> route_;
  vector<BSInfo> bs_tbl_;
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
	pthread_t p_recv_stats_, p_compute_route_;

	BSStatsTable bs_stats_tbl_;
	RoutingTable routing_tbl_;	
	Tun tun_;
};

/** Wrapper function for pthread_create. */
void* LaunchRecvStats(void* arg);
void* LaunchSelectBS(void* arg);

