#include "india_aqi.h"
#include <cassert>
#include <cstdio>
int main(){
using namespace india_aqi;
assert(subindex(30,0)==50);assert(subindex(31,0)==51);
assert(subindex(60,0)==100);assert(subindex(61,0)==101);
assert(subindex(90,0)==200);assert(subindex(91,0)==201);
assert(subindex(120,0)==300);assert(subindex(121,0)==301);
assert(subindex(250,0)==400);assert(subindex(251,0)==401);
assert(subindex(45,0)==75);assert(subindex(50,1)==50);
assert(subindex(100,1)==100);assert(subindex(80,2)==100);
assert(subindex(380,3)==200);assert(subindex(400,4)==100);
float m[5]={45,NAN,30,20,NAN};uint8_t n[5]={24,0,24,24,0};
auto a=calculate(m,n);assert(a.value==75 && a.pollutants==3 && !a.severe);
n[2]=15;assert(!std::isfinite(calculate(m,n).value));
float gases[5]={NAN,NAN,30,20,20};uint8_t gn[5]={0,0,24,24,24};
assert(!std::isfinite(calculate(gases,gn).value));
printf("PASS: CPCB breakpoint examples, minimum coverage and particulate requirements.\n");
}
