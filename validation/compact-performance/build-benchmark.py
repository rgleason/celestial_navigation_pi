#!/usr/bin/env python3
"""Link the planner benchmark against an already-built Ninja test tree.
Usage: build-benchmark.py SOURCE BUILD OUTPUT
Requires the celestial_tests target to have been built in BUILD.
"""
import shlex
import subprocess
import sys
from pathlib import Path

source, build, output = (Path(a).resolve() for a in sys.argv[1:])
commands = subprocess.check_output(
    ['ninja', '-t', 'commands', 'test/celestial_tests'], cwd=build, text=True).splitlines()
compile = next(shlex.split(line) for line in commands
               if 'NavigationAlgorithms.cpp.o' in line and ' -c ' in line)
compile[compile.index('-o') + 1] = str(output) + '.o'
compile[compile.index('-c') + 1] = str(Path(__file__).with_name('benchmark.cpp').resolve())
compile.insert(1, '-DISSUE365_SOURCE="' + str(source) + '"')
if (source / 'src/CompactEphemerisProvider.h').is_file():
    compile.insert(1, '-DISSUE365_COMPACT')
subprocess.run(compile, cwd=build, check=True)
link = shlex.split(commands[-1])
link = link[link.index('/usr/bin/c++'):]
if link[-2:] == ['&&', ':']:
    link = link[:-2]
link[link.index('-o') + 1] = str(output)
link.insert(1, str(output) + '.o')
subprocess.run(link, cwd=build, check=True)
print(output)
