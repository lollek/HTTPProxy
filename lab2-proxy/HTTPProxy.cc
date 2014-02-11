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
  cout << "Data transfer: client -> proxy ..." << flush;
  vector<char> client_data_array = client->recvall();
  string client_data(client_data_array.data());
  string target_hostname = findHostName(client_data);
  if (target_hostname.size() == 0) {
    cout << "No hostname found - return" << endl;
    return 1;
  }
  cout << "DONE (" << client_data_array.size() << ")" << endl;

  /* Connect to true target */
  cout << "Connecting proxy -> " << target_hostname << ":80..." << flush;
  TCPSocket target = TCPSocket(IPV4);
  if (target.connect(findHostName(client_data), 80) != 0 ) {
    return 1;
  }
  cout << " DONE" << endl;

  /* Send data client -> target */
  cout << "Data transfer: proxy -> target ..." << flush;
  target.send(client_data_array);
  cout << "DONE" << endl;

  /* Send data target -> proxy -> client */
  cout << "Data transfer: target -> proxy ..." << flush;
  vector<char> target_data_array = target.recvall();
  client->send(target_data_array);
  cout << "DONE (" << target_data_array.size() << ")" << endl;

  cout << "Data transfer: proxy -> client ..." << flush;
  client->send(target_data_array);
  cout << "DONE" << endl;

  /* If keep-alive: do some loopy stuff */
  bool do_break = false;
  while (isKeepAlive(client_data) && !do_break) {
    cout << "(KEEP-ALIVE) Data transfer: client -> proxy ..." << flush;
    client_data_array = client->recvall();
    cout << "DONE (" << client_data_array.size() << ")" << endl;
    cout << "(KEEP-ALIVE) Data transfer: proxy -> target ..." << flush;
    if (client_data_array.size() == 0 ||
        target.send(client_data_array) != 0) {
      cout << "BREAK!" << endl;
      break;
    }
    cout << "DONE" << endl;



    cout << "(KEEP-ALIVE) Data transfer: target -> proxy ..." << flush;
    target_data_array = target.recvall();
    cout << "DONE (" << target_data_array.size() << ")" << endl;
    cout << "(KEEP-ALIVE) Data transfer: proxy -> client ..." << flush;
    if (target_data_array.size() == 0 ||
        client->send(target_data_array) != 0) {
      cout << "BREAK!" << endl;
      break;
    }
    cout << "DONE" << endl;
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

void HTTPProxy::removeKeepAlive(vector<char> &data) const {
  char *real_data = data.data();
  const char *replace_str = "close";
  cout << "OLD DATA (" << data.size() << "):\n" << data.data() << endl;
  for (unsigned i = 0; i < data.size(); ++i) {
    if (!strncmp(real_data + i, "Keep-Alive", 10) ||
        !strncmp(real_data + i, "keep-alive", 10)) {
      cout << "Removed keepalive" << endl;
      memcpy(real_data + i, replace_str, 5);
      data.erase(data.begin() + i + 5, data.begin() + i + 10);
      if (!strncmp(real_data + i, "close\r\n\r\n", 9)) {
        cout << "replace - OK! i = " << i << endl;
      }
  cout << "NEW DATA (" << data.size() << "):\n" << data.data() << endl;
      return;
    }
  }

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
