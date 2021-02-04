#pragma once

#include <bits/stdc++.h>

//
// STL namespace
//
using std::vector, std::string, std::array, std::pair;

//
// STL I/O
//
namespace std {
template<class ...Ts>        istream& operator>>(istream& i,       tuple<Ts...>& x) { apply([&](auto&&... ys){ ((i >> ys), ...); }, x); return i; }
template<class ...Ts>        ostream& operator<<(ostream& o, const tuple<Ts...>& x) { o << "("; auto s = ""; apply([&](auto&&... y){ ((o << s << y, s = ", "), ...); }, x); return o << ")"; }
template<class T1, class T2> istream& operator>>(istream& i,       pair<T1, T2>& x) { return i >> tie(x.first, x.second); }
template<class T1, class T2> ostream& operator<<(ostream& o, const pair<T1, T2>& x) { return o << tie(x.first, x.second); }
template<class T, class = decltype(begin(declval<T>())), class = enable_if_t<!is_same<T, string>::value>>
istream& operator>>(istream& i,       T& x) { for (auto& y : x) { i >> y; } return i; }
template<class T, class = decltype(begin(declval<T>())), class = enable_if_t<!is_same<T, string>::value>>
ostream& operator<<(ostream& o, const T& x) { o << "{"; auto s = ""; for (auto& y : x) { o << s << y; s = ", "; } return o << "}"; }
}

#define dbg(...) do { std::cerr << #__VA_ARGS__ ": " << std::make_tuple(__VA_ARGS__) << std::endl; } while (0)
#define dbg2(X) do { std::cerr << #X ":\n"; for (auto& __Y : X) { std::cerr << __Y << std::endl; } } while (0)

//
// Python-like print
//

template<class ...Ts>
void _print(std::ostream& ostr, const string& sep, const string& end, Ts... args) {
  string s = "";
  ((ostr << s << args, s = sep), ...);
  ostr << end;
}

template<class ...Ts>
void print(Ts... args) {
  _print(std::cout, " ", "\n", args...);
}

template<class ...Ts>
string toString(Ts... args) {
  std::ostringstream ostr;
  _print(ostr, " ", "", args...);
  return ostr.str();
}

//
// Assertion
//

// Assertion always on
#define ASSERT(EXPR) \
  if(!static_cast<bool>(EXPR)) { \
    std::fprintf(stderr, "[%s:%d] ASSERT EXPR = (%s)\n", __FILE__, __LINE__, #EXPR); \
    std::abort(); \
  }

// Assertion disabled on "Release" build
#ifdef NDEBUG
  #define ASSERT_HOT(EXPR)
#else
  #define ASSERT_HOT(EXPR) ASSERT(EXPR)
#endif

//
// std::array utility
//
template<class T, size_t N1, size_t N2>
using array2 = array<array<T, N2>, N1>;

template<class T, size_t N1, size_t N2, size_t N3>
using array3 = array<array<array<T, N3>, N2>, N1>;

template<class T, size_t N1, size_t N2, size_t N3, size_t N4>
using array4 = array<array<array<array<T, N4>, N3>, N2>, N1>;

template<class A, class T>
void fillArray(A& a, T v) { std::fill_n(reinterpret_cast<T*>(&a[0]), sizeof(A) / sizeof(T), v); }

//
// PCG PRNG (cf. https://github.com/imneme/pcg-c-basic)
//
struct Rng {
  uint64_t state, inc;
  Rng() : Rng(0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL) {}
  Rng(uint64_t init_state, uint64_t init_seq) { seed(init_state, init_seq); }

  void seed(uint64_t init_state, uint64_t init_seq) {
    state = 0u;
    inc = (init_seq << 1u) | 1u;
    next();
    state += init_state;
    next();
  }

  uint32_t next() {
    uint64_t oldstate = state;
    state = oldstate * 6364136223846793005ULL + inc;
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
  }

  uint64_t next64() {
    auto x = next(), y = next();
    return (uint64_t(x) << 32) | y;
  }

  float uniform() {
    return next() / float(1ULL << 32);
  }
};

//
// (Non-seekable) istream from char array (Cf. https://stackoverflow.com/questions/7781898/get-an-istream-from-a-char)
//
struct CharStreambuf : std::streambuf {
  CharStreambuf(char* ptr, int size) { setg(ptr, ptr, ptr + size); }
};

//
// Fixed size queue
//
template<class T, size_t N>
struct SimpleQueue {
  array<T, N> data = {};
  size_t first = 0, last = 0;

  T& get() { ASSERT_HOT(first < last); return data[first++]; }
  void put(const T& x) { ASSERT_HOT(last < N); data[last++] = x; }
  void clear() { first = last = 0; }
  bool empty() const { return first == last; }
  size_t size() const { return last - first; }

  T* begin() { return &data[first]; }
  T* end() { return &data[last]; }
  const T* begin() const { return &data[first]; }
  const T* end() const { return &data[last]; }

  T& operator[](int i) { return data[first + i]; }
  const T& operator[](int i) const { return data[first + i]; }
};

//
// Thread-safe Queue (API is similar to https://docs.python.org/3/library/queue.html)
//
template<class T>
struct Queue {
  std::deque<T> queue;
  std::mutex mutex;
  std::condition_variable cv_not_empty;

  void put(const T& data) {
    std::unique_lock<std::mutex> lock(mutex);
    queue.push_back(data);
    lock.unlock();
    cv_not_empty.notify_one();
  }

  T get() {
    std::unique_lock<std::mutex> lock(mutex);
    cv_not_empty.wait(lock, [&](){ return !queue.empty(); });
    auto data = queue.front(); queue.pop_front();
    return data;
  }
};

//
// Command line parser
//
struct Cli {
  const int argc;
  const char** argv;
  vector<string> flags = {};

  string help() {
    string res = "Usage: <program>";
    for (auto flag : flags) { res += " " + flag; }
    return res;
  }

  template<typename T>
  T parse(const char* s) {
    std::istringstream stream{s};
    T result;
    stream >> result;
    return result;
  }

  template<typename T>
  std::optional<T> getArg(const string& flag) {
    flags.push_back(flag);
    for (auto i = 1; i < argc; i++) {
      if (flag == argv[i] && i + 1 < argc) {
        return parse<T>(argv[i + 1]);
      }
    }
    return {};
  }
};

//
// Iterate between two pointers by `for (auto x : PointerIterator{first, last}) ...`
//

template<class T>
struct PointerIterator {
  T* first = nullptr;
  T* last = nullptr;
  T* begin() { return first; }
  T* end() { return last; }
  const T* begin() const { return first; }
  const T* end() const { return last; }
};

//
// max_element by key function
//

template<class Iterable, class F>
decltype(auto) maxElementByKey(Iterable xs, F f) {
  ASSERT(xs.begin() != xs.end());
  auto it = xs.begin();
  auto max_x = *it;
  auto max_y = f(max_x);
  for (it++; it != xs.end(); it++) {
    auto x = *it;
    auto y = f(x);
    if (max_y < y) { max_x = x; max_y = y; }
  }
  return max_x;
};
