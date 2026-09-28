"""Independent 22:05 triples at the PROJ WGS84 90deg/10kn destination."""
import hashlib
import json
import math
from pathlib import Path
import xml.etree.ElementTree as E

directory = Path(__file__).resolve().parent
source = directory / 'usno-220500-motion.json'
objects = {x['object'].lower(): x for x in json.loads(source.read_text())['properties']['data']}
moon = objects['moon']
ma, mc = moon['almanac_data'], moon['altitude_corrections']
moon_center = ma['hc'] - mc['pa'] - mc['refr']
templates = E.parse(directory / 'session-sights.xml').getroot().findall('Sight')
root = E.Element('OpenCPNCelestialNavigation', version='2.8', creator='Independent moving-observer USNO fixture')
for name, template in zip(('spica', 'vega'), templates[2:]):
    body = objects[name]
    ba, bc = body['almanac_data'], body['altitude_corrections']
    body_center = ba['hc'] - bc['pa'] - bc['refr']
    h1, h2, z1, z2 = map(math.radians, (moon_center, body_center, ma['zn'], ba['zn']))
    separation = math.degrees(math.acos(math.sin(h1)*math.sin(h2) + math.cos(h1)*math.cos(h2)*math.cos(z1-z2)))
    attrs = dict(template.attrib, Visible='0', LunarMoonAltitude=str(ma['hc']-mc['sum']), LunarBodyAltitude=str(body_center), Measurement=str(separation-mc['sd']))
    E.SubElement(root, 'Sight', attrs)
E.indent(root)
fixture = directory / 'session-motion130.xml'
E.ElementTree(root).write(fixture, encoding='utf-8', xml_declaration=True)
reference = {
    'source': 'usno-220000.json original static triples and usno-220500-motion.json extra destination triples',
    'motion_provenance': 'manifest-motion123.json: independent PROJ WGS84 destination 51.477897896095,0.022214513995 after 300s at true90deg/10kn from51.4779,0',
    'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'fixture_sha256': hashlib.sha256(fixture.read_bytes()).hexdigest(),
    'selection': 'Only original22:00 Spica/Vega and extra moving22:05 Spica/Vega; exclude original static22:05 observations',
    'expected_reference_position': [51.4779, 0],
    'COG_true_deg': 90, 'SOG_knots': 10,
    'expected_clock_seconds': 0, 'clock_tolerance_seconds': 10,
    'angular_RMS_limit_arcmin': 0.5,
    'joint_mode_limits_declared_before_joint_solve': {
        'clock_tolerance_seconds': 60,
        'reference_position_tolerance_nm': 3,
        'angular_RMS_limit_arcmin': 0.5
    },
    'scope': 'Known position at earliest distance-reading epoch; advance with session COG/SOG. Robust on, bias off. Approximate frame/refraction fixture tolerances fixed before solve.'
}
(directory / 'session-motion130-reference.json').write_text(json.dumps(reference, indent=2)+'\n')
print('Created two public moving observations; independent source and limits retained.')
