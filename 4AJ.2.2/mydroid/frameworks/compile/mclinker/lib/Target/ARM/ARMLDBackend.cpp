//===- ARMLDBackend.cpp ---------------------------------------------------===//
//
//                     The MCLinker Project
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include <llvm/ADT/Triple.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/ELF.h>
#include <llvm/Support/ErrorHandling.h>

#include <mcld/LD/SectionMap.h>
#include <mcld/MC/MCLDInfo.h>
#include <mcld/MC/MCLDOutput.h>
#include <mcld/MC/MCLinker.h>
#include <mcld/MC/MCRegionFragment.h>
#include <mcld/Support/MemoryRegion.h>
#include <mcld/Support/TargetRegistry.h>

#include <cstring>

#include "ARM.h"
#include "ARMELFDynamic.h"
#include "ARMLDBackend.h"
#include "ARMRelocationFactory.h"

using namespace mcld;

ARMGNULDBackend::ARMGNULDBackend()
  : m_pRelocFactory(NULL),
    m_pGOT(NULL),
    m_pPLT(NULL),
    m_pRelDyn(NULL),
    m_pRelPLT(NULL),
    m_pDynamic(NULL),
    m_pEXIDX(NULL),
    m_pEXTAB(NULL),
    m_pAttributes(NULL) {
}

ARMGNULDBackend::~ARMGNULDBackend()
{
  if (m_pRelocFactory)
    delete m_pRelocFactory;
  if(m_pGOT)
    delete m_pGOT;
  if(m_pPLT)
    delete m_pPLT;
  if(m_pRelDyn)
    delete m_pRelDyn;
  if(m_pRelPLT)
    delete m_pRelPLT;
  if(m_pDynamic)
    delete m_pDynamic;
}

bool ARMGNULDBackend::initRelocFactory(const MCLinker& pLinker)
{
  if (NULL == m_pRelocFactory) {
    m_pRelocFactory = new ARMRelocationFactory(1024, *this);
    m_pRelocFactory->setLayout(pLinker.getLayout());
  }
  return true;
}

RelocationFactory* ARMGNULDBackend::getRelocFactory()
{
  assert(NULL != m_pRelocFactory);
  return m_pRelocFactory;
}

bool ARMGNULDBackend::initTargetSectionMap(SectionMap& pSectionMap)
{
  if (!pSectionMap.push_back(".ARM.exidx", ".ARM.exidx") ||
      !pSectionMap.push_back(".ARM.extab", ".ARM.extab") ||
      !pSectionMap.push_back(".ARM.attributes", ".ARM.attributes"))
    return false;
  return true;
}

void ARMGNULDBackend::initTargetSections(MCLinker& pLinker)
{
 // FIXME: Currently we set exidx and extab to "Exception" and directly emit
 // them from input
  m_pEXIDX        = &pLinker.getOrCreateOutputSectHdr(".ARM.exidx",
                                                      LDFileFormat::Exception,
                                                      llvm::ELF::SHT_ARM_EXIDX,
                                                      llvm::ELF::SHF_ALLOC | llvm::ELF::SHF_LINK_ORDER,
                                                      bitclass() / 8);
  m_pEXTAB        = &pLinker.getOrCreateOutputSectHdr(".ARM.extab",
                                                      LDFileFormat::Exception,
                                                      llvm::ELF::SHT_PROGBITS,
                                                      llvm::ELF::SHF_ALLOC,
                                                      0x1);
  m_pAttributes   = &pLinker.getOrCreateOutputSectHdr(".ARM.attributes",
                                                      LDFileFormat::Target,
                                                      llvm::ELF::SHT_ARM_ATTRIBUTES,
                                                      0x0,
                                                      0x1);
}

void ARMGNULDBackend::initTargetSymbols(MCLinker& pLinker)
{
  // Define the symbol _GLOBAL_OFFSET_TABLE_ if there is a symbol with the
  // same name in input
  m_pGOTSymbol = pLinker.defineSymbol<MCLinker::AsRefered, MCLinker::Resolve>(
                   "_GLOBAL_OFFSET_TABLE_",
                   false,
                   ResolveInfo::Object,
                   ResolveInfo::Define,
                   ResolveInfo::Local,
                   0x0,  // size
                   0x0,  // value
                   NULL, // FragRef
                   ResolveInfo::Hidden);
}

void ARMGNULDBackend::doPreLayout(const Output& pOutput,
                                  const MCLDInfo& pInfo,
                                  MCLinker& pLinker)
{
  // when building shared object, the .got section is must.
  if(pOutput.type() == Output::DynObj && (NULL == m_pGOT)) {
      createARMGOT(pLinker, pOutput);
  }
}

