#include <vector>
#include <cmath>
#include "wspace_controller.h"
#include "base_rate.h"
#include "monotonic_timer.h"

using namespace std;

WspaceController *wspace_controller;

int main(int argc, char **argv)
{
	const char* opts = "C:";
	wspace_controller = new WspaceController(argc, argv, opts);

	wspace_controller->tun_.InitSock();

	Pthread_create(&wspace_controller->p_recv_stats_, NULL, LaunchRecvStats, NULL);
	Pthread_create(&wspace_controller->p_select_bs_, NULL, LaunchSelectBS, NULL);

	Pthread_join(wspace_controller->p_recv_stats_, NULL);
	Pthread_join(wspace_controller->p_select_bs_, NULL);

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
				strncpy(tun_.controller_ip_eth_,optarg,16);
				printf("controller_ip_eth: %s\n", tun_.controller_ip_eth_);
				break;
			default:
				Perror("Usage: %s -C controller_ip_eth \n", argv[0]);
		}
	}
	assert(tun_.controller_ip_eth_[0]);
}

WspaceController::~WspaceController()
{
}


/*msg include client_ip_eth_, laptop type, start timer, end timer, loss rate of all rates*/
void WspaceController::ParseLossRate(char * pkt, uint16_t recv_len)
{
	LossRatePktHeader header;
	memcpy(&header, pkt, sizeof(header));
	//double* loss_rate = new double[mac80211abg_num_rates];
	//memcpy(loss_rate, pkt+sizeof(header), recv_len-sizeof(header));

	printf("%s %s %d: \n", header.client_ip_eth_, header.server_ip_eth_, header.laptop_);
	double loss_rate;
	for (int i=0; i<mac80211abg_num_rates; i++)
	{
		memcpy(&loss_rate, pkt + sizeof(header) + i * sizeof(double), sizeof(double));
		printf("%f ", loss_rate);
	}
	printf("\n");
/*
	ClientLaptop client_laptop;
	strncpy(client_laptop.client_ip_eth, header.client_ip_eth_, 16);
	client_laptop.laptop = header.laptop_;

	std::map<ClientLaptop, BSRateLoss>::iterator it;
	it =  client_bs_map_.Find(client_laptop);
	if (it!=client_bs_map_.GetEnd())
	{
		//update map
	}
	else	//insert into map
	{
		BSRateLoss bs_rate_loss;
		strncpy(bs_rate_loss.server_ip_eth, header.server_ip_eth_, 16);
		
	}
*/
//	delete[] loss_rate;
}


void* WspaceController::RecvStats(void* arg)
{
	uint16 nread=0;
	char *buf = new char[PKT_SIZE];
	while (1)
	{
		nread = tun_.Read(buf, PKT_SIZE);
		ParseLossRate(buf, nread);
	}
	delete[] buf;

	return (void*)NULL;
}

void* WspaceController::SelectBS(void* arg)
{

	return (void*)NULL;
}


void* LaunchRecvStats(void* arg)
{
	wspace_controller->RecvStats(arg);
}

void* LaunchSelectBS(void* arg)
{
	wspace_controller->SelectBS(arg);
}


