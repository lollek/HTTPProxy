
#include <string>
#include <vector>

enum IPV { IPV4, IPV6 };

class TCPSocket {
  public:
    TCPSocket (IPV ip_version);
    TCPSocket (const TCPSocket &other);
    ~TCPSocket();

    /* Connects to remote host
     * hostname = hostname or ip of remote host
     * port     = port of remote host to connect to
     * returns 0 on success
     */
    int connect(const std::string &hostname, int port);

    /* Bind to a local port
     * port = local port to bind to
     * returns 0 on success
     */
    int bind(int port);

    /* Listen to incoming connections
     * num = how many incoming connections that can be queued
     * returns 0 on success
     */
    int listen(int num) const;

    /* Accept an incoming connection
     * returns a pointer to the client socket, which must later be deleted
     */
    TCPSocket *accept() const;

    /* Receive data from socket
     * num = size of buffer
     * returns a string with received data,
     *  which is empty if nothing is received
     */
    std::vector<char> recv(int num) const;

    /* Receive all data from socket
     * This is basically calling recv() several times */
    std::vector<char> recvall() const;

    /* Send data to socket
     * message = data to send
     * returns 0 on success
     */
    int send(const std::vector<char> &message) const;
    int send(const std::string &message) const;
    int send(const char *message, long length) const;

    /* Close socket */
    void close();

    /* Set if socket should reuse port or not
     * Must be set before bind to have an effect 
     */
    void reuseAddr(bool var);

    /* Get hostname of which the socket is connected to */
    std::string getHostname() const;

    /* Forbidden methods */
    void operator=(const TCPSocket&) = delete;

  private:

    TCPSocket(int sock, const char *ip, IPV ip_version);
    int construct(const char *hostname, const char *port);

    /* Member variables */
    const IPV ip_version_;
    int sock_;
    char *ip_;
    bool reuseaddr_;
};
