#include <time.h>
#include <cmath>
#include "wspace_asym_util.h"
#include "tun.h"
#include "select_bs.h"


class WspaceController
{
public:
	WspaceController(int argc, char *argv[], const char *optstring);
	~WspaceController();
	
	void* RecvStats(void* arg);
	void* SelectBS(void* arg);



// Data member
	pthread_t p_recv_stats_, p_select_bs_;
	ClientBSMap client_bs_map_;
	Tun tun_;


private:
	/**
	 * @param laptop type, start timer, end timer
	 */
	void ParseLossRate(char * pkt, uint16_t recv_len);
};

/** Wrapper function for pthread_create. */
void* LaunchRecvStats(void* arg);
void* LaunchSelectBS(void* arg);

