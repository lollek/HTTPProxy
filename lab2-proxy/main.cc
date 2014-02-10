#include <stdexcept>
#include <iostream>

#include "HTTPProxy.hh"

using namespace std;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cerr << "Usage: %s port" << argv[0] << endl;
    return 1;
  }
  try {
    HTTPProxy proxy = HTTPProxy(stoi(argv[1]));
    return proxy.run();
  } catch (const invalid_argument &e) {
    cerr << "Error: port contains non-digits!\n";
    return 1;
  }
}
