

#include <windows.h>
#include "Patcher.h"


// Global variables
_Patch* Patcher::nextPatchAddress = NULL;
_Patch* Patcher::patchListHead = NULL;
int Patcher::numPatches = 0;

DWORD Patcher::imageBase = NULL;


// Rewrites an arbitrarily-sized chunk of code or data at the specified address
// expectedBytesBuffer (optional) points to a buffer containing the data expected for that address for sanity checking
// All the Patcher::Patch* functions (including templated ones) chain down to this function
_Patch* Patcher::Patch(void *address, void *newBytesBuffer, void *expectedBytesBuffer, unsigned int sizeOfBytesBuffer, bool onlyQueue) {
  _Patch *patch;

  if (!address) {
    return NULL;
  }

  if (nextPatchAddress) {
    patch = nextPatchAddress;
    *patch = _Patch(address, newBytesBuffer, expectedBytesBuffer, sizeOfBytesBuffer, !onlyQueue);
    nextPatchAddress = (_Patch*)NULL;
  }
  else {
    patch = new _Patch(address, newBytesBuffer, expectedBytesBuffer, sizeOfBytesBuffer, !onlyQueue);
  }
  if (!onlyQueue && !patch->Valid()) {
    delete patch;
    return NULL;
  }
  AppendPatchToList(patch);
  return patch;
}

_Patch* Patcher::Patch(void *address, void *newBytesBuffer, unsigned int sizeOfBytesBuffer, bool onlyQueue) {
  return Patch(address, newBytesBuffer, NULL, sizeOfBytesBuffer, onlyQueue);
}


// Inserts a jump instruction to our function at the specified address. If done in the middle of a function,
//  remember to reproduce any overwritten instructions (inserted instruction overwrites 5 bytes)
_Patch* Patcher::PatchFunction(void *functionAddress, void *newFunctionAddress, bool onlyQueue) {
  #pragma pack(push,1)
  struct JMP_32 {
    unsigned char opcode;
    void *address;
  };
  #pragma pack(pop)

  if (!functionAddress || !newFunctionAddress) {
    return NULL;
  }

  JMP_32 code;
  code.opcode = 0xE9; // JMP near rel32
  code.address = (void*)((char*)newFunctionAddress - ((char*)functionAddress + 5));

  return Patch(functionAddress, &code, sizeof(JMP_32), onlyQueue);
}


// Inserts a call instruction to our function at the specified address If done in the middle of a function,
//  remember to reproduce any overwritten instructions (inserted instruction overwrites 5 bytes)
_Patch* Patcher::PatchFunctionCall(void *functionCallAddress, void *newFunctionAddress, bool onlyQueue) {
  #pragma pack(push,1)
  struct CALL_32 {
    unsigned char opcode;
    void *address;
  };
  #pragma pack(pop)

  if (!functionCallAddress || !newFunctionAddress) {
    return NULL;
  }

  CALL_32 code;
  code.opcode = 0xE8; // CALL near rel32
  code.address = (void*)((char*)newFunctionAddress - ((char*)functionCallAddress + 5));

  return Patch(functionCallAddress, &code, sizeof(CALL_32), onlyQueue);
}


// Replaces the specified virtual function table pointer with a pointer to our function
_Patch* Patcher::PatchFunctionVirtual(void *vtblAddress, void *functionAddress, void *newFunctionAddress, bool onlyQueue) {
  if (!vtblAddress || !functionAddress || !newFunctionAddress) {
    return NULL;
  }

  void **vtbl = (void**)vtblAddress;

  // Iterate through the vtbl until we find the function entry we want to replace
  for (int i = 0; i < 1024 && vtbl[i]; ++i) {
    if (vtbl[i] == functionAddress) {
      return PatchFunctionVirtual(vtblAddress, i, newFunctionAddress, onlyQueue);
    }
  }

  return NULL; // Unable to find function in virtual function table
}


// Replaces the specified virtual function table entry index with a pointer to our function
_Patch* Patcher::PatchFunctionVirtual(void *vtblAddress, unsigned int vtblEntryIndex, void *newFunctionAddress, bool onlyQueue) {
  if (!vtblAddress || !newFunctionAddress) {
    return NULL;
  }

  void **vtbl = (void**)vtblAddress;

  return Patch(&vtbl[vtblEntryIndex], &newFunctionAddress, sizeof(vtbl[vtblEntryIndex]), onlyQueue);
}


