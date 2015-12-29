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

BSStatsTable::BSStatsTable()
{
	Pthread_mutex_init(&lock_, NULL);
	//currently size of stats_ 256*256
	stats_.resize(256);
	for (int i=0; i<256; i++)
	{
		vector<double> array;
		array.resize(256);
		stats_[i]=array;
	}

}

BSStatsTable::~BSStatsTable()
{
	Pthread_mutex_destroy(&lock_);
}

void BSStatsTable::Update(int bs, int client, double throughput)
{
	Lock();
	stats_[bs][client] = throughput;
	UnLock();
	printf("%d %d %f\n", bs, client, throughput);
}

double BSStatsTable::FindStats(int bs, int client)
{
	Lock();
	double throughput = stats_[bs][client];
	UnLock();
	return throughput;
}

int main(int argc, char **argv)
{
	const char* opts = "C:i:";
	wspace_controller = new WspaceController(argc, argv, opts);

	wspace_controller->tun_.InitSock();

	Pthread_create(&wspace_controller->p_recv_stats_, NULL, LaunchRecvStats, NULL);
	Pthread_create(&wspace_controller->p_compute_route_, NULL, LaunchComputeRoute, NULL);

	Pthread_join(wspace_controller->p_recv_stats_, NULL);
	Pthread_join(wspace_controller->p_compute_route_, NULL);

	delete wspace_controller;
	return 0;
}

WspaceController::WspaceController(int argc, char *argv[], const char *optstring) : routing_tbl_(256)	//max client number is set
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
	assert(tun_.controller_ip_eth_[0]);
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
		nread = tun_.Read(buf, PKT_SIZE);
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
				bs_stats_tbl_.Update(bs_id, client_id, throughput);
			}
		}
	}
	delete[] buf;

	return (void*)NULL;
}

void* WspaceController::ComputeRoute(void* arg)
{

	return (void*)NULL;
}



