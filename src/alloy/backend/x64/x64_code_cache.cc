/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include <alloy/backend/x64/x64_code_cache.h>

#include <alloy/backend/x64/tracing.h>

using namespace alloy;
using namespace alloy::backend;
using namespace alloy::backend::x64;


namespace alloy {
namespace backend {
namespace x64 {

class X64CodeChunk {
public:
  X64CodeChunk(size_t chunk_size);
  ~X64CodeChunk();
public:
  X64CodeChunk* next;
  size_t    capacity;
  uint8_t*  buffer;
  size_t    offset;

  // Estimate of function sized use to determine initial table capacity.
  const static uint32_t ESTIMATED_FN_SIZE = 512;
  // Size of unwind info per function.
  // TODO(benvanik): move this to emitter.
  const static uint32_t UNWIND_INFO_SIZE = 4 + (2 * 1);

  void*             fn_table_handle;
  RUNTIME_FUNCTION* fn_table;
  uint32_t          fn_table_count;
  uint32_t          fn_table_capacity;

  void AddTableEntry(uint8_t* code, size_t code_size);
};


}  // namespace x64
}  // namespace backend
}  // namespace alloy


X64CodeCache::X64CodeCache(size_t chunk_size) :
    chunk_size_(chunk_size),
    head_chunk_(NULL), active_chunk_(NULL) {
  lock_ = AllocMutex();
}

X64CodeCache::~X64CodeCache() {
  LockMutex(lock_);
  auto chunk = head_chunk_;
  while (chunk) {
    auto next = chunk->next;
    delete chunk;
    chunk = next;
  }
  head_chunk_ = NULL;
  UnlockMutex(lock_);
  FreeMutex(lock_);
}

int X64CodeCache::Initialize() {
  return 0;
}

void* X64CodeCache::PlaceCode(void* machine_code, size_t code_size) {
  // Add unwind info into the allocation size. Keep things 16b aligned.
  code_size += XEROUNDUP(X64CodeChunk::UNWIND_INFO_SIZE, 16);

  // Always move the code to land on 16b alignment. We do this by rounding up
  // to 16b so that all offsets are aligned.
  code_size = XEROUNDUP(code_size, 16);

  LockMutex(lock_);

  if (active_chunk_) {
    if (active_chunk_->capacity - active_chunk_->offset < code_size) {
      auto next = active_chunk_->next;
      if (!next) {
        XEASSERT(code_size < chunk_size_); // need to support larger chunks
        next = new X64CodeChunk(chunk_size_);
        active_chunk_->next = next;
      }
      active_chunk_ = next;
    }
  } else {
    head_chunk_ = active_chunk_ = new X64CodeChunk(chunk_size_);
  }

  uint8_t* final_address = active_chunk_->buffer + active_chunk_->offset;
  active_chunk_->offset += code_size;

  // Add entry to fn table.
  active_chunk_->AddTableEntry(final_address, code_size);

  UnlockMutex(lock_);

  // Copy code.
  xe_copy_struct(final_address, machine_code, code_size);

  // This isn't needed on x64 (probably), but is convention.
  FlushInstructionCache(GetCurrentProcess(), final_address, code_size);
  return final_address;
}

X64CodeChunk::X64CodeChunk(size_t chunk_size) :
    next(NULL),
    capacity(chunk_size), buffer(0), offset(0) {
  buffer = (uint8_t*)VirtualAlloc(
      NULL, capacity,
      MEM_RESERVE | MEM_COMMIT,
      PAGE_EXECUTE_READWRITE);

  fn_table_capacity = (uint32_t)XEROUNDUP(capacity / ESTIMATED_FN_SIZE, 16);
  size_t table_size = fn_table_capacity * sizeof(RUNTIME_FUNCTION);
  fn_table = (RUNTIME_FUNCTION*)xe_malloc(table_size);
  fn_table_count = 0;
  fn_table_handle = 0;
  RtlAddGrowableFunctionTable(
      &fn_table_handle,
      fn_table,
      fn_table_count,
      fn_table_capacity,
      (ULONG_PTR)buffer, (ULONG_PTR)buffer + capacity);
}

X64CodeChunk::~X64CodeChunk() {
  if (fn_table_handle) {
    RtlDeleteGrowableFunctionTable(fn_table_handle);
  }
  if (buffer) {
    VirtualFree(buffer, 0, MEM_RELEASE);
  }
}

