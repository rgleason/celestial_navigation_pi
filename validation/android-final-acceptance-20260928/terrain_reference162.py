#!/usr/bin/env python3
"""Independent reader/contact check: CSPICE, ERFA and a full-grid support scan.
No CelNav libraries/imports; conventions match the documented eclipse model.
Developer dependencies: spiceypy 8.2.0, pyerfa 2.0.1.5, numpy, scipy.
"""
import json, math, struct, sys
from pathlib import Path
import numpy as np
import erfa
import spiceypy as sp
from scipy.optimize import brentq

folder, frame, output = map(Path, sys.argv[1:4])
for filename in ('de440s.bsp', 'moon_pa_de440_200625.bpc'):
    sp.furnsh(str(folder / filename))
sp.furnsh(str(frame))
lat, lon, delta_t = 25.505, 33.18333333333333, 76.06
origin = sum(erfa.cal2jd(2027, 8, 2))
fixed = erfa.gd2gc(1, math.radians(lon), math.radians(lat), 0.0) / 1000

def state(seconds):
    ut2 = origin - 2451545.0 + seconds / 86400
    tt2 = ut2 + delta_t / 86400
    tdb = erfa.dtdb(2451545.0, tt2, (seconds / 86400) % 1, 0, 0, 0)
    et = tt2 * 86400 + tdb
    rotate = erfa.c2t06a(2451545.0, tt2, 2451545.0, ut2, 0, 0)
    observer = rotate.T @ fixed
    sun = sp.spkpos('SUN', et, 'J2000', 'CN', 'EARTH')[0]
    moon = sp.spkpos('MOON', et, 'J2000', 'CN', 'EARTH')[0]
    return et, observer, np.array(sun), np.array(moon)

def contact(seconds, radius, internal):
    _, observer, sun, moon = state(seconds)
    s, m = sun - observer, moon - observer
    sn, mn = np.linalg.norm(s), np.linalg.norm(m)
    # atan2 of cross/dot avoids small-separation arccos cancellation.
    separation = math.atan2(np.linalg.norm(np.cross(s, m)), np.dot(s, m))
    sun_sd, moon_sd = math.asin(696000 / sn), math.asin(radius / mn)
    edge = abs(moon_sd - sun_sd) if internal else moon_sd + sun_sd
    return separation - edge

pack = folder / 'lola64-pa.bin'
with pack.open('rb') as f:
    magic, width, height, radius0 = struct.unpack('<8sIId', f.read(24))
assert magic == b'CLRO64\r\n' and width == height * 2
assert pack.stat().st_size == 24 + 2 * width * height
grid = np.memmap(pack, dtype='<i2', mode='r', offset=24, shape=(height, width))
longitudes = 2 * np.pi * (np.arange(width) + .5) / width
coslon, sinlon = np.cos(longitudes), np.sin(longitudes)

def independent_support(direction):
    # Scan every valid cell, rather than using the engine's local +/-2deg window.
    best = -np.inf
    best_cell = None
    for start in range(0, height, 128):
        stop = min(start + 128, height)
        latitudes = np.pi/2 - np.pi * (np.arange(start, stop) + .5) / height
        dot = np.cos(latitudes)[:, None] * (direction[0]*coslon + direction[1]*sinlon)[None, :] + np.sin(latitudes)[:, None] * direction[2]
        cells = grid[start:stop]
        support = (radius0 + cells.astype(float)/1000) * dot
        support[cells == -32768] = -np.inf
        value = float(support.max())
        if value > best:
            best = value
            rr, cc = np.unravel_index(support.argmax(), support.shape)
            best_cell = [int(start+rr), int(cc)]
    return best, best_cell

rows = []
for name, bracket, internal in [('C1', (8*3600, 9*3600), False), ('C2', (10*3600, 10*3600+5*60), True), ('C3', (10*3600+8*60, 10*3600+15*60), True), ('C4', (11*3600, 12*3600), False)]:
    baseline_radius = (0.272281 if internal else 0.272488) * 6378.137
    standard = brentq(lambda t: contact(t, baseline_radius, internal), *bracket, xtol=1e-7)
    et, observer, sun, moon = state(standard)
    matrix = sp.pxform('J2000', 'MOON_PA_DE440', et)
    view = matrix @ (observer - moon); view /= np.linalg.norm(view)
    solar = matrix @ (sun - moon)
    direction = solar - view * np.dot(solar, view); direction /= np.linalg.norm(direction)
    terrain_radius, best_cell = independent_support(direction)
    centre_row = int(np.floor((np.pi/2-np.arcsin(direction[2]))*height/np.pi))
    centre_col = int(np.floor((np.arctan2(direction[1],direction[0])%(2*np.pi))*width/(2*np.pi)))
    rr = np.arange(max(0,centre_row-128), min(height,centre_row+129))
    cc = np.arange(centre_col-128,centre_col+129)%width
    ll = np.pi/2-np.pi*(rr+.5)/height
    localdot = np.cos(ll)[:,None]*(direction[0]*coslon[cc]+direction[1]*sinlon[cc])[None,:]+np.sin(ll)[:,None]*direction[2]
    localcells = grid[np.ix_(rr,cc)]
    localsupport = (radius0+localcells.astype(float)/1000)*localdot
    localsupport[localcells == -32768] = -np.inf
    local_radius = float(localsupport.max())
    local_contact = brentq(lambda t: contact(t, local_radius, internal),standard-180,standard+180,xtol=1e-7)
    terrain = brentq(lambda t: contact(t, terrain_radius, internal), standard-180, standard+180, xtol=1e-7)
    row = {'contact': name, 'standard_UT1_seconds': standard, 'terrain_support_radius_km': terrain_radius, 'terrain_UT1_seconds': terrain, 'shift_seconds': terrain-standard, 'direction_body':direction.tolist(), 'global_support_cell':best_cell, 'local_2degree_support_radius_km':local_radius, 'local_2degree_contact_UT1_seconds':local_contact, 'local_2degree_shift_seconds':local_contact-standard}
    rows.append(row); print(json.dumps(row), flush=True)
output.write_text(json.dumps({'method':'Independent CSPICE CN vectors and official lunar PA transform, pyERFA WGS84/Earth orientation, scipy root finding, numpy full-grid support scan; no DUT routines', 'scope':'Agreement under the documented geometric contact model, not atmospheric/observational accuracy', 'reference_versions':{'spiceypy':sp.__version__, 'cspice':sp.tkvrsn('TOOLKIT'), 'pyerfa':erfa.__version__},'date':'2027-08-02','position':[lat,lon],'DeltaT_seconds':delta_t,'height_metres':0,'contact_limit_seconds':0.1,'rows':rows},indent=2)+'\n')