void ARMGNULDBackend::doPostLayout(const Output& pOutput,
                                   const MCLDInfo& pInfo,
                                   MCLinker& pLinker)
{
  // emit program headers
  if(pOutput.type() == Output::DynObj || pOutput.type() == Output::Exec)
    emitProgramHdrs(pLinker.getLDInfo().output());

  ELFFileFormat *file_format = getOutputFormat(pOutput);

  // apply PLT
  if (file_format->hasPLT()) {
    // Since we already have the size of LDSection PLT, m_pPLT should not be
    // NULL.
    assert(NULL != m_pPLT);
    m_pPLT->applyPLT0();
    m_pPLT->applyPLT1();
  }

  // apply GOT
  if (file_format->hasGOT()) {
    // Since we already have the size of GOT, m_pGOT should not be NULL.
    assert(NULL != m_pGOT);
    if (pOutput.type() == Output::DynObj)
      m_pGOT->applyGOT0(file_format->getDynamic().addr());
    else {
      // executable file and object file? should fill with zero.
      m_pGOT->applyGOT0(0);
    }
  }
}

/// dynamic - the dynamic section of the target machine.
/// Use co-variant return type to return its own dynamic section.
ARMELFDynamic& ARMGNULDBackend::dynamic()
{
  if (NULL == m_pDynamic)
    m_pDynamic = new ARMELFDynamic(*this);

  return *m_pDynamic;
}

/// dynamic - the dynamic section of the target machine.
/// Use co-variant return type to return its own dynamic section.
const ARMELFDynamic& ARMGNULDBackend::dynamic() const
{
  assert( NULL != m_pDynamic);
  return *m_pDynamic;
}

bool ARMGNULDBackend::isPIC(const MCLDInfo& pLDInfo, const Output& pOutput) const
{
  return (pOutput.type() == Output::DynObj);
}

void ARMGNULDBackend::createARMGOT(MCLinker& pLinker, const Output& pOutput)
{
  // get .got LDSection and create MCSectionData
  ELFFileFormat* file_format = getOutputFormat(pOutput);

  LDSection& got = file_format->getGOT();
  m_pGOT = new ARMGOT(got, pLinker.getOrCreateSectData(got));

  // define symbol _GLOBAL_OFFSET_TABLE_ when .got create
  if( m_pGOTSymbol != NULL ) {
    pLinker.defineSymbol<MCLinker::Force, MCLinker::Unresolve>(
                     "_GLOBAL_OFFSET_TABLE_",
                     false,
                     ResolveInfo::Object,
                     ResolveInfo::Define,
                     ResolveInfo::Local,
                     0x0, // size
                     0x0, // value
                     pLinker.getLayout().getFragmentRef(*(m_pGOT->begin()), 0x0),
                     ResolveInfo::Hidden);
  }
  else {
    m_pGOTSymbol = pLinker.defineSymbol<MCLinker::Force, MCLinker::Resolve>(
                     "_GLOBAL_OFFSET_TABLE_",
                     false,
                     ResolveInfo::Object,
                     ResolveInfo::Define,
                     ResolveInfo::Local,
                     0x0, // size
                     0x0, // value
                     pLinker.getLayout().getFragmentRef(*(m_pGOT->begin()), 0x0),
                     ResolveInfo::Hidden);
  }

}

void ARMGNULDBackend::createARMPLTandRelPLT(MCLinker& pLinker,
                                            const Output& pOutput)
{
  ELFFileFormat* file_format = getOutputFormat(pOutput);

  // get .plt and .rel.plt LDSection
  LDSection& plt = file_format->getPLT();
  LDSection& relplt = file_format->getRelPlt();
  // create MCSectionData and ARMPLT
  m_pPLT = new ARMPLT(plt, pLinker.getOrCreateSectData(plt), *m_pGOT);
  // set info of .rel.plt to .plt
  relplt.setLink(&plt);
  // create MCSectionData and ARMRelDynSection
  m_pRelPLT = new OutputRelocSection(relplt,
                                     pLinker.getOrCreateSectData(relplt),
                                     8);
}

void ARMGNULDBackend::createARMRelDyn(MCLinker& pLinker,
                                      const Output& pOutput)
{
  ELFFileFormat* file_format = getOutputFormat(pOutput);

  // get .rel.dyn LDSection and create MCSectionData
  LDSection& reldyn = file_format->getRelDyn();
  // create MCSectionData and ARMRelDynSection
  m_pRelDyn = new OutputRelocSection(reldyn,
                                     pLinker.getOrCreateSectData(reldyn),
                                     8);
}

ELFFileFormat* ARMGNULDBackend::getOutputFormat(const Output& pOutput) const
{
  switch (pOutput.type()) {
    case Output::DynObj:
      return getDynObjFileFormat();
    case Output::Exec:
      return getExecFileFormat();
    // FIXME: We do not support building .o now
    case Output::Object:
    default:
      llvm::report_fatal_error(llvm::Twine("Unsupported output file format: ") +
                               llvm::Twine(pOutput.type()));
      return NULL;
  }
}

bool ARMGNULDBackend::isSymbolNeedsPLT(const ResolveInfo& pSym,
                                       const MCLDInfo& pLDInfo,
                                       const Output& pOutput) const
{
  return (Output::DynObj == pOutput.type() &&
          ResolveInfo::Function == pSym.type() &&
          (pSym.isDyn() || pSym.isUndef() ||
            isSymbolPreemptible(pSym, pLDInfo, pOutput)));
}

