// LF stylized lake (T01.3). Emissive = toon-lit colour; every look value is a parameter (MI_LF_Lake).
// Inputs: P world pos, N faceted normal, V camera vector, L sun dir, SunLum sun illuminance,
// WD water depth (SceneDepth - PixelDepth, cm), PD pixel depth (cm), H wave height (cm), T time (s).
#define LF_HASH(q) frac(sin(dot((q), float2(127.1, 311.7))) * 43758.5453)
const float INV_PI = 0.31830989;
float3 n = normalize(N);
float3 l = normalize(L);
float3 v = normalize(V);

// 6. Depth colour: shallow turquoise -> deep teal-blue, full deep at DeepDepth.
// WD is measured along the view ray; x|V.z| turns it into vertical water depth, so underwater parts of posts/banks
// read as deep (not as pale shallow 'reflections' hanging under them).
// Clamped: at grazing angles |V.z| -> 0 collapses the estimate and far water turns into dashed false foam.
float WDv = WD * max(abs(v.z), 0.25);
// Signed distance (cm) from this water point to each boat's real gunwale outline (RowboatPlan in LFBoat.cpp x .958/.91,
// same polygon as the hull mask; boat pos + yaw from MPC Boat0..3). Negative = inside the hull. Used for boat foam and the
// near-hull deep colour. (A capsule was used before: it is ~30 cm wider than the pointed bow/stern and its solid-foam fill
// read as a white shadow under the flecks.)
const float2 PL[15] = { float2(-151.4, -34.6), float2(-121.7, -51.9), float2(-71.8, -59.1), float2(-14.4, -61.0), float2(46.0, -57.3), float2(93.9, -41.0), float2(129.3, -18.2), float2(151.4, 0.0), float2(129.3, 18.2), float2(93.9, 41.0), float2(46.0, 57.3), float2(-14.4, 61.0), float2(-71.8, 59.1), float2(-121.7, 51.9), float2(-151.4, 34.6) };
float4 BN[4] = { B0, B1, B2, B3 };
float dP[4];
float boatNear = 0.0;
for (int k = 0; k < 4; k++)
{
    float2 r = P.xy - BN[k].xy;
    float2 l2 = float2(cos(BN[k].z) * r.x + sin(BN[k].z) * r.y, -sin(BN[k].z) * r.x + cos(BN[k].z) * r.y);
    float dmin = 1e9;
    bool inPoly = false;
    int jj = 14;
    for (int ii = 0; ii < 15; ii++)
    {
        float2 a = PL[jj];
        float2 e = PL[ii] - a;
        float2 w = l2 - a;
        float2 pr = w - e * saturate(dot(w, e) / dot(e, e));
        dmin = min(dmin, dot(pr, pr));
        if (((PL[ii].y > l2.y) != (a.y > l2.y)) && (l2.x < e.x * (l2.y - a.y) / e.y + a.x)) inPoly = !inPoly;
        jj = ii;
    }
    dP[k] = (inPoly ? -1.0 : 1.0) * sqrt(dmin);
    // Near a hull the depth difference only measures the hull, which painted a pale slab: treat as deep.
    boatNear = max(boatNear, 1.0 - smoothstep(BoatDeepMargin * 0.75, max(BoatDeepMargin, 1.0), dP[k]));
}
// Water in front of a hull (custom stencil == HullStencil, written by ALFBoat::HullMesh) ignores scene depth entirely:
// deep colour and no depth foam, so submerged hull edges never show through at any angle or distance.
float hullBehind = abs(HS.r - HullStencil) < 0.5 ? 1.0 : 0.0;
float d01 = max(max(saturate(WDv / max(DeepDepth, 1.0)), boatNear), hullBehind);
float3 albedo = lerp(ShallowColor, DeepColor, d01);

// 7. Toon light: 3 bands. Measured relative to a flat surface so the facets read at any sun height.
float s = 0.5 + (dot(n, l) - l.z) * FacetContrast;
float shade = s < Band1 ? ShadeDark : (s < Band2 ? ShadeMid : ShadeLight);
float3 light = SunLum * INV_PI * (Ambient + saturate(l.z) * shade) * Brightness;
float3 col = albedo * light;

