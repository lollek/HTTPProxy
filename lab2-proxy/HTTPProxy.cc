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
  cout << "New connection from client ..." << flush;
  vector<char> client_data_array = client->recv(BUFSIZE);

  string client_data(client_data_array.data());

  // Check if bad GET URL
  // Remove Keep-Alive

  string target_hostname = findHostName(client_data);
  if (target_hostname.size() == 0) {
    cout << "No hostname found - return :( string was:\n" 
         << "SIZE: " << client_data.size() << " "
         << client_data << endl;
    return 1;
  }

  /* Connect to true target */
  cout << "to " << target_hostname << ":80..." << flush;
  TCPSocket target = TCPSocket(IPV4);
  if (target.connect(findHostName(client_data), 80) != 0 ) {
    return 1;
  }
  cout << " DONE" << endl;

  cout << "Sending clientdata1 -> target ..." << flush;
  /* Send first array to target */
  target.send(client_data_array);
  cout << "DONE" << endl;

  cout << "Sending clientdata2 -> target ..." << flush;
  /* Now send the rest */
  if (client_data_array.size() == BUFSIZE) {
    while ((client_data_array = client->recv(BUFSIZE)).size()) {
      target.send(client_data_array);
    }
  }
  cout << "DONE" << endl;

  /* Send data from target to client */
  vector<char> target_data_array = target.recv(BUFSIZE);
  /* Check if Content-Type; text/  */
  if (contentIsText(string(target_data_array.data()))) {
    client->send(target_data_array);
    client->send(target.recvall());

  /* Else: binary */
  } else {
    client->send(target_data_array);
    if (target_data_array.size() == BUFSIZE) {
      while ((target_data_array = target.recv(BUFSIZE)).size()) {
        client->send(target_data_array);
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

