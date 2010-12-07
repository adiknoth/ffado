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
# Help strings for ffado problems
#

class HelpStrings:
    def __init__(self):
        strings = {}
        strings['MODULES_OLD_STACK_NOT_INSTALLED']="""
FireWire kernel stack not present. Please compile the kernel with 
FireWire support.
"""
        strings['MODULES_BOTH_STACKS_LOADED']="""
Both old and new FireWire kernel modules are loaded, your system 
configuration is bogus.
"""
        strings['MODULES_NEW_STACK_LOADED']="""
The new FireWire kernel stack is loaded. 
This is still kind of experimental. If you encounter problems, please also check
with the old stack.
"""
        strings['MODULES_OLD_STACK_NOT_LOADED']="""
FireWire kernel module(s) not found. 
Please ensure that the raw1394 module is loaded.
"""
        strings['DEVNODE_OLD_STACK_NOT_PRESENT']="""
/dev/raw1394 device node not present. 
Please fix your udev configuration or add it as a static node.
"""
        strings['DEVNODE_OLD_STACK_NO_PERMISSION']="""
Not enough permissions to access /dev/raw1394 device. 
Please fix your udev configuration, the node permissions or
the user/group permissions.
"""

        self.strings=strings

    def get(self, sid):
        return self.strings[sid]

    def show(self, sid):
        print self.get(sid)
    