bool ARMGNULDBackend::isSymbolNeedsDynRel(const ResolveInfo& pSym,
                                          const Output& pOutput,
                                          bool isAbsReloc) const
{
  if(pSym.isUndef() && (Output::Exec == pOutput.type()))
    return false;
  if(pSym.isAbsolute())
    return false;
  if(Output::DynObj == pOutput.type() && isAbsReloc)
    return true;
  if(pSym.isDyn() || pSym.isUndef())
    return true;

  return false;
}

bool ARMGNULDBackend::isSymbolPreemptible(const ResolveInfo& pSym,
                                          const MCLDInfo& pLDInfo,
                                          const Output& pOutput) const
{
  if(pSym.other() != ResolveInfo::Default)
    return false;

  if(Output::DynObj != pOutput.type())
    return false;

  if(pLDInfo.options().Bsymbolic())
    return false;

  return true;
}

/// checkValidReloc - When we attempt to generate a dynamic relocation for
/// ouput file, check if the relocation is supported by dynamic linker.
void ARMGNULDBackend::checkValidReloc(Relocation& pReloc,
                                      const MCLDInfo& pLDInfo,
                                      const Output& pOutput) const
{
  // If not building a PIC object, no relocation type is invalid
  if (!isPIC(pLDInfo, pOutput))
    return;

  switch(pReloc.type()) {
    case llvm::ELF::R_ARM_RELATIVE:
    case llvm::ELF::R_ARM_COPY:
    case llvm::ELF::R_ARM_GLOB_DAT:
    case llvm::ELF::R_ARM_JUMP_SLOT:
    case llvm::ELF::R_ARM_ABS32:
    case llvm::ELF::R_ARM_ABS32_NOI:
    case llvm::ELF::R_ARM_PC24:
    case llvm::ELF::R_ARM_TLS_DTPMOD32:
    case llvm::ELF::R_ARM_TLS_DTPOFF32:
    case llvm::ELF::R_ARM_TLS_TPOFF32:
      break;

    default:
      llvm::report_fatal_error(llvm::Twine("Attempt to generate unsupported") +
                               llvm::Twine(" relocation type ") +
                               llvm::Twine((int)pReloc.type()) +
                               llvm::Twine(" for symbol '") +
                               llvm::Twine(pReloc.symInfo()->name()) +
                               llvm::Twine("', recompile with -fPIC")
                              );
      break;
  }
}

void ARMGNULDBackend::updateAddend(Relocation& pReloc,
                                   const LDSymbol& pInputSym,
                                   const Layout& pLayout) const
{
  // Update value keep in addend if we meet a section symbol
  if(pReloc.symInfo()->type() == ResolveInfo::Section) {
    pReloc.setAddend(pLayout.getOutputOffset(
                     *pInputSym.fragRef()) + pReloc.addend());
  }
}

