//===- X86AndroidSectLinker.cpp -------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "X86AndroidSectLinker.h"

#include <mcld/CodeGen/SectLinkerOption.h>

using namespace mcld;

X86AndroidSectLinker::X86AndroidSectLinker(SectLinkerOption &pOption,
                                           TargetLDBackend &pLDBackend)
  : AndroidSectLinker(pOption,
                      pLDBackend) {
  MCLDInfo &info = pOption.info();
  // set up target-dependent constraints of attibutes
  info.attrFactory().constraint().disableWholeArchive();
  info.attrFactory().constraint().disableAsNeeded();
  info.attrFactory().constraint().setSharedSystem();

  // set up the predefined attributes
  info.attrFactory().predefined().unsetWholeArchive();
  info.attrFactory().predefined().setDynamic();

}

X86AndroidSectLinker::~X86AndroidSectLinker() {
}
