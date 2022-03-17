#ifndef DEBUG_HELPERS_H_UEILIMND
#define DEBUG_HELPERS_H_UEILIMND
#include <iostream>
#include <iterator>

template <class T>
struct printer {
  T const& iterable;
  decltype(std::begin(iterable)) begin() const { return std::begin(iterable); }
  decltype(std::end(iterable)) end() const { return std::end(iterable); }
};
template <class T>
printer(T) -> printer<T>;

template <class CharT, class Traits, class V>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& out,
                                              printer<V> const& p) {
  auto it = std::begin(p);
  auto end = std::end(p);

  out << '{';
  if (it != end) {
    out << *it;
    ++it;
    while (it != end) {
      out << ", " << *it;
      it++;
    }
  }
  out << '}';
  return out;
}
#endif /* end of include guard: DEBUG_HELPERS_H_UEILIMND */
