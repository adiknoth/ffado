#
# Copyright (C) 2008-2009 by Pieter Palmers
#               2009      by Arnold Krille
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# FFADO is based upon FreeBoB.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import urllib
import ConfigParser, os

from ffado.config import REGISTER_URL, INI_FILE_PATH, FFADO_CONFIG_DIR
from PyQt4.QtGui import QMessageBox
from PyQt4.QtCore import QByteArray

from ffado.regdialog import *

import logging
log = logging.getLogger('registration')

class ffado_registration:
    def __init__(self, ffado_version,
                       guid,
                       vendor_id,
                       model_id,
                       vendor_string,
                       model_string):
        # only use the section before the SVN mark
        # we don't need to keep track of all SVN version changes
        self.ffado_version = ffado_version.split('-')[0]
        self.guid = guid
        self.vendor_id = vendor_id
        self.model_id = model_id
        self.vendor_string = vendor_string
        self.model_string = model_string

        #check if config file path exists, if not, create it
        config_path = os.path.expanduser(FFADO_CONFIG_DIR)
        if not os.path.exists(config_path):
            os.makedirs(config_path)

        # parse the ini file
        self.config_filename = os.path.expanduser(INI_FILE_PATH)
        self.parser = ConfigParser.SafeConfigParser()
        self.parser.read(self.config_filename)
        self.section_name = "%s:%X" % (self.ffado_version, self.guid)
        self.email = "(optional)"
        if self.parser.has_section("history") \
           and self.parser.has_option("history", "email"):
            self.email = self.parser.get("history", "email")

    def register_ffado_usage(self):
        post_vals = {}
        post_vals['guid'] = self.guid
        post_vals['vendor_id'] = self.vendor_id
        post_vals['model_id'] = self.model_id
        post_vals['vendor_string'] = self.vendor_string
        post_vals['model_string'] = self.model_string
        post_vals['ffado_version'] = self.ffado_version
        post_vals['email'] = self.email

        try:
            response = urllib.urlopen(REGISTER_URL,
                                      urllib.urlencode(post_vals))
        except:
            log.error("failed, network error")
            return (-1, "Network Error")
    
        lines = response.readlines()
        
        ok = False
        errline = "Bad response from server"
        for i in range(len(lines)):
            if lines[i][0:10] == "RESULT: OK":
                ok = True
            elif lines[i][0:12] == "RESULT: FAIL":
                ok = False
                if len(lines)>i+1:
                    errline = lines[i+1]
        if not ok:
            log.info("registration failed %s" % errline)
            return (-2, errline)
        else:
            return (0, "")
    
    def check_for(self, what):
        if not self.parser.has_section(self.section_name):
            return False
        if not self.parser.has_option(self.section_name, what):
            return False
        return self.parser.getboolean(self.section_name, what)
    
    def check_if_already_registered(self):
        return self.check_for("registered")
    
    def check_for_ignore(self):
        return self.check_for("ignore")
    
    def mark(self, what, value):
        if not self.parser.has_section(self.section_name):
            self.parser.add_section(self.section_name)
        self.parser.set(self.section_name, what, str(value))
    
    def mark_version_registered(self):
        self.mark("registered", True)
    
    def mark_ignore_version(self):
        self.mark("ignore", True)

    def remember_email(self, email):
        if not self.parser.has_section("history"):
            self.parser.add_section("history")
        self.parser.set("history", "email", str(email))

    def check_for_registration(self):

        if self.check_for_ignore():
            log.debug("user requested to ignore registration")
        else:
            if self.check_if_already_registered():
                log.debug("version/GUID combo already registered")
            else:
                log.debug("show dialog...")

                dlg = ffadoRegDialog(self.vendor_string, "0x%X" % self.vendor_id,
                                     self.model_string, "0x%X" % self.model_id,
                                     "0x%016X" % self.guid, self.ffado_version,
                                     self.email)
                dlg.exec_()

                if dlg.choice == "neversend":
                    self.mark_ignore_version()
                elif dlg.choice == "send":
                    asciiData = dlg.getEmail().toAscii()
                    self.email = asciiData.data()
                    self.remember_email(self.email)

                    retval = self.register_ffado_usage()
                    msg = QMessageBox()
                    if retval[0] == 0:
                        log.debug("registration successful")
                        devinfomsg = "<p>Device: %s %s<br> Vendor/Model Id: %X/%X<br>Device GUID: %016X<br>FFADO Version: %s<br>E-Mail: %s</p>" % \
                            (self.vendor_string, self.model_string, self.vendor_id, self.model_id, self.guid, self.ffado_version, self.email)
                        tmp = msg.question( msg, "Registration Successful",
                                            "<qt><b>Thank you.</b>" +
                                            "<p>The registration of the following information was successful:</p>" +
                                            devinfomsg +
                                            "</p>For this device you won't be asked to register again until you upgrade to a new version of FFADO.</p>",
                                            QMessageBox.Ok )
                        self.mark_version_registered()
                    else:
                        log.error("error: " + retval[1])
                        tmp = msg.question( msg, "Registration Failed", 
                                            "<qt><b>The registration at ffado.org failed.</b>" +
                                            "<p>Error message:</p><p>" + retval[1] +
                                            "</p><p>Try again next time?</p></qt>", 
                                            QMessageBox.Yes, QMessageBox.No )
                        if tmp == 4:
                            self.mark_ignore_version()
                elif dlg.choice == "nosend":
                    pass
        # write the updated config
        f = open(self.config_filename, "w+")
        self.parser.write(f)
        f.close()

# vim: et