void ARMGNULDBackend::scanLocalReloc(Relocation& pReloc,
                                     const LDSymbol& pInputSym,
                                     MCLinker& pLinker,
                                     const MCLDInfo& pLDInfo,
                                     const Output& pOutput)
{
  // rsym - The relocation target symbol
  ResolveInfo* rsym = pReloc.symInfo();

  updateAddend(pReloc, pInputSym, pLinker.getLayout());

  switch(pReloc.type()){

    // Set R_ARM_TARGET1 to R_ARM_ABS32
    // Ref: GNU gold 1.11 arm.cc, line 9892
    case llvm::ELF::R_ARM_TARGET1:
       pReloc.setType(llvm::ELF::R_ARM_ABS32);
    case llvm::ELF::R_ARM_ABS32:
    case llvm::ELF::R_ARM_ABS32_NOI: {
      // If buiding PIC object (shared library or PIC executable),
      // a dynamic relocations with RELATIVE type to this location is needed.
      // Reserve an entry in .rel.dyn
      if(isPIC(pLDInfo, pOutput)) {
        // create .rel.dyn section if not exist
        if(NULL == m_pRelDyn)
          createARMRelDyn(pLinker, pOutput);
        m_pRelDyn->reserveEntry(*m_pRelocFactory);
        // set Rel bit
        rsym->setReserved(rsym->reserved() | 0x1u);
        }
      return;
    }

    case llvm::ELF::R_ARM_ABS16:
    case llvm::ELF::R_ARM_ABS12:
    case llvm::ELF::R_ARM_THM_ABS5:
    case llvm::ELF::R_ARM_ABS8:
    case llvm::ELF::R_ARM_BASE_ABS:
    case llvm::ELF::R_ARM_MOVW_ABS_NC:
    case llvm::ELF::R_ARM_MOVT_ABS:
    case llvm::ELF::R_ARM_THM_MOVW_ABS_NC:
    case llvm::ELF::R_ARM_THM_MOVT_ABS: {
      // Update value keep in relocation place if we meet a section symbol
      if(rsym->type() == ResolveInfo::Section) {
        pReloc.target() = pLinker.getLayout().getOutputOffset(
                            *pInputSym.fragRef()) + pReloc.target();
      }

      // If building PIC object (shared library or PIC executable),
      // a dynamic relocation for this location is needed.
      // Reserve an entry in .rel.dyn
      if(isPIC(pLDInfo, pOutput)) {
        checkValidReloc(pReloc, pLDInfo, pOutput);
        // create .rel.dyn section if not exist
        if(NULL == m_pRelDyn)
          createARMRelDyn(pLinker, pOutput);
        m_pRelDyn->reserveEntry(*m_pRelocFactory);
        // set Rel bit
        rsym->setReserved(rsym->reserved() | 0x1u);
      }
      return;
    }
    case llvm::ELF::R_ARM_GOTOFF32:
    case llvm::ELF::R_ARM_GOTOFF12: {
      // A GOT section is needed
      if(NULL == m_pGOT)
        createARMGOT(pLinker, pOutput);
      return;
    }

    // Set R_ARM_TARGET2 to R_ARM_GOT_PREL
    // Ref: GNU gold 1.11 arm.cc, line 9892
    case llvm::ELF::R_ARM_TARGET2:
      pReloc.setType(llvm::ELF::R_ARM_GOT_PREL);
    case llvm::ELF::R_ARM_GOT_BREL:
    case llvm::ELF::R_ARM_GOT_PREL: {
      // A GOT entry is needed for these relocation type.
      // return if we already create GOT for this symbol
      if(rsym->reserved() & 0x6u)
        return;
      if(NULL == m_pGOT)
        createARMGOT(pLinker, pOutput);
      m_pGOT->reserveEntry();
      // If building PIC object, a dynamic relocation with
      // type RELATIVE is needed to relocate this GOT entry.
      // Reserve an entry in .rel.dyn
      if(isPIC(pLDInfo, pOutput)) {
        // create .rel.dyn section if not exist
        if(NULL == m_pRelDyn)
          createARMRelDyn(pLinker, pOutput);
        m_pRelDyn->reserveEntry(*m_pRelocFactory);
        // set GOTRel bit
        rsym->setReserved(rsym->reserved() | 0x4u);
        return;
      }
      // set GOT bit
      rsym->setReserved(rsym->reserved() | 0x2u);
      return;
    }

    case llvm::ELF::R_ARM_BASE_PREL: {
      // FIXME: Currently we only support R_ARM_BASE_PREL against
      // symbol _GLOBAL_OFFSET_TABLE_
      if(rsym != m_pGOTSymbol->resolveInfo()) {
        llvm::report_fatal_error(llvm::Twine("Do not support relocation '") +
                                 llvm::Twine("R_ARM_BASE_PREL' against symbol '") +
                                 llvm::Twine(rsym->name()) +
                                 llvm::Twine(".'"));
      }
      return;
    }
    case llvm::ELF::R_ARM_COPY:
    case llvm::ELF::R_ARM_GLOB_DAT:
    case llvm::ELF::R_ARM_JUMP_SLOT:
    case llvm::ELF::R_ARM_RELATIVE: {
      // These are relocation type for dynamic linker, shold not
      // appear in object file.
      llvm::report_fatal_error(llvm::Twine("unexpected reloc ") +
                               llvm::Twine((int)pReloc.type()) +
                               llvm::Twine(" in object file"));
      break;
    }
    default: {
      break;
    }
  } // end switch
}

