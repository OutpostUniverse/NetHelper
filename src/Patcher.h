

#ifndef PATCHER_H
#define PATCHER_H

// Uncomment this line if using MinHook; comment otherwise:
//#define PATCHER_MINHOOK

#include <windows.h>
#include <memory>
#include <vector>
#include <type_traits>

namespace Patcher {

// Forward declarations
class patch;
namespace Util { template <class T> inline T* _MakeDummy(); }

// Recommended to use one of the Patch factory functions to instantiate
// Use Unpatch to handle deletion of patches created by Patch functions

// General arbitrary memory patch
std::shared_ptr<patch> Patch(void *address, size_t patchSize, const void *newBytes,
                             const void *expectedBytes = nullptr,
                             bool enable = true);
template <class T>
std::shared_ptr<patch> Patch(void *address, T newValue, T expectedValue,
                             bool enable = true) {
  return Patch(address, sizeof(T), &newValue, &expectedValue, enable);
}
template <class T>
std::shared_ptr<patch> Patch(void *address, T newValue, bool enable = true) {
  return Patch(address, sizeof(T), &newValue, nullptr, enable);
}

// Inserts a jump instruction. Can use MinHook.
std::shared_ptr<patch> PatchFunction(void *address, const void *newFunction,
                                     bool enable = true);
// Inserts/rewrites a call instruction
std::shared_ptr<patch> PatchFunctionCall(void *address, const void *newFunction,
                                         bool enable = true);

// Replaces virtual function table entry by function address
std::shared_ptr<patch> PatchFunctionVirtual(void *vftableAddress,
                                            const void *oldFunction,
                                            const void *newFunction,
                                            bool enable = true);
template <class T>
std::shared_ptr<patch> PatchFunctionVirtual(T &obj, const void *oldFunction,
                                            const void *newFunction,
                                            bool enable = true) {
  return PatchFunctionVirtual(*reinterpret_cast<void**>(&obj), oldFunction,
                              newFunction, enable);
}
template <class T>
std::shared_ptr<patch> PatchFunctionVirtual(const void *oldFunction,
                                            const void *newFunction,
                                            bool enable = true) {
  std::unique_ptr<T> obj(Util::_MakeDummy<T>()); // Dummy for getting vftable
  return obj ?
    PatchFunctionVirtual(*reinterpret_cast<void**>(obj.get()), oldFunction,
                         newFunction, enable) : nullptr;
}

// Replaces virtual function table entry by index
std::shared_ptr<patch> PatchFunctionVirtual(void *vftableAddress,
                                            int vftableEntryIndex,
                                            const void *newFunction,
                                            bool enable = true);
template <class T>
std::shared_ptr<patch> PatchFunctionVirtual(T &obj, int vftableEntryIndex,
                                            const void *newFunction,
                                            bool enable = true) {
  return PatchFunctionVirtual(*reinterpret_cast<void**>(&obj),
                              vftableEntryIndex, newFunction, enable);
}
template <class T>
std::shared_ptr<patch> PatchFunctionVirtual(int vftableEntryIndex,
                                            const void *newFunction,
                                            bool enable = true) {
  std::unique_ptr<T> obj(Util::_MakeDummy<T>()); // Dummy for getting vftable
  return obj ?
    PatchFunctionVirtual(*reinterpret_cast<void**>(obj.get()), vftableEntryIndex,
                         newFunction, enable) : nullptr;
}

// Patches all references to a global variable/object in base relocation table
bool PatchGlobalReferences(const void *oldGlobalAddress,
                           const void *newGlobalAddress,
                           std::vector<std::shared_ptr<patch>> *out = nullptr,
                           bool enable = true,
                           HMODULE module =
                             reinterpret_cast<HMODULE>(-1));

// Helper function to delete patches created by factory functions
bool Unpatch(std::shared_ptr<patch> &which, bool doDelete = true,
             bool force = false);

// Enables all unapplied patches and optionally reapplies enabled patches
bool PatchAll(bool force = false);
// Disables and optionally deletes all patches
bool UnpatchAll(bool doDelete = true, bool force = false);


// Patch abstract class
class patch {
public:
  virtual ~patch() {}

  virtual bool Enable(bool force = false) = 0;
  virtual bool Disable(bool force = false) = 0;

  virtual const void* GetOriginal() = 0;

  bool GetEnabled() { return enabled; }
  bool GetValid() { return !invalid; }

protected:
  patch() { enabled = false; module = reinterpret_cast<HMODULE>(-1); }

  bool VerifyModule();
  void GetModuleState(HMODULE &moduleOut, size_t &hashOut);

  bool enabled,
       invalid;

  void *address;
  HMODULE module;
  size_t moduleHash;
};

// Memory patch class
class MemPatch : public patch {
public:
  MemPatch(void *_address, size_t patchSize, const void *newBytes,
           const void *expectedBytes, bool enable = true);
  virtual ~MemPatch();

  virtual bool Enable(bool force = false);
  virtual bool Disable(bool force = false);

