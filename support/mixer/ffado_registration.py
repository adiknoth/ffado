import urllib
import ConfigParser, os

from ffadomixer_config import REGISTER_URL, INI_FILE_PATH

from qt import *
from ffado_regdialog import *

class ffado_registration():
    def __init__(self, ffado_version,
                       guid,
                       vendor_id,
                       model_id,
                       vendor_string,
                       model_string):
        self.ffado_version = ffado_version
        #import pdb;pdb.set_trace()
        self.guid = guid
        self.vendor_id = vendor_id
        self.model_id = model_id
        self.vendor_string = vendor_string
        self.model_string = model_string

        self.config_filename = os.path.expanduser(INI_FILE_PATH)
        self.parser = ConfigParser.SafeConfigParser()
        self.parser.read(self.config_filename)
        self.section_name = "%s:%X" % (ffado_version, guid)
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
            print "failed, network error"
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
            print "registration failed" 
            print " " + errline
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
            print "user requested to ignore registration"
        else:
            if self.check_if_already_registered():
                print "version/GUID combo already registered"
            else:
                print "show dialog..."
                devtext = "%s %s" % (self.vendor_string,
                                              self.model_string)
                vendormodel = "0x%X / 0x%X" % (self.vendor_id,
                                               self.model_id)
                dlg = ffadoRegDialog(devtext, vendormodel, 
                                     self.guid, self.ffado_version,
                                     self.email)
                dlg.exec_loop()

                #import pdb;pdb.set_trace()
                if dlg.choice == "neversend":
                    self.mark_ignore_version()
                elif dlg.choice == "send":
                    self.email = dlg.getEmail().latin1()
                    self.remember_email(self.email)

                    retval = self.register_ffado_usage()
                    msg = QMessageBox()
                    if retval[0] == 0:
                        print "registration successful"
                        devinfomsg = "<p>Device: %s<br> Vendor/Model Id: %s<br>Device GUID: %016X<br>FFADO Version: %s<br>E-Mail: %s</p>" % \
                            (devtext, vendormodel, self.guid, self.ffado_version, self.email)
                        tmp = msg.question( msg, "Registration Successful",
                                            "<qt><b>Thank you.</b>" +
                                            "<p>The registration of the following information was successful:</p>" +
                                            devinfomsg +
                                            "</p>For this device you won't be asked to register again until you upgrade to a new version of FFADO.</p>",
                                            QMessageBox.Ok )
                        self.mark_version_registered()
                    else:
                        print "error: " + retval[1]
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
