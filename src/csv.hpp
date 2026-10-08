#pragma once
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>
namespace csv {
inline std::string quote(const std::string& value) {
    if (value.find_first_of(",\"\r\n")==std::string::npos) return value;
    std::string out="\"";
    for (char c:value) { if(c=='"') out+='"'; out+=c; }
    return out+'"';
}
// RFC-style quoted fields, escaped quotes, CRLF and quoted line breaks.
inline bool read(std::istream& input,std::vector<std::string>& fields) {
    fields.clear(); std::string field; bool quoted=false,closed=false,seen=false;
    char c;
    while(input.get(c)) {
        seen=true;
        if(quoted) {
            if(c=='"') {
                if(input.peek()=='"') { input.get(c); field+='"'; }
                else { quoted=false; closed=true; }
            } else field+=c;
        } else if(c==',') { fields.push_back(field);field.clear();closed=false; }
        else if(c=='\n' || c=='\r') {
            if(c=='\r' && input.peek()=='\n') input.get(c);
            fields.push_back(field);return true;
        } else if(c=='"' && field.empty() && !closed) quoted=true;
        else {
            if(closed || c=='"') throw std::runtime_error("Malformed CSV quoting");
            field+=c;
        }
    }
    if(quoted) throw std::runtime_error("Unterminated CSV quote");
    if(seen) fields.push_back(field);
    return seen;
}
}
