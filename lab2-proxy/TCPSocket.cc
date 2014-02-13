#include <unistd.h>
#include <string.h>

#include <string>
#include <iostream>

#include <netdb.h>
#include <arpa/inet.h>

#include "TCPSocket.hh"

using namespace std;

TCPSocket::TCPSocket(IPV ip_version) :
  ip_version_(ip_version),
  sock_(-1),
  ip_(new char[INET6_ADDRSTRLEN]),
  reuseaddr_(false)
{
  memset(ip_, 0, INET6_ADDRSTRLEN);
}

TCPSocket::TCPSocket(const TCPSocket &other) :
  ip_version_(other.ip_version_),
  sock_(other.sock_),
  ip_(new char[INET6_ADDRSTRLEN]),
  reuseaddr_(other.reuseaddr_)
{
  strncpy(ip_, other.ip_, INET6_ADDRSTRLEN);
  ip_[INET6_ADDRSTRLEN -1] = '\0';
}

TCPSocket::~TCPSocket() {
  delete[] ip_;
  ip_ = NULL;
}

void TCPSocket::reuseAddr(bool var) {
  reuseaddr_ = var;
}

TCPSocket::TCPSocket(int sock, const char *ip, IPV ip_version) :
  ip_version_(ip_version),
  sock_(sock),
  ip_(new char[INET6_ADDRSTRLEN]),
  reuseaddr_(false)
{
  strncpy(ip_, ip, INET6_ADDRSTRLEN);
  ip_[INET6_ADDRSTRLEN -1] = '\0';
}

int TCPSocket::construct(const char *hostname, const char *port) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = ip_version_ == IPV4 ? AF_INET : AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  if (hostname == NULL) {
    hints.ai_flags = AI_PASSIVE;
  }

  struct addrinfo *results = NULL;
  int status = getaddrinfo(hostname, port, &hints, &results);
  if (status != 0) {
    cerr << "Error while looking up address info: "
         << gai_strerror(status) << '\n';
    return 1;
  }

  for (struct addrinfo *p = results; p != NULL; p = p->ai_next) {
    sock_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock_ == -1) {
      perror("socket:");
      continue;
    }

    if (hostname == NULL) {
      if (reuseaddr_) {
        int one = 1;
        if (setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR,
                       &one, sizeof(one)) == -1) {
          perror("setsockopt");
        }
      }

      if (::bind(sock_, p->ai_addr, p->ai_addrlen) == -1) {
        ::close(sock_);
        perror("bind");
        sock_ = -1;
      } else {
        break;
      }

    } else {
      if (::connect(sock_, p->ai_addr, p->ai_addrlen) == -1) {
        ::close(sock_);
        perror("connect");
        sock_ = -1;
      } else {
        break;
      }
    }
  }

  freeaddrinfo(results);

  return sock_ == -1;
}

int TCPSocket::connect(const string &hostname, int port) {
  strncpy(ip_, hostname.c_str(), INET6_ADDRSTRLEN);
  ip_[INET6_ADDRSTRLEN -1] = '\0';
  return construct(hostname.c_str(), to_string(port).c_str());
}

int TCPSocket::bind(int port) {
  return construct(NULL, to_string(port).c_str());
}

int TCPSocket::listen(int num) const {
  if (::listen(sock_, num) == -1) {
    perror("listen");
    return 1;
  }
  return 0;
}

TCPSocket *TCPSocket::accept() const {
  struct sockaddr_storage client_addr;
  socklen_t socklen = sizeof(client_addr);
  int client = ::accept(sock_, (struct sockaddr *)&client_addr, &socklen);
  if (client == -1) {
    return NULL;
  }

  void *sin_addr = NULL;
  if (((struct sockaddr *)&client_addr)->sa_family == AF_INET) {
    sin_addr = &(((struct sockaddr_in *)&client_addr)->sin_addr);
  } else {
    sin_addr = &(((struct sockaddr_in6 *)&client_addr)->sin6_addr);
  }
  char client_ip[INET6_ADDRSTRLEN];
  inet_ntop(client_addr.ss_family, sin_addr, client_ip, INET6_ADDRSTRLEN);

  return new TCPSocket(client, client_ip, ip_version_);
}

vector<char> TCPSocket::recv(int num) const {
  vector<char> data(num);
  int datalen = ::recv(sock_, data.data(), num, 0);
  if (datalen == -1) {
    cerr << "Failed to read from client\n";
    return vector<char>(0);
  } else {
    data.resize(datalen);
    return data;
  }
}

vector<char> TCPSocket::recvall() const {
  const unsigned BUFSIZE = 1024;
  vector<char> data(BUFSIZE);
  int datalen = ::recv(sock_, data.data(), BUFSIZE, 0);
  if (datalen == -1 ) {
    cerr << "Failed to read from client\n";
    return vector<char>(0);
  } else {
    data.resize(datalen);

    if (datalen == BUFSIZE) {
      vector<char> tmp(BUFSIZE);
      while ((datalen = ::recv(sock_, tmp.data(), BUFSIZE, 0)) > 0) {
        for (int i = 0; i < datalen; ++i) {
          data.push_back(tmp[i]);
        }
      }
      for (int i = 0; i < datalen; ++i) {
        data.push_back(tmp[i]);
      }
    }
    return data;
  }
}

int TCPSocket::send(const vector<char> &message) const {
  return send(message.data(), message.size());
}

int TCPSocket::send(const string &message) const {
  return send(message.c_str(), message.length());
}

int TCPSocket::send(const char *data, long data_length) const {
  int data_left = data_length;
  while (data_left > 0) {
    int tmp = ::send(sock_, data +data_length -data_left, data_left, 0);
    if (tmp == -1) {
      cerr << "Failed to send!\n";
      return 1;
    }
    data_left -= tmp;
  }
  return 0;
}

void TCPSocket::close() {
  ::close(sock_);
  delete[] ip_;
  ip_ = NULL;
}

string TCPSocket::getHostname() const {
  return string(ip_);
}
