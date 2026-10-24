#pragma once
namespace DrnasTouch {
struct Point { double x, y; };
// Keep the scene point under the previous two-finger midpoint under the new
// midpoint after scaling. Translation and zoom are one camera operation.
inline Point cameraCenter(Point oldCenter, Point viewportCenter,
                          Point previousFingerCenter, Point fingerCenter,
                          double oldScale, double newScale)
{
    if (oldScale <= 0 || newScale <= 0) return oldCenter;
    return {oldCenter.x + (previousFingerCenter.x-viewportCenter.x)/oldScale
                        - (fingerCenter.x-viewportCenter.x)/newScale,
            oldCenter.y + (previousFingerCenter.y-viewportCenter.y)/oldScale
                        - (fingerCenter.y-viewportCenter.y)/newScale};
}
}
