#!/usr/bin/env python 

#  Cookie Cutter Sweeper Inkscape Extension
#
#  Copyright (c) 2015 Christian Walther
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#  THE SOFTWARE.

import sys
import os
import subprocess
import tempfile
import shutil
import platform
import optparse

if __name__ == '__main__':
	if sys.platform.startswith('darwin'):
		bindir = 'mac'
	elif sys.platform.startswith('win32'):
		bindir = 'windows'
	elif sys.platform.startswith('linux') and platform.machine() == 'x86_64':
		bindir = 'linux-x86_64'
	elif sys.platform.startswith('linux') and platform.machine() == 'i686':
		bindir = 'linux-i386'
	else:
		sys.stderr.write('Your platform "' + sys.platform + ' ' + platform.machine() + '" is not currently supported by the Cookie Cutter Sweeper integration. Try building Cookie Cutter Sweeper from source (https://github.com/cwalther/cookie-cutter-sweeper) and adjusting the Inkscape extension in ' + os.path.join(os.getcwd(), __file__) + '.')
		sys.exit(1)
	
	optionparser = optparse.OptionParser(usage='usage: %prog [options] SVGfile')
	optionparser.add_option('--id', action='append', type='string', dest='ids', default=[], help='id attribute of selected object, ignored')
	optionparser.add_option('-o', '--outputfile', action='store', type='string', dest='outputfile', default='~/cookie.stl', help='STL output file')
	options, arguments = optionparser.parse_args()
	
	tempdir = tempfile.mkdtemp()
	process = subprocess.Popen(['inkscape', '--export-type=png', '--export-filename=' + os.path.join(tempdir, 'cookie.png'), '--export-area-drawing', '--export-dpi=254', '--export-background=#000000', '--export-background-opacity=0', arguments[-1]], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = process.communicate()[0]
	if process.returncode != 0:
		# Python 2+3 compatibility
		binstderr = sys.stderr.buffer if hasattr(sys.stderr, 'buffer') else sys.stderr
		binstderr.write(b'Calling inkscape failed:\n')
		binstderr.write(output)
		shutil.rmtree(tempdir, ignore_errors=True)
		sys.exit(process.returncode)
	
	process = subprocess.Popen([os.path.join(os.getcwd(), bindir, 'sweep'), '--flip-x', os.path.join(os.getcwd(), 'section.png'), os.path.join(tempdir, 'cookie.png'), os.path.expanduser(options.outputfile)], stderr=subprocess.PIPE)
	output = process.communicate()[1]
	if process.returncode != 0:
		binstderr = sys.stderr.buffer if hasattr(sys.stderr, 'buffer') else sys.stderr
		binstderr.write(b'Calling sweep failed:\n')
		binstderr.write(output)
		shutil.rmtree(tempdir, ignore_errors=True)
		sys.exit(process.returncode)
	
	shutil.rmtree(tempdir, ignore_errors=True)
