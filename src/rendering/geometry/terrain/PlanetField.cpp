#include "rendering/geometry/terrain/PlanetField.h"
#include "rendering/geometry/terrain/TerrainQueryCache.h"
#include "rendering/geometry/terrain/TerrainTopology.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
namespace rendering {
namespace { int seedOffset(int seed,unsigned offset) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(seed)+offset);
}
double policySink(const PlanetField& field,const glm::dvec3& radial,const TerrainSurfacePolicy& policy) {
    if(!policy.enabled()) return 0;
    const auto profile=policy.profile(radial);
    const double curvature=policy.curvatureMeters(profile);
    if(profile.x==policy.spacing[7] && profile.y==policy.spacing[7]) return 0;
    // Match the topology's stored float sink, including its safety/config cap.
    return static_cast<float>(std::min(policy.distances[3],field.omittedReliefMeters(profile)+curvature));
}
}
double PlanetField::heightAt(const glm::dvec3& radial,const TerrainSurfacePolicy& policy) const {
    const double length = glm::length(radial);
    if (!std::isfinite(length) || length <= 0.0) {
        throw std::invalid_argument("Terrain height needs a finite radial direction");
    }
    const glm::dvec3 direction = radial / length;
    return heightMeters(direction, policy.profile(direction)) / metersPerUnit_;
}

double PlanetField::regionPlainWeight(const glm::dvec3& radial) const {
    if (!landscape_.enabled) return 0.0;
    const double sample = valueNoise(glm::normalize(radial) * 2.7, seedOffset(landscape_.seed, 1));
    return 1.0 - smoothstep(landscape_.plain_threshold - 0.06,
                            landscape_.plain_threshold + 0.06, sample);
}

double PlanetField::regionCliffWeight(const glm::dvec3& radial) const {
    if (!landscape_.enabled) return 0.0;
    const double sample = valueNoise(glm::normalize(radial) * 2.4, seedOffset(landscape_.seed, 2));
    return smoothstep(landscape_.cliff_threshold - 0.06,
                      landscape_.cliff_threshold + 0.06, sample);
}

glm::dvec3 PlanetField::landscapeColorFactors(double heightMeters, double slope,
                                         double waterLevelMeters,
                                         double beachWidthMeters,
                                         double maximumHeightMeters,
                                         const config::PlanetConfig::TerrainMaterial& material) {
    const glm::dvec3 seabed(0.30, 0.40, 0.19);
    const glm::dvec3 beach(4.20, 1.90, 0.18);
    const glm::dvec3 grass(1.10, 1.30, 0.18);
    const glm::dvec3 snow(4.60, 2.30, 0.92);
    const double beachTop = waterLevelMeters + beachWidthMeters;
    const double beachFade = std::max(0.15, 0.15 * beachWidthMeters);
    const double usableRelief = std::max(1.0, maximumHeightMeters - waterLevelMeters);
    const double snowStart = std::max(beachTop + 1.0,
                                      waterLevelMeters + 0.25 * usableRelief);
    const double snowEnd = std::max(snowStart + 1.0,
                                    waterLevelMeters + 0.40 * usableRelief);

    const double aboveWater = smoothstep(
        waterLevelMeters - std::max(0.05, 0.05 * beachWidthMeters),
        waterLevelMeters + std::max(0.02, 0.02 * beachWidthMeters),
        heightMeters);
    const double beachWeight = aboveWater *
        (1.0 - smoothstep(beachTop, beachTop + beachFade, heightMeters));
    const double snowWeight = smoothstep(snowStart, snowEnd, heightMeters);
    glm::dvec3 land = glm::mix(grass, beach, beachWeight);
    land = glm::mix(land, snow, snowWeight);

    // Convert rise/run to the shader's 1-cos(angle) measure, keeping the
    // broad color darkening and fine gray-rock blend on the same range.
    const auto range = material.slopeMetricRange();
    const double steep = smoothstep(range[0], range[1],
        1.0 - 1.0 / std::sqrt(1.0 + slope * slope));
    land *= glm::mix(1.0, 0.50, steep);

    const double submerged = 1.0 - smoothstep(
        waterLevelMeters - std::max(0.5, 0.05 * beachWidthMeters),
        waterLevelMeters + std::max(0.02, 0.02 * beachWidthMeters),
        heightMeters);
    return glm::mix(land, seabed, submerged);
}

