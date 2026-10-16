#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace rendering {
// Planet-local angular triangle footprint, independent of terrain height,
// sinking, triangle numbering and winding. Quantization is ~6 micrometres on
// Earth, below the supported centimetre detail. field.comp matches this oracle.
// XYZ: 96-bit identity; W: canonical corner permutation (two bits/corner).
inline std::array<std::uint32_t,4> grassCellIdentity(std::array<std::array<double,3>,3> corners) {
    for(auto& p:corners) for(auto& x:p) {x=std::nearbyint(x*1099511627776.0);if(x==0) x=0;}
    std::array<unsigned,3> order{0,1,2};
    std::sort(order.begin(),order.end(),[&](unsigned a,unsigned b){return corners[a]<corners[b];});
    std::array<std::uint32_t,4> key{2166136261u,0x9e3779b9u,0x85ebca6bu,0};
    for(auto i:order) for(auto x:corners[i]) {
        const auto bits=std::bit_cast<std::uint64_t>(x);
        for(auto word:{std::uint32_t(bits),std::uint32_t(bits>>32)})
            for(int h=0;h<3;++h) key[h]=(key[h]^word)*16777619u;
    }
    key[3]=order[0]|(order[1]<<2)|(order[2]<<4);return key;
}
}