  // Returns a pointer to the original bytes
  virtual const void* GetOriginal();

private:
  std::unique_ptr<BYTE[]> oldBytesBuffer,
                          newBytesBuffer;
  size_t size;
};

#ifdef PATCHER_MINHOOK
// MinHook function hook patch class
class MHPatch : public patch {
public:
  MHPatch(void *function, const void *_newFunction, bool enable = true);
  virtual ~MHPatch();

  virtual bool Enable(bool force = false);
  virtual bool Disable(bool unused = false);

  // Returns a pointer to a MinHook function trampoline
  virtual const void* GetOriginal();

private:
  const void *newFunction,
             *trampoline;
};
#endif


// Helper functions

// Fixes up a pointer to correct for module relocation
void* FixPtr(const void *pointer, HMODULE module = reinterpret_cast<HMODULE>(-1));
inline void* FixPtr(uintptr_t address,
                    HMODULE module = reinterpret_cast<HMODULE>(-1)) {
  return FixPtr(reinterpret_cast<const void*>(address), module);
}

// Cast pointer to member function to void*. Used by _GetPointer() macro in Clang.
// NOTE: For virtual PMFs, uses Itanium ABI (not MSVC), and object instance passed
// or class is default, copy, or move constructible. Class cannot multiply inherit.
template <class T, class U>
typename std::enable_if<std::is_member_function_pointer<U T::*>::value>::type*
  PMFCast(U T::*pmf, const T *self = nullptr) {
  union {
    U T::*in;
    void *out;
    uintptr_t vftOffset;
  } forcedCast;

  forcedCast.in = pmf;

  if (forcedCast.vftOffset & 1) {
    // Virtual (per Itanium ABI specification)
    // Requires an object instance to get the vftable pointer
    std::unique_ptr<T> dummy;
    if (!self) {
      dummy.reset(Util::_MakeDummy<T>());
      if (!(self = dummy.get())) {
        return nullptr;
      }
    }

    return *reinterpret_cast<void**>
      (*reinterpret_cast<const uintptr_t*>(self) + forcedCast.vftOffset - 1);
  }
  else {
    return forcedCast.out;
  }
}

namespace Util {

#if !defined(_MSC_VER) || _MSC_VER >= 1900
template <class T>
typename std::enable_if<std::is_default_constructible<T>::value, T*>::type
  _MakeDummy_impl() { return new T(); }
template <class T>
typename std::enable_if<!std::is_default_constructible<T>::value &&
                        (std::is_move_constructible<T>::value ||
                         std::is_copy_constructible<T>::value), T*>::type
  _MakeDummy_impl() {
  // Warning: may be unsafe depending on constructor or destructor implementation
  char blank[sizeof(T)] = {};
  T *result = nullptr;
  try { result = new T(std::move(*reinterpret_cast<T*>(blank))); } catch (...) {}
  return result;
}
template <class T>
typename std::enable_if<!std::is_default_constructible<T>::value &&
                        !std::is_move_constructible<T>::value &&
                        !std::is_copy_constructible<T>::value, T*>::type
  _MakeDummy_impl() { return nullptr; }
#else
// MSVC versions prior to 2015 use a primitive version of type_traits
template <class T>
T* _MakeDummy_impl() { return new T(); }
#endif

// Ancillary function for PatchFunctionVirtual and PMFCast. Creates dummy object
// from which vftable can be obtained using default, move, or copy constructor.
template <class T>
inline T* _MakeDummy() { return _MakeDummy_impl<T>(); }

} // namespace Util
} // namespace Patcher

// Helper macro to get void* pointer to a class member function
// Use like PatchFunction(_GetPointer(Class::MemberFunction), &newFunction)
#if defined(__clang__)
// Clang
// To work on virtual functions, class must be default, move, or copy constructible
#define _GetPointer(function) Patcher::PMFCast(&function)
#elif defined(__GNUC__)
// GCC-compatible
#define _GetPointer(function) ({                                               \
  _Pragma("GCC diagnostic push")                                               \
  _Pragma("GCC diagnostic ignored \"-Wpmf-conversions\"")                      \
  void *f_p = reinterpret_cast<void*>(&function);                              \
  _Pragma("GCC diagnostic pop")                                                \
  f_p; })
#else
// MSVC/C1, other
#define _GetPointer(function)                                                  \
  []() ->void* { void *p; _ASM_GET_PTR_(function, p); return p; }()
#endif

// ASM hack to get address of any non-overloaded function
#if defined(_MSC_VER) && !defined(__clang__)
// MSVC/C1-style inline assembly
// NOTE: Requires incremental linking to be disabled to work correctly!
#define _ASM_GET_PTR_(function, ptr)                                           \
  do { __asm mov eax, function __asm mov ptr, eax } while (0)
#else
// GCC-style inline assembly
// ** FIXME Broken for virtual functions; function instead of &function errors
#define _ASM_GET_PTR_(function, ptr)                                           \
  __asm__ __volatile__("movl %1, %%eax\n\t movl %%eax, %0\n\t"                 \
    :"=r"(ptr) :"r"(&function) :"%eax", "memory")
#endif


#endif // PATCHER_H