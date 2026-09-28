import json,math,xml.etree.ElementTree as ET
from pathlib import Path
root=Path(__file__).resolve().parent
records=ET.Element('OpenCPNCelestialNavigation',version='2.8',creator='Public USNO Android acceptance fixture')
for stamp in ('220000','220500'):
 d={x['object'].lower():x for x in json.loads((root/f'usno-{stamp}.json').read_text())['properties']['data']}
 moon=d['moon']; ma=moon['almanac_data'];mc=moon['altitude_corrections']
 moon_center=ma['hc']-mc['pa']-mc['refr']
 for name in ('spica','vega'):
  b=d[name];ba=b['almanac_data'];bc=b['altitude_corrections'];body_center=ba['hc']-bc['pa']-bc['refr']
  h1,h2,z1,z2=map(math.radians,(moon_center,body_center,ma['zn'],ba['zn']))
  separation=math.degrees(math.acos(math.sin(h1)*math.sin(h2)+math.cos(h1)*math.cos(h2)*math.cos(z1-z2)))
  attrs=dict(Visible='1',Type='2',Body=name.capitalize(),BodyLimb='0',LunarMoonAltitude=str(ma['hc']-mc['sum']),LunarMoonLimb='0',LunarBodyAltitude=str(body_center),LunarBodyLimb='1',LunarBodyDistanceLimb='1',LunarMoonAltitudeUncertainty='0.5',LunarBodyAltitudeUncertainty='0.5',LunarSeparateTimes='0',LunarTimeIsWatch='0',LunarMoonTimeOffsetSeconds='0',LunarBodyTimeOffsetSeconds='0',LunarMovingObserver='0',LunarCourseTrue='0',LunarSpeedKnots='0',Date='2024-06-21',Time=f'{stamp[:2]}:{stamp[2:4]}:{stamp[4:]}',Milliseconds='0',TimeCertainty='1800',Measurement=str(separation-mc['sd']),MeasurementCertainty='0.5',EyeHeight='0',Temperature='10',Pressure='1013',IndexError='0',DipShort='0',DipShortDistance='0',ArtificialHorizon='0',ShiftNm='0',ShiftBearing='0',MagneticShiftBearing='0',ColourName='midnight blue',Colour='rgba(47, 47, 79, 0.588)',Transparency='150',DRLat='51.4779',DRLon='0',DRBoatPosition='0',DRMagneticAzimuth='0',TimeCorrection='0')
  ET.SubElement(records,'Sight',attrs)
ET.indent(records)
ET.ElementTree(records).write(root/'session-sights.xml',encoding='utf-8',xml_declaration=True)
print('Created four public fixture records; no private original records included.')
