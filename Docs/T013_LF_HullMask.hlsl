// LF hull water mask (T01.2, revised T01.3 at the director's request): clip lake pixels behind a boat's WaterMask cap
// (custom stencil == S) ONLY where the water surface point lies inside a hull's gunwale outline in world XY (boat pos + yaw
// from MPC Boat0..3). Water outside the hull is kept, so it hides the submerged hull instead of exposing it.
// Outline = RowboatPlan in LFBoat.cpp (x*.958, y*.91, cm), same as the WaterMask cap. Ignores hull pitch/roll.
bool inside = abs(ST.r - S) < 0.5;
bool behind = PD > CD.r + 2.0;
if (!(inside && behind)) return 1.0;
const float2 PL[15] = { float2(-151.4, -34.6), float2(-121.7, -51.9), float2(-71.8, -59.1), float2(-14.4, -61.0), float2(46.0, -57.3), float2(93.9, -41.0), float2(129.3, -18.2), float2(151.4, 0.0), float2(129.3, 18.2), float2(93.9, 41.0), float2(46.0, 57.3), float2(-14.4, 61.0), float2(-71.8, 59.1), float2(-121.7, 51.9), float2(-151.4, 34.6) };
float4 BB[4] = { B0, B1, B2, B3 };
for (int b = 0; b < 4; b++)
{
    float2 r = P.xy - BB[b].xy;
    float c = cos(BB[b].z);
    float s = sin(BB[b].z);
    float2 lp = float2(c * r.x + s * r.y, -s * r.x + c * r.y);
    bool inPoly = false;
    int j = 14;
    for (int i = 0; i < 15; i++)
    {
        if (((PL[i].y > lp.y) != (PL[j].y > lp.y)) && (lp.x < (PL[j].x - PL[i].x) * (lp.y - PL[i].y) / (PL[j].y - PL[i].y) + PL[i].x))
            inPoly = !inPoly;
        j = i;
    }
    if (inPoly) return 0.0;
}
return 1.0;
