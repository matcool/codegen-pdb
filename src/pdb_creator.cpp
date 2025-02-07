/**
   Copyright 2019 LLVM project
   Copyright 2019-2021 Mikhail Paulyshka
   2025 mat

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

#include <llvm/DebugInfo/CodeView/SymbolSerializer.h>
#include <llvm/DebugInfo/PDB/Native/DbiModuleDescriptorBuilder.h>
#include <llvm/Object/COFF.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Parallel.h>

#include "pdb_creator.h"

template<typename R, class FuncTy>
void parallelSort(R &&Range, FuncTy Fn) {
    llvm::parallelSort(std::begin(Range), std::end(Range), Fn);
}

PdbCreator::PdbCreator(PeFile& pefile) : _pefile(pefile), _pdbBuilder(_allocator) {}

bool PdbCreator::init() {
    //initialize builder
    if (_pdbBuilder.initialize(4096)) {
        return false;
    }

    // Create streams in MSF for predefined streams, namely PDB, TPI, DBI and IPI.
    for (int I = 0; I < (int) llvm::pdb::kSpecialStreamCount; ++I) {
        if (_pdbBuilder.getMsfBuilder().addStream(0).takeError()) {
            return false;
        }
    }

    // Add an Info stream.
    auto &InfoBuilder = _pdbBuilder.getInfoBuilder();
    InfoBuilder.setVersion(llvm::pdb::PdbRaw_ImplVer::PdbImplVC70);
    InfoBuilder.setHashPDBContentsToGUID(false);
    InfoBuilder.setAge(_pefile.getPdbAge()); // typically 2

    auto guid_d = _pefile.getPdbGuid();
    llvm::codeview::GUID guid{};
    memcpy(guid.Guid, guid_d.data(), guid_d.size());
    InfoBuilder.setGuid(guid);

    //Add an empty DBI stream.
    auto &DbiBuilder = _pdbBuilder.getDbiBuilder();
    DbiBuilder.setAge(InfoBuilder.getAge());
    DbiBuilder.setVersionHeader(llvm::pdb::PdbDbiV70);
    DbiBuilder.setMachineType(llvm::COFF::MachineTypes::IMAGE_FILE_MACHINE_AMD64);
    DbiBuilder.setFlags(llvm::pdb::DbiFlags::FlagStrippedMask);

    // Technically we are not link.exe 14.11, but there are known cases where
    // debugging tools on Windows expect Microsoft-specific version numbers or
    // they fail to work at all.  Since we know we produce PDBs that are
    // compatible with LINK 14.11, we set that version number here.
    DbiBuilder.setBuildNumber(14, 11);

    return true;
}

void PdbCreator::importData(nlohmann::json const& data) {
    //TPI
    addTypeInfo(_pdbBuilder.getTpiBuilder());

    //IPI
    addTypeInfo(_pdbBuilder.getIpiBuilder());

    //GSI
    processGSI(data);

    //Symbols
    // processSymbols();

    //Sections
    processSections();
}

bool PdbCreator::save(std::filesystem::path &path) {
    auto guid = _pdbBuilder.getInfoBuilder().getGuid();
    if (_pdbBuilder.commit(path.string(), &guid)) {
        return false;
    }

    return true;
}

void PdbCreator::addTypeInfo(llvm::pdb::TpiStreamBuilder &TpiBuilder) {
    // Start the TPI or IPI stream header.
    TpiBuilder.setVersionHeader(llvm::pdb::PdbTpiV80);
}

void PdbCreator::processGSI(nlohmann::json const& data) {
    auto &GsiBuilder = _pdbBuilder.getGsiBuilder();

    std::vector<llvm::pdb::BulkPublic> Publics;

    //Functions
    for (auto const& cl : data["classes"]) {
        auto name = cl["name"].get<std::string>();
        for (auto const& f : cl["functions"]) {
            auto func = name + "::" + f["name"].get<std::string>();
            auto addrJson = f["bindings"]["win"];
            if (addrJson.is_number()) {
                auto addr = addrJson.get<uintptr_t>();
                m_highestFuncAddr = std::max(m_highestFuncAddr, addr);
                // leak the string because llvm doesnt own it
                // but it needs to live until the pdb is saved
                auto* data = (char*)malloc(func.size() + 1);
                strcpy(data, func.c_str());
                Publics.push_back(createPublicSymbol(data, addr));
            }
        }
    }

    if (!Publics.empty()) {
        // Sort the public symbols and add them to the stream.
        parallelSort(Publics, [](const llvm::pdb::BulkPublic &L, const llvm::pdb::BulkPublic &R) {
            return strcmp(L.Name, R.Name) < 0;
        });

        GsiBuilder.addPublicSymbols(std::move(Publics));
    }
}

bool PdbCreator::processSections() {
    auto &DbiBuilder = _pdbBuilder.getDbiBuilder();

    llvm::object::coff_section textSection{};
    strcpy(textSection.Name, ".text");
    textSection.Characteristics = llvm::COFF::IMAGE_SCN_CNT_CODE | llvm::COFF::IMAGE_SCN_MEM_EXECUTE | llvm::COFF::IMAGE_SCN_MEM_READ;
    textSection.VirtualAddress = 0x1000;
    textSection.VirtualSize = m_highestFuncAddr + 0x100;
    textSection.SizeOfRawData = ((textSection.VirtualSize / 0x100) + 1) * 0x100;
    textSection.PointerToRawData = 0x400;

    std::vector<llvm::object::coff_section> sections;
    sections.push_back(textSection);

    DbiBuilder.createSectionMap(sections);

    // Add COFF section header stream.
    auto sectionsTable = llvm::ArrayRef<uint8_t>(reinterpret_cast<const uint8_t *>(sections.data()),
                                                    reinterpret_cast<const uint8_t *>(sections.data() + sections.size()));
    if (DbiBuilder.addDbgStream(llvm::pdb::DbgHeaderType::SectionHdr, sectionsTable)) {
        return false;
    }

    return true;
}

llvm::pdb::BulkPublic PdbCreator::createPublicSymbol(const char* name, uintptr_t rva) {
    llvm::pdb::BulkPublic public_sym;
    public_sym.Name = name;
    public_sym.NameLen = strlen(name);
    public_sym.setFlags(llvm::codeview::PublicSymFlags::Function);
    public_sym.Segment = 1; //_pefile.GetSectionIndexForRVA(idaFunc.start_rva);
    public_sym.Offset = rva - 0x1000; // _pefile.GetSectionOffsetForRVA(idaFunc.start_rva);

    return public_sym;
}
