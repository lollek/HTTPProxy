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
  vector<char> client_data = client->recvall();
  if (hasBlockedContents(client_data)) {
    return redirectToError1(client);
  }

  string target_hostname = findHostName(client_data);
  if (target_hostname.size() == 0) {
    cerr << "Received a request without hostname!\n";
    return 1;
  }

  removeKeepAlive(client_data);
  shortenLongGets(client_data);

  /* Connect to true target */
  TCPSocket target = TCPSocket(IPV4);
  if (target.connect(target_hostname, 80) != 0 ) {
    cout << "Failed to connect to " << target_hostname << endl;
    return 1;
  }

  /* Send data-chunk to target */
  if (target.send(client_data) != 0) {
    cerr << "Error sending data1 to " << target_hostname << endl;
    target.close();
    return 1;
  }

  /* Send data from target to client */
  vector<char> target_data = target.recv(BUFSIZE);

  /* If Content-Type; text/
   * We download all data, look it through, and then pass it on */
  if (contentIsText(target_data)) {
    vector<char> all_data = target_data;
    while ((target_data = target.recv(BUFSIZE)).size()) {
      all_data.insert(all_data.end(), target_data.begin(), target_data.end());
    }
    if (hasBlockedContents(all_data)) {
      target.close();
      return redirectToError2(client);
    }
    if (client->send(all_data) != 0) {
      target.close();
      cerr << "Error sending data(text) from " << target_hostname << endl;
      return 1;
    }
  }

  /* Otherwise, we just pass it on */
  else {
    if (client->send(target_data) != 0) {
      cerr << "Error sending data(bin) from " << target_hostname << endl;
      target.close();
      return 1;
    }
    if (target_data.size() == BUFSIZE) {
      while ((target_data = target.recv(BUFSIZE)).size()) {
        if (client->send(target_data) != 0) {
          cerr << "Error sending data(bin) from " << target_hostname << endl;
          target.close();
          return 1;
        }
      }
    }
  }

  /* Exit */
  cout << "Received connection to " << target_hostname << endl;
  target.close();
  return 0;
}

int HTTPProxy::redirectToError1(TCPSocket *client) const {
  cout << "Redirected client due to bad request" << endl;
  return redirectToURL(client, "HTTP/1.1 301 Moved Permanently\r\nLocation: http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error1.html\r\nConnection: close\r\n\r\n");
}

int HTTPProxy::redirectToError2(TCPSocket *client) const {
  cout << "Redirected client due to bad content" << endl;
  return redirectToURL(client, "HTTP/1.1 301 Moved Permanently\r\nLocation: http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error2.html\r\nConnection: close\r\n\r\n");
}

int HTTPProxy::redirectToURL(TCPSocket *client, const char *url) const {
  if (url == NULL) {
    return 1;
  }
  vector<char> redir_array(strlen(url));
  memcpy(redir_array.data(), url, strlen(url));

  client->send(redir_array);
  return 0;
}

string HTTPProxy::findHostName(const vector<char> &data) const {
  const char *data_ptr = data.data();

  /* Check for Host: www.hostname.dom\r\n */
  for (unsigned i = 0; i < data.size(); ++i) {
    if (!strncmp(data_ptr + i, "\r\nHost: ", strlen("\r\nHost: "))) {
      i += strlen("\r\nHost: ");
      unsigned end = i;
      while (strncmp(data_ptr + ++end, "\r\n", 2))
        ;
      string hostname;
      hostname.insert(0, data_ptr + i, end - i);
      return hostname;
    }

    if (strlen("\r\n\r\n") > data.size() - i ||
        !strncmp(data_ptr + i, "\r\n\r\n", 4)) {
      break;
    }
  }

  /* Otherwise, check for http://www.hostname.dom */
  for (unsigned i = 0; i < data.size(); ++i) {
    if (!strncmp(data_ptr + i, "://", strlen("://"))) {
      i += strlen("://");
      unsigned end = i;
      while (*(data_ptr + end) != ' ' &&
             *(data_ptr + end) != '\r' &&
             *(data_ptr + end) != '/') {
        ++end;
      }
      string hostname;
      hostname.insert(0, data_ptr + i, end - i);
      return hostname;
    }

    if (!strncmp(data_ptr + i, "\r\n", strlen("\r\n"))) {
      break;
    }
  }

  /* If all fails, return empty string */
  return "";
}

bool HTTPProxy::hasBlockedContents(const vector<char> &data) const {
  /* Check if the url contains bad words */

  const char *data_ptr = data.data();
  const vector<const char *> wordlist = {
    "Norrkoping", "ParisHilton", "SpongeBob", "BritneySpears",
    "Norrk%c3%b6ping", "Paris%20Hilton", "Sponge%20Bob", "Britney%20Spears",
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


bool HTTPProxy::contentIsText(const vector<char> &data) const {
  const char *data_ptr = data.data();
  const vector<const char *> keywords = {
  "\r\nContent-Type: text/", "\r\nAccept: text/"
  };

  for (unsigned i = 0; i < data.size(); ++i) {
    for (unsigned word = 0; word < keywords.size(); ++word) {
      if (!strncmp(data_ptr + i, keywords[word], strlen(keywords[word]))) {
        return true;
      }
    }
    if (!strncmp(data_ptr + i, "\r\n\r\n", strlen("\r\n\r\n"))) {
      break;
    }
  }
  return false;
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
    else if (!strncmp(data_ptr + i, "\r\n\r\n", strlen("\r\n\r\n"))) {
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
