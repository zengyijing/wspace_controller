#include "tun.h"

int Tun::AllocTun(char *dev, int flags) {
  struct ifreq ifr;
  int fd, err;
  char *clonedev = "/dev/net/tun";

  if( (fd = open(clonedev , O_RDWR)) < 0 ) {
    perror("Opening /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = flags;

  if (*dev) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  if((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
    perror("ioctl(TUNSETIFF)");
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);
  return fd;
}

void Tun::CreateAddr(const char *ip, int port, sockaddr_in *addr) {
  memset(addr, 0, sizeof(sockaddr_in));
  addr->sin_family = AF_INET;
  if (!strcmp(ip, controller_ip_eth_)) {
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
    addr->sin_addr.s_addr = inet_addr(ip);
  }
  addr->sin_port = htons(port);
}

void Tun::BindSocket(int fd, sockaddr_in *addr) {
  int optval = 1;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0)  
    perror("setsocketopt()");

  if(bind(fd, (struct sockaddr*)addr, addr_len) < 0)  
    perror("ath bind()");
}

void Tun::InitSock() {
  /* initialize tun/tap interface */
  if ((tun_fd_ = AllocTun(if_name_, tun_type_ | IFF_NO_PI)) < 0 ) {
    perror("Error connecting to tun/tap interface!");
  }

  // Create sockets
  sock_fd_eth_ = CreateSock();
  CreateAddr(controller_ip_eth_, port_eth_, &controller_addr_eth_); 
  BindSocket(sock_fd_eth_, &controller_addr_eth_);

}

int Tun::CreateSock() {
  int sock_fd;
  if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
  {
    perror("UDP socket()");
  }
  return sock_fd;
}

uint16_t Tun::Read(const IOType &type, char *buf, uint16_t len) {
  uint16_t nread=-1;
  if (type == kTun)
    nread = cread(tun_fd_, buf, len);
  else if (type == kControl)
    nread = recvfrom(sock_fd_eth_, buf, len, 0, NULL, NULL);

  assert(nread > 0);
  return nread;
}

uint16_t Tun::Write(const IOType &type, char *buf, uint16_t len, sockaddr_in *server_addr_eth) {
  uint16_t nwrite=-1;
  assert(len > 0);
  socklen_t addr_len = sizeof(struct sockaddr_in);
  if (type == kTun)
    nwrite = cwrite(tun_fd_, buf, len);
  else if (type == kControl)
    nwrite = sendto(sock_fd_eth_, buf, len, 0, (struct sockaddr*)server_addr_eth, addr_len);

  assert(nwrite == len);
  return nwrite;
}

bool Tun::IsValidClient(int client_id) {
  return client_ip_tbl_.count(client_id);
}

inline int cread(int fd, char *buf, int n) {
  int nread;

  if((nread=read(fd, buf, n)) < 0)
  {
    perror("Reading data");
  }
  return nread;
}

inline int cwrite(int fd, char *buf, int n) {
  int nwrite;
  if((nwrite=write(fd, buf, n)) < 0)
  {
    perror("Writing data");
  }
  return nwrite;
}
