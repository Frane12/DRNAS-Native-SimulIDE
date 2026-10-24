#include "touch_camera_math.h"
#include <cassert>
#include <cmath>
#include <iostream>
using DrnasTouch::Point;
static void equal(Point a, Point b) {
    assert(std::abs(a.x-b.x) < 1e-8 && std::abs(a.y-b.y) < 1e-8);
}
int main() {
    const Point c{100,200}, v{200,150}, p{250,180}, q{300,210};
    equal(DrnasTouch::cameraCenter(c,v,p,q,2,2), {75,185});
    equal(DrnasTouch::cameraCenter(c,v,p,p,2,2), c);
    for (double oldScale : {0.25,1.0,2.0,8.0}) {
        for (double newScale : {0.25,1.0,3.0,8.0}) {
            const Point moved=DrnasTouch::cameraCenter(c,v,p,q,oldScale,newScale);
            const Point anchor{c.x+(p.x-v.x)/oldScale,c.y+(p.y-v.y)/oldScale};
            equal({v.x+(anchor.x-moved.x)*newScale,v.y+(anchor.y-moved.y)*newScale},q);
            equal(DrnasTouch::cameraCenter(moved,v,q,p,newScale,oldScale),c);
        }
    }
    equal(DrnasTouch::cameraCenter(c,v,p,q,0,1),c);
    std::cout << "PASS: two-finger pan, zoom anchor, combined gestures and inverse camera moves\n";
}
