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

class Tun
{
public:
	Tun(): port_eth_(PORT_ETH)
	{
		server_ip_eth_[0] = '\0';
		controller_ip_eth_[0] = '\0';
	}

	~Tun()
	{
	 	close(sock_fd_eth_);
	}
	
	void InitSock();
	int CreateSock();
	void BindSocket(int fd, sockaddr_in *addr);
	void CreateAddr(const char *ip, int port, sockaddr_in *addr);
	uint16_t Read(char *buf, uint16_t len);
	uint16_t Write(char *buf, uint16_t len, sockaddr_in *server_addr_eth_);

// Data members:

	char server_ip_eth_[16];

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