void ARMGNULDBackend::scanGlobalReloc(Relocation& pReloc,
                                      const LDSymbol& pInputSym,
                                      MCLinker& pLinker,
                                      const MCLDInfo& pLDInfo,
                                      const Output& pOutput)
{
  // rsym - The relocation target symbol
  ResolveInfo* rsym = pReloc.symInfo();

  switch(pReloc.type()) {

    // Set R_ARM_TARGET1 to R_ARM_ABS32
    // Ref: GNU gold 1.11 arm.cc, line 9892
    case llvm::ELF::R_ARM_TARGET1:
      pReloc.setType(llvm::ELF::R_ARM_ABS32);
    case llvm::ELF::R_ARM_ABS32:
    case llvm::ELF::R_ARM_ABS16:
    case llvm::ELF::R_ARM_ABS12:
    case llvm::ELF::R_ARM_THM_ABS5:
    case llvm::ELF::R_ARM_ABS8:
    case llvm::ELF::R_ARM_BASE_ABS:
    case llvm::ELF::R_ARM_MOVW_ABS_NC:
    case llvm::ELF::R_ARM_MOVT_ABS:
    case llvm::ELF::R_ARM_THM_MOVW_ABS_NC:
    case llvm::ELF::R_ARM_THM_MOVT_ABS:
    case llvm::ELF::R_ARM_ABS32_NOI: {
      // Absolute relocation type, symbol may needs PLT entry or
      // dynamic relocation entry
      if(isSymbolNeedsPLT(*rsym, pLDInfo, pOutput)) {
        // create plt for this symbol if it does not have one
        if(!(rsym->reserved() & 0x8u)){
          // Create .got section if it doesn't exist
          if(NULL == m_pGOT)
            createARMGOT(pLinker, pOutput);
          // create .plt and .rel.plt if not exist
          if(NULL == m_pPLT)
            createARMPLTandRelPLT(pLinker, pOutput);
          // Symbol needs PLT entry, we need to reserve a PLT entry
          // and the corresponding GOT and dynamic relocation entry
          // in .got and .rel.plt. (GOT entry will be reserved simultaneously
          // when calling ARMPLT->reserveEntry())
          m_pPLT->reserveEntry();
          m_pRelPLT->reserveEntry(*m_pRelocFactory);
          // set PLT bit
          rsym->setReserved(rsym->reserved() | 0x8u);
        }
      }

      if(isSymbolNeedsDynRel(*rsym, pOutput, true)) {
        checkValidReloc(pReloc, pLDInfo, pOutput);
        // symbol needs dynamic relocation entry, reserve an entry in .rel.dyn
        // create .rel.dyn section if not exist
        if(NULL == m_pRelDyn)
          createARMRelDyn(pLinker, pOutput);
        m_pRelDyn->reserveEntry(*m_pRelocFactory);
        // set Rel bit
        rsym->setReserved(rsym->reserved() | 0x1u);
      }
      return;
    }

    case llvm::ELF::R_ARM_GOTOFF32:
    case llvm::ELF::R_ARM_GOTOFF12: {
      // A GOT section is needed
      if(NULL == m_pGOT)
        createARMGOT(pLinker, pOutput);
      return;
    }

    case llvm::ELF::R_ARM_BASE_PREL:
      // FIXME: Currently we only support R_ARM_BASE_PREL against
      // symbol _GLOBAL_OFFSET_TABLE_
      if(rsym != m_pGOTSymbol->resolveInfo()) {
        llvm::report_fatal_error(llvm::Twine("Do not support relocation '") +
                                 llvm::Twine("R_ARM_BASE_PREL' against symbol '") +
                                 llvm::Twine(rsym->name()) +
                                 llvm::Twine(".'"));
      }
    case llvm::ELF::R_ARM_REL32:
    case llvm::ELF::R_ARM_LDR_PC_G0:
    case llvm::ELF::R_ARM_SBREL32:
    case llvm::ELF::R_ARM_THM_PC8:
    case llvm::ELF::R_ARM_MOVW_PREL_NC:
    case llvm::ELF::R_ARM_MOVT_PREL:
    case llvm::ELF::R_ARM_THM_MOVW_PREL_NC:
    case llvm::ELF::R_ARM_THM_MOVT_PREL:
    case llvm::ELF::R_ARM_THM_ALU_PREL_11_0:
    case llvm::ELF::R_ARM_THM_PC12:
    case llvm::ELF::R_ARM_REL32_NOI:
    case llvm::ELF::R_ARM_ALU_PC_G0_NC:
    case llvm::ELF::R_ARM_ALU_PC_G0:
    case llvm::ELF::R_ARM_ALU_PC_G1_NC:
    case llvm::ELF::R_ARM_ALU_PC_G1:
    case llvm::ELF::R_ARM_ALU_PC_G2:
    case llvm::ELF::R_ARM_LDR_PC_G1:
    case llvm::ELF::R_ARM_LDR_PC_G2:
    case llvm::ELF::R_ARM_LDRS_PC_G0:
    case llvm::ELF::R_ARM_LDRS_PC_G1:
    case llvm::ELF::R_ARM_LDRS_PC_G2:
    case llvm::ELF::R_ARM_LDC_PC_G0:
    case llvm::ELF::R_ARM_LDC_PC_G1:
    case llvm::ELF::R_ARM_LDC_PC_G2:
    case llvm::ELF::R_ARM_ALU_SB_G0_NC:
    case llvm::ELF::R_ARM_ALU_SB_G0:
    case llvm::ELF::R_ARM_ALU_SB_G1_NC:
    case llvm::ELF::R_ARM_ALU_SB_G1:
    case llvm::ELF::R_ARM_ALU_SB_G2:
    case llvm::ELF::R_ARM_LDR_SB_G0:
    case llvm::ELF::R_ARM_LDR_SB_G1:
    case llvm::ELF::R_ARM_LDR_SB_G2:
    case llvm::ELF::R_ARM_LDRS_SB_G0:
    case llvm::ELF::R_ARM_LDRS_SB_G1:
    case llvm::ELF::R_ARM_LDRS_SB_G2:
    case llvm::ELF::R_ARM_LDC_SB_G0:
    case llvm::ELF::R_ARM_LDC_SB_G1:
    case llvm::ELF::R_ARM_LDC_SB_G2:
    case llvm::ELF::R_ARM_MOVW_BREL_NC:
    case llvm::ELF::R_ARM_MOVT_BREL:
    case llvm::ELF::R_ARM_MOVW_BREL:
    case llvm::ELF::R_ARM_THM_MOVW_BREL_NC:
    case llvm::ELF::R_ARM_THM_MOVT_BREL:
    case llvm::ELF::R_ARM_THM_MOVW_BREL: {
      // Relative addressing relocation, may needs dynamic relocation
      if(isSymbolNeedsDynRel(*rsym, pOutput, false)) {
        checkValidReloc(pReloc, pLDInfo, pOutput);
        // create .rel.dyn section if not exist
        if(NULL == m_pRelDyn)
          createARMRelDyn(pLinker, pOutput);
        m_pRelDyn->reserveEntry(*m_pRelocFactory);
        // set Rel bit
        rsym->setReserved(rsym->reserved() | 0x1u);
      }
      return;
    }

    case llvm::ELF::R_ARM_THM_CALL:
    case llvm::ELF::R_ARM_PLT32:
    case llvm::ELF::R_ARM_CALL:
    case llvm::ELF::R_ARM_JUMP24:
    case llvm::ELF::R_ARM_THM_JUMP24:
    case llvm::ELF::R_ARM_SBREL31:
    case llvm::ELF::R_ARM_PREL31:
    case llvm::ELF::R_ARM_THM_JUMP19:
    case llvm::ELF::R_ARM_THM_JUMP6:
    case llvm::ELF::R_ARM_THM_JUMP11:
    case llvm::ELF::R_ARM_THM_JUMP8: {
      // These are branch relocation (except PREL31)
      // A PLT entry is needed when building shared library

      // return if we already create plt for this symbol
      if(rsym->reserved() & 0x8u)
        return;

      // if symbol is defined in the ouput file and it's not
      // preemptible, no need plt
      if(rsym->isDefine() && !rsym->isDyn() &&
         !isSymbolPreemptible(*rsym, pLDInfo, pOutput)) {
        return;
      }

      // Create .got section if it doesn't exist
      if(NULL == m_pGOT)
        createARMGOT(pLinker, pOutput);

      // create .plt and .rel.plt if not exist
      if(NULL == m_pPLT)
         createARMPLTandRelPLT(pLinker, pOutput);
      // Symbol needs PLT entry, we need to reserve a PLT entry
      // and the corresponding GOT and dynamic relocation entry
      // in .got and .rel.plt. (GOT entry will be reserved simultaneously
      // when calling ARMPLT->reserveEntry())
      m_pPLT->reserveEntry();
      m_pRelPLT->reserveEntry(*m_pRelocFactory);
      // set PLT bit
      rsym->setReserved(rsym->reserved() | 0x8u);
      return;
    }

    // Set R_ARM_TARGET2 to R_ARM_GOT_PREL
    // Ref: GNU gold 1.11 arm.cc, line 9892
    case llvm::ELF::R_ARM_TARGET2:
      pReloc.setType(llvm::ELF::R_ARM_GOT_PREL);
    case llvm::ELF::R_ARM_GOT_BREL:
    case llvm::ELF::R_ARM_GOT_ABS:
    case llvm::ELF::R_ARM_GOT_PREL: {
      // Symbol needs GOT entry, reserve entry in .got
      // return if we already create GOT for this symbol
      if(rsym->reserved() & 0x6u)
        return;
      if(NULL == m_pGOT)
        createARMGOT(pLinker, pOutput);
      m_pGOT->reserveEntry();
      // If building shared object or the symbol is undefined, a dynamic
      // relocation is needed to relocate this GOT entry. Reserve an
      // entry in .rel.dyn
      if(Output::DynObj == pOutput.type() || rsym->isUndef() || rsym->isDyn()) {
        // create .rel.dyn section if not exist
        if(NULL == m_pRelDyn)
          createARMRelDyn(pLinker, pOutput);
        m_pRelDyn->reserveEntry(*m_pRelocFactory);
        // set GOTRel bit
        rsym->setReserved(rsym->reserved() | 0x4u);
        return;
      }
      // set GOT bit
      rsym->setReserved(rsym->reserved() | 0x2u);
      return;
    }

    case llvm::ELF::R_ARM_COPY:
    case llvm::ELF::R_ARM_GLOB_DAT:
    case llvm::ELF::R_ARM_JUMP_SLOT:
    case llvm::ELF::R_ARM_RELATIVE: {
      // These are relocation type for dynamic linker, shold not
      // appear in object file.
      llvm::report_fatal_error(llvm::Twine("Unexpected reloc ") +
                               llvm::Twine((int)pReloc.type()) +
                               llvm::Twine(" in object file"));
      break;
    }
    default: {
      break;
    }
  } // end switch
}

