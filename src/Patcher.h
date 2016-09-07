

#ifndef PATCHER_H
#define PATCHER_H

#include <windows.h>

#define EXPECTED_OP2_ADDR 0x00400000
#define EXPECTED_OP2_NAME NULL // Calling GetModuleHandle(NULL) from a DLL returns the handle of the parent process


class _Patch;
class Patcher;


class Patcher {
  // Static functions
public:
  // These functions rewrite arbitrary code or data
  static _Patch* Patch(void *address, void *newBytesBuffer, void *expectedBytesBuffer, unsigned int sizeOfBytesBuffer, bool onlyQueue);
  static _Patch* Patch(void *address, void *newBytesBuffer, unsigned int sizeOfBytesBuffer, bool onlyQueue);
  template <typename PatchType>
    static _Patch* Patch(void *address, PatchType newBytes, PatchType expectedBytes, bool onlyQueue);
  template <typename PatchType>
    static _Patch* Patch(void *address, PatchType newBytes, bool onlyQueue);


  // These functions rewrite/insert a JUMP or CALL instruction
  static _Patch* PatchFunction(void *functionAddress, void *newFunctionAddress, bool onlyQueue);
  static _Patch* PatchFunctionCall(void *functionCallAddress, void *newFunctionAddress, bool onlyQueue);


  // These functions patch a function pointer in a virtual function table (can also work with general pointer tables)
  static _Patch* Patcher::PatchFunctionVirtual(void *vtblAddress, void *functionAddress, void *newFunctionAddress, bool onlyQueue);
  template <class Class>
    static _Patch* PatchFunctionVirtual(void *functionAddress, void *newFunctionAddress, bool onlyQueue);
  template <class Class> // Use this if the class has no default constructor
    static _Patch* PatchFunctionVirtual(Class &classObj, void *functionAddress, void *newFunctionAddress, bool onlyQueue);

  static _Patch* Patcher::PatchFunctionVirtual(void *vtblAddress, unsigned int vtblEntryIndex, void *newFunctionAddress, bool onlyQueue);
  template <class Class>
    static _Patch* PatchFunctionVirtual(int vtblEntryIndex, void *newFunctionAddress, bool onlyQueue);
  template <class Class> // Use this if the class has no default constructor
    static _Patch* PatchFunctionVirtual(Class &classObj, unsigned int vtblEntryIndex, void *newFunctionAddress, bool onlyQueue);


  // This function patches all references to a global variable/object (provided they are in the module's base relocation table)
  static _Patch* Patcher::PatchGlobalReferences(void *oldGlobalAddress, void *newGlobalAddress, bool onlyQueue);


  // These functions mass patch/unpatch; mass patch is only useful for queued patches
  static int DoPatchAll();
  static int DoUnpatchAll(bool doDelete, bool force = true);


  // Pre-allocates memory for the next patch object; only used with the #define functions
  static _Patch* GetNextPatchAddress();

private:
  static void AppendPatchToList(_Patch *patch);
  static void RemovePatchFromList(_Patch *patch, bool forceUnpatch = false);

  static void AllocateNextPatch();

  static DWORD InitModuleBase(char module[] = EXPECTED_OP2_NAME);

  // Static variables
private:
  static _Patch *patchListHead;          // Latest applied patch. List is sorted newest -> oldest
  static int numPatches;

  static _Patch *nextPatchAddress;        // Next allocated block of memory to store a new patch in

  static DWORD imageBase;

public:
  // Friend classes
  friend class _Patch;

  // Friend functions
  friend inline DWORD OP2Addr(DWORD address);    // Fixes provided address against OP2's expected address and its load address (VA to VA)
  friend inline void* OP2Addr(void *address);
  friend inline DWORD _OP2Addr(DWORD address);  // Simplified version of OP2Addr() for use in code hooks or loops
  friend inline void* _OP2Addr(void *address);
  friend DWORD OP2RVAToVA(DWORD relAddress);    // Same as OP2Addr, but provided address is assumed to be a relative offset (RVA to VA)
  friend inline void* OP2RVAToVA(void *relAddress);
};