double PlanetField::smoothstep(double low, double high, double value) {
    const double t = std::clamp((value - low) / (high - low), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double PlanetField::heightMeters(const glm::dvec3& direction,const glm::dvec3& profile) const {
    const auto resolved=[&](double f) { return TerrainSurfacePolicy::weight(f,radius_*metersPerUnit_,profile); };
    double height = 0.0;
    double detailMask = 1.0;
    if (landscape_.enabled) {
        const double continent = 2.0 * valueNoise(
            direction * landscape_.continent_frequency, landscape_.seed) - 1.0;
        const double plain = regionPlainWeight(direction);
        const double cliff = regionCliffWeight(direction);
        height = landscape_.elevation_offset_m +
                 landscape_.continent_amplitude_m * continent * (1.0 - 0.8 * plain) * resolved(landscape_.continent_frequency);
        const double ridgeSignal = 2.0 * valueNoise(
            direction * landscape_.cliff_frequency, seedOffset(landscape_.seed, 3)) - 1.0;
        double ridgeDistance = std::abs(ridgeSignal);
        if (landscape_.ridge_smoothing > 0.0) {
            const double smoothing = landscape_.ridge_smoothing;
            const double scale = std::sqrt(1.0 + smoothing * smoothing) - smoothing;
            ridgeDistance = (std::sqrt(ridgeSignal * ridgeSignal +
                                       smoothing * smoothing) - smoothing) / scale;
        }
        const double ridge = 1.0 - std::clamp(ridgeDistance, 0.0, 1.0);
        height += landscape_.cliff_amplitude_m * cliff * std::pow(ridge, 5.0) * resolved(landscape_.cliff_frequency);
        detailMask = 1.0 - 0.9 * plain;
    }
    for (const auto& function : functions_) {
        if (function.amplitude_m == 0.0) continue;
        double frequency = frequencyFor(function);
        double weight = 1.0;
        double full = 0.0;
        double weightSum = 0.0;
        double broad = 0.0;
        for (int octave = 0; octave < function.octaves; ++octave) {
            const double retained=resolved(frequency);
            const double signedValue = retained>0 ?
                2.0 * valueNoise(direction * frequency, function.seed) - 1.0 : 0.0;
            const double sample = (function.type == "ridged_fbm" ?
                1.0 - 2.0 * std::abs(signedValue) : signedValue) * retained;
            if (octave == 0) broad = sample;
            full += weight * sample;
            weightSum += weight;
            frequency *= function.lacunarity;
            weight *= function.persistence;
        }
        const double mixed = broad + (full / weightSum - broad);
        height += detailMask * function.amplitude_m * mixed;
    }
    return height;
}

double PlanetField::omittedReliefMeters(const glm::dvec3& profile) const {
    const auto omitted=[&](double f) { return 1-TerrainSurfacePolicy::weight(f,radius_*metersPerUnit_,profile); };
    double bound=landscape_.enabled ? landscape_.continent_amplitude_m*omitted(landscape_.continent_frequency)+
        landscape_.cliff_amplitude_m*omitted(landscape_.cliff_frequency) : 0;
    for(const auto& n:functions_) {
        double frequency=frequencyFor(n),weight=1,total=0,missing=0;
        for(int octave=0;octave<n.octaves;++octave) {
            total+=weight;missing+=weight*omitted(frequency);
            frequency*=n.lacunarity;weight*=n.persistence;
        }
        bound+=n.amplitude_m*missing/total;
    }
    return bound;
}

std::uint32_t PlanetField::hash(int x, int y, int z, int seed) {
    std::uint32_t h = static_cast<std::uint32_t>(seed);
    h ^= static_cast<std::uint32_t>(x) * 0x9e3779b1u;
    h ^= static_cast<std::uint32_t>(y) * 0x85ebca77u;
    h ^= static_cast<std::uint32_t>(z) * 0xc2b2ae3du;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

double PlanetField::valueNoise(const glm::dvec3& point, int seed) const {
    const int ix = static_cast<int>(std::floor(point.x));
    const int iy = static_cast<int>(std::floor(point.y));
    const int iz = static_cast<int>(std::floor(point.z));
    auto smooth = [](double f) { return f * f * (3.0 - 2.0 * f); };
    const glm::dvec3 f(smooth(point.x - ix), smooth(point.y - iy),
                       smooth(point.z - iz));
    double result = 0.0;
    for (int z = 0; z <= 1; ++z) {
        for (int y = 0; y <= 1; ++y) {
            for (int x = 0; x <= 1; ++x) {
                const double value = hash(ix + x, iy + y, iz + z,
                                          seed) / 4294967295.0;
                result += value * (x ? f.x : 1.0 - f.x) *
                                  (y ? f.y : 1.0 - f.y) *
                                  (z ? f.z : 1.0 - f.z);
            }
        }
    }
    return result;
}

PlanetFieldGradient PlanetField::gradientAt(const glm::dvec3& radial, double heightMeters, TerrainQueryCache* queries,const TerrainSurfacePolicy& policy) const {
    const glm::dvec3 reference = std::abs(radial.z) < 0.8 ?
        glm::dvec3(0.0, 0.0, 1.0) : glm::dvec3(0.0, 1.0, 0.0);
    const glm::dvec3 tangentA = glm::normalize(glm::cross(reference, radial));
    const glm::dvec3 tangentB = glm::normalize(glm::cross(radial, tangentA));
    const double distanceMeters = parameters_.gradient[2];
    const glm::dvec3 sampleA = glm::normalize(
        parameters_.gradient[1] * radial + parameters_.gradient[0] * tangentA);
    const glm::dvec3 sampleB = glm::normalize(
        parameters_.gradient[1] * radial + parameters_.gradient[0] * tangentB);
    const double surfaceHeight=heightMeters-policySink(*this,radial,policy);
    const double gradientA =
        ((queries ? queries->heightAt(sampleA) : heightAt(sampleA,policy)) * metersPerUnit_ -
         policySink(*this,sampleA,policy)-surfaceHeight) / distanceMeters;
    const double gradientB =
        ((queries ? queries->heightAt(sampleB) : heightAt(sampleB,policy)) * metersPerUnit_ -
         policySink(*this,sampleB,policy)-surfaceHeight) / distanceMeters;
    return {std::hypot(gradientA, gradientB),
            glm::normalize(radial - gradientA * tangentA - gradientB * tangentB)};
}

glm::dvec3 PlanetField::colorAt(double heightWorld, double slope) const {
    if (!landscape_.enabled) {
        const double normalizedHeight = totalAmplitudeMeters_ == 0.0 ? 0.0 :
            heightWorld / (totalAmplitudeMeters_ / metersPerUnit_);
        const double tint = 0.72 + 0.45 * normalizedHeight;
        return glm::dvec3(tint);
    }
    const double heightMeters = heightWorld * metersPerUnit_;
    return landscapeColorFactors(heightMeters, slope,
        waterLevelMeters_.value_or(0.0), 0.1,
        totalAmplitudeMeters_,
        material_);
}

PlanetField::PlanetField(const std::vector<config::PlanetConfig::SurfaceNoiseFunction>& functions,
                              double radiusWorld, double metersPerWorldUnit,
               const config::PlanetConfig::TerrainLandscape& landscape,
               std::optional<double> waterLevelMeters,
               const config::PlanetConfig::TerrainMaterial& material)
    : functions_(functions), landscape_(landscape), radius_(radiusWorld),
      metersPerUnit_(metersPerWorldUnit), waterLevelMeters_(waterLevelMeters), material_(material) {
    landscape_.validate();
    material_.validate();
    for (const auto& function : functions_) {
        function.validate();
        totalAmplitudeMeters_ += function.amplitude_m;
    }
    totalAmplitudeMeters_ += landscape_.maximumAbsoluteHeightMeters();
    if (!std::isfinite(radius_) || radius_ <= 0.0 ||
        !std::isfinite(metersPerUnit_) || metersPerUnit_ <= 0.0 ||
        !std::isfinite(totalAmplitudeMeters_) ||
        totalAmplitudeMeters_ >= radius_ * metersPerUnit_ ||
        functions_.size() > 8) {
        throw std::invalid_argument("Invalid terrain radius or amplitude");
    }
    if (waterLevelMeters_ && !std::isfinite(*waterLevelMeters_))
        throw std::invalid_argument("Invalid terrain water level");
    parameters_.scale={radius_,metersPerUnit_,totalAmplitudeMeters_,waterLevelMeters_.value_or(0)};
    parameters_.landscapeShape={landscape_.elevation_offset_m,landscape_.continent_amplitude_m,
        landscape_.continent_frequency,landscape_.cliff_amplitude_m};
    parameters_.landscapeMask={landscape_.plain_threshold,landscape_.cliff_threshold,
        landscape_.cliff_frequency,landscape_.ridge_smoothing};
    parameters_.material={material_.rock_start_degrees,material_.rock_end_degrees,.1,0};
    double step=.25;bool physicalNoise=false;
    for(const auto& n:functions_) {
        const double finest=frequencyFor(n)*std::pow(n.lacunarity,n.octaves-1);
        if(!std::isfinite(finest) || finest>1e9)
            throw std::invalid_argument("Terrain noise wavelength exceeds the supported lattice precision");
        if(n.wavelength_m>0 && n.amplitude_m>0) {
            physicalNoise=true;step=std::min(step,radius_*metersPerUnit_/finest*.05);
        }
    }
    const double angle=std::clamp(step/(radius_*metersPerUnit_),physicalNoise?1e-10:1e-5,.01);
    parameters_.gradient={std::sin(angle),std::cos(angle),radius_*metersPerUnit_*angle,0};
    parameters_.flags={version,static_cast<std::uint32_t>(functions_.size()),
        static_cast<std::uint32_t>(landscape_.enabled),static_cast<std::uint32_t>(waterLevelMeters_.has_value())};
    parameters_.seeds[0]=static_cast<std::uint32_t>(landscape_.seed);
    for(std::size_t i=0;i<functions_.size();++i) {
        const auto& n=functions_[i];auto& out=parameters_.noise[i];
        out.amplitudeMeters=n.amplitude_m;out.frequency=frequencyFor(n);
        out.persistence=n.persistence;out.lacunarity=n.lacunarity;
        out.type=n.type=="ridged_fbm" ? 1 : 0;out.octaves=n.octaves;out.seed=static_cast<std::uint32_t>(n.seed);
    }
    // Hash semantic words in explicit little-endian order, excluding padding.
    auto hash=14695981039346656037ull;
    for(const auto* values:{&parameters_.scale,&parameters_.landscapeShape,&parameters_.landscapeMask,
                           &parameters_.material,&parameters_.gradient})
        for(double v:*values) hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(v));
    for(auto v:parameters_.flags) hash=terrainHashWord(hash,v,4);
    for(auto v:parameters_.seeds) hash=terrainHashWord(hash,v,4);
    for(const auto& n:parameters_.noise) {
        for(double v:{n.amplitudeMeters,n.frequency,n.persistence,n.lacunarity})
            hash=terrainHashWord(hash,std::bit_cast<std::uint64_t>(v));
        for(auto v:{n.type,n.octaves,n.seed}) hash=terrainHashWord(hash,v,4);
    }
    fingerprint_=hash;
}

PlanetFieldSample PlanetField::sample(const glm::dvec3& radial,double heightWorld,
                                     TerrainQueryCache* queries,const TerrainSurfacePolicy& policy) const {
    const double length2=glm::dot(radial,radial);
    if(!std::isfinite(length2) || std::abs(length2-1)>1e-12 || !std::isfinite(heightWorld) ||
       (queries && &queries->field()!=this)) throw std::invalid_argument("Invalid planet field sample contract");
    const auto gradient=gradientAt(radial,heightWorld*metersPerUnit_,queries,policy);
    return {radial*(1.0+heightWorld/radius_),gradient.normal,colorAt(heightWorld,gradient.slope)};
}
} // namespace rendering
