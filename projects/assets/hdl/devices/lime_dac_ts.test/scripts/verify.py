#!/usr/bin/env python
# This file is protected by Copyright. Please refer to the COPYRIGHT file
# distributed with this source distribution.
#
# This file is part of OpenCPI <http://www.opencpi.org>
#
# OpenCPI is free software: you can redistribute it and/or modify it under the
# terms of the GNU Lesser General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# OpenCPI is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License along
# with this program. If not, see <http://www.gnu.org/licenses/>.


import sys
import numpy as np

class color:
    PURPLE = '\033[95m'
    CYAN = '\033[96m'
    DARKCYAN = '\033[36m'
    BLUE = '\033[94m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    END = '\033[0m'

if len(sys.argv) < 2:
    print("Exit: Must enter the captured binary UUT output file")
    sys.exit(1)
elif len(sys.argv) < 3:
    print("Exit: Must enter the expected data binary file")
    sys.exit(1)

ifile = open(sys.argv[2], 'rb')
ofile = open(sys.argv[1], 'rb')

expected_data = np.fromfile(ifile, dtype=np.uint32, count=-1)
captured_data = np.fromfile(ofile, dtype=np.uint32, count=-1)

ifile.close()
ofile.close()

if len(expected_data) != len(captured_data):
    print color.RED + color.BOLD + 'File sizes do not match. Expected Data is ' + str(len(expected_data)) + ' samples while Captured Data is ' + str(len(captured_data)) + ' samples.' + color.END
    sys.exit(1)

for i in xrange(0, len(expected_data)):
    if expected_data[i] != captured_data[i]:
        print color.RED + color.BOLD + 'FAILED at sample:', i, 'with expected value:', format(expected_data[i], '#X'), 'and captured value:', format(captured_data[i], '#X') + color.END
        sys.exit(1)

print 'Data matched expected results.'
print color.GREEN + color.BOLD + 'PASSED' + color.END
print '*** End validation ***\n'