// Patches all crossreferences to a global by scanning the PE base relocation table and patching each address found
_Patch* Patcher::PatchGlobalReferences(void *oldGlobalAddress, void *newGlobalAddress, bool onlyQueue) {
  #pragma pack(push,1)
  struct TypeOffset {
    unsigned short offset  :12; // offset, relative to the VirtualAddress value of the parent IMAGE_BASE_RELOCATION block
    unsigned short type    :4;  // IMAGE_REL_BASED_xxx - usually 3, 0 is sometimes used as a terminator/padder
  };
  #pragma pack(pop)

  _Patch *patch, *first = NULL;

  if (!oldGlobalAddress || !newGlobalAddress) {
    return NULL;
  }

  // Obtain PE file header info and use it to locate the base relocation table and get its size
  static IMAGE_DOS_HEADER *dosHeader = (IMAGE_DOS_HEADER*)OP2Addr(EXPECTED_OP2_ADDR);  // Call to OP2Addr ensures imageBase is initialized
  if (!dosHeader) {
    return NULL;
  }
  static IMAGE_OPTIONAL_HEADER *optHeader = (IMAGE_OPTIONAL_HEADER*)(imageBase + dosHeader->e_lfanew + 0x18);
  static IMAGE_DATA_DIRECTORY *relocDataDir = &optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
  if (!relocDataDir->VirtualAddress || !relocDataDir->Size) { // No base relocation table
    return NULL;
  }

  static IMAGE_BASE_RELOCATION *baseRelocTable
    = (IMAGE_BASE_RELOCATION*)(imageBase + relocDataDir->VirtualAddress); // 0x00489000

  IMAGE_BASE_RELOCATION *relocBlock = baseRelocTable; // Relocation table starts with the first block's header
  // Iterate through each relocation table block (for OP2 and most PE apps, each block represents 4096 bytes, i.e. 0x401000-0x402000)
  while ((DWORD)relocBlock < (DWORD)baseRelocTable + relocDataDir->Size - 1 && relocBlock->SizeOfBlock) {
    TypeOffset *relocArray = (TypeOffset*)((DWORD)relocBlock + sizeof(IMAGE_BASE_RELOCATION));
    // Iterate through each relocation in this page to find references to the global and replace them
    for (DWORD i = 0; i < (relocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(relocArray[0]); ++i) {
      if (relocArray[i].type != IMAGE_REL_BASED_HIGHLOW) {
        continue;
      }

      DWORD *addr = (DWORD*)(imageBase + relocBlock->VirtualAddress + relocArray[i].offset);
      if (*addr == (DWORD)oldGlobalAddress) { // Global reference found, patch it with a pointer to our global
        if ((patch = Patch(addr,
            &newGlobalAddress,
            &oldGlobalAddress,
            sizeof(newGlobalAddress),
            onlyQueue))) {
          if (!first) {
            first = patch;
          }
        }
        else { // Failed to apply patch
          while (first) { // Clean up any previously applied reference patches
            _Patch *delPatch = first;
            first = first->previous;
            delPatch->Unpatch(true);
          }
          return NULL;
        }
      }
    }

    // Set pointer to next relocation table block
    relocBlock = (IMAGE_BASE_RELOCATION*)((DWORD)relocBlock + relocBlock->SizeOfBlock);
  }

  return first; // You can do while (first) { [...] first = first->previous; } to get all the reference patches
                //  (if done before any new patches are applied)
                // If no patches applied/global not found in relocation table, this returned value will be NULL
}


// Mass applies all patches that are queued or otherwise not currently applied
int Patcher::DoPatchAll() {
  if (numPatches <= 0) {
    return -1;
  }

  int returnedValue = 1, numErrors = 0;
  _Patch *curPatch = patchListHead;

  while (curPatch) {
    if (!curPatch->Applied()) {
      if (curPatch->Patch() == 0) {
        returnedValue = 0;
        ++numErrors;
      }
    }

    curPatch = curPatch->next;
  }

  return returnedValue;
}


// Mass unapplies all patches. You should call Patcher::DoUnpatchAll(true,true) on unload for cleanup.
int Patcher::DoUnpatchAll(bool doDelete, bool force) {
  if (numPatches <= 0) {
    return -1;
  }

  int returnedValue = 1, numErrors = 0;
  _Patch *curPatch = patchListHead;
  _Patch *nextPatch = NULL;

  while (curPatch) {
    nextPatch = curPatch->next;

    if (curPatch->Applied()) {
      if (curPatch->Unpatch(doDelete, force) == 0) {
        returnedValue = 0;
        ++numErrors;
      }
    }

    curPatch = nextPatch;
  }

  return returnedValue;
}




// Gets the address of pre-allocated patch; only used with the #define functions
_Patch* Patcher::GetNextPatchAddress() {
  if (!nextPatchAddress) {
    AllocateNextPatch();
  }
  return nextPatchAddress;
}


// Adds patch to the linked list (used for mass patch/unpatch)
void Patcher::AppendPatchToList(_Patch *patch) {
  if (patchListHead) {
    patchListHead->previous = patch;
  }

  patch->next = patchListHead;
  patchListHead = patch;

  ++numPatches;
}


// Removes patch from the linked list
void Patcher::RemovePatchFromList(_Patch *patch, bool forceUnpatch) {
  if (patchListHead == patch) {
    patchListHead = patchListHead->next;
  }

  if (patch->previous) {
    patch->previous->next = patch->next;
  }
  if (patch->next) {
    patch->next->previous = patch->previous;
  }

  if (forceUnpatch && patch->Applied()) {
    patch->Unpatch(false, true);
  }

  --numPatches;
}


// Pre-allocates memory for the next patch, only used within the #define functions
void Patcher::AllocateNextPatch() {
  if (!nextPatchAddress) {
    nextPatchAddress = new _Patch();
  }
}


DWORD Patcher::InitModuleBase(char module[]) {
  return (!imageBase) ? (imageBase = (DWORD)GetModuleHandle(module)) : imageBase;
}



// Main constructor
_Patch::_Patch(void *address, void *newBytes, void *expectedBytes, unsigned int sizeOfBytes, bool doApply) {
  DWORD oldProtection;

  this->address = address;
  this->patchSize = sizeOfBytes;
  this->oldBytesBuffer = new unsigned char[this->patchSize];
  this->newBytesBuffer = new unsigned char[this->patchSize];
  memcpy(this->newBytesBuffer, newBytes, this->patchSize);

  this->invalid = false;

  // Test expected bytes vs. actual, and copy original bytes for future unpatching
  if (!VirtualProtect(this->address, this->patchSize, PAGE_READWRITE, &oldProtection)
    || (expectedBytes != NULL && memcmp(expectedBytes, this->address, this->patchSize) != 0)
    || memcpy(this->oldBytesBuffer, this->address, this->patchSize) < (void*)0
    || !VirtualProtect(this->address, this->patchSize, oldProtection, &oldProtection))
    this->invalid = true;

  this->isApplied = false;
  this->isPermanent = false;
  this->leaveMemoryUnprotected = false;
  if (doApply) {
    this->Patch();
  }

  this->previous = NULL;
  this->next = NULL;
}

// Dummy default constructor, only used by Patcher::AllocateNextPatch()
_Patch::_Patch() {
  memset(this, NULL, sizeof(_Patch));
  this->invalid = true;
}


_Patch::~_Patch() {
  if (this->isApplied) {
    this->Unpatch(false);
  }

  if (this->oldBytesBuffer) {
    delete [] this->oldBytesBuffer;
  }
  if (this->newBytesBuffer) {
    delete [] this->newBytesBuffer;
  }
}


// Apply patch
int _Patch::Patch(bool repatch) {
  DWORD oldAttr;

  if (this->invalid) {
    return 0;
  }

  if (this->isApplied && !repatch) {
    return 1;
  }

  if (!VirtualProtect(address, this->patchSize, PAGE_EXECUTE_READWRITE, &oldAttr)) {
    return 0;
  }
  memcpy(this->address, this->newBytesBuffer, this->patchSize);
  if (!this->leaveMemoryUnprotected) {
    VirtualProtect(address, this->patchSize, oldAttr, &oldAttr);
  }
  else {
    this->oldPageProtection = oldAttr;
  }

  this->isApplied = true;

  if (this->isPermanent && !repatch) {
    Patcher::RemovePatchFromList(this);
    delete this;
  }

  return 1;
}


// Unapply patch
int _Patch::Unpatch(bool doDelete, bool force) {
  int returnedValue = 1;
  DWORD oldAttr;

  if (this->invalid) {
    return 0;
  }

  if (!this->isApplied && !force) {
    returnedValue = 1;
  }
  else if (this->isPermanent) {
    returnedValue = 0;
  }
  else {
    if (!this->leaveMemoryUnprotected) {
      if (!VirtualProtect(address, this->patchSize, PAGE_EXECUTE_READWRITE, &oldAttr))
        returnedValue = 0;
    }
    memcpy(this->address, this->oldBytesBuffer, this->patchSize);
    if (!this->leaveMemoryUnprotected) {
      VirtualProtect(address, this->patchSize, oldAttr, &oldAttr);
    }

    this->isApplied = false;
  }

  if (doDelete) {
    Patcher::RemovePatchFromList(this);
    delete this;
  }

  return returnedValue;
}


// Use this if you want a copy of the original bytes this patch overwrites
void* _Patch::CopyOriginalBytes(void *buffer, unsigned int sizeOfBuffer) {
  return memcpy(buffer, this->oldBytesBuffer, ((sizeOfBuffer >= this->patchSize) ? this->patchSize : sizeOfBuffer));
}

  
bool _Patch::Applied() {
  return this->isApplied;
}


bool _Patch::Valid() {
  return !(this->invalid);
}


void _Patch::SetPermanent(bool permanent) {
  this->isPermanent = permanent;
}


// Sets whether to leave memory access as PAGE_EXECUTE_READWRITE after overwriting, by default it resets previous protection
void _Patch::SetLeaveMemoryUnprotected(bool leaveUnprotected) {
  if (!this->isApplied) {
    this->leaveMemoryUnprotected = leaveUnprotected;
  }
}


_Patch* _Patch::GetNext() {
  return this->next;    // Older patch
}


_Patch* _Patch::GetPrevious() {
  return this->previous;  // Newer patch
}




// Fixes the provided relative address against OP2's load address (RVA to VA)
DWORD OP2RVAToVA(DWORD relAddress) {
  if (Patcher::imageBase == NULL
    && !Patcher::InitModuleBase(EXPECTED_OP2_NAME)) {
      return NULL;
  }

  return (DWORD)(relAddress + Patcher::imageBase);
}