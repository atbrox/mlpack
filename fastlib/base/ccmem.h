// Copyright 2007 Georgia Institute of Technology. All rights reserved.
// ABSOLUTELY NOT FOR DISTRIBUTION
/**
 * @file ccmem.h
 *
 * Low-level (repeat: scary) memory management routines used by
 * core datastructures.
 *
 * If you need to allocate single objects, use new and delete.  If you need
 * an array, just use ArrayList -- it will even do bounds checking for you
 * in debug mode, which is very handy for machine learning problems.
 *
 * If you really need to manage your own memory, use these instead of
 * malloc and free, because these will perform "memory poising" in
 * debug mode.
 */

#ifndef BASE_CCMEM_H
#define BASE_CCMEM_H

#include "base/basic_types.h"
#include "base/scale.h"
#include "debug.h"
#include "cc.h"

#include <cstdlib>
#include <cstring>
#include <new>

/**
 * Wrappers for low-level memory access.
 *
 * This contains things such as:
 *
 * - debugging-helpful memory allocation wrappers
 *
 * - syntax-friendly access to C++ constructors for variables and arrays
 *
 * - swapping memory regions
 *
 */
namespace mem {
  template<size_t t_elems, typename T = char>
  struct Chunk {
    T data[t_elems];
  };
  
  /**
   * In debug mode, sets the entire chunk of memory to a BIG_BAD_NUMBER.
   * @param array chunk of memory
   * @param bytes number of *bytes*
   */
  template<typename T>
  void DebugPoisonBytes(T* array, size_t bytes) {
#ifdef DEBUG
    uint32 *s = reinterpret_cast<uint32*>(array);
    size_t ints = bytes / sizeof(uint32);
    
    for (size_t i = 0; i < ints; i++) {
      s[i] = uint32(BIG_BAD_NUMBER);
    }
#endif
  }
  
  /**
   * In debug mode, sets the entire chunk of memory to a BIG_BAD_NUMBER.
   * @param array chunk of memory
   * @param elems number of *elements*
   */
  template<typename T>
  void DebugPoison(T* array, size_t elems) {
    DEBUG_ONLY(DebugPoisonBytes(array, elems * sizeof(T)));
  }
  
  /**
   * Allocates the specified number of bytes.
   * @param bytes number of bytes
   * @return a pointer that must be freed with mem::Free
   */
  template<typename T>
  inline T * AllocBytes(size_t bytes) {
     T *p = reinterpret_cast<T*>(::malloc(bytes));
     DEBUG_ONLY(DebugPoisonBytes(p, bytes));
     return p;
  }
  /**
   * Allocates the specified number of elements.
   * @param elems number of *elements*
   * @return a pointer that must be freed with mem::Free
   */
  template<typename T>
  inline T * Alloc(size_t elems = 1) {
#ifdef FL_SCALE_NORMAL
     // This check is only enabled if the program is run on 32-bit
     // scales.
     DEBUG_ASSERT(elems < BIG_BAD_NUMBER);
#endif
     return AllocBytes<T>(elems * sizeof(T));
  }
  /**
   * Allocates the specified number of elements, zeroing them out.
   * @param elems number of *elements*
   * @return a pointer that must be freed with mem::Free
   */
  template<typename T>
  inline T * AllocZeroed(size_t elems = 1) {
     return reinterpret_cast<T*>(::calloc(elems * sizeof(T), 1));
  }

  /**
   * Allocates the specified number of elements, constructing each one.
   * @param elems number of *elements*
   * @return a pointer that must be freed with mem::Free
   */
  template<typename T>
  inline T * AllocConstruct(size_t elems) {
    T *p = Alloc<T>(elems);
    for (size_t i = 0; i < elems; i++) {
      new(p[i])T();
    }
  }
  /**
   * Allocates the specified number of elements, initializing all of them
   * to the specified value.
   * @param elems number of *elements*
   * @return a pointer that must be freed with mem::Free
   */
  template<typename T>
  inline T * AllocConstruct(const T& initial, size_t elems) {
    T *p = Alloc<T>(elems);
    for (size_t i = 0; i < elems; i++) {
      new(p[i])T(initial);
    }
  }
  
  /**
   * Resizes a chunk of allocated memory.
   * @param bytes the desired number of *bytes*
   * @param ptr a pointer allocated with mem::Alloc
   * @return a new pointer
   */
  template<typename T>
  inline T * ReallocBytes(T* ptr, size_t bytes) {
     T *new_ptr = reinterpret_cast<T*>(realloc(ptr, bytes));
     return new_ptr;
  }
  /**
   * Resizes a chunk of allocated memory.
   * @param elems the desired number of *elements*
   * @param ptr a pointer allocated with mem::Alloc
   * @return a new pointer
   */
  template<typename T>
  inline T * Resize(T* ptr, size_t elems = 1) {
     return ReallocBytes<T>(ptr, elems * sizeof(T));
  }
  
  /**
   * Copies bit-by-bit from one location to another.
   * @param dest the destination to copy to
   * @param src the source data
   * @param bytes the number of bytes to copy
   */
  template<typename TDest, typename TSrc>
  inline TDest * CopyBytes(TDest* dest, const TSrc* src, size_t bytes) {
     memcpy(dest, src, bytes); return dest;
  }
  /**
   * Copies bit-by-bit from one location to another (memcpy).
   * @param dest the destination
   * @param src the source
   * @param elems the desired number of *elements*
   * @return the destination pointer
   */
  template<typename T>
  inline T * Copy(T* dest, const T* src, size_t elems) {
     return CopyBytes(dest, src, elems * sizeof(T));
  }
  
  template<typename ChunkType, size_t elems, typename T>
  inline void ChunkCopy(T* dest, const T* src) {
    *reinterpret_cast<Chunk<sizeof(T), ChunkType>*>(dest)
        = *reinterpret_cast<const Chunk<sizeof(T), ChunkType>*>(src);
  }
  
  template<bool mod_2, bool mod_4, bool mod_8, size_t bytes, typename T>
  struct CopyHelper {
    static void DoCopy(T* dest, const T* src) {
      ChunkCopy<char, bytes>(dest, src);
    }
  };

  template<size_t bytes, typename T>
  struct CopyHelper<0, 1, 1, bytes, T> {
    static void DoCopy(T* dest, const T* src) {
      ChunkCopy<uint16, bytes/2>(dest, src);
    }
  };

  template<size_t bytes, typename T>
  struct CopyHelper<0, 0, 1, bytes, T> { 
    static void DoCopy(T* dest, const T* src) {
      ChunkCopy<uint32, bytes/4>(dest, src);
    }
  };

  template<size_t bytes, typename T>
  struct CopyHelper<0, 0, 0, bytes, T> {
    static void DoCopy(T* dest, const T* src) {
      ChunkCopy<uint32, bytes/8>(dest, src);
    }
  };
  
  /**
   * Copies bit-by-bit from one location to another (memcpy).
   *
   * Attempts to be smart when it can.
   *
   * @param dest 
   */
  template<typename T>
  inline T * Copy(T* dest, const T* src) {
     /*CopyHelper<strideof(T) % 2 != 0,
                strideof(T) % 4 != 0,
                strideof(T) % 8 != 0,
                sizeof(T),
                T>::DoCopy(dest, src);*/
     CopyBytes(dest, src, sizeof(T));
     return dest;
  }
  
  template<typename T>
  inline T * DupBytes(const T* src, size_t size) {
     T* p = AllocBytes<T>(size); return CopyBytes(p, src, size);
  }
  template<typename T>
  inline T * Dup(const T* src, size_t elems = 1) {
     return DupBytes(src, elems * sizeof(T));
  }
  
  template<typename T>
  inline void Zero(T* start, size_t count = 1) {
     ZeroBytes(start, count * sizeof(T));
  }
  template<typename T>
  inline void ZeroBytes(T* start, size_t bytes) {
     ::memset(start, 0, bytes);
  }
  
  template<typename T>
  inline void Free(T* ptr) {
     ::free(ptr);
  }
  
  /**
   * Calls the default constructor on an object.
   *
   * This template is "overloaded" so that for primitive types like int,
   * this will not actually leave it initialized rather than setting it to
   * zero.
   */
  template<typename T>
  inline T* Construct(T* p) {
     new(p)T(); return p;
  }
  template<typename T>
  inline T* ConstructAll(T* m, size_t elems) {
     for (size_t i = 0; i < elems; i++) new(m+i)T(); return m;
  }
#define BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(T) \
  template<> inline T* ConstructAll<T>(T* m, size_t elems) { return m; }
  
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(char)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(short)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(int)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(long)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(long long)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(unsigned char)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(unsigned short)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(unsigned int)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(unsigned long)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(unsigned long long)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(float)
  BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR(double)
  
#undef BASE_CCMEM__AVOID_DEFAULT_CONSTRUCTOR
  
  template<typename T, typename U>
  inline T* Construct(T* p, U u) {
     new(p)T(u); return p;
  }
  template<typename T, typename U>
  inline T* ConstructAll(T* m, U u, size_t elems) {
     for (size_t i = 0; i < elems; i++) new(m+i)T(u); return m;
  }
  
