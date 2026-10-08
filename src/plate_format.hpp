#pragma once
#include <algorithm>
#include <string>
#include <utility>

namespace plate {
inline bool letter(char c) { return std::string("ABEKMHOPCTYX").find(c) != std::string::npos; }
inline bool digit(char c) { return c >= '0' && c <= '9'; }
inline bool letterPosition(std::size_t i) { return i == 0 || i == 4 || i == 5; }

// Canonical Latin lookalikes. Preserve unknown characters so they cannot silently
// disappear into an apparently valid registration. Whitespace/separators only.
inline std::string canonical(const std::string& raw) {
    static const std::pair<std::string, char> cyrillic[] = {
        {u8"А",'A'},{u8"В",'B'},{u8"Е",'E'},{u8"К",'K'},
        {u8"М",'M'},{u8"Н",'H'},{u8"О",'O'},{u8"Р",'P'},
        {u8"С",'C'},{u8"Т",'T'},{u8"У",'Y'},{u8"Х",'X'},
        {u8"а",'A'},{u8"в",'B'},{u8"е",'E'},{u8"к",'K'},
        {u8"м",'M'},{u8"н",'H'},{u8"о",'O'},{u8"р",'P'},
        {u8"с",'C'},{u8"т",'T'},{u8"у",'Y'},{u8"х",'X'}};
    std::string out;
    for (std::size_t i = 0; i < raw.size();) {
        bool matched = false;
        for (const auto& p : cyrillic) if (raw.compare(i,p.first.size(),p.first)==0) {
            out += p.second; i += p.first.size(); matched = true; break;
        }
        if (matched) continue;
        char c = raw[i++];
        if (c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='-') continue;
        if (c>='a' && c<='z') c = static_cast<char>(c-'a'+'A');
        out += c;
    }
    return out;
}
inline bool valid(const std::string& s) {
    if (s.size()!=8 && s.size()!=9) return false;
    for (std::size_t i=0;i<s.size();++i)
        if (!(letterPosition(i) ? letter(s[i]) : digit(s[i]))) return false;
    return true; // Format only, not a region registry or issuance check.
}
struct Result {
    std::string text;
    bool valid = false;
    int corrections = 0;
    float confidence = 0;
    float score() const { return std::max(0.0f, confidence - 7.0f*corrections); }
};
inline Result normalize(const std::string& raw, float confidence = 0) {
    Result r{canonical(raw),false,0,std::clamp(confidence,0.0f,100.0f)};
    if (r.text.size()!=8 && r.text.size()!=9) return r;
    std::string corrected = r.text;
    for (std::size_t i=0;i<corrected.size();++i) {
        char& c = corrected[i]; char before = c;
        if (letterPosition(i)) {
            if (c=='0') c='O'; else if (c=='8') c='B'; else if (c=='4') c='A';
        } else {
            if (c=='O' || c=='Q' || c=='D') c='0';
            else if (c=='B') c='8'; else if (c=='A') c='4';
            else if (c=='I' || c=='L') c='1'; else if (c=='S') c='5';
            else if (c=='Z') c='2'; else if (c=='T') c='7';
        }
        r.corrections += (c != before);
    }
    r.valid = valid(corrected);
    if (r.valid) r.text = corrected;
    else r.corrections = 0;
    return r;
}
inline bool better(const Result& a, const Result& b) {
    if (a.valid != b.valid) return a.valid;
    if (a.text.empty() != b.text.empty()) return !a.text.empty();
    if (a.score() != b.score()) return a.score() > b.score();
    return a.corrections < b.corrections;
}
} // namespace plate
