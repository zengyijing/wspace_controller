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

RoutingTable::RoutingTable()
{
	Pthread_mutex_init(&lock_, NULL);
}

RoutingTable::~RoutingTable()
{
	Pthread_mutex_destroy(&lock_);
}

void RoutingTable::Init(Tun &tun_)
{
	//TODO: Initialize bs_tbl_, currently hard code
	BSInfo info;
	int bs_id;
	printf("routing table init, tun_.server_count_:%d\n", tun_.server_count_);

	Lock();

	for(int i = 0; i < tun_.server_count_; i++)
	{
		strncpy(info.ip_eth, tun_.server_ip_eth_[i], 16);
		strncpy(info.ip_tun, tun_.server_ip_tun_[i], 16);
		printf("eth ip:%s, tun ip:%s \n", info.ip_eth, info.ip_tun);
		info.port = PORT_ETH;
		info.socket_id = tun_.sock_fd_eth_;
		//Pthread_create(&info1.p_forward_, NULL, LaunchForward, NULL);
		//Pthread_join(info1.p_forward_, NULL);
		bs_id = atoi(strrchr(info.ip_tun,'.') + 1);
		printf("bs_id: %d\n", bs_id);
		bs_tbl_[bs_id] = info;
		route_[bs_id] = bs_id;		//route to bs 
		printf("route_[%d]: %d\n", bs_id, route_[bs_id]);
	}
	UnLock();
}

void RoutingTable::UpdateRoute(BSStatsTable &bs_stats_tbl, Tun &tun_)
{
	Lock();
	route_.clear();
	vector<int> clients = bs_stats_tbl.GetClients();
	for (int i = 0; i < clients.size(); i++)
	{
		route_[clients[i]] = bs_stats_tbl.FindMaxThroughputBS(clients[i]);
		//printf("%d %d\n", clients[i], bs_stats_tbl.FindMaxThroughputBS(clients[i]));
	}

	BSInfo info;
	int bs_id;

	for (int i = 0; i < tun_.server_count_; i++)	//since all routes are cleared, need to restore routes to every bs
	{
		bs_id = atoi(strrchr(tun_.server_ip_tun_[i],'.') + 1);
		route_[bs_id] = bs_id;
	}
	UnLock();
}

BSInfo* RoutingTable::GetRoute(int dest_id)
{
	int bs_id = route_[dest_id];
	printf("route_[%d]: %d\n", dest_id, route_[dest_id]);
	return &bs_tbl_[bs_id];
}

bool RoutingTable::IsBS(int dest_id)
{
	if (route_[dest_id] == dest_id)
		return true;
	else
		return false;
}

int main(int argc, char **argv)
{
	const char* opts = "C:i:S:s:";
	wspace_controller = new WspaceController(argc, argv, opts);

	wspace_controller->tun_.InitSock();
	wspace_controller->routing_tbl_.Init(wspace_controller->tun_);

	Pthread_create(&wspace_controller->p_recv_stats_, NULL, LaunchRecvStats, NULL);
	Pthread_create(&wspace_controller->p_compute_route_, NULL, LaunchComputeRoute, NULL);
	Pthread_create(&wspace_controller->p_forward_, NULL, LaunchForward, NULL);

	Pthread_join(wspace_controller->p_recv_stats_, NULL);
	Pthread_join(wspace_controller->p_compute_route_, NULL);
	Pthread_join(wspace_controller->p_forward_, NULL);

	delete wspace_controller;
	return 0;
}

