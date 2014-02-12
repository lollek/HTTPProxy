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

  const unsigned BUFSIZE = 1024;

  /* Receive data from client */
  vector<char> client_data_array = client->recv(BUFSIZE);
  string client_data(client_data_array.data());
  // Check if bad GET URL

  cout << "BEFORE:\n" << string(client_data_array.data()) << "\n" << endl;
  removeKeepAlive(client_data_array);
  cout << "AFTER:\n" << string(client_data_array.data()) << "\n" << endl;

  string target_hostname = findHostName(client_data);
  if (target_hostname.size() == 0) {
    cerr << "No hostname found - Data(" << client_data.size() << ")"
         << client_data << '\n';
    return 1;
  }
  cout << "Received connection to " << target_hostname << endl;


  /* Connect to true target */
  TCPSocket target = TCPSocket(IPV4);
  if (target.connect(findHostName(client_data), 80) != 0 ) {
    cout << "Failed to connect to " << target_hostname << endl;
    return 1;
  }

  /* Send first array to target */
  if (target.send(client_data_array) != 0) {
    cerr << "Error sending data1 to " << target_hostname << endl;
    target.close();
    return 1;
  }
  /* Now send the rest */
  if (client_data_array.size() == BUFSIZE) {
    while ((client_data_array = client->recv(BUFSIZE)).size()) {
      if (target.send(client_data_array) != 0) {
        cerr << "Error sending data2 to " << target_hostname << endl;
        target.close();
        return 1;
      }
    }
  }

  /* Send data from target to client */
  vector<char> target_data_array = target.recv(BUFSIZE);
  /* Check if Content-Type; text/  */
  if (contentIsText(string(target_data_array.data()))) {
    if (client->send(target_data_array) != 0 ||
        client->send(target.recvall()) != 0) {
      target.close();
      cerr << "Error sending data(text) from " << target_hostname << endl;
      return 1;
    }

  /* Else: binary */
  } else {
    if (client->send(target_data_array) != 0) {
      cerr << "Error sending data(bin) from " << target_hostname << endl;
      target.close();
      return 1;
    }
    if (target_data_array.size() == BUFSIZE) {
      while ((target_data_array = target.recv(BUFSIZE)).size()) {
        if (client->send(target_data_array) != 0) {
          cerr << "Error sending data(bin) from " << target_hostname << endl;
          target.close();
          return 1;
        }
      }
    }
  }

  target.close();
  return 0;
}

string HTTPProxy::findHostName(const string &data) const {
  /* Check for Host: www.hostname.dom\r\n */
  const string start_delim = "Host: ";
  unsigned start = data.find(start_delim);
  if (start != string::npos) {
    start += start_delim.length();

    unsigned end = start;
    while (isalpha(data[end]) ||
           isdigit(data[end]) ||
           data[end] == '.'   ||
           data[end] == '-' ) {
      ++end;
    }
    return data.substr(start, end - start);
  }

  /* Otherwise, check for COMMAND https?://www.hostname.com\r\n */
  for (start = 0; data[start] != '\n'; ++start) {
    if (data[start] == ':' && data[start +1] == '/' && data[start +2] == '/') {
      start += 3;
      break;
    }
  }

  if (data[start] != '\n') {
    unsigned end = start;
    while (isalpha(data[end]) ||
           isdigit(data[end]) ||
           data[end] == '.'   ||
           data[end] == '-' ) {
      ++end;
    }
    return data.substr(start, end - start);
  }

  /* If all fails, return empty string */
  return "";
}

bool HTTPProxy::isBadUrl(const string &data) const {
    /* Check if the url contains bad words */
    const string not_allowed[] = {"norrkoping",
      "parishilton", "spongebob", "britneyspears"};
    for (int i=0; i<4; i++) {
        if (data.find(not_allowed[i]) == string::npos) {
            return true;
        }
    }
    return false;
}


bool HTTPProxy::hasBadContent(const string &data) const {
    /* Check if the content contains bad words */
    const string not_allowed[] = {"Norrköping", "Paris Hilton", "SpongeBob", "BritneySpears"};
    for (int i=0; i<4; i++) {
        if (data.find(not_allowed[i]) == string::npos) {
            return true;
        }
    }
    return false;
}

bool HTTPProxy::contentIsText(const string &msg) const {
  return msg.find("Content-Type: text/") != string::npos;
}

void HTTPProxy::removeKeepAlive(vector<char> &data) const {
  char *datadata = data.data();
  const char *open1 = "Connection: Keep-Alive";
  const char *open2 = "Connection: keep-alive";
  unsigned openlen = sizeof(open1);

  const char *closed1 = "Connection: Close";
  const char *closed2 = "Connection: close";
  unsigned closedlen = sizeof(closed1);

  for (unsigned i = 0; i < data.size(); ++i) {
    if (!strncmp(datadata + i, open1, openlen) ||
        !strncmp(datadata + i, open2, openlen)) {
      i += 12;
      memcpy(datadata + i, "Close", 5);
      data.erase(data.begin() +i +5, data.begin() +i +9);
      return;
    } else if (!strncmp(datadata + i, closed1, closedlen) ||
               !strncmp(datadata + i, closed2, closedlen)) {
      return;
    }
  }

  for (unsigned i = 0; i < data.size(); ++i) {
    if (!strncmp(datadata + i, "\r\n", 2)) {
      i += 2;
      const char *tmpdata = "Connection: Close\r\n";
      unsigned tmpdatalen = sizeof(tmpdata);
      vector<char> tmp(tmpdatalen);
      memcpy(tmp.data(), tmpdata, tmpdatalen);
      data.insert(data.begin() +i +1, tmp.begin(), tmp.end());
      return;
    }
  }
}