// 6b. Fresnel lift toward a pale sky tint at grazing angles (flat up vector -> stable, no facet noise).
float fr = pow(1.0 - saturate(v.z), FresnelPower) * FresnelStrength;
col = lerp(col, SkyTint * light, saturate(fr));

// Value noise (shared by foam edge + crest break-up).
float2 fp = P.xy * FoamNoiseScale + float2(0.37, -0.23) * (T * FoamWobbleSpeed);
float2 i0 = floor(fp);
float2 f0 = frac(fp);
f0 = f0 * f0 * (3.0 - 2.0 * f0);
float noise = lerp(lerp(LF_HASH(i0), LF_HASH(i0 + float2(1, 0)), f0.x),
                   lerp(LF_HASH(i0 + float2(0, 1)), LF_HASH(i0 + float2(1, 1)), f0.x), f0.y);

// Foam fragments: fine 2-octave value noise (cell = FoamFragScale cm), drifting. Foam bands are thresholded against it so
// they break into small flecks that get sparser away from the contact edge (solid only in a thin FoamSolidCore line).
float2 fq = P.xy / max(FoamFragScale, 1.0) + float2(0.31, -0.19) * (T * FoamWobbleSpeed);
float2 fi = floor(fq);
float2 ff = frac(fq);
ff = ff * ff * (3.0 - 2.0 * ff);
float fragA = lerp(lerp(LF_HASH(fi), LF_HASH(fi + float2(1, 0)), ff.x), lerp(LF_HASH(fi + float2(0, 1)), LF_HASH(fi + float2(1, 1)), ff.x), ff.y);
fq = fq * 2.13 + 7.7;
fi = floor(fq);
ff = frac(fq);
ff = ff * ff * (3.0 - 2.0 * ff);
float fragB = lerp(lerp(LF_HASH(fi), LF_HASH(fi + float2(1, 0)), ff.x), lerp(LF_HASH(fi + float2(0, 1)), LF_HASH(fi + float2(1, 1)), ff.x), ff.y);
float frag = fragA * 0.65 + fragB * 0.35;

// 8. Contact foam: depth-fade band with a noisy, wobbling edge.
float wob = sin(T * FoamWobbleSpeed * 2.0 + noise * 6.2831853) * 0.5 + 0.5;
float edge = FoamWidth * (1.0 - FoamNoiseAmount * (noise * 0.7 + wob * 0.3));
// Clamped: at depth discontinuities (hull bottoms, shore walls, lake-bed edge) fwidth spikes and the soft threshold drew
// thin flickering lines tracing the hidden geometry.
float aaF = clamp(fwidth(WDv), 0.5, 3.0);
float thF = lerp(FoamFragDensity, 1.02, saturate(WDv / max(edge, 1.0)));
float foam = max(1.0 - smoothstep(FoamSolidCore - aaF, FoamSolidCore + aaF, WDv), smoothstep(thF - 0.03, thF + 0.03, frag) * (1.0 - smoothstep(edge - aaF, edge + aaF, WDv)) * lerp(1.0, FoamOuterAlpha, saturate(WDv / max(edge, 1.0)))) * FoamOpacity;
foam *= 1.0 - hullBehind;

