/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef ALLOY_FRONTEND_PPC_PPC_TRANSLATOR_H_
#define ALLOY_FRONTEND_PPC_PPC_TRANSLATOR_H_

#include <alloy/core.h>
#include <alloy/backend/assembler.h>
#include <alloy/compiler/compiler.h>
#include <alloy/runtime/symbol_info.h>


namespace alloy {
namespace frontend {
namespace ppc {

class PPCFrontend;
class PPCHIRBuilder;
class PPCScanner;


class PPCTranslator {
public:
  PPCTranslator(PPCFrontend* frontend);
  ~PPCTranslator();

  int Translate(runtime::FunctionInfo* symbol_info,
                uint32_t debug_info_flags,
                runtime::Function** out_function);

private:
  void DumpSource(runtime::FunctionInfo* symbol_info,
                  StringBuffer* string_buffer);
  void DumpSourceJson(runtime::FunctionInfo* symbol_info,
                      StringBuffer* string_buffer);

private:
  PPCFrontend*          frontend_;
  PPCScanner*           scanner_;
  PPCHIRBuilder*        builder_;
  compiler::Compiler*   compiler_;
  backend::Assembler*   assembler_;

  StringBuffer          string_buffer_;
};


}  // namespace ppc
}  // namespace frontend
}  // namespace alloy


#endif  // ALLOY_FRONTEND_PPC_PPC_TRANSLATOR_H_
