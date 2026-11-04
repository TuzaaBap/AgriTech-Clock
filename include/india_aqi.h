#pragma once
#include <cmath>
#include <cstdint>

// CPCB 24-hour breakpoints: PM2.5, PM10, NO2, SO2, NH3 (ug/m3).
// Model-derived estimate from the available pollutants, not a station reading.
namespace india_aqi {
struct Estimate { float value; bool severe; uint8_t pollutants; };
inline float subindex(float mean,unsigned pollutant) {
    if(!std::isfinite(mean) || mean<0 || pollutant>=5) return NAN;
    static const float high[5][5]={
        {30,60,90,120,250},{50,100,250,350,430},
        {40,80,180,280,400},{40,80,380,800,1600},
        {200,400,800,1200,1800}
    };
    static const int indexLow[]={0,51,101,201,301};
    static const int indexHigh[]={50,100,200,300,400};
    const float concentration=std::round(mean);
    for(unsigned band=0;band<5;++band) {
        if(concentration<=high[pollutant][band]) {
            const float low=band?high[pollutant][band-1]+1:0;
            return std::round(indexLow[band]+(concentration-low)*
                (indexHigh[band]-indexLow[band])/(high[pollutant][band]-low));
        }
    }
    // The published severe band has no finite upper concentration breakpoint.
    // Show 401+ rather than invent an exact value within that open-ended band.
    return 401;
}
inline Estimate calculate(const float* means,const uint8_t* counts) {
    float worst=0;uint8_t valid=0;bool particulate=false;
    for(unsigned i=0;i<5;++i) {
        const float value=counts[i]>=16?subindex(means[i],i):NAN;
        if(std::isfinite(value)) {
            ++valid;if(i<2) particulate=true;
            if(value>worst) worst=value;
        }
    }
    if(valid<3 || !particulate) return {NAN,false,valid};
    return {worst,worst>=401,valid};
}
}
