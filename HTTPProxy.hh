#ifndef __HTTP_PROXY_HH__
#define __HTTP_PROXY_HH__

#include <vector>

#include <signal.h>

#include "TCPSocket/TCPSocket.hh"

void sigchld_handle(int sig);

class HTTPProxy {
  public:
    HTTPProxy(int port);
    int run() const;
  private:

    /* Socket methods */
    int handleRequest(TCPSocket *client) const;

    /* Actions */
    int redirectToError1(TCPSocket *client) const;
    int redirectToError2(TCPSocket *client) const;
    int redirectToURL(TCPSocket *client, const char *url) const;

    /* Data reading methods */
    std::string findHostName(const std::vector<char> &data) const;
    bool hasBlockedContents(const std::vector<char> &data) const;
    bool contentIsText(const std::vector<char> &data) const;

    /* Data modification methods */
    void removeKeepAlive(std::vector<char> &data) const;
    void shortenLongGets(std::vector<char> &data) const;

    /* Variables */
    const int port_;
    struct sigaction sigchld_handle_;
};

#endif //__HTTP_PROXY_HH__
