#include "csv.hpp"
#include "levenshtein.hpp"
#include "plate_format.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
using Records=std::map<std::string,std::string>;
Records load(const std::filesystem::path& path,bool truth) {
    std::ifstream in(path,std::ios::binary);
    if(!in) throw std::runtime_error("Cannot open "+path.string());
    // Excel UTF-8 BOM is permitted.
    if(in.peek()==0xEF) {char bom[3];in.read(bom,3);if(std::string(bom,3)!="\xEF\xBB\xBF") throw std::runtime_error("Invalid BOM");}
    std::vector<std::string> header,row;
    if(!csv::read(in,header)) throw std::runtime_error("Empty CSV: "+path.string());
    auto column=[&](const std::string& key) {
        auto it=std::find(header.begin(),header.end(),key);
        if(it==header.end() || std::count(header.begin(),header.end(),key)!=1) throw std::runtime_error("Missing/duplicate column "+key);
        return static_cast<std::size_t>(it-header.begin());
    };
    auto filename=column("filename"),number=column("plate_number");Records records;
    while(csv::read(in,row)) {
        if(row.size()!=header.size()) throw std::runtime_error("Wrong field count in "+path.string());
        auto name=row[filename];auto plate=plate::canonical(row[number]);
        if(name.empty()) throw std::runtime_error("Empty filename");
        if(truth && !plate.empty() && !plate::valid(plate)) throw std::runtime_error("Invalid ground truth for "+name);
        if(!std::all_of(plate.begin(),plate.end(),[](unsigned char c){return c<128;}))
            throw std::runtime_error("Unsupported non-ASCII plate characters for "+name);
        if(!records.emplace(name,plate).second) throw std::runtime_error("Duplicate filename: "+name);
    }
    if(truth && records.empty()) throw std::runtime_error("No ground truth rows");
    return records;
}
int main(int argc,char** argv) {
    try {
        if(argc<3 || argc>4) {std::cerr << "Usage: evaluate RESULTS.csv GROUND_TRUTH.csv [ERRORS.csv]\n";return 1;}
        auto results=load(argv[1],false),truth=load(argv[2],true);
        std::filesystem::path errors=argc==4?argv[3]:"output/errors.csv";
        auto normalized=[](const std::filesystem::path& p){return std::filesystem::weakly_canonical(p);};
        if(normalized(errors)==normalized(argv[1]) || normalized(errors)==normalized(argv[2]))
            throw std::runtime_error("Errors file must not overwrite an input");
        if(!errors.parent_path().empty()) std::filesystem::create_directories(errors.parent_path());
        std::ofstream out(errors);out.exceptions(std::ios::failbit|std::ios::badbit);
        out << "filename,expected,predicted,distance,reason\n";
        std::size_t exact=0,edits=0,characters=0,missing=0,extra=0;
        auto error=[&](const std::string& name,const std::string& expected,const std::string& predicted,std::size_t distance,const char* reason) {
            out << csv::quote(name) << ',' << csv::quote(expected) << ',' << csv::quote(predicted) << ',' << distance << ',' << reason << '\n';
            std::cout << reason << ": " << csv::quote(name) << " expected=" << expected << " predicted=" << predicted << '\n';
        };
        for(const auto& [name,expected]:truth) {
            auto it=results.find(name);bool absent=it==results.end();
            auto predicted=absent?std::string():it->second;
            auto distance=levenshtein(expected,predicted);
            characters+=expected.size();edits+=distance;
            if(absent) {++missing;error(name,expected,predicted,distance,"missing_result");}
            else if(predicted==expected) ++exact;
            else error(name,expected,predicted,distance,predicted.empty()?"unrecognized":"mismatch");
        }
        for(const auto& [name,predicted]:results) if(!truth.count(name)) {
            ++extra;error(name,"",predicted,predicted.size(),"extra_result");
        }
        out.close();
        std::cout << std::fixed << std::setprecision(6)
                  << "samples=" << truth.size() << "\nexact=" << exact
                  << "\naccuracy=" << double(exact)/truth.size()
                  << "\nedit_distance=" << edits << "\nreference_characters=" << characters << "\nCER=";
        if(characters) std::cout << double(edits)/characters; else std::cout << "N/A";
        std::cout << "\nmissing=" << missing << "\nextra=" << extra << '\n';
        return (missing || extra)?2:0;
    } catch(const std::exception& e) {std::cerr << "Error: " << e.what() << '\n';return 1;}
}
