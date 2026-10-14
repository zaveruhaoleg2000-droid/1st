#pragma once
#include <cstdlib>
#include <iostream>

namespace gz::test {
inline void check(bool passed,const char* expression,const char* file,int line) {
    if(!passed) {
        std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

// Test conditions and their side effects must run even with NDEBUG enabled.
#define GZ_CHECK(expression) \
    ::gz::test::check(bool(expression),#expression,__FILE__,__LINE__)
