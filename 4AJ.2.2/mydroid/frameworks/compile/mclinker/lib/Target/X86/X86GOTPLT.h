//===- X86GOTPLT.h --------------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef MCLD_X86_GOTPLT_H
#define MCLD_X86_GOTPLT_H
#ifdef ENABLE_UNITTEST
#include <gtest.h>
#endif

#include "mcld/Target/GOT.h"

namespace mcld
{
class LDSection;

/** \class X86GOTPLT
 *  \brief X86 .got.plt section.
 */

const unsigned int X86GOT0Num = 3;

class X86GOTPLT : public GOT
{
  typedef llvm::DenseMap<const ResolveInfo*, GOTEntry*> SymbolIndexMapType;

public:
  typedef llvm::MCSectionData::iterator iterator;
  typedef llvm::MCSectionData::const_iterator const_iterator;

public:
  X86GOTPLT(LDSection &pSection, llvm::MCSectionData& pSectionData);

  ~X86GOTPLT();

  iterator begin();

  const_iterator begin() const;

  iterator end();

  const_iterator end() const;

// For GOT0
public:
  void applyGOT0(const uint64_t pAddress);

// For GOTPLT
public:
  void reserveGOTPLTEntry();

  void applyAllGOTPLT(const uint64_t pPLTBase);

  GOTEntry*& lookupGOTPLTMap(const ResolveInfo& pSymbol);

  iterator getNextGOTPLTEntry();

private:
  iterator m_GOTPLTIterator;
  SymbolIndexMapType m_GOTPLTMap;
};

} // namespace of mcld

#endif
