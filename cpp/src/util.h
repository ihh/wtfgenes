#ifndef UTIL_INCLUDED
#define UTIL_INCLUDED

#include <numeric>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cassert>
#include <mutex>
#include <sys/stat.h>

/* uncomment to enable NaN checks */
#define NAN_DEBUG

/* Errors, warnings, assertions.
   Fail(...) and Require(...) are quieter versions of Abort(...) and Assert(...)
   that do not print a stack trace or throw an exception,
   but merely call exit().
   Test(...) does not exit or throw an exception,
   just prints a warning and returns false if the assertion fails.
   Desire(...) is a macro wrapper for Test(...)
   that returns false from the calling function if the test fails.
*/
void Abort(const char* error, ...);
void Assert(int assertion, const char* error, ...);
void Warn(const char* warning, ...);
void Fail(const char* error, ...);
void Require(int assertion, const char* error, ...);
bool Test(int assertion, const char* error, ...);
#define Desire(...) do { if (!Test(__VA_ARGS__)) return false; } while (0)

void CheckGsl (int gslErrorCode);

/* singular or plural? */
std::string plural (long n, const char* singular);
std::string plural (long n, const char* singular, const char* plural);

/* stringify */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/* join */
template<class Container>
std::string join (const Container& c, const char* sep = " ") {
  std::string j;
  for (const auto& s : c) {
    if (!j.empty())
      j += sep;
    j += s;
  }
  return j;
}

/* to_string_join */
template<class Container>
std::string to_string_join (const Container& c, const char* sep = " ") {
  std::ostringstream j;
  int n = 0;
  for (const auto& s : c) {
    if (n++ > 0)
      j << sep;
    j << s;
  }
  return j.str();
}

/* split */
std::vector<std::string> split (const std::string& s, const char* splitChars = " \t\n");

/* toupper */
std::string toupper (const std::string& s);

/* escaping a string
   http://stackoverflow.com/questions/2417588/escaping-a-c-string
 */
template<class OutIter>
OutIter write_quoted_escaped(std::string const& s, OutIter out) {
  *out++ = '"';
  for (std::string::const_iterator i = s.begin(), end = s.end(); i != end; ++i) {
    unsigned char c = *i;
    if (' ' <= c and c <= '~' and c != '\\' and c != '"') {
      *out++ = c;
    }
    else {
      *out++ = '\\';
      switch(c) {
      case '"':  *out++ = '"';  break;
      case '\\': *out++ = '\\'; break;
      case '\t': *out++ = 't';  break;
      case '\r': *out++ = 'r';  break;
      case '\n': *out++ = 'n';  break;
      default:
        char const* const hexdig = "0123456789ABCDEF";
        *out++ = 'x';
        *out++ = hexdig[c >> 4];
        *out++ = hexdig[c & 0xF];
      }
    }
  }
  *out++ = '"';
  return out;
}

/* keys and values */
template<typename TK, typename TV>
std::vector<TK> extract_keys(std::map<TK, TV> const& input_map) {
  std::vector<TK> retval;
  for (auto const& element : input_map) {
    retval.push_back(element.first);
  }
  return retval;
}

template<typename TK, typename TV>
std::vector<TV> extract_values(std::map<TK, TV> const& input_map) {
  std::vector<TV> retval;
  for (auto const& element : input_map) {
    retval.push_back(element.second);
  }
  return retval;
}    
#endif /* UTIL_INCLUDED */
