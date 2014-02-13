
#include <string>
#include <vector>

enum IPV { IPV4, IPV6 };

class TCPSocket {
  public:
    TCPSocket (IPV ip_version);
    TCPSocket (const TCPSocket &other);
    ~TCPSocket();

#ifndef __TCPSOCKET_HIDDEN_FROM_SOURCE__
    /* Connects to remote host
     * hostname = hostname or ip of remote host
     * port     = port of remote host to connect to
     * returns 0 on success
     */
    int connect(const std::string &hostname, int port)
      { return _connect(hostname, port); }

    /* Bind to a local port
     * port = local port to bind to
     * returns 0 on success
     */
    int bind(int port)
      { return _bind(port); }

    /* Listen to incoming connections
     * num = how many incoming connections that can be queued
     * returns 0 on success
     */
    int listen(int num) const
      { return _listen(num); }

    /* Accept an incoming connection
     * returns a pointer to the client socket, which must later be deleted
     */
    TCPSocket *accept() const
      { return _accept(); }

    /* Receive data from socket
     * num = size of buffer
     * returns a string with received data,
     *  which is empty if nothing is received
     */
    std::vector<char> recv(int num) const
      { return _recv(num); }

    /* Send data to socket
     * message = data to send
     * returns 0 on success
     */
    int send(const std::vector<char> &message) const
      { return _send(message); }

    /* Close socket */
    void close()
      { return _close(); }
#endif

    /* Receive all data from socket
     * This is basically calling recv() several times */
    std::vector<char> recvall() const;

    /* Set if socket should reuse port or not
     * Must be set before bind to have an effect 
     */
    void reuseAddr(bool var);

    /* Get hostname of which the socket is connected to */
    std::string getHostname() const;

    /* Forbidden methods */
    void operator=(const TCPSocket&) = delete;

  private:

    /* Server */
    int _bind(int port);
    int _listen(int num) const;
    TCPSocket *_accept() const;

    /* Client */
    int _connect(const std::string &hostname, int port);

    /* Both */
    TCPSocket(int sock, const char *ip, IPV ip_version);
    int construct(const char *hostname, const char *port);
    int _send(const std::vector<char> &message) const;
    std::vector<char> _recv(int num) const;
    void _close();

    /* Member variables */
    const IPV ip_version_;
    int sock_;
    char *ip_;
    bool reuseaddr_;
};
