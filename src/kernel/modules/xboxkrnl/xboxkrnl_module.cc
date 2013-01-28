/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "kernel/modules/xboxkrnl/xboxkrnl_module.h"

#include "kernel/modules/xboxkrnl/xboxkrnl_table.h"


using namespace xe;
using namespace xe::kernel;
using namespace xe::kernel::xboxkrnl;


namespace {

  // bit 27 = something?
  typedef struct {
    uint32_t    flags;
    uint8_t     processor_count;
    uint8_t     unknown0;
    uint8_t     unknown1;
    uint8_t     unknown2;
    uint32_t    unknown4;
    uint16_t    unknown5;
    uint16_t    unknown6;
  } XboxHardwareInfo_t;
  // XboxHardwareInfo_t XboxHardwareInfo = {
  //   0x00000000, 3, 0, 0, 0, 0, 0, 0,
  // };

  // This should remain zero for now.
  // The pointer returned should have a 4b value at 0x58 that is the first
  // argument to RtlImageXexHeaderField (header base?)
  //uint32_t XexExecutableModuleHandle = 0x00000000;

}


XboxkrnlModule::XboxkrnlModule(xe_pal_ref pal, xe_memory_ref memory,
                               shared_ptr<ExportResolver> resolver) :
    KernelModule(pal, memory, resolver) {
  resolver->RegisterTable(
      "xboxkrnl.exe", xboxkrnl_export_table, XECOUNT(xboxkrnl_export_table));

  // TODO(benvanik): alloc heap memory somewhere in user space
  // TODO(benvanik): tools for reading/writing to heap memory

  // HACK: register some dummy globals for now.
  // KeDebugMonitorData
  resolver->SetVariableMapping(
      "xboxkrnl.exe", 0x00000059,
      0);
  // XboxHardwareInfo
  resolver->SetVariableMapping(
      "xboxkrnl.exe", 0x00000156,
      0);
  // XexExecutableModuleHandle
  resolver->SetVariableMapping(
      "xboxkrnl.exe", 0x00000193,
      0);

  // 0x0000012B, RtlImageXexHeaderField
}

XboxkrnlModule::~XboxkrnlModule() {
}