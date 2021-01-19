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

template<class T>
string toString(const T& v) {
  std::ostringstream ostr;
  ostr << v;
  return ostr.str();
}

//
// Assertion
//

// Always on
#define ASSERT(EXPR) \
  if(!static_cast<bool>(EXPR)) { \
    std::fprintf(stderr, "[%s:%d] ASSERT EXPR = (%s)\n", __FILE__, __LINE__, #EXPR); \
    std::abort(); \
  }

// Disabled on Release build
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
};

//
// Fixed size queue
//
template<class T, size_t N>
struct SimpleQueue {
  array<T, N> data;
  size_t first = 0, last = 0;

  T& get() { ASSERT_HOT(first < last); return data[first++]; }
  void put(const T& x) { ASSERT_HOT(last < N); data[last++] = x; }
  bool empty() { return first == last; }
  void clear() { first = last = 0; }

  T* begin() { return &data[first]; }
  T* end() { return &data[last]; }
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