  template<typename T, typename U, typename V>
  inline T* Construct(T* p, U u, V v) {
     new(p)T(u, v); return p;
  }
  template<typename T, typename U, typename V>
  inline T* ConstructAll(T* m, U u, V v, size_t elems) {
     for (size_t i = 0; i < elems; i++) new(m+i)T(u, v); return m;
  }

  template<typename T, typename U, typename V, typename W>
  inline T* Construct(T* p, U u, V v, W w) {
     new(p)T(u, v, w); return p;
  }
  template<typename T, typename U, typename V, typename W>
  inline T* ConstructAll(T* m, U u, V v, W w, size_t elems) {
     for (size_t i = 0; i < elems; i++) new(m+i)T(u, v, w); return m;
  }

  template<typename T>
  void Destruct(T* m) {
     m->~T();
     DEBUG_ONLY(DebugPoison(m, 1));
  }
  template<typename T>
  void DestructAll(T* m, size_t elems) {
     for (size_t i = 0; i < elems; i++) m[i].~T();
     DEBUG_ONLY(DebugPoison(m, elems));
  }

  template<typename T>
  inline T* CopyConstruct(T* dest, const T* src, size_t elems = 1) {
     for (size_t i = 0; i < elems; i++) new(dest+i)T(src[i]); return dest;
  }
  template<>
  inline char* CopyConstruct<char>(char* dest, const char* src, size_t elems) {
     ::memcpy(dest, src, elems); return dest;
  }
  template<typename T>
  inline T* DupConstruct(const T* src, size_t elems = 1) {
     return CopyConstruct(Alloc<T>(elems), src, elems);
  }

  inline void SwapBytes__Chars(long *a_lp_in, long *b_lp_in, size_t remaining) {
    char *a_cp = reinterpret_cast<char*>(a_lp_in);
    char *b_cp = reinterpret_cast<char*>(b_lp_in);
    
    while (remaining) {
      char ta = *a_cp;
      char tb = *b_cp;
      remaining--;
      *b_cp = ta;
      b_cp++;
      *a_cp = tb;
      a_cp++;
    }
  }

  template<typename T>
  void SwapBytes(T* a, T* b, size_t bytes) {
    long *a_lp = reinterpret_cast<long*>(a);
    long *b_lp = reinterpret_cast<long*>(b);
    ssize_t remaining = bytes;

    //DEBUG_MSG(3.0,"Swapping %d bytes, %d left", int(elems), int(remaining));
    
    // TODO: Not as good as an MMX memcpy, but still good...
    // TODO: replace 'remaining' decrement with end pointer
    while (likely((remaining -= sizeof(long)) >= 0)) {
      long ta = *a_lp;
      long tb = *b_lp;
      *b_lp = ta;
      b_lp++;
      *a_lp = tb;
      a_lp++;
      
    }
    
    remaining += sizeof(long);
    
    if (unlikely(remaining != 0)) {
      SwapBytes__Chars(a_lp, b_lp, remaining);
    }
  }

  template<typename T>
  inline void Swap(T* a, T* b, size_t elems = 1) {
    SwapBytes(a, b, elems * sizeof(T));
  }
  
  /**
   * Adds a byte-by-byte difference to a pointer.
   *
   * This is different from pointer addition because this requires an
   * intermediate cast to character in order to get per-byte addition.
   *
   * @param x the pointer offset
   * @param difference_in_bytes the number of bytes to add
   * @return the sum
   */
  template<typename T>
  inline T* PointerAdd(T* x, ptrdiff_t difference_in_bytes) {
    return
        reinterpret_cast<T*>(
            const_cast<char*>(
                reinterpret_cast<const char*>(x)
                + difference_in_bytes));
  }

  /**
   * Finds the byte-by-byte distance between two pointers, lhs - rhs.
   *
   * This is different from pointer subtraction because this requires an
   * intermediate cast to character in order to get per-byte differences.
   *
   * @param lhs the "positive" pointer
   * @param rhs the "negative" pointer
   * @return the difference, (char*)rhs - (char*)lhs
   */
  template<typename A, typename B>
  inline ptrdiff_t PointerDiff(const A* lhs, const B* rhs) {
    return reinterpret_cast<const char*>(lhs) - reinterpret_cast<const char*>(rhs);
  }

  /**
   * Finds the inter-valued absolute address of a pointer.
   *
   * @param pointer the pointer to get the absolute address of
   * @return the pointer, but in integer form
   */
  template<typename T>
  inline ptrdiff_t PointerAbsoluteAddress(const T* pointer) {
    return reinterpret_cast<ptrdiff_t>(pointer);
  }
};


#endif
