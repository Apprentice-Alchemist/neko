#include <neko.h>

#if defined(__STDC_NO_ATOMICS__) || (defined(_MSC_VER) && _MSC_VER < 1935)
#ifdef _MSC_VER
#include <intrin.h>
#define _Atomic
#ifdef _M_ARM
#error "please fix atomics for armv7"
#endif

// MSVC does not have a simple cross-platform atomic load intrinsic
inline value atomic_load(value *loc) {
#ifdef _M_ARM64
  return (value)__ldar64(value)
#else
  return *loc;
#endif
}
inline void atomic_store(value *loc, value v) {
#ifdef _M_ARM64
  __stlr64(loc, (__int64)v)
#else
  *loc = v;
#endif
}

inline value atomic_exchange(value *loc, value v) {
  return (value)_InterlockedExchangePointer((void **)loc, (void *)v);
}

inline bool atomic_compare_exchange_weak(value *loc, value *expected,
                                            value desired){
    value orig = (value)_InterlockedCompareExchangePointer((void **)loc,
                                                    (void *)desired,
                                                    (void *)*expected);
    bool ret = orig == *expected;
    *expected = orig;
    return ret;
}
#else
#define _Atomic
inline value atomic_load(value *loc) {
  value ret;
  __atomic_load(loc, &ret, __ATOMIC_SEQ_CST);
  return ret;
}
inline void atomic_store(value *loc, value v) {
  __atomic_store(loc, &v, __ATOMIC_SEQ_CST);
}

inline value atomic_exchange(value *loc, value v) {
  value ret;
  __atomic_exchange(loc, &v, &ret, __ATOMIC_SEQ_CST);
  return ret;
}
inline bool atomic_compare_exchange_weak(value *loc, value *expected,
                                           value desired) {
  return __atomic_compare_exchange(loc, expected, &desired, true,
                                   __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
#endif
#else
#include <stdatomic.h>
#endif
typedef _Atomic value atomic_value;

DEFINE_KIND(k_atomic)
#define val_atomic(loc) ((atomic_value *)val_data(loc))

static value neko_make_atomic(value init) {
  atomic_value *loc = (atomic_value *)alloc(sizeof(atomic_value));
  *loc = init;
  return alloc_abstract(k_atomic, loc);
}

static value neko_atomic_load(value loc) {
  val_check_kind(loc, k_atomic);
  return atomic_load(val_atomic(loc));
}

static value neko_atomic_store(value loc, value v) {
  val_check_kind(loc, k_atomic);
  atomic_store(val_atomic(loc), v);
  return v;
}

static value neko_atomic_exchange(value loc, value v) {
  val_check_kind(loc, k_atomic);
  return atomic_exchange(val_atomic(loc), v);
}

static value neko_atomic_compare_exchange(value loc, value expected,
                                          value desired) {
  // orig = *loc
  // if (orig == expected)
  // 	*loc = desired;
  // return orig
  val_check_kind(loc, k_atomic);
  value orig = neko_atomic_load(loc);
  while (val_compare(orig, expected) == 0) {
    if (atomic_compare_exchange_weak(val_atomic(loc), &orig, desired)) {
      break;
    } else {
      continue;
    }
  }
  return orig;
}

static value neko_atomic_fetch_update(value loc, value fun) {
  val_check_kind(loc, k_atomic);
  val_check_function(fun, 1);

  value orig = neko_atomic_load(loc);
  while (true) {
    value new = val_call1(fun, orig);
    if (atomic_compare_exchange_weak(val_atomic(loc), &orig, new)) {
      return orig;
    } else {
      continue;
    }
  }
}

DEFINE_PRIM_WITH_NAME(neko_make_atomic, make_atomic, 1)
DEFINE_PRIM_WITH_NAME(neko_atomic_load, atomic_load, 1)
DEFINE_PRIM_WITH_NAME(neko_atomic_store, atomic_store, 2)
DEFINE_PRIM_WITH_NAME(neko_atomic_exchange, atomic_exchange, 2)
DEFINE_PRIM_WITH_NAME(neko_atomic_compare_exchange, atomic_compare_exchange, 3)
DEFINE_PRIM_WITH_NAME(neko_atomic_fetch_update, atomic_fetch_update, 2)
