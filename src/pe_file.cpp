/**
   Copyright 2019 Mikhail Paulyshka

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

#include <llvm/Support/Error.h>

#include "pe_file.h"

PeFile::PeFile(const std::filesystem::path& path) : _binary(llvm::object::createBinary(path.string()))
{
    if (!_binary.takeError()) {
        _obj = llvm::dyn_cast<llvm::object::COFFObjectFile>((*_binary).getBinary());
    }
}

std::vector<uint8_t> PeFile::getPdbGuid()
{
    const llvm::codeview::DebugInfo* DebugInfo;
    llvm::StringRef PDBFileName;

    if(_obj->getDebugPDBInfo(DebugInfo, PDBFileName) ||!DebugInfo){
        return std::vector<uint8_t>(16);
    }

    return std::vector<uint8_t>(&DebugInfo->PDB70.Signature[0], &DebugInfo->PDB70.Signature[16]);
}

uint32_t PeFile::getPdbAge()
{
    const llvm::codeview::DebugInfo* DebugInfo = nullptr;
    llvm::StringRef PDBFileName;

    if(_obj->getDebugPDBInfo(DebugInfo, PDBFileName) || !DebugInfo){
        return 0;
    }

    return DebugInfo->PDB70.Age;
}