void ARMGNULDBackend::scanRelocation(Relocation& pReloc,
                                     const LDSymbol& pInputSym,
                                     MCLinker& pLinker,
                                     const MCLDInfo& pLDInfo,
                                     const Output& pOutput)
{
  // rsym - The relocation target symbol
  ResolveInfo* rsym = pReloc.symInfo();
  assert(NULL != rsym && "ResolveInfo of relocation not set while scanRelocation");

  // Scan relocation type to determine if an GOT/PLT/Dynamic Relocation
  // entries should be created.
  // FIXME: Below judgements concern only .so is generated as output
  // FIXME: Below judgements concern nothing about TLS related relocation

  // A refernece to symbol _GLOBAL_OFFSET_TABLE_ implies that a .got section
  // is needed
  if(NULL == m_pGOT && NULL != m_pGOTSymbol) {
    if(rsym == m_pGOTSymbol->resolveInfo()) {
      createARMGOT(pLinker, pOutput);
    }
  }

  // rsym is local
  if(rsym->isLocal())
    scanLocalReloc(pReloc, pInputSym, pLinker, pLDInfo, pOutput);

  // rsym is external
  else
    scanGlobalReloc(pReloc, pInputSym, pLinker, pLDInfo, pOutput);

}

uint64_t ARMGNULDBackend::emitSectionData(const Output& pOutput,
                                          const LDSection& pSection,
                                          const MCLDInfo& pInfo,
                                          MemoryRegion& pRegion) const
{
  assert(pRegion.size() && "Size of MemoryRegion is zero!");

  ELFFileFormat* file_format = getOutputFormat(pOutput);

  if (&pSection == m_pAttributes) {
    // FIXME: Currently Emitting .ARM.attributes directly from the input file.
    const llvm::MCSectionData* sect_data = pSection.getSectionData();
    assert(sect_data &&
           "Emit .ARM.attribute failed, MCSectionData doesn't exist!");

    uint8_t* start =
              llvm::cast<MCRegionFragment>(
                     sect_data->getFragmentList().front()).getRegion().start();

    memcpy(pRegion.start(), start, pRegion.size());
    return pRegion.size();
  }

  if (&pSection == &(file_format->getPLT())) {
    assert(NULL != m_pPLT && "emitSectionData failed, m_pPLT is NULL!");
    uint64_t result = m_pPLT->emit(pRegion);
    return result;
  }

  if (&pSection == &(file_format->getGOT())) {
    assert(NULL != m_pGOT && "emitSectionData failed, m_pGOT is NULL!");
    uint64_t result = m_pGOT->emit(pRegion);
    return result;
  }

  llvm::report_fatal_error(llvm::Twine("Unable to emit section `") +
                           pSection.name() +
                           llvm::Twine("'.\n"));
  return 0x0;
}