// http://msdn.microsoft.com/en-us/library/ssa62fwe.aspx
namespace {
typedef enum _UNWIND_OP_CODES {
  UWOP_PUSH_NONVOL = 0, /* info == register number */
  UWOP_ALLOC_LARGE,     /* no info, alloc size in next 2 slots */
  UWOP_ALLOC_SMALL,     /* info == size of allocation / 8 - 1 */
  UWOP_SET_FPREG,       /* no info, FP = RSP + UNWIND_INFO.FPRegOffset*16 */
  UWOP_SAVE_NONVOL,     /* info == register number, offset in next slot */
  UWOP_SAVE_NONVOL_FAR, /* info == register number, offset in next 2 slots */
  UWOP_SAVE_XMM128,     /* info == XMM reg number, offset in next slot */
  UWOP_SAVE_XMM128_FAR, /* info == XMM reg number, offset in next 2 slots */
  UWOP_PUSH_MACHFRAME   /* info == 0: no error-code, 1: error-code */
} UNWIND_CODE_OPS;

typedef union _UNWIND_CODE {
  struct {
    uint8_t CodeOffset;
    uint8_t UnwindOp : 4;
    uint8_t OpInfo   : 4;
  };
  USHORT FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;

typedef struct _UNWIND_INFO {
  uint8_t Version       : 3;
  uint8_t Flags         : 5;
  uint8_t SizeOfProlog;
  uint8_t CountOfCodes;
  uint8_t FrameRegister : 4;
  uint8_t FrameOffset   : 4;
  UNWIND_CODE UnwindCode[1];
/*  UNWIND_CODE MoreUnwindCode[((CountOfCodes + 1) & ~1) - 1];
*   union {
*       OPTIONAL ULONG ExceptionHandler;
*       OPTIONAL ULONG FunctionEntry;
*   };
*   OPTIONAL ULONG ExceptionData[]; */
} UNWIND_INFO, *PUNWIND_INFO;
}  // namespace

void X64CodeChunk::AddTableEntry(uint8_t* code, size_t code_size) {
  // NOTE: we assume a chunk lock.

  if (fn_table_count + 1 > fn_table_capacity) {
    // Table exhausted, need to realloc. If this happens a lot we should tune
    // the table size to prevent this.
    XELOGW("X64CodeCache growing FunctionTable - adjust ESTIMATED_FN_SIZE");
    RtlDeleteGrowableFunctionTable(fn_table_handle);
    size_t old_size = fn_table_capacity * sizeof(RUNTIME_FUNCTION);
    size_t new_size = old_size * 2;
    auto new_table = (RUNTIME_FUNCTION*)xe_realloc(fn_table, old_size, new_size);
    XEASSERTNOTNULL(new_table);
    if (!new_table) {
      return;
    }
    fn_table = new_table;
    fn_table_capacity *= 2;
    RtlAddGrowableFunctionTable(
        &fn_table_handle,
        fn_table,
        fn_table_count,
        fn_table_capacity,
        (ULONG_PTR)buffer, (ULONG_PTR)buffer + capacity);
  }

  // Allocate unwind data. We know we have space because we overallocated.
  // This should be the tailing 16b with 16b alignment.
  size_t unwind_info_offset = offset;
  offset += UNWIND_INFO_SIZE;

  // TODO(benvanik): take as parameters?
  bool has_prolog = false;
  uint8_t prolog_size = 4;
  uint8_t stack_bytes = 16;

  // http://msdn.microsoft.com/en-us/library/ddssxxy8.aspx
  UNWIND_INFO* unwind_info = (UNWIND_INFO*)(buffer + unwind_info_offset);
  unwind_info->Version = 1;
  unwind_info->Flags = 0;
  unwind_info->SizeOfProlog = has_prolog ? prolog_size : 0;
  unwind_info->CountOfCodes = has_prolog ? 1 : 0;
  unwind_info->FrameRegister = 0;
  unwind_info->FrameOffset = 0;

  // http://msdn.microsoft.com/en-us/library/ck9asaa9.aspx
  auto& code_0 = unwind_info->UnwindCode[0];
  code_0.CodeOffset = 4; // end of instruction + 1 == offset of next instruction
  code_0.UnwindOp = UWOP_ALLOC_SMALL;
  code_0.OpInfo = stack_bytes;
  XEASSERT(stack_bytes < 128);

  // Add entry.
  auto& fn_entry = fn_table[fn_table_count++];
  fn_entry.BeginAddress = (DWORD)(code - buffer);
  fn_entry.EndAddress = (DWORD)(fn_entry.BeginAddress + code_size);
  fn_entry.UnwindData = (DWORD)unwind_info_offset;

  // Notify the function table that it has new entries.
  RtlGrowFunctionTable(fn_table_handle, fn_table_count);
}
