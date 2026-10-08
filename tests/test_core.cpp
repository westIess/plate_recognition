#include "plate_format.hpp"
#include "levenshtein.hpp"
#include "csv.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
int main() {
    try {
        check(plate::normalize("A123BC777").valid,"valid 3-digit region");
        check(plate::normalize("K900XX77").valid,"valid 2-digit region");
        check(plate::normalize("0482MP199").text=="O482MP199","0 to O");
        check(plate::normalize("4I23BC77").text=="A123BC77","4 to A and I to 1");
        check(plate::normalize("AO8B8C77").text=="A088BC77","position aware O/B/8");
        check(plate::normalize(u8"а 123 вс-777").text=="A123BC777","Cyrillic lookalikes");
        for(const auto& s:{"", "A123BC7", "A123BC7777", "F123BC777", "A123BC777RUS", "A12?BC777", "A123BC7X"})
            check(!plate::normalize(s).valid,"reject invalid");
        check(plate::normalize("A125BC777").text=="A125BC777","do not guess valid digit substitutions");
        check(plate::better(plate::normalize("A123BC77",15),plate::normalize("JUNK",99)),"validity priority");
        check(plate::better(plate::normalize("O482MP199",80),plate::normalize("0482MP199",85)),"correction penalty");
        check(levenshtein("kitten","sitting")==3,"Levenshtein");
        check(levenshtein("","A123BC77")==8,"missing prediction distance");
        std::string filename="car,\"one\"\n.jpg";
        std::istringstream input(csv::quote(filename)+",A123BC77\r\n");std::vector<std::string> row;
        check(csv::read(input,row) && row.size()==2 && row[0]==filename,"CSV round trip");
        check(!csv::read(input,row),"CSV EOF");
        bool rejected=false;
        try {std::istringstream bad("\"unterminated");csv::read(bad,row);} catch(const std::runtime_error&) {rejected=true;}
        check(rejected,"malformed CSV");
        std::cout << "core tests passed\n";return 0;
    } catch(const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
