/**
   Copyright 2019-2021 Mikhail Paulyshka

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
**/

#pragma once

//llvm
#include <llvm/DebugInfo/CodeView/SymbolRecord.h>
#include <llvm/DebugInfo/MSF/MSFBuilder.h>
#include <llvm/DebugInfo/PDB/Native/PDBFileBuilder.h>
#include <llvm/DebugInfo/PDB/Native/DbiStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/GSIStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/InfoStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/TpiStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/RawConstants.h>

//fakepdb
#include "pe_file.h"
#include "nlohmann/json.hpp"

class PdbCreator {
public:
    PdbCreator(PeFile& peFile);

    bool init();

    void importData(nlohmann::json const& data);

    bool save(std::filesystem::path &path);

private:
    void addTypeInfo(llvm::pdb::TpiStreamBuilder &TpiBuilder);

    void processGSI(nlohmann::json const& data);

    bool processSections();

    llvm::pdb::BulkPublic createPublicSymbol(const char* name, uintptr_t rva);

    PeFile& _pefile;

    llvm::BumpPtrAllocator _allocator;
    llvm::pdb::PDBFileBuilder _pdbBuilder;
    std::vector<llvm::pdb::SecMapEntry> _sectionMap;

    uintptr_t m_highestFuncAddr = 0;
};
