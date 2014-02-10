#include <string.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <iostream>

#include <sys/wait.h>

#include "HTTPProxy.hh"

using namespace std;

void sigchld_handler(int sig) {
  (void)sig;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

HTTPProxy::HTTPProxy(int port) :
  port_(port),
  sigchld_handle_()
{
  memset(&sigchld_handle_, 0, sizeof(sigchld_handle_));
  sigchld_handle_.sa_handler = sigchld_handler;
  if (sigaction(SIGCHLD, &sigchld_handle_, 0)) {
    perror("sigaction");
    exit(1);
  }
}

int HTTPProxy::run() const {
  /* Init */
  TCPSocket serv(IPV4);
  serv.reuseAddr(true);
  if (serv.bind(port_) != 0 ||
      serv.listen(10) != 0 ) {
    return 1;
  }

  /* Loop */
  for (;;) {
    TCPSocket *client = serv.accept();
    if (client == NULL) {
      continue;
    }

    /* Fork off new processes */
    if (!fork()) {
      handleRequest(client);
      client->close();
      delete client;
      exit(0);
    }
    client->close();
  }
  serv.close();
  return 0;
}

int HTTPProxy::handleRequest(TCPSocket *client) const {

  /* Receive data from client */
  vector<char> client_data_array = client->recvall();
  string client_data(client_data_array.data());
  string target_hostname = findHostName(client_data);

  /* Connect to true target */
  cout << "Connecting to " << target_hostname << ":80" << endl;
  TCPSocket target = TCPSocket(IPV4);
  if (target.connect(findHostName(client_data), 80) != 0 ) {
    return 1;
  }

  /* Send data client -> target and then target -> client */
  target.send(client_data_array);
  vector<char> target_data_array = target.recvall();
  string target_data(target_data_array.data());
  client->send(target_data_array);

  cout << "client -> target:\n"
       << client_data << endl;
  cout << "target -> client\n"
       << target_data << endl;

  /* If keep-alive: do some loopy stuff */
  while (isKeepAlive(client_data)) {
    client_data_array = client->recvall();
    if (client_data_array.size() == 0 ||
        target.send(client_data_array) != 0) {
      break;
    }

    target_data_array = target.recvall();
    if (target_data_array.size() == 0 ||
        client->send(target_data_array) != 0) {
      break;
    }
  }

  target.close();
  return 0;
}

string HTTPProxy::findHostName(const string &data) const {
  /* Return the Host: www.google.com part 
   * Should work with HTTP/1.1 but maybe not HTTP/1.0 */
  const string start_delim = "Host: ";
  unsigned start = data.find(start_delim);
  if (start == string::npos) {
    return "";
  } else {
    start += start_delim.length();
  }

  unsigned end = start;
  while (isalpha(data[end]) ||
         isdigit(data[end]) ||
         data[end] == '.'   ||
         data[end] == '-' ) {
    ++end;
  }

  return data.substr(start, end - start);
}

bool HTTPProxy::isKeepAlive(const string &data) const {
  /* Check first if there's an (Proxy-)Connection: (keep-alive|close) */
  unsigned start = data.find("Connection: ");
  if (start != string::npos) {
    start += strlen("Connection: ");
    string result = data.substr(start, strlen("close"));
    for (auto p = result.begin(); p != result.end(); ++p) {
      *p = tolower(*p);
    }
    if (result == "close") {
      return false;
    } else {
      return true;
    }
  }

  /* If not found, use keep-alive if HTTP/1.1 or 1.2 - else close */
  else {
    if (data.find("HTTP/1.1") != string::npos ||
        data.find("HTTP/1.2") != string::npos) {
      return true;
    } else {
      return false;
    }
  }
}
