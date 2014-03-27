#include <stdexcept>
#include <iostream>

#include "HTTPProxy.hh"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cerr << "Usage: %s port" << argv[0] << endl;
    return 1;
  }

  int port_number;
  try {
    port_number = stoi(argv[1]);
  } catch (const invalid_argument &e) {
    cerr << "Error: port contains non-digits!\n";
    return 1;
  }
  HTTPProxy proxy = HTTPProxy(port_number);
  return proxy.run();
}