/// finalizeSymbol - finalize the symbol value
/// If the symbol's reserved field is not zero, MCLinker will call back this
/// function to ask the final value of the symbol
bool ARMGNULDBackend::finalizeSymbol(LDSymbol& pSymbol) const
{
  return false;
}

/// allocateCommonSymbols - allocate common symbols in the corresponding
/// sections.
/// @refer Google gold linker: common.cc: 214
bool
ARMGNULDBackend::allocateCommonSymbols(const MCLDInfo& pInfo, MCLinker& pLinker) const
{
  SymbolCategory& symbol_list = pLinker.getOutputSymbols();

  if (symbol_list.emptyCommons() && symbol_list.emptyLocals())
    return true;

  // addralign := max value of all common symbols
  uint64_t addralign = 0x0;

  // Due to the visibility, some common symbols may be forcefully local.
  SymbolCategory::iterator com_sym, com_end = symbol_list.localEnd();
  for (com_sym = symbol_list.localBegin(); com_sym != com_end; ++com_sym) {
    if (ResolveInfo::Common == (*com_sym)->desc()) {
      if ((*com_sym)->value() > addralign)
        addralign = (*com_sym)->value();
    }
  }

  // global common symbols.
  com_end = symbol_list.commonEnd();
  for (com_sym = symbol_list.commonBegin(); com_sym != com_end; ++com_sym) {
    if ((*com_sym)->value() > addralign)
      addralign = (*com_sym)->value();
  }

  // FIXME: If the order of common symbols is defined, then sort common symbols
  // com_sym = symbol_list.commonBegin();
  // std::sort(com_sym, com_end, some kind of order);

  // get or create corresponding BSS LDSection
  LDSection* bss_sect_hdr = NULL;
  if (ResolveInfo::ThreadLocal == (*com_sym)->type()) {
    bss_sect_hdr = &pLinker.getOrCreateOutputSectHdr(
                                   ".tbss",
                                   LDFileFormat::BSS,
                                   llvm::ELF::SHT_NOBITS,
                                   llvm::ELF::SHF_WRITE | llvm::ELF::SHF_ALLOC);
  }
  else {
    bss_sect_hdr = &pLinker.getOrCreateOutputSectHdr(".bss",
                                   LDFileFormat::BSS,
                                   llvm::ELF::SHT_NOBITS,
                                   llvm::ELF::SHF_WRITE | llvm::ELF::SHF_ALLOC);
  }

  // get or create corresponding BSS MCSectionData
  assert(NULL != bss_sect_hdr);
  llvm::MCSectionData& bss_section = pLinker.getOrCreateSectData(*bss_sect_hdr);

  // allocate all common symbols
  uint64_t offset = bss_sect_hdr->size();

  // allocate all local common symbols
  com_end = symbol_list.localEnd();
  for (com_sym = symbol_list.localBegin(); com_sym != com_end; ++com_sym) {
    if (ResolveInfo::Common == (*com_sym)->desc()) {
      // We have to reset the description of the symbol here. When doing
      // incremental linking, the output relocatable object may have common
      // symbols. Therefore, we can not treat common symbols as normal symbols
      // when emitting the regular name pools. We must change the symbols'
      // description here.
      (*com_sym)->resolveInfo()->setDesc(ResolveInfo::Define);
      llvm::MCFragment* frag = new llvm::MCFillFragment(0x0, 1, (*com_sym)->size());
      (*com_sym)->setFragmentRef(new MCFragmentRef(*frag, 0));
      uint64_t size = pLinker.getLayout().appendFragment(*frag,
                                                         bss_section,
                                                         (*com_sym)->value());
      offset += size;
    }
  }

  // allocate all global common symbols
  com_end = symbol_list.commonEnd();
  for (com_sym = symbol_list.commonBegin(); com_sym != com_end; ++com_sym) {
    // We have to reset the description of the symbol here. When doing
    // incremental linking, the output relocatable object may have common
    // symbols. Therefore, we can not treat common symbols as normal symbols
    // when emitting the regular name pools. We must change the symbols'
    // description here.
    (*com_sym)->resolveInfo()->setDesc(ResolveInfo::Define);
    llvm::MCFragment* frag = new llvm::MCFillFragment(0x0, 1, (*com_sym)->size());
    (*com_sym)->setFragmentRef(new MCFragmentRef(*frag, 0));
    uint64_t size = pLinker.getLayout().appendFragment(*frag,
                                                       bss_section,
                                                       (*com_sym)->value());
    offset += size;
  }

  bss_sect_hdr->setSize(offset);
  symbol_list.changeCommonsToGlobal();
  return true;
}

