#include <flt/util/cache_adaptors.hpp>

#include <iostream>

int main() {
  auto test_f = [](int a, int b) { return a + b; };
  using flt::util::ordered_cache_adaptor;
  auto k = ordered_cache_adaptor<decltype(test_f), int, int>(test_f);
  std::cout << "1+2 = " << k(1, 2) << std::endl;
  std::cout << "3+4 = " << k(3, 4) << std::endl;
  std::cout << "1+2 = " << k(1, 2) << std::endl;
}
