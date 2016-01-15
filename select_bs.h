// @yijing: Remove select_bs* file if they are not in use.
#ifndef SELECT_BS_H_
#define SELECT_BS_H_

#include <map>
#include <algorithm>
#include <iostream>

#include "base_rate.h"
#include "monotonic_timer.h"
#include "wspace_asym_util.h"


struct ClientLaptop
{
	char client_ip_eth[16];
	Laptop laptop;
};

struct BSRateLoss
{
	char server_ip_eth[16];
	int32_t rate;
	double loss_rate;
};
/*
struct LossInfo
{
	uint32_t n_sent; 
	uint32_t n_ack;
	double loss;
};

class LossMap
{
public:
	LossMap(const int *rate_arr, int num_rates);
	~LossMap();

	//**
	// * Update the key of the map.
	// * Note: Locking is included.
	// * @previous_weight: the weight for the history.
	// *
	void UpdateLoss(uint16_t rate, double loss, double previous_weight = -1);

	void UpdateSendCnt(uint16_t rate, uint32_t n_sent, uint32_t n_ack);
	
	//**
	// * Return the loss rate of a given data rate.
	// * Note: Locking is included.
	// *
	double GetLossRate(uint16_t rate);

	uint32_t GetNSent(uint16_t rate);

	uint32_t GetNAck(uint16_t rate);

	void Print();

private:
	void Lock() { Pthread_mutex_lock(&lock_); }

	void UnLock() { Pthread_mutex_unlock(&lock_); }

	std::map<uint16_t, LossInfo> loss_map_; // Store the loss rate of each data rate.
	pthread_mutex_t lock_;
};
*/


class ClientBSMap
{
public:
	ClientBSMap();
	~ClientBSMap();
//	std::map<ClientLaptop, BSRateLoss>::iterator Find(ClientLaptop client_laptop) {return client_bs_map_.find(client_laptop); };
//	std::map<ClientLaptop, BSRateLoss>::iterator GetEnd() {return client_bs_map_.end(); };
private:
	void Lock() { Pthread_mutex_lock(&lock_); }
	void UnLock() { Pthread_mutex_unlock(&lock_); }
	std::map<ClientLaptop, BSRateLoss> client_bs_map_;
	pthread_mutex_t lock_;
};

template <class T>
int FindMaxInd(const vector<T> &arr)
{
	size_t sz = arr.size();
	if (sz == 0)
		return -1;
	int max_ind = 0;
	for (int i = 1; i < sz; i++) 
		if (arr[max_ind] < arr[i])
			max_ind = i;
	return max_ind;
}

template <class T>
void PrintVector(const vector<T> &arr)
{
	class vector<T>::const_iterator it;
	cout << "size[" << arr.size() << "] ";
	for (it = arr.begin(); it != arr.end(); it++) 
		cout << *it << " ";
	cout << endl;
}

#endif 
