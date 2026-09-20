#pragma once

// A one-bit mailbox between the main-loop and render callbacks. The
// freestanding build has no C++ library; use compiler atomics on both paths.
#if defined(_MSC_VER)
#include <intrin.h>
#endif
namespace obvr {
class AtomicFlag {
public:
 void Set(bool value) {
#if defined(_MSC_VER)
  _InterlockedExchange(&m_value,value ? 1 : 0);
#else
  __atomic_store_n(&m_value,value ? 1 : 0,__ATOMIC_SEQ_CST);
#endif
 }
 bool Get() const {
#if defined(_MSC_VER)
  return _InterlockedCompareExchange(&m_value,0,0)!=0;
#else
  return __atomic_load_n(&m_value,__ATOMIC_SEQ_CST)!=0;
#endif
 }
 bool Take() {
#if defined(_MSC_VER)
  return _InterlockedExchange(&m_value,0)!=0;
#else
  return __atomic_exchange_n(&m_value,0,__ATOMIC_SEQ_CST)!=0;
#endif
 }
private:
 mutable volatile long m_value=0;
};
}