WspaceController::WspaceController(int argc, char *argv[], const char *optstring)
{
	int option;
	while ((option = getopt(argc, argv, optstring)) > 0)
	{
		switch(option)
		{
			case 'C':
			{
				strncpy(tun_.controller_ip_eth_,optarg,16);
				printf("controller_ip_eth: %s\n", tun_.controller_ip_eth_);
				break;
			}
			case 'S':
			{
				char buff[PKT_SIZE] = {0};
				strcpy(buff,optarg);
				char* p = strtok(buff,",");
				strncpy(tun_.server_ip_eth_[0], p, 16);
				printf("tun_.server_ip_eth_[0]: %s\n", tun_.server_ip_eth_[0]);
				int count = 1;
				while(p = strtok(NULL,","))
				{
					strncpy(tun_.server_ip_eth_[count], p, 16);
					printf("tun_.server_ip_eth_[%d]: %s\n", count, tun_.server_ip_eth_[count]);
					count++;
				}
				if (tun_.server_count_ == 0)
					tun_.server_count_ = count;
				else if (tun_.server_count_ != count)
					Perror("number of server do not match\n");
				break;
			}
			case 's':
			{
				char buff[PKT_SIZE] = {0};
				strcpy(buff,optarg);
				char* p = strtok(buff,",");
				strncpy(tun_.server_ip_tun_[0], p, 16);
				printf("tun_.server_ip_tun_[0]: %s\n", tun_.server_ip_tun_[0]);
				int count = 1;
				while(p = strtok(NULL,","))
				{
					strncpy(tun_.server_ip_tun_[count], p, 16);
					printf("tun_.server_ip_tun_[%d]: %s\n", count, tun_.server_ip_tun_[count]);
					count++;
				}
				if (tun_.server_count_ == 0)
					tun_.server_count_ = count;
				else if (tun_.server_count_ != count)
					Perror("number of server do not match\n");
				break;
			}
			case 'i':
			{
				strncpy(tun_.if_name_, optarg, IFNAMSIZ-1);
				tun_.tun_type_ = IFF_TUN;
				break;
			}
			default:
			{
				Perror("Usage: %s  -i tun0/tap0 -C controller_ip_eth \n", argv[0]);
			}
		}
	}
	assert(tun_.if_name_[0] && tun_.controller_ip_eth_[0] && tun_.server_count_ <= MAX_SERVER);
	for (int i = 0; i < tun_.server_count_; i++)
	{
		assert(tun_.server_ip_eth_[i][0]);
		assert(tun_.server_ip_tun_[i][0]);
	}

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
		else if (buf[0] == CONTROL_BS)
			tun_.Write(Tun::kTun, buf + 1, nread - 1, NULL);
	}
	delete[] buf;

	return (void*)NULL;
}

void* WspaceController::ComputeRoute(void* arg)
{
	while(1)
	{
		routing_tbl_.UpdateRoute(bs_stats_tbl_, tun_);
		usleep(1000000);
	}
	return (void*)NULL;
}

void* WspaceController::Forward(void* arg)
{
	printf("forward start\n");

	uint16 len = 0;
	char *pkt = new char[PKT_SIZE];

	char ip_tun[16] = {0};
	char ip_eth[16] = {0};
	int dest_id = 0;
	struct in_addr addr;
	struct sockaddr_in server_addr_eth;
	BSInfo* info;
	while (1)
	{
		len = tun_.Read(Tun::kTun, pkt + 1, PKT_SIZE - 1);
		memcpy(&addr.s_addr, pkt + 1 + 16, sizeof(long));//suppose it's a IP packet and get the destination inner IP address like 10.0.0.2
		strncpy(ip_tun, inet_ntoa(addr), 16);
		printf("read %d bytes from tun to %s\n", len, ip_tun);
		dest_id = atoi(strrchr(ip_tun,'.') + 1);
		printf("dest_id: %d\n", dest_id);
		info = routing_tbl_.GetRoute(dest_id);
		strncpy(ip_eth, info->ip_eth, 16);
		tun_.CreateAddr(ip_eth, info->port, &server_addr_eth);
		printf("convert address to %s\n", info->ip_eth);
		if(routing_tbl_.IsBS(dest_id))
		{
			*pkt = CONTROL_BS;
			printf("is to BS\n");
		}
		else
		{
			*pkt = FORWARD_DATA;
			printf("is to client\n");
		}
		tun_.Write(Tun::kControl, pkt, len + 1, &server_addr_eth);
	}
	delete[] pkt;

	return (void*)NULL;
}
