#include "geometry.h"
#include <cstdio>
int main() {
    if(!geo::init()) return 1;
    for(int i=0;i<geo::LED_COUNT;++i) std::printf("%d,%d,%d\n",i,geo::ledRow[i],geo::ledX[i]);
}
