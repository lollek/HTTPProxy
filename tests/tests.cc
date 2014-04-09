#include <fstream>
#include <vector>
#include <string>
#include <iostream>

#include <gtest/gtest.h>

#include "../HTTPProxy.hh"

using namespace std;

vector<char> read_file(const string &filename) {
  vector<char> return_data;
  ifstream file("tests/" + filename);
  if (file) {
    file.seekg(0, ios::end);
    return_data.resize(file.tellg());
    file.seekg(0, ios::beg);
    file.read(return_data.data(), return_data.size());
    file.close();
  }
  return return_data;
}

TEST(ShortenLongGets, all) {
  /* void shortenLongGets(std::vector<char> &data) const;
   *
   * This function changes a request from
   * "GET http://www.youtube.com\r\n" to "GET /\r\n"
   * to get around some issues 
   */
  HTTPProxy proxy(8080);

  /* Standard GET */
  vector<char> expected = read_file("GET1_trimmed.txt");
  expected.resize(expected.size() +1);
  expected[expected.size()] = '\0';

  vector<char> result = read_file("GET1.txt");
  proxy.shortenLongGets(result);
  result.resize(result.size() +1);
  result[result.size()] = '\0';

  ASSERT_EQ(result.size(), expected.size());
  ASSERT_STREQ(result.data(), expected.data());

}
