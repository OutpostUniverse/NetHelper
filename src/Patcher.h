

#ifndef PATCHER_H
#define PATCHER_H

// Uncomment this line if using MinHook; comment otherwise:
//#define PATCHER_MINHOOK

#if defined(_MSC_VER) && !defined(__clang__) && !defined(__GNUC__)
#define PATCHER_MSVC
#endif

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

  bool GetEnabled() { return enabled; }
  bool GetValid() { return !invalid; }

protected:
  patch() { enabled = false; module = reinterpret_cast<HMODULE>(-1); }

  bool VerifyModule();
  void GetModuleInfo(HMODULE &moduleOut, size_t &hashOut);

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

private:
  std::unique_ptr<BYTE[]> newBytesBuffer;
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
  const void* GetTrapoline();

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

// Cast pointer to member function to void*. May be used by _GetPointer() macro.
// NOTE: For virtual PMFs, an object instance must be passed or class is default,
// copy, or move constructible. Class cannot multiply inherit.
template <class T, class U>
typename std::enable_if<std::is_member_function_pointer<U T::*>::value>::type*
  PMFCast(U T::*pmf, const T *self = nullptr) {
  union {
    U T::*in;
    void *out;
    uintptr_t vftOffset;
  } u;

  u.in = pmf;

  #ifdef PATCHER_MSVC
  static BYTE vcall[] =
    #ifdef _M_X64
    { 0x48, 0x8B, 0x01, 0xFF }; // mov rax, [rcx]; jmp qword ptr [rax+?]
    #else
    { 0x8B, 0x01, 0xFF };       // mov eax, [ecx]; jmp dword ptr [eax+?]
    #endif
  auto *operand = reinterpret_cast<BYTE*>(u.out) + _countof(vcall);

  if (memcmp(u.out, vcall, _countof(vcall)) == 0 && *operand & 0x20) {
  #else
  if (u.vftOffset & 1) {
  #endif
    // Virtual; requires an object instance to get the vftable pointer
    std::unique_ptr<T> dummy;
    if (!self) {
      dummy.reset(Util::_MakeDummy<T>());
      if (!(self = dummy.get())) {
        return nullptr;
      }
    }

    uintptr_t offset =
      #ifdef PATCHER_MSVC
      *operand == 0x60 ? *(operand + 1) :
      *operand == 0xA0 ? *reinterpret_cast<DWORD*>(operand + 1) : 0;
      #else
      u.vftOffset - 1;
      #endif

    return *reinterpret_cast<void**>(*reinterpret_cast<const uintptr_t*>(self) +
                                     offset);
  }
  else {
    return u.out;
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
// Used like PatchFunction(_GetPointer(Class::MemberFunction), &newFunction)
#if defined(__clang__) || (defined(PATCHER_MSVC) && defined(_M_X64))
// MSVC/C1 (x64), Clang: See comments of PMFCast about restrictions
#define _GetPointer(function) Patcher::PMFCast(&function)
#elif defined(__GNUC__)
// GCC, ICC, etc.
#define _GetPointer(function) ({                                               \
  _Pragma("GCC diagnostic push")                                               \
  _Pragma("GCC diagnostic ignored \"-Wpmf-conversions\"")                      \
  void *f_p = reinterpret_cast<void*>(&function);                              \
  _Pragma("GCC diagnostic pop")                                                \
  f_p; })
#elif defined(PATCHER_MSVC)
// MSVC/C1 (x86): Requires incremental linking to be disabled to work correctly
#define _GetPointer(function) []() ->void* {                                   \
  void *p; { __asm mov eax, function __asm mov p, eax } return p; }()
#else
// Unsupported compiler
#define _GetPointer(function) []() ->void* { static_assert(false,              \
  "_GetPointer is only supported in MSVC, GCC, or Clang"); return nullptr; }()
#endif


#endif // PATCHER_H