#ifndef __HTTP_PROXY_HH__
#define __HTTP_PROXY_HH__

#include <string>

#include <signal.h>

#include "TCPSocket.hh"

void sigchld_handle(int sig);

class HTTPProxy {
  public:
    HTTPProxy(int port);
    int run() const;
  private:

    /* Socket methods */
    int handleRequest(TCPSocket *client) const;

    /* Data matching methods */
    std::string findHostName(const std::string &data) const;

    /* Variables */
    const int port_;
    struct sigaction sigchld_handle_;
};

#endif //__HTTP_PROXY_HH__