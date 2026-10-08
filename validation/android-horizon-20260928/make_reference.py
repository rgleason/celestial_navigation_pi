"""Independent public inputs and forward spherical direction, no plugin imports."""
import hashlib
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
from scipy.optimize import least_squares

directory = Path(__file__).resolve().parent
source = directory.parent / 'android-usno-greenwich-20240621/usno-20240614-170000.json'
sun = next(row for row in json.loads(source.read_text())['properties']['data']
           if row['object'] == 'Sun')
almanac, corrections = sun['almanac_data'], sun['altitude_corrections']
declination = math.radians(almanac['dec'])
horizontal_parallax = corrections['pa'] / math.cos(math.radians(almanac['hc']))
altitude_deg = -34 / 60 - corrections['sd'] + horizontal_parallax
altitude, azimuth = math.radians(altitude_deg), math.radians(60)
target = np.array([math.cos(altitude) * math.sin(azimuth),
                   math.cos(altitude) * math.cos(azimuth), math.sin(altitude)])

def difference(position):
    latitude, longitude = map(math.radians, position)
    hour_angle = longitude + math.radians(almanac['gha'])
    east = -math.cos(declination) * math.sin(hour_angle)
    north = (math.sin(declination) * math.cos(latitude) -
             math.cos(declination) * math.sin(latitude) * math.cos(hour_angle))
    up = (math.sin(declination) * math.sin(latitude) +
          math.cos(declination) * math.cos(latitude) * math.cos(hour_angle))
    return np.array([east, north, up]) - target

positions = []
for latitude in (-70, -30, 0, 30, 70):
    for longitude in (-175, -120, 0, 120, 175):
        solution = least_squares(difference, [latitude, longitude],
                                 bounds=([-89.99, -180], [89.99, 180]),
                                 gtol=1e-13, ftol=1e-13, xtol=1e-13)
        if np.linalg.norm(solution.fun) < 1e-10 and not any(
                np.linalg.norm(solution.x - previous) < 1e-5 for previous in positions):
            positions.append(solution.x)
assert len(positions) == 2
positions.sort(key=lambda point: -point[0])
report = {
    'source_url': 'https://aa.usno.navy.mil/api/celnav?date=2024-06-14&time=17:00:00&coords=51.4779,0',
    'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'utc': '2024-06-14T17:00:00Z', 'true_bearing_deg': 60,
    'magnetic_bearing_deg': 57, 'variation_deg': 5, 'deviation_deg': -2,
    'centre_altitude_deg': altitude_deg, 'sun': sun,
    'positions': [point.tolist() for point in positions],
    'position_tolerance_nm': 1, 'method': 'Multi-start least squares of forward spherical ENU direction',
}
(directory / 'reference135.json').write_text(json.dumps(report, indent=2) + '\n')
attributes = {
    'Visible':'0', 'Type':'3', 'Body':'Sun', 'BodyLimb':'2',
    'Date':'2024-06-14', 'Time':'17:00:00', 'Milliseconds':'0',
    'TimeCertainty':'2', 'Measurement':'60', 'MeasurementCertainty':'0.5',
    'EyeHeight':'0', 'Temperature':'10', 'Pressure':'1010', 'IndexError':'0',
    'ColourName':'red', 'Colour':'rgba(255, 0, 0, 0.588)', 'Transparency':'150',
    'DRLat':str(positions[0][0]), 'DRLon':str(positions[0][1]), 'DRBoatPosition':'0',
    'HorizonEvent':'0', 'HorizonBearingProvided':'1', 'HorizonBearingMagnetic':'1',
    'HorizonBearing':'57', 'HorizonVariation':'5', 'HorizonDeviation':'-2',
    'HorizonBearingUncertainty':'2', 'HorizonAltitudeUncertainty':'10',
    'HorizonQuality':'0', 'HorizonTimeSource':'Other / manual entry',
}
root = ET.Element('OpenCPNCelestialNavigation')
ET.SubElement(root, 'Sight', attributes)
ET.indent(root)
(directory / 'fixture135.xml').write_bytes(ET.tostring(root, encoding='utf-8', xml_declaration=True))
print(json.dumps({'positions':report['positions'], 'altitude_deg':altitude_deg}, indent=2))
