#pragma once
#include <algorithm>
#include <numeric>
#include <string>
#include <vector>
inline std::size_t levenshtein(const std::string& a,const std::string& b) {
    std::vector<std::size_t> row(b.size()+1);std::iota(row.begin(),row.end(),0);
    for(std::size_t i=1;i<=a.size();++i) {
        auto diagonal=row[0];row[0]=i;
        for(std::size_t j=1;j<=b.size();++j) {
            auto old=row[j];
            row[j]=std::min({row[j]+1,row[j-1]+1,diagonal+(a[i-1]!=b[j-1])});
            diagonal=old;
        }
    }
    return row.back();
}