// 8b. Boat contact foam + 11. wake. The hull water mask clips depth-fade foam at the hull, so boats get a world-space
// noisy ring around a capsule hull outline, from Boat0..3 in MPC_LF_Water (X, Y, yaw rad, speed cm/s; pushed by
// ALFLakeSurface). Wake = straight V of foam lines behind the stern above WakeStartSpeed (attached to the hull, no path history).
float boatFoam = 0.0;
float4 BS[4] = { B0, B1, B2, B3 };
for (int b = 0; b < 4; b++)
{
    float2 rel = P.xy - BS[b].xy;
    float cy = cos(BS[b].z);
    float sy = sin(BS[b].z);
    float2 lp = float2(cy * rel.x + sy * rel.y, -sy * rel.x + cy * rel.y);
    float sp01 = saturate(BS[b].w / max(WakeFullSpeed, 1.0));
    float dB = dP[b];
    float edgeB = BoatFoamWidth * (1.0 + sp01) * (1.0 - FoamNoiseAmount * (noise * 0.7 + wob * 0.3));
    float aaB = max(fwidth(dB), 0.5);
    float thB = lerp(FoamFragDensity, 1.02, saturate(dB / max(edgeB, 1.0)));
    // Outer flecks fade to FoamOuterAlpha so the water colour shows between and through them (not a solid white ring).
    boatFoam = max(boatFoam, max(1.0 - smoothstep(FoamSolidCore - aaB, FoamSolidCore + aaB, dB), smoothstep(thB - 0.03, thB + 0.03, frag) * (1.0 - smoothstep(edgeB - aaB, edgeB + aaB, dB)) * lerp(1.0, FoamOuterAlpha, saturate(dB / max(edgeB, 1.0)))));

    float wk = saturate((BS[b].w - WakeStartSpeed) / max(WakeFullSpeed - WakeStartSpeed, 1.0));
    float behind = -lp.x - BoatHalfLength * 0.8;
    float t = behind / max(WakeLength * wk, 1.0);
    float arm = BoatRadius * 0.6 + max(behind, 0.0) * WakeSpread;
    float dw = abs(abs(lp.y) - arm);
    float ww = WakeWidth * saturate(1.0 - t) * (0.6 + 0.8 * noise) * smoothstep(0.35, 0.45, frag);
    float aaW = max(fwidth(dw), 0.5);
    float inWake = (wk > 0.0 && behind > 0.0) ? saturate(1.0 - t) * WakeOpacity : 0.0;
    boatFoam = max(boatFoam, (1.0 - smoothstep(ww - aaW, ww + aaW, dw)) * inWake);
}
foam = max(foam, boatFoam * FoamOpacity);

// 9. Crest foam: faint white on the highest crests only.
float crest = smoothstep(CrestStart, CrestEnd, H + (noise - 0.5) * CrestNoise) * smoothstep(0.62, 0.68, frag) * CrestOpacity; // flecks, not blobs

// 10. Surface lines: two Voronoi edge layers panning in opposite directions, fading with distance.
float lines = 0.0;
for (int layer = 0; layer < 2; layer++)
{
    float sc = layer == 0 ? LineScaleA : LineScaleB;
    float2 dir = layer == 0 ? float2(0.8, 0.6) : float2(-0.6, -0.8);
    float2 uv = (P.xy + dir * (T * LineSpeed)) / max(sc, 1.0) + layer * 17.3;
    float2 ci = floor(uv);
    float2 cf = frac(uv);
    float f1 = 8.0;
    float f2 = 8.0;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float2 g = float2(x, y);
            float2 q = ci + g;
            float2 o = float2(LF_HASH(q), LF_HASH(q + 19.19));
            o = 0.5 + 0.35 * sin(T * LineWobble + 6.2831853 * o);
            float dd = length(g + o - cf);
            if (dd < f1) { f2 = f1; f1 = dd; }
            else if (dd < f2) { f2 = dd; }
        }
    }
    float e = f2 - f1;
    float aa = max(fwidth(e), 1e-4);
    lines = max(lines, 1.0 - smoothstep(LineWidth - aa, LineWidth + aa, e));
}
float fade = 1.0 - saturate((PD - LineFadeStart) / max(LineFadeEnd - LineFadeStart, 1.0));
lines *= LineOpacity * fade;

// Composite: foam over water, then the hard-edged sun highlight (7) on top (not on foam).
crest *= fade; // far crest flecks alias into dashed rows along the straight wave crests
float foamAll = saturate(max(foam, max(crest, lines)));
col = lerp(col, FoamColor * light, foamAll);
float3 h = normalize(l + v);
float sp = pow(saturate(dot(n, h)), SpecPower);
// Faded with distance like the lines: at grazing angles the hard glint aliases into dashed rows along the 1 m facet grid.
float spec = smoothstep(SpecThreshold - SpecSoftness, SpecThreshold + SpecSoftness, sp) * (1.0 - foam) * fade * smoothstep(0.55, 0.6, frag); // sparkles, not a disc
col += SpecColor * SunLum * INV_PI * (spec * SpecIntensity * Brightness);
return col;
