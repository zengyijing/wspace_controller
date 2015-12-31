#ifndef TUN_H_
#define TUN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>

/* buffer for reading from tun/tap interface, must be >= 1500 */
//#define PKT_SIZE 2000   
#define PORT_ETH 55554
#define PORT_ATH 55555

#define MAX_SERVER 3

class Tun
{
public:
	enum IOType
	{
		kTun=1,
		kWspace, 
		kCellular, 
		kControl,
	};

	Tun(): tun_type_(IFF_TUN), port_eth_(PORT_ETH)
	{
		bzero(if_name_, sizeof(if_name_));
		bzero(server_ip_eth_, sizeof(server_ip_eth_));
		bzero(server_ip_tun_, sizeof(server_ip_tun_));
		bzero(controller_ip_eth_, sizeof(controller_ip_eth_));
		server_count_ = 0;
	}

	~Tun()
	{
		close(tun_fd_);
	 	close(sock_fd_eth_);
	}
	
	void InitSock();
	int AllocTun(char *dev, int flags);
	int CreateSock();
	void BindSocket(int fd, sockaddr_in *addr);
	void CreateAddr(const char *ip, int port, sockaddr_in *addr);
	uint16_t Read(const IOType &type, char *buf, uint16_t len);
	uint16_t Write(const IOType &type, char *buf, uint16_t len, sockaddr_in *server_addr_eth_);

// Data members:
	int tun_fd_;
	int tun_type_;    		// TUN or TAP
	char if_name_[IFNAMSIZ];

	char server_ip_eth_[MAX_SERVER][16];
	char server_ip_tun_[MAX_SERVER][16];
	int server_count_;

	struct sockaddr_in server_addr_eth_; 
	uint16_t port_eth_;
	int sock_fd_eth_;

	//modified by Zeng
	char controller_ip_eth_[16];
	struct sockaddr_in controller_addr_eth_;
	//end modification
};

int cread(int fd, char *buf, int n);
int cwrite(int fd, char *buf, int n);
int read_n(int fd, char *buf, int n);

#endif
