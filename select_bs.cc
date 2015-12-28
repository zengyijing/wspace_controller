#include "select_bs.h"

using namespace std;

ClientBSMap::ClientBSMap()
{
	Pthread_mutex_init(&lock_, NULL);
}

ClientBSMap::~ClientBSMap()
{
	Pthread_mutex_destroy(&lock_);
}

/** LossMap */
/*
LossMap::LossMap(const int *rate_arr, int num_rates)
{
	LossInfo info = {0, 0, INVALID_LOSS_RATE};
	for (int i = 0; i < num_rates; i++)
		loss_map_[rate_arr[i]] = info;
	Pthread_mutex_init(&lock_, NULL);
}

LossMap::~LossMap()
{
	Pthread_mutex_destroy(&lock_);
}

void LossMap::UpdateLoss(uint16_t rate, double loss, double previous_weight)
{
	Lock();
	assert(loss_map_.count(rate) > 0);  // Ensure rate is in the map. 
	assert(loss == INVALID_LOSS_RATE || (loss >= 0 && loss <= 1));
	assert(previous_weight <= 1);
	if (previous_weight > 0 && loss != INVALID_LOSS_RATE && loss_map_[rate].loss != INVALID_LOSS_RATE)
	{
		loss_map_[rate].loss = loss_map_[rate].loss * previous_weight + loss * (1 - previous_weight);
	}
	else if (previous_weight <= 0 || loss_map_[rate].loss == INVALID_LOSS_RATE)// initial phase 
	{
		loss_map_[rate].loss = loss;
	}
	else // if loss == -1, don't update and use the previous loss rate.
	{
	}
//	if (loss_map_[rate].loss != INVALID_LOSS_RATE)
//		loss_map_[rate].loss = loss_map_[rate].loss / 2.;
	UnLock();
}

void LossMap::UpdateSendCnt(uint16_t rate, uint32_t n_sent, uint32_t n_ack)
{
	Lock();
	assert(loss_map_.count(rate) > 0);  // Ensure rate is in the map.
	loss_map_[rate].n_sent = n_sent;
	loss_map_[rate].n_ack = n_ack;
	UnLock();
}

double LossMap::GetLossRate(uint16_t rate)
{
	double loss;
	Lock();
	assert(loss_map_.count(rate) > 0);  
	loss = loss_map_[rate].loss;
	UnLock();
	return loss;
}

uint32_t LossMap::GetNSent(uint16_t rate)
{
	uint32_t n_sent;	
	Lock();
	assert(loss_map_.count(rate) > 0);  
	n_sent = loss_map_[rate].n_sent;
	UnLock();
	return n_sent;
}

uint32_t LossMap::GetNAck(uint16_t rate)
{
	uint32_t n_ack;	
	Lock();
	assert(loss_map_.count(rate) > 0);  
	n_ack = loss_map_[rate].n_ack;
	UnLock();
	return n_ack;
}

void LossMap::Print()
{
	Lock();
	map<uint16_t, LossInfo>::const_iterator it_map;
	for (it_map = loss_map_.begin(); it_map != loss_map_.end(); it_map++)
		printf("%u\t%.3f\n", it_map->first, it_map->second.loss);
	UnLock();
}
*/

