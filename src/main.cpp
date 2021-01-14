#include "precomputation.hpp"
#include "uci.hpp"

int main(int, const char**) {
  UCI uci(std::cin, std::cout, std::cerr);
  return uci.mainLoop();
}
