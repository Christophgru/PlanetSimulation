#pragma once
#include "config/FoliageConfig.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rendering {
struct GrassBudgetSignals {
    std::uint64_t nowNs=0,memoryNs=0,sampleReserved=0,gpuSample=0,gpuNs=0;
    std::optional<std::uint64_t> freeBytes,capBytes;
    std::optional<double> gpuMs;
    std::optional<unsigned> utilization;
};
// Small resident-planner policy; all physical readings are cached worker data.
// Publication supplies its remaining aggregate/per-generation allowance.
class GrassBudgetController {
    GrassBudgetSignals signals_;
    std::uint64_t revision_=1,lastChange_=0,lastGpu_=std::numeric_limits<std::uint64_t>::max();
    std::optional<std::uint64_t> memoryReference_;
    unsigned high_=0,low_=0;
    double scale_=1;
public:
    bool freshMemory() const {return signals_.freeBytes && signals_.memoryNs && signals_.nowNs>=signals_.memoryNs && signals_.nowNs-signals_.memoryNs<=2000000000ull;}
    void observe(GrassBudgetSignals s) {
        signals_=s;
        const auto free=freshMemory()?s.freeBytes:std::nullopt;
        const bool cooldown=!lastChange_ || (s.nowNs>=lastChange_ && s.nowNs-lastChange_>=2000000000ull);
        bool memoryChange=free.has_value()!=memoryReference_.has_value();
        if(free && memoryReference_) memoryChange=std::abs(double(*free)-double(*memoryReference_))>std::max(16.*1024*1024,double(*memoryReference_)*.25);
        if(memoryChange && cooldown) {memoryReference_=free;++revision_;lastChange_=s.nowNs;}
        if(!s.gpuMs || !s.gpuNs || s.nowNs<s.gpuNs || s.nowNs-s.gpuNs>2000000000ull || s.gpuSample==lastGpu_) return;
        lastGpu_=s.gpuSample;
        const bool busy=*s.gpuMs>41.667 && (!s.utilization || *s.utilization>=90);
        high_=busy?std::min(3u,high_+1):0;low_=*s.gpuMs<25.?std::min(3u,low_+1):0;
        if(!lastChange_ || (s.nowNs>=lastChange_ && s.nowNs-lastChange_>=2000000000ull)) {
            const double next=high_>=3 ? std::max(.05,scale_*.8) : low_>=3 ? std::min(1.,scale_*1.1) : scale_;
            if(next!=scale_) {scale_=next;++revision_;lastChange_=s.nowNs;high_=low_=0;}
        }
    }
    std::uint64_t available(std::uint64_t reserved,std::uint64_t logical) const {
        constexpr std::uint64_t mib=1024*1024;
        std::uint64_t bytes=0;
        if(freshMemory()) {
            bytes=*signals_.freeBytes/2;
            const auto growth=reserved>signals_.sampleReserved?reserved-signals_.sampleReserved:0;
            bytes-=std::min(bytes,growth); // Outstanding reservations may precede the physical sample.
        } else bytes=std::min(64*mib,256*mib-std::min(256*mib,reserved));
        if(signals_.capBytes) bytes=std::min(bytes,*signals_.capBytes-std::min(*signals_.capBytes,reserved));
        return std::min(logical,bytes);
    }
    std::uint32_t softBudget(std::uint32_t hard,const config::FoliageConfig& f) const {
        return std::max(1u,std::uint32_t(hard*f.budget_fraction*scale_));
    }
    std::uint64_t revision() const {return revision_;}
    double scale() const {return scale_;}
    const GrassBudgetSignals& signals() const {return signals_;}
};
struct GrassFalloff {
    bool enabled=false,locked=false,nearInfeasible=false;
    std::uint32_t capacity=0,budget=0;
    double protectedMeters=0,sigmaMeters=0,density=0;
    nlohmann::json json() const {
        return {{"version",1},{"capacity",capacity},{"budget",budget},{"protected_m",protectedMeters},
            {"sigma_m",sigmaMeters},{"density",density},{"near_infeasible",nearInfeasible}};
    }
    static GrassFalloff replay(const nlohmann::json& j,const config::FoliageConfig& f) {
        GrassFalloff p;p.enabled=p.locked=true;
        if(j.at("version")!=1 || !j.at("capacity").is_number_integer() || !j.at("budget").is_number_integer() || !j.at("near_infeasible").is_boolean())
            throw std::invalid_argument("Invalid foliage policy replay version/types");
        if(j.at("capacity").get<double>()<1 || j.at("capacity").get<double>()>f.max_blades ||
           j.at("budget").get<double>()<1 || j.at("budget").get<double>()>j.at("capacity").get<double>())
            throw std::invalid_argument("Invalid foliage policy replay capacity");
        p.capacity=j.at("capacity").get<std::uint32_t>();p.budget=j.at("budget").get<std::uint32_t>();
        p.protectedMeters=j.at("protected_m").get<double>();p.sigmaMeters=j.at("sigma_m").get<double>();p.density=j.at("density").get<double>();p.nearInfeasible=j.at("near_infeasible").get<bool>();
        if(!p.capacity || p.capacity>std::uint32_t(f.max_blades) || !p.budget || p.budget>p.capacity ||
           !std::isfinite(p.protectedMeters) || p.protectedMeters!=f.quadDistanceMeters() ||
           !std::isfinite(p.sigmaMeters) || p.sigmaMeters<0 || p.sigmaMeters>f.draw_distance_m*f.gaussian_sigma_fraction*(1+1e-6) ||
           !std::isfinite(p.density) || p.density<0 || p.density>f.density_per_m2)
            throw std::invalid_argument("Invalid foliage policy replay limits");
        return p;
    }
};
}
