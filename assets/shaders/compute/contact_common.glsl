// Vecinos de contacto: celdas 3×3 alrededor del punto (con el mundo envuelto).
uint cellIndex(int cx, int cy) {
    int w = int(uGridW), h = int(uGridH);
    cx = ((cx % w) + w) % w;
    cy = ((cy % h) + h) % h;
    return uint(cy * w + cx);
}

uint ringDistance(uint a, uint b, uint size) {
    uint d = a > b ? a - b : b - a;
    return min(d, size - d);
}

bool memberActive(uint m) {
    Member me = members[m];
    return (me.flags & 2u) != 0u && solving(bodies[me.body].lod);
}

// Corrección de contacto del punto m contra las aristas vecinas. `probe`: en vez de corregir,
// solo decide si hay alguna arista ajena al alcance (más una holgura).
// Los puntos de una celda llegan en un orden que cambia entre corridas (atomicAdd): la suma se
// hace en punto fijo entero, que no depende del orden. Paso 2^-22 µm (~2e-7 µm), muy por debajo
// de cualquier escala física; rango ±512 µm, y una corrección no pasa de unos µm.
const float kContactFix = 4194304.0;
vec2 contactTerm(uint m, bool probe, out bool near) {
    near = false;
    Member me = members[m];
    vec4 pk = pos[me.slot];
    vec2 wk = vec2(wrapCoord(pk.x, uWorld.x), wrapCoord(pk.y, uWorld.y));
    int cx = int(memberCell[m] % uGridW), cy = int(memberCell[m] / uGridW);
    ivec2 acc = ivec2(0);
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            uint cell = cellIndex(cx + ox, cy + oy);
            uint count = cellCount[uParity * uNumCells + cell];
            uint start = gridData[cell];
            for (uint it = 0u; it < count; ++it) {
                uint am = cellSlot[start + it];
                Member a = members[am];
                Member bm = members[a.next];
                if (probe) {
                    if (a.owner == me.owner || a.group == me.group)
                        continue;
                } else {
                    if (a.owner != me.owner && a.group == me.group)
                        continue;
                    if (a.owner == me.owner &&
                        min(ringDistance(me.ringPos, a.ringPos, me.ringSize), ringDistance(me.ringPos, bm.ringPos, me.ringSize)) <= 3u)
                        continue;
                }
                vec4 pa = pos[a.slot], pb = pos[bm.slot];
                float ax0 = minImage(pa.x - wk.x, uWorld.x), ay0 = minImage(pa.y - wk.y, uWorld.y);
                float ex = minImage(pb.x - pa.x, uWorld.x), ey = minImage(pb.y - pa.y, uWorld.y);
                float len2 = ex * ex + ey * ey;
                float t = len2 > 0.0 ? clamp(-(ax0 * ex + ay0 * ey) / len2, 0.0, 1.0) : 0.0;
                float dx = -(ax0 + ex * t), dy = -(ay0 + ey * t);
                float d2 = dx * dx + dy * dy;
                if (probe) {
                    float reach = pk.z + pa.z + uSlack;
                    if (d2 < reach * reach) {
                        near = true;
                        return vec2(0.0);
                    }
                    continue;
                }
                float minD = pk.z + pa.z;
                if (d2 >= minD * minD || d2 < 1e-12)
                    continue;
                float d = sqrt(d2);
                float wkm = pk.w, wm = 0.5 * (pa.w + pb.w);
                float share = wkm + wm > 0.0 ? wkm / (wkm + wm) : 0.5;
                float push = share * (minD - d) / d;
                acc += ivec2(round(vec2(dx, dy) * (push * kContactFix)));
            }
        }
    }
    return vec2(acc) / kContactFix;
}
