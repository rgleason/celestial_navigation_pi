import json,math,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2];points=Path(sys.argv[1])
data=json.loads((root/'validation/android-usno-greenwich-20240621/usno-220000.json').read_text())
spica=next(x for x in data['properties']['data'] if x['object'].casefold()=='spica')['almanac_data']
lat0=math.radians(spica['dec']);lon0=math.radians(-spica['gha']);bearing=220.63953512345679;limit=.01
rows=[]
for line in points.read_text().splitlines():
 if not line.startswith('POINT '):continue
 lat,lon=map(float,line.split()[1:]);phi=math.radians(lat);lam=math.radians(lon)
 zn=math.degrees(math.atan2(math.sin(lon0-lam)*math.cos(lat0),math.cos(phi)*math.sin(lat0)-math.sin(phi)*math.cos(lat0)*math.cos(lon0-lam)))%360
 err=abs(math.remainder(zn-bearing,360))
 rows.append({'latitude':lat,'longitude':lon,'independent_bearing':zn,'error_deg':err})
assert len(rows)>75
maximum=max(x['error_deg'] for x in rows)
print('points',len(rows),'maximum bearing error',maximum,'limit',limit)
out={'source':'8bc86ec shared Sight geometry','reference':'archived USNO Greenwich2024-06-21T22:00:00Z Spica GHA/Dec with independent spherical initial bearing atan2','reference_spica':spica,'bearing_true_deg':bearing,'uncertainty_arcmin':1,'motion_nm':0,'time_uncertainty_seconds':0,'limit_deg':limit,'maximum_error_deg':maximum,'points':rows,'status':'PASS' if maximum<limit else 'FAIL','scope':'Numerical shared LOP centre geometry; tablet chart binding is separate. No pixel/projection accuracy claim.'}
(root/'validation/android-final-acceptance-20260928/azimuth-geometry165.json').write_text(json.dumps(out,indent=2)+'\n')
assert maximum<limit
