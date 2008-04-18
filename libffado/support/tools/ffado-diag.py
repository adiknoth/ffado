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

#
# Test for common FFADO problems
#

import os
import sys
import commands
import re
import logging

from listirqinfo import IRQ,SoftIRQ,IRQInfo
from helpstrings import HelpStrings

## message strings
FFADODIAG_VERSION = "0.1"

welcome_msg = """

FFADO diagnostic utility """ + FFADODIAG_VERSION + """
============================
(C) 2008 Pieter Palmers

"""

help_msg = """
Usage: ffado-diag [verboselevel]

  verboselevel : verbosity level. (optional)

"""

## logging setup
logging.basicConfig()
log = logging.getLogger('diag')

## helper routines

# kernel
def get_kernel_version():
    (exitstatus, outtext) = commands.getstatusoutput('uname -r')
    log.debug("uname -r outputs: %s" % outtext)
    return outtext

def get_kernel_rt_patched():
    print "FIXME: implement test for RT kernel"
    return False

# modules
def check_for_module_loaded(modulename):
    log.info("Checking if module '%s' is loaded... " % modulename)
    f = open('/proc/modules')
    lines = f.readlines()
    f.close()
    for l in lines:
        mod = l.split()[0]
        if mod == modulename:
            log.info(" found")
            return True
    log.info(" not found")
    return False
    
def check_for_module_present(modulename):
    log.info("Checking if module '%s' is present... " % modulename)
    kver = get_kernel_version()
    (exitstatus, outtext) = commands.getstatusoutput("find \"/lib/modules/%s/\" -name '%s.ko' | grep '%s'" % \
                                                     (kver, modulename, modulename) )
    log.debug("find outputs: %s" % outtext)
    if outtext == "":
        log.info(" not found")
        return False
    else:
        log.info(" found")
        return True

def check_1394oldstack_loaded():
    retval = True
    if not check_for_module_loaded('ieee1394'):
        retval = False
    if not check_for_module_loaded('ohci1394'):
        retval = False
    if not check_for_module_loaded('raw1394'):
        retval = False
    return retval

def check_1394oldstack_present():
    retval = True
    if not check_for_module_present('ieee1394'):
        retval = False
    if not check_for_module_present('ohci1394'):
        retval = False
    if not check_for_module_present('raw1394'):
        retval = False
    return retval

def check_1394newstack_loaded():
    retval = True
    if not check_for_module_loaded('fw-core'):
        retval = False
    if not check_for_module_loaded('fw-ohci'):
        retval = False
    return retval

def check_1394newstack_present():
    retval = True
    if not check_for_module_present('fw-core'):
        retval = False
    if not check_for_module_present('fw-ohci'):
        retval = False
    return retval

def check_1394oldstack_devnode_present():
    return os.path.exists('/dev/raw1394')

def check_1394oldstack_devnode_permissions():
    f = open('/dev/raw1394','w')
    if f:
        f.close()
        return True
    else:
        return False


# 

## main program
if __name__== '__main__':

    print welcome_msg

    num_args = len(sys.argv)
    if num_args not in [1,2]:
        print help
        sys.exit(0)

    if num_args == 2:
        loglevel = eval(sys.argv[1])
        if loglevel == 1:
            logging.getLogger('diag').setLevel(logging.INFO)
        elif loglevel == 2:
            logging.getLogger('diag').setLevel(logging.DEBUG)

    print "=== CHECK ==="
    print " Base system..."
    
    # check kernel
    kernel_version = get_kernel_version()
    print "  kernel version............ " + str(kernel_version)
    kernel_is_rt_patched = get_kernel_rt_patched()
    print "   RT patched............... " + str(kernel_is_rt_patched)
    
    # check modules
    oldstack_present = check_1394oldstack_present()
    oldstack_loaded = check_1394oldstack_loaded()
    newstack_present = check_1394newstack_present()
    newstack_loaded = check_1394newstack_loaded()
    
    print "  old 1394 stack present.... " + str(oldstack_present)
    print "  old 1394 stack loaded..... " + str(oldstack_loaded)
    print "  new 1394 stack present.... " + str(newstack_present)
    print "  new 1394 stack loaded..... " + str(newstack_loaded)
    
    # check /dev/raw1394 node presence
    devnode_present = check_1394oldstack_devnode_present()
    print "  /dev/raw1394 node present. " + str(devnode_present)
    if devnode_present:
        # check /dev/raw1394 access permissions
        devnode_permissions = check_1394oldstack_devnode_permissions()
        print "  /dev/raw1394 permissions.. " + str(devnode_permissions)
    else:
        devnode_permissions = None
    
    # check libraries
    # libraw
    
    print " Hardware..."
    # check host controller
    print "  todo..."

    print " Configuration..."
    # check RT settings
    
    # check IRQ settings 
    print "  todo..."

    print ""
    print "=== REPORT ==="
    
    help = HelpStrings()
    
    # do the interpretation of the tests
    print "FireWire kernel drivers:"
    ## FIXME: what about in-kernel firewire? (i.e. no modules)
    if not oldstack_present:
        help.show('MODULES_OLD_STACK_NOT_INSTALLED')
        sys.exit(-1)
    else:
        if newstack_loaded and oldstack_loaded:
            help.show('MODULES_BOTH_STACKS_LOADED')
            sys.exit(-1)
        elif newstack_loaded:
            help.show('MODULES_NEW_STACK_LOADED')
            sys.exit(-1)
        elif not oldstack_loaded:
            help.show('MODULES_OLD_STACK_NOT_LOADED')
            sys.exit(-1)
        else:
            print "[PASS] Kernel modules present and correctly loaded."

    if not devnode_present:
        help.show('DEVNODE_OLD_STACK_NOT_PRESENT')
        sys.exit(-1)
    else:
        if not devnode_permissions:
            help.show('DEVNODE_OLD_STACK_NO_PERMISSION')
            sys.exit(-1)
        else:
            print "[PASS] /dev/raw1394 node present and accessible."
    
    
    