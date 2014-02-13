#include <string.h>
#include <strings.h>
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
    if (!fork()) {
      handleRequest(client);
      client->close();
      delete client;
      exit(0);
    }
    client->close();
  }

  /* Exit */
  serv.close();
  return 0;
}

int HTTPProxy::handleRequest(TCPSocket *client) const {

  const unsigned BUFSIZE = 1024;

  /* Receive data from client */
  vector<char> client_data_array = client->recv(BUFSIZE);
  string client_data(client_data_array.data());
  if (isBadURL(client_data_array)) {
    return redirectToError1(client);
  }

  removeKeepAlive(client_data_array);
  shortenLongGets(client_data_array);

  string target_hostname = findHostName(client_data);
  if (target_hostname.size() == 0) {
    cerr << "No hostname found - Data(" << client_data.size() << ")"
         << client_data << '\n';
    return 1;
  }


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
    vector<char> full_contents = target_data_array;
    while ((target_data_array = target.recv(BUFSIZE)).size()) {
      full_contents.insert(full_contents.end(),
                           target_data_array.begin(),
                           target_data_array.end());
    }
    if (client->send(full_contents) != 0) {
      target.close();
      cerr << "Error sending data(text) from " << target_hostname << endl;
      return 1;
    }
  }

  /* Else: binary */
  else {
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

  cout << "Received connection to " << target_hostname 
       << "\nData was " << string(client_data_array.data()) << endl;
  target.close();
  return 0;
}

int HTTPProxy::redirectToError1(TCPSocket *client) const {
  cout << "Redirected client due to bad request" << endl;

  //TODO: Make socket->send take char * as well

  const char *redir_ptr = "HTTP/1.1 302 Found\r\nLocation: http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error1.html\r\nConnection: close\r\n\r\n";
  vector<char> redir_array(strlen(redir_ptr));
  memcpy(redir_array.data(), redir_ptr, strlen(redir_ptr));

  client->send(redir_array);
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

bool HTTPProxy::isBadURL(const vector<char> &data) const {
  /* Check if the url contains bad words */

  const char *data_ptr = data.data();
  const vector<const char *> wordlist = {
    "Norrkoping", "ParisHilton", "SpongeBob", "BritneySpears",
    "Paris%20Hilton", "Sponge%20Bob", "Britney%20Spears",
    "Paris_Hilton", "Sponge_Bob", "Britney_Spears",
    "Paris+Hilton", "Sponge+Bob", "Britney+Spears"
  };
  for (unsigned i = 0; i < data.size(); ++i) {
    for (unsigned word = 0; word < wordlist.size(); ++word) {
      if (strlen(wordlist[word]) <= data.size() -i &&
          !strncasecmp(data_ptr + i, wordlist[word], strlen(wordlist[word]))) {
        return true;
      }
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
  char *data_ptr = data.data();

  const char *open = "Connection: Keep-Alive";
  const char *closed = "Connection: Close";

  for (unsigned i = 0; i < data.size(); ++i) {

    /* If Connection: Keep-Alive is found */
    if (strlen(open) <= data.size() - i &&
        !strncasecmp(data_ptr + i, open, strlen(open))) {
      i += 12;
      memcpy(data_ptr + i, "close", 5);
      memcpy(data_ptr + i + 5, data_ptr + i + 11, data.size() - i - 5);
      data.resize(data.size() - 5);
      return;
    }

    /* If Connection: close is found */
    else if (strlen(closed) <= data.size() - i &&
             !strncasecmp(data_ptr + i, closed, strlen(closed))) {
      return;
    }

    /* Else if \r\n\r\n, we have reached the end of the header
     * In that case we manually add it */
    else if (!strncmp(data_ptr + i, "\r\n\r\n", 4)) {
      i += 2;
      const char *raw_data_to_add = "Connection: Close\r\n";
      vector<char> data_to_add(strlen(raw_data_to_add));
      memcpy(data_to_add.data(), raw_data_to_add, strlen(raw_data_to_add));
      data.insert(data.begin() +i +1, data_to_add.begin(), data_to_add.end());
      return;
    }
  }

  cerr << "EOL at removeKeepAlive reached! Data was:\n" 
       << string(data_ptr) << endl;
  return;

}

void HTTPProxy::shortenLongGets(vector<char> &msg) const {
  /* This function changes a request from
   * "GET http://www.youtube.com\r\n" to "GET /\r\n"
   * to get around some issues */
  char *msg_ptr = msg.data();

  for (unsigned i = 0; i < msg.size(); ++i) {
    /* Look for first space */
    if (msg[i] == ' ') {
      if (msg[++i] == '/') {
        return;
      }
      unsigned end = i;
      while (msg[++end] != '/') ;
      while (msg[++end] != '/') ;
      while (msg[++end] != '/') ;
      memcpy(msg_ptr +i, msg_ptr +end, msg.size() - end);
      msg.resize(msg.size() - (end - i));
      return;

    /* Otherwise if it's letters, continue */
    } else if (isalpha(msg[i])) {
      continue;

    /* There shouldn't be anything else, so in that case we just return */
    } else {
      cerr << "Err: shortenLongGets received a \"" << msg[i]
           << "\"!\n";
      return;
    }
  }
  cerr << "Err: shortenLongGets ran out of data before the GET was over!\n";
}
