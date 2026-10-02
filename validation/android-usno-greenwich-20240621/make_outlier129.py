"""Independent extra Deneb triple: add a deliberate 12 arcminute LD error."""
import hashlib
import json
import math
from pathlib import Path
import xml.etree.ElementTree as E

directory = Path(__file__).resolve().parent
response = directory / 'usno-220500.json'
objects = {x['object'].lower(): x for x in json.loads(response.read_text())['properties']['data']}
moon, body = objects['moon'], objects['deneb']
ma, mc = moon['almanac_data'], moon['altitude_corrections']
ba, bc = body['almanac_data'], body['altitude_corrections']
moon_center = ma['hc'] - mc['pa'] - mc['refr']
body_center = ba['hc'] - bc['pa'] - bc['refr']
h1, h2, z1, z2 = map(math.radians, (moon_center, body_center, ma['zn'], ba['zn']))
separation = math.degrees(math.acos(math.sin(h1) * math.sin(h2) + math.cos(h1) * math.cos(h2) * math.cos(z1-z2)))
clean_ld = separation - mc['sd']
template = E.parse(directory / 'session-sights.xml').getroot().findall('Sight')[2]
attrs = dict(template.attrib, Body='Deneb', Visible='0', LunarBodyAltitude=str(body_center), Measurement=str(clean_ld + 12.0/60.0))
root = E.Element('OpenCPNCelestialNavigation', version='2.8', creator='Independent USNO disposable outlier fixture')
E.SubElement(root, 'Sight', attrs)
E.indent(root)
fixture = directory / 'session-outlier129.xml'
E.ElementTree(root).write(fixture, encoding='utf-8', xml_declaration=True)
reference = {
    'source': 'Retained primary USNO celnav response at Greenwich, 2024-06-21 22:05 UT1',
    'source_sha256': hashlib.sha256(response.read_bytes()).hexdigest(),
    'fixture_sha256': hashlib.sha256(fixture.read_bytes()).hexdigest(),
    'operation': 'Independent apparent centre separation minus Moon semidiameter, plus deliberate +12 arcmin LD error; altitude readings unchanged. Select this Deneb triple with the four original clean Spica/Vega triples.',
    'clean_distance_deg': clean_ld,
    'imposed_distance_error_arcmin': 12,
    'expected_clock_seconds': 0,
    'clock_tolerance_seconds': 10,
    'expected_deneb_outlier': True,
    'minimum_deneb_abs_distance_residual_arcmin': 6,
    'maximum_clean_abs_residual_arcmin': 1,
    'overall_RMS_limit': None,
    'scope': 'Known position, robust fit enabled, bias and motion disabled. Limits fixed before physical solve. Global RMS intentionally includes the retained outlier; it is not required below the clean-reference limit.'
}
(directory / 'session-outlier129-reference.json').write_text(json.dumps(reference, indent=2)+'\n')
print('Created only public Deneb observation and predeclared robust acceptance limits.')
