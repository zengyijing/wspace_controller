#include <vector>
#include <cmath>
#include "wspace_controller.h"
#include "base_rate.h"
#include "monotonic_timer.h"

using namespace std;

WspaceController *wspace_controller;


void* LaunchRecvStats(void* arg)
{
	wspace_controller->RecvStats(arg);
}

void* LaunchComputeRoute(void* arg)
{
	wspace_controller->ComputeRoute(arg);
}

void* LaunchForward(void* arg)
{
	wspace_controller->Forward(arg);
}

BSStatsTable::BSStatsTable()
{
	Pthread_mutex_init(&lock_, NULL);
}

BSStatsTable::~BSStatsTable()
{
	Pthread_mutex_destroy(&lock_);
}

void BSStatsTable::Update(int client, int bs, double throughput)
{
	Lock();
	stats_[client][bs]= throughput;
/*
	stats_[client][bs+1]= throughput + 1; //For test
	stats_[client][bs+2]= throughput + 2; //For test
	stats_[client][bs+3]= throughput + 3; //For test
	stats_[client][bs+4]= throughput + 4; //For test
*/
	UnLock();
/*
	unordered_map< int, unordered_map<int, double> >::iterator it_1;
	unordered_map<int, double>::iterator it_2;
	for (it_1 = stats_.begin(); it_1 != stats_.end(); it_1++)
	{
		for(it_2 = it_1->second.begin(); it_2 != it_1->second.end(); it_2++)
		{
			printf("%d %d %f\n", it_1->first, it_2->first, it_2->second);
		}
	}
	printf("\n");
*/

}

vector<int> BSStatsTable::GetClients()
{
	vector<int> clients;
	unordered_map< int, unordered_map<int, double> >::iterator it_1;
	Lock();
	for (it_1 = stats_.begin(); it_1 != stats_.end(); it_1++)
	{
		clients.push_back(it_1->first);
	}
	UnLock();
	return clients;
}

double BSStatsTable::FindStats(int client, int bs)
{
	Lock();
	double throughput = stats_[client][bs];
	UnLock();
	return throughput;
}

int BSStatsTable::FindMaxThroughputBS(int client)
{
	Lock();
	int bs_id;
	bs_id = max_element(stats_[client].begin(),stats_[client].end())->first;
	UnLock();
	return bs_id;
}

RoutingTable::RoutingTable(Tun &tun_)
{
	Pthread_mutex_init(&lock_, NULL);
	//TODO: Initialize bs_tbl_, currently hard code
	BSInfo info1;
	strncpy(info1.ip, "128.105.22.164", 16);
	info1.port = PORT_ETH;
	info1.socket_id = tun_.sock_fd_eth_;
	//Pthread_create(&info1.p_forward_, NULL, LaunchForward, NULL);
	//Pthread_join(info1.p_forward_, NULL);
	int bs_id = atoi(strrchr(info1.ip,'.') + 1);
	bs_tbl_[bs_id] = info1;

	BSInfo info2;
	strncpy(info2.ip, "128.105.22.165", 16);
	info2.port = PORT_ETH;
	info2.socket_id = tun_.sock_fd_eth_;
	//Pthread_create(&info2.p_forward_, NULL, LaunchForward, NULL);
	//Pthread_join(info2.p_forward_, NULL);
	bs_id = atoi(strrchr(info2.ip,'.') + 1);
	bs_tbl_[bs_id] = info2;

}

RoutingTable::~RoutingTable()
{
	Pthread_mutex_destroy(&lock_);
}

void RoutingTable::UpdateRoute(BSStatsTable &bs_stats_tbl)
{
	Lock();
	route_.clear();
	vector<int> clients = bs_stats_tbl.GetClients();
	for (int i = 0; i < clients.size(); i++)
	{
		route_[clients[i]] = bs_stats_tbl.FindMaxThroughputBS(clients[i]);
		printf("%d %d\n", clients[i], bs_stats_tbl.FindMaxThroughputBS(clients[i]));
	}

	UnLock();
}

int main(int argc, char **argv)
{
	const char* opts = "C:i:";
	wspace_controller = new WspaceController(argc, argv, opts);

	wspace_controller->tun_.InitSock();

	Pthread_create(&wspace_controller->p_recv_stats_, NULL, LaunchRecvStats, NULL);
	Pthread_create(&wspace_controller->p_compute_route_, NULL, LaunchComputeRoute, NULL);
	Pthread_create(&wspace_controller->p_forward_, NULL, LaunchForward, NULL);

	Pthread_join(wspace_controller->p_recv_stats_, NULL);
	Pthread_join(wspace_controller->p_compute_route_, NULL);
	Pthread_join(wspace_controller->p_forward_, NULL);

	delete wspace_controller;
	return 0;
}

WspaceController::WspaceController(int argc, char *argv[], const char *optstring): routing_tbl_(tun_)
{
	int option;
	while ((option = getopt(argc, argv, optstring)) > 0)
	{
		switch(option)
		{
			case 'C':
				strncpy(tun_.controller_ip_eth_,optarg,16);
				printf("controller_ip_eth: %s\n", tun_.controller_ip_eth_);
				break;
			case 'i':
				strncpy(tun_.if_name_, optarg, IFNAMSIZ-1);
				tun_.tun_type_ = IFF_TUN;
				break;
			default:
				Perror("Usage: %s  -i tun0/tap0 -C controller_ip_eth \n", argv[0]);
		}
	}
	assert(tun_.if_name_[0] && tun_.controller_ip_eth_[0]);
}

WspaceController::~WspaceController()
{
}


void* WspaceController::RecvStats(void* arg)
{
	uint16 nread=0;
	char *buf = new char[PKT_SIZE];
	while (1)
	{
		nread = tun_.Read(Tun::kControl, buf, PKT_SIZE);
		if (buf[0] == STAT_DATA && nread == sizeof(BSStatsPkt))
		{
			BSStatsPkt stats_pkt;
			memcpy(&stats_pkt, buf, sizeof(BSStatsPkt));
			static uint32 current_seq = 0;
			uint32 seq;
			int bs_id;
			int client_id;
			double throughput;
			stats_pkt.ParsePkt(&seq, &bs_id, &client_id, &throughput);
			if(current_seq < seq)
			{
				current_seq = seq;
				bs_stats_tbl_.Update(client_id, bs_id, throughput);
			}
		}
	}
	delete[] buf;

	return (void*)NULL;
}

void* WspaceController::ComputeRoute(void* arg)
{
	while(1)
	{
		routing_tbl_.UpdateRoute(bs_stats_tbl_);
		usleep(1000000);
	}
	return (void*)NULL;
}

void* WspaceController::Forward(void* arg)
{
	printf("forward start\n");

	uint16 len = 0;
	char *pkt = new char[PKT_SIZE];

	while (1)
	{
		len = tun_.Read(Tun::kTun, pkt, PKT_SIZE);
		printf("%d\n", len);
		//tun_.Write()
	}
	delete[] pkt;

	return (void*)NULL;
}
