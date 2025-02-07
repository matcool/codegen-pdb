#include <filesystem>
#include <iostream>
#include <fstream>

#include "pdb_creator.h"
#include "pe_file.h"

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: CodegenPDB <gd exe> <CodegenData.json> <out>" << std::endl;
        return 1;
    }

    std::filesystem::path path_exe = argv[1];
    std::filesystem::path path_json = argv[2];
    std::filesystem::path path_out = argv[3];

    if (!std::filesystem::exists(path_exe)) {
        std::cerr << ".exe file does not exists";
        return 1;
    }

    if (!std::filesystem::exists(path_json)) {
        std::cerr << ".json file does not exists";
        return 1;
    }

    nlohmann::json j = nlohmann::json::parse(std::ifstream(path_json));
    PeFile pefile(path_exe);
    PdbCreator creator(pefile);

    std::cout << "age: " << pefile.getPdbAge();
    std::cout << "  guid: ";
    for (auto &byte : pefile.getPdbGuid()) {
        std::cout << std::hex << (int)byte;
    }
    std::cout << std::endl;

    creator.init();
    creator.importData(j);

    creator.save(path_out);
    
    std::cout << "done!" << std::endl;
}