// Do not instantiate this class yourself. Use Patcher to do so.
class _Patch {
  // Member functions
public:
  _Patch();
  _Patch(void *address, void *newBytes, void *expectedBytes, unsigned int sizeOfBytes, bool doApply);
  ~_Patch();

  int Patch(bool repatch = false);
  int Unpatch(bool doDelete, bool force = false);

  // Use these if you want a copy of the original unmodified bytes this patch overwrites
  void* CopyOriginalBytes(void *buffer, unsigned int sizeOfBuffer);
  template <typename ReturnType>
    ReturnType GetOriginalBytes();

  bool Applied();
  bool Valid();

  void SetPermanent(bool permanent);
  void SetLeaveMemoryUnprotected(bool leaveUnprotected);

  _Patch* GetNext();
  _Patch* GetPrevious();

  // Member variables
private:
  _Patch *next;          // Points towards older patches
  _Patch *previous;        // Points towards newer patches

  void *address;
  unsigned int patchSize;
  unsigned char *oldBytesBuffer;
  DWORD oldPageProtection;
  unsigned char *newBytesBuffer;

  bool isApplied;
  bool isPermanent;        // If the patch is set to permanent before being applied, the object will be deleted upon application;
                  // if set to be permanent after application, the object will be preserved
  bool leaveMemoryUnprotected;
  bool invalid;

  // Friend classes
  friend class Patcher;
};




// Templates and inlined functions must be declared and defined in the same file

template <typename PatchType>
_Patch* Patcher::Patch(void *address, PatchType newBytes, bool onlyQueue) {
  return Patch(address, &newBytes, NULL, sizeof(PatchType), onlyQueue); 
}


template <typename PatchType>
_Patch* Patcher::Patch(void *address, PatchType newBytes, PatchType expectedBytes, bool onlyQueue) {
  return Patch(address, &newBytes, &expectedBytes, sizeof(PatchType), onlyQueue); 
}


template <class Class>
_Patch* Patcher::PatchFunctionVirtual(void *functionAddress, void *newFunctionAddress, bool onlyQueue) {
  Class classObj; // Dummy for getting the vtbl pointer, doesn't work if the class has no default constructor
  return PatchFunctionVirtual<Class>(classObj, functionAddress, newFunctionAddress, onlyQueue);
}


template <class Class>
_Patch* Patcher::PatchFunctionVirtual(Class &classObj, void *functionAddress, void *newFunctionAddress, bool onlyQueue) {
  void **vtbl = (void**&)classObj;
  return PatchFunctionVirtual((void*)vtbl, functionAddress, newFunctionAddress, onlyQueue);
}


template <class Class>
_Patch* Patcher::PatchFunctionVirtual(int vtblEntryIndex, void *newFunctionAddress, bool onlyQueue) {
  Class classObj; // Dummy for getting the vtbl pointer, doesn't work if the class has no default constructor
  return PatchFunctionVirtual<Class>(classObj, vtblEntryIndex, newFunctionAddress, onlyQueue);
}


template <class Class>
_Patch* Patcher::PatchFunctionVirtual(Class &classObj, unsigned int vtblEntryIndex, void *newFunctionAddress, bool onlyQueue) {
  void **vtbl = (void**&)classObj;
  return PatchFunctionVirtual(vtbl, vtblEntryIndex, newFunctionAddress, onlyQueue);
}


// Fixes provided address against OP2's expected address and its load address (VA to VA)
inline DWORD OP2Addr(DWORD address) { return OP2RVAToVA(address - EXPECTED_OP2_ADDR); }
inline void* OP2Addr(void *address) { return (void*)(OP2Addr((DWORD)address)); }

// Simplified inline versions of OP2Addr() for use in loops or code hooks. These assume imageBase is already set!
inline DWORD _OP2Addr(DWORD address) { return (DWORD)(address - EXPECTED_OP2_ADDR + Patcher::imageBase); }
inline void* _OP2Addr(void *address) { return (void*)(_OP2Addr((DWORD)address)); }

// void* input/output for OP2RVAToVA(). Input address must be relative offset
inline void* OP2RVAToVA(void *relAddress) { return (void*)OP2RVAToVA((DWORD)relAddress); }


template <typename ReturnType>
ReturnType _Patch::GetOriginalBytes() {
  ReturnType returnedValue;
  _Patch::CopyOriginalBytes(&returnedValue, sizeof(ReturnType));
  return returnedValue;
}


// Inline assembler trick to get a direct pointer to any non-overloaded function (any symbol?)
#if defined(_MSC_VER) // Microsoft VC uses Intel syntax
#define _GetFuncPtr_(func, ptr) do { __asm mov eax, func __asm mov ptr, eax } while (0)
#elif defined(__ICC) || defined(__INTEL_COMPILER) || defined(__clang__) || defined(__GNUC__) || defined(__GNUG__) // GCC, LLVM, ICC use/default to AT&T syntax
#define _GetFuncPtr_(func, ptr) __asm__ __volatile__("movl %1, %%eax; movl %%eax, %0;" :"=r"(ptr) :"r"(func) :"%eax" : :"memory") /* TODO needs testing */
#else  // Fail
#define _GetFuncPtr_(func, ptr)
#endif

// This macro is just to reduce common code in the macros below
#define _GetFuncPtrs_(func1, func2) void *_ptr1_, *_ptr2_; _GetFuncPtr_(func1, _ptr1_); _GetFuncPtr_(func2, _ptr2_)

// These "functions" are used as ex. Patcher::PatchMemberFunction(Class::Function, ...);
//  They are used when you need to patch a class member function by directly referencing
//  Class::Function. Usually only necessary when it is a virtual function or you are not
//  using MSVC; otherwise you can get a plain void* pointer with &(void*&)Class::Function
//  (trying with virtual functions just returns a pointer to a specially-generated thunk)
// A caveat is that, if you use these in conditional statements, enclosing the body within
//  brackets is not optional, despite it looking like only one line of written code. This
//  method has no way of distinguishing between function overloads; as a result, it is not
//  possible to directly take the pointer of an overloaded virtual function.

// Patcher::PatchVirtualFunction
#define PatchVirtualFunction(Class, classFunctionToPatch, newFunction, onlyQueue)            \
  GetNextPatchAddress(); do { _GetFuncPtrs_(classFunctionToPatch, newFunction);              \
    Patcher::PatchFunctionVirtual<Class>(_ptr1_, _ptr2_, onlyQueue); } while (0)

// Patcher::PatchObjectVirtualFunction
#define PatchObjectVirtualFunction(obj, Class, classFunctionToPatch, newFunction, onlyQueue) \
  GetNextPatchAddress(); do { _GetFuncPtrs_(classFunctionToPatch, newFunction);              \
    Patcher::PatchFunctionVirtual<Class>(obj, _ptr1_, _ptr2_, onlyQueue); } while (0)

// Patcher::PatchMemberFunction
#define PatchMemberFunction(classFunctionToPatch, newFunction, onlyQueue)                    \
  GetNextPatchAddress(); do { _GetFuncPtrs_(classFunctionToPatch, newFunction);              \
    Patcher::PatchFunction(_ptr1_, _ptr2_, onlyQueue); } while (0)

// Patcher::PatchReferencedVirtualFunction
// To use this you need to make a struct filled with function pointers that represents the
//  vftable you want to hook
#define PatchReferencedVirtualFunction(referencedFunctionToPatch, newFunction, onlyQueue)    \
  GetNextPatchAddress(); do { _GetFuncPtrs_(referencedFunctionToPatch, newFunction);         \
    Patcher::Patch(&_ptr1_, &_ptr2_, sizeof(void*), onlyQueue); } while (0)


#endif