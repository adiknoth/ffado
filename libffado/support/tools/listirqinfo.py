#!/usr/bin/python
#

#
# Copyright (C) 2008 Pieter Palmers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import os
import commands
import re

VERSION="0.2"

class IRQ:
	def __init__(self):
		self.number = None
		self.scheduling_class = None
		self.scheduling_priority = None
		self.process_id = None
		self.drivers = []
		self.cpu_counts = []
	def __str__(self):
		s = " IRQ %4s: PID: %5s, count: %18s, Sched %4s (priority %4s), drivers: %s" % \
		    (self.number, self.process_id, self.cpu_counts,
		     self.scheduling_class, self.scheduling_priority,
		     self.drivers)
		return s

class SoftIRQ:
	def __init__(self):
		self.name = None
		self.fullname = None
		self.scheduling_class = None
		self.scheduling_priority = None
		self.process_id = None
		self.cpu_counts = []
	def __str__(self):
		s = " SoftIRQ %12s: PID %6s, Sched %4s (priority %4s), name: %s" % \
		    (self.name, self.process_id ,self.scheduling_class, self.scheduling_priority, self.fullname)
		return s

def sortedDictValues(adict):
    items = adict.items()
    items.sort()
    return [value for key, value in items]

print ""
print "Interrupt list utility " + VERSION
print "=========================="
print "(C) 2008 Pieter Palmers"
print ""

# get PID info
(exitstatus, outtext) = commands.getstatusoutput('ps -eLo pid,cmd,class,rtprio | grep IRQ')

rawstr = r"""([0-9]+) +\[IRQ-([0-9]+)\] +([A-Z]{2}) +([-0-9]+)"""
compile_obj = re.compile(rawstr)

IRQs = {}
for line in outtext.splitlines():
	match_obj = compile_obj.search(line)
	if match_obj:
		irq = IRQ()
		irq.process_id = int(match_obj.group(1))
		irq.number = int(match_obj.group(2))
		irq.scheduling_class = match_obj.group(3)
		if match_obj.group(4) != '-':
			irq.scheduling_priority = int(match_obj.group(4))
		else:
			irq.scheduling_priority = None
		IRQs[irq.number] = irq

(exitstatus, outtext) = commands.getstatusoutput('ps -eLo pid,cmd,class,rtprio | grep softirq')

rawstr = r"""([0-9]+) +\[softirq-(.*)\] +([A-Z]+) +([-0-9]+)"""
compile_obj = re.compile(rawstr)

softIRQs = {}
for line in outtext.splitlines():
	match_obj = compile_obj.search(line)
	if match_obj:
		irq = SoftIRQ()
		irq.process_id = int(match_obj.group(1))
		irq.name = "%s-%s" % (match_obj.group(2),irq.process_id)
		irq.fullname = "softirq-%s" % match_obj.group(2)
		irq.scheduling_class = match_obj.group(3)
		if match_obj.group(4) != '-':
			irq.scheduling_priority = int(match_obj.group(4))
		else:
			irq.scheduling_priority = None
		softIRQs[irq.name] = irq

# get irq info
(exitstatus, outtext) = commands.getstatusoutput('cat /proc/interrupts')
lines = outtext.splitlines()
nb_cpus = len(lines[0].split())
str0 = "([0-9]+): +";
str_cpu = "([0-9]+) +"
str1="([\w\-]+) +([\w\-, :]+)"

rawstr = str0;
for i in range(nb_cpus):
	rawstr += str_cpu
rawstr += str1
compile_obj = re.compile(rawstr)

for line in outtext.splitlines():
	match_obj = compile_obj.search(line)
	if match_obj:
		irq_number = int(match_obj.group(1))
		if not irq_number in IRQs.keys():
			IRQs[irq_number] = IRQ()
			IRQs[irq_number].number = irq_number
		
		irq = IRQs[irq_number]
		cpu_counts = []
		for i in range(nb_cpus):
			cpu_counts.append(int(match_obj.group(1 + 1)))
		irq.cpu_counts = cpu_counts
		irq.type = match_obj.group(nb_cpus + 2)
		drivers = match_obj.group(nb_cpus + 3).split(',')
		for driver in drivers:
			irq.drivers.append(driver.strip())

		IRQs[irq.number] = irq

print "Hardware Interrupts:"
print "--------------------"

for irq in sortedDictValues(IRQs):
	print irq

print ""
print "Software Interrupts:"
print "--------------------"
for irq in sortedDictValues(softIRQs):
	print irq
	
print ""