bool ARMGNULDBackend::readSection(Input& pInput,
                                  MCLinker& pLinker,
                                  LDSection& pInputSectHdr)
{
  LDSection& out_sect = pLinker.getOrCreateOutputSectHdr(pInputSectHdr.name(),
                                                         pInputSectHdr.kind(),
                                                         pInputSectHdr.type(),
                                                         pInputSectHdr.flag());
  // FIXME: (Luba)
  // Handle ARM attributes in the right way.
  // In current milestone, MCLinker goes through the shortcut.
  // It reads input's ARM attributes and copies the first ARM attributes
  // into the output file. The correct way is merge these sections, not
  // just copy.
  if ((0 == out_sect.name().compare(".ARM.attributes")) &&
      (0 != out_sect.size()))
    return true;

  MemoryRegion* region = pInput.memArea()->request(pInputSectHdr.offset(),
                                                   pInputSectHdr.size());

  llvm::MCSectionData& sect_data = pLinker.getOrCreateSectData(pInputSectHdr);

  new MCRegionFragment(*region, &sect_data);

  out_sect.setSize(out_sect.size() + pInputSectHdr.size());
  return true;
}

ARMGOT& ARMGNULDBackend::getGOT()
{
  assert(NULL != m_pGOT && "GOT section not exist");
  return *m_pGOT;
}

const ARMGOT& ARMGNULDBackend::getGOT() const
{
  assert(NULL != m_pGOT && "GOT section not exist");
  return *m_pGOT;
}

ARMPLT& ARMGNULDBackend::getPLT()
{
  assert(NULL != m_pPLT && "PLT section not exist");
  return *m_pPLT;
}

const ARMPLT& ARMGNULDBackend::getPLT() const
{
  assert(NULL != m_pPLT && "PLT section not exist");
  return *m_pPLT;
}

OutputRelocSection& ARMGNULDBackend::getRelDyn()
{
  assert(NULL != m_pRelDyn && ".rel.dyn section not exist");
  return *m_pRelDyn;
}

const OutputRelocSection& ARMGNULDBackend::getRelDyn() const
{
  assert(NULL != m_pRelDyn && ".rel.dyn section not exist");
  return *m_pRelDyn;
}

OutputRelocSection& ARMGNULDBackend::getRelPLT()
{
  assert(NULL != m_pRelPLT && ".rel.plt section not exist");
  return *m_pRelPLT;
}

const OutputRelocSection& ARMGNULDBackend::getRelPLT() const
{
  assert(NULL != m_pRelPLT && ".rel.plt section not exist");
  return *m_pRelPLT;
}

unsigned int
ARMGNULDBackend::getTargetSectionOrder(const Output& pOutput,
                                       const LDSection& pSectHdr) const
{
  ELFFileFormat* file_format = getOutputFormat(pOutput);

  if (&pSectHdr == &file_format->getGOT())
    return SHO_DATA;

  if (&pSectHdr == &file_format->getPLT())
    return SHO_PLT;

  return SHO_UNDEFINED;
}

namespace mcld {

//===----------------------------------------------------------------------===//
/// createARMLDBackend - the help funtion to create corresponding ARMLDBackend
///
TargetLDBackend* createARMLDBackend(const llvm::Target& pTarget,
                                    const std::string& pTriple)
{
  Triple theTriple(pTriple);
  if (theTriple.isOSDarwin()) {
    assert(0 && "MachO linker is not supported yet");
    /**
    return new ARMMachOLDBackend(createARMMachOArchiveReader,
                               createARMMachOObjectReader,
                               createARMMachOObjectWriter);
    **/
  }
  if (theTriple.isOSWindows()) {
    assert(0 && "COFF linker is not supported yet");
    /**
    return new ARMCOFFLDBackend(createARMCOFFArchiveReader,
                               createARMCOFFObjectReader,
                               createARMCOFFObjectWriter);
    **/
  }
  return new ARMGNULDBackend();
}

} // namespace of mcld

//=============================
// Force static initialization.
extern "C" void LLVMInitializeARMLDBackend() {
  // Register the linker backend
  mcld::TargetRegistry::RegisterTargetLDBackend(TheARMTarget, createARMLDBackend);
}
