#!/usr/bin/python
#

from qt import *
from mixer_phase88ui import *
import sys
if __name__ == "__main__":
    app = QApplication(sys.argv)
    f = Phase88Control()
    f.show()
    app.setMainWidget(f)
    app.exec_loop()
