# coding=utf8
#
# Copyright (C) 2009 by Arnold Krille
# Copyright (C) 2013 by Philippe Carriere
#
# This file is part of FFADO
# FFADO = Free Firewire (pro-)audio drivers for linux
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from PyQt4 import QtGui, QtCore, Qt
import dbus, math, decimal

import logging
log = logging.getLogger("matrixmixer")

def toDBvalue(value):
    n = int(value)
    c2p14 = 16384.0
    if n > 164:
        return round(20.0*math.log10(float(n)/c2p14), 2)
    else:
        return -40.0

def fromDBvalue(value):
    v = float(value)
    c2p14 = 16384.0
    if (v > -40):
        return int(round(math.pow(10.0, (value/20.0))*c2p14, 0))
    else:
        return 0

class ColorForNumber:
    def __init__(self):
        self.colors = dict()

    def addColor(self, n, color):
        self.colors[n] = color

    def getColor(self, n):
        #print "ColorForNumber.getColor( %g )" % (n)
        keys = self.colors.keys()
        keys.sort()
        low = keys[-1]
        high = keys[-1]
        for i in range(len(keys)-1):
            if keys[i] <= n and keys[i+1] > n:
                low = keys[i]
                high = keys[i+1]
        #print "%g is between %g and %g" % (n, low, high)
        f = 0
        if high != low:
            f = (n-low) / (high-low)
        lc = self.colors[low]
        hc = self.colors[high]
        return QtGui.QColor(
                (1-f)*lc.red()   + f*hc.red(),
                (1-f)*lc.green() + f*hc.green(),
                (1-f)*lc.blue()  + f*hc.blue() )

class BckgrdColorForNumber(ColorForNumber):
    def __init__(self):
        ColorForNumber.__init__(self)
        self.addColor(             0.0, QtGui.QColor(  0,   0,   0))
        self.addColor(             1.0, QtGui.QColor(  0,   0, 128))
        self.addColor(   math.pow(2,6), QtGui.QColor(  0, 255,   0))
        self.addColor(  math.pow(2,14), QtGui.QColor(255, 255,   0))
        self.addColor(math.pow(2,16)-1, QtGui.QColor(255,   0,   0))

    def getFrgdColor(self, color):
        if color.valueF() < 0.6:
            return QtGui.QColor(255, 255, 255)
        else:
            return QtGui.QColor(0, 0, 0)
    
class MixerNode(QtGui.QAbstractSlider):
    def __init__(self, input, output, value, max, muted, inverted, parent, matrix_obj):
        QtGui.QAbstractSlider.__init__(self, parent)
        #log.debug("MixerNode.__init__( %i, %i, %i, %i, %s )" % (input, output, value, max, str(parent)) )

        # Store a direct link back to the underlying matrix object so the mute
        # and invert interfaces can be easily found.  By the time the matrix 
        # has been set into the full widget hierarchy, its parent is unlikely
        # to still be the top-level matrix object.
        self.matrix_obj = matrix_obj;

        self.pos = QtCore.QPointF(0, 0)
        self.input = input
        self.output = output
        self.setOrientation(Qt.Qt.Vertical)
        if max == -1:
            max = pow(2, 16)-1
        self.setRange(0, max)
        self.setValue(value)
        self.connect(self, QtCore.SIGNAL("valueChanged(int)"), self.internalValueChanged)

        self.setSmall(False)

        self.bgcolors = BckgrdColorForNumber()

        self.setContextMenuPolicy(Qt.Qt.ActionsContextMenu)
        self.mapper = QtCore.QSignalMapper(self)
        self.connect(self.mapper, QtCore.SIGNAL("mapped(const QString&)"), self.directValues)

        self.spinbox = QtGui.QDoubleSpinBox(self)
        self.spinbox.setRange(-40, 12)
        self.spinbox.setSuffix(" dB")
        if value != 0:
            self.spinbox.setValue(toDBvalue(value))            

        self.connect(self.spinbox, QtCore.SIGNAL("valueChanged(const QString&)"), self.directValues)
        action = QtGui.QWidgetAction(self)
        action.setDefaultWidget(self.spinbox)
        self.addAction(action)

        for text in ["3 dB", "0 dB", "-3 dB", "-20 dB", "-inf dB"]:
            action = QtGui.QAction(text, self)
            self.connect(action, QtCore.SIGNAL("triggered()"), self.mapper, QtCore.SLOT("map()"))
            self.mapper.setMapping(action, text)
            self.addAction(action)

        # Only show the mute menu item if a value has been supplied
        self.mute_action = None
        if (muted != None):
            action = QtGui.QAction(text, self)
            action.setSeparator(True)
            self.addAction(action)
            self.mute_action = QtGui.QAction("Mute", self)
            self.mute_action.setCheckable(True)
            self.mute_action.setChecked(muted)
            self.connect(self.mute_action, QtCore.SIGNAL("triggered()"), self.mapper, QtCore.SLOT("map()"))
            self.mapper.setMapping(self.mute_action, "Mute")
            self.addAction(self.mute_action)

        # Similarly, only show a phase inversion menu item if in use
        self.inv_action = None
        if (inverted != None):
            if (muted == None):
                action = QtGui.QAction(text, self)
                action.setSeparator(True)
                self.addAction(action)
            self.inv_action = QtGui.QAction("Invert", self)
            self.inv_action.setCheckable(True)
            self.inv_action.setChecked(inverted)
            self.connect(self.inv_action, QtCore.SIGNAL("triggered()"), self.mapper, QtCore.SLOT("map()"))
            self.mapper.setMapping(self.inv_action, "Invert")
            self.addAction(self.inv_action)

    def directValues(self,text):
        #log.debug("MixerNode.directValues( '%s' )" % text)
        if text == "Mute":
            #log.debug("Mute %d" % self.mute_action.isChecked())
            self.update()
            self.matrix_obj.mutes_interface.setValue(self.output, self.input, self.mute_action.isChecked())
        elif text == "Invert":
            #log.debug("Invert %d" % self.inv_action.isChecked())
            self.update()
            self.matrix_obj.inverts_interface.setValue(self.output, self.input, self.inv_action.isChecked())
        else:
            text = text.split(" ")[0].replace(",",".")
            n = fromDBvalue(float(text))
            #log.debug("  linear value: %g" % n)
            self.setValue(n)

    def mousePressEvent(self, ev):
        if ev.buttons() & Qt.Qt.LeftButton:
            self.pos = ev.posF()
            self.tmpvalue = self.value()
            ev.accept()
            #log.debug("MixerNode.mousePressEvent() %s" % str(self.pos))

    def mouseMoveEvent(self, ev):
        if hasattr(self, "tmpvalue") and self.pos is not QtCore.QPointF(0, 0):
            newpos = ev.posF()
            change = newpos.y() - self.pos.y()
            #log.debug("MixerNode.mouseReleaseEvent() change %s" % (str(change)))
            self.setValue( self.tmpvalue - math.copysign(pow(abs(change), 2), change) )
            ev.accept()

    def mouseReleaseEvent(self, ev):
        if hasattr(self, "tmpvalue") and self.pos is not QtCore.QPointF(0, 0):
            newpos = ev.posF()
            change = newpos.y() - self.pos.y()
            #log.debug("MixerNode.mouseReleaseEvent() change %s" % (str(change)))
            self.setValue( self.tmpvalue - math.copysign(pow(abs(change), 2), change) )
            self.pos = QtCore.QPointF(0, 0)
            del self.tmpvalue
            ev.accept()

    def paintEvent(self, ev):
        p = QtGui.QPainter(self)
        rect = self.rect()
        v = self.value()
        if (self.mute_action!=None and self.mute_action.isChecked()):
            color = QtGui.QColor(64, 64, 64)
        else:
            color = self.bgcolors.getColor(v)
        p.fillRect(rect, color)

        if self.small:
            return

        p.setPen(self.bgcolors.getFrgdColor(color))

        lv=decimal.Decimal('-Infinity')
        if v != 0:
            lv = toDBvalue(v)
            #log.debug("new value is %g dB" % lv)
        text = "%.2g dB" % lv
        if v == 0:
            symb_inf = u"\u221E"
            text = "-" + symb_inf + " dB"
        p.drawText(rect, Qt.Qt.AlignCenter, QtCore.QString.fromUtf8(text))
        if (self.inv_action!=None and self.inv_action.isChecked()):
            p.drawText(rect, Qt.Qt.AlignLeft|Qt.Qt.AlignTop, QtCore.QString.fromUtf8(" ϕ"))

    def internalValueChanged(self, value):
        #log.debug("MixerNode.internalValueChanged( %i )" % value)
        if value != 0:
            dB = toDBvalue(value)
            if self.spinbox.value() is not dB:
                self.spinbox.setValue(dB)
        self.emit(QtCore.SIGNAL("valueChanged"), (self.input, self.output, value) )
        self.update()

    def setSmall(self, small):
        self.small = small
        if small:
            self.setMinimumSize(10, 10)
        else:
            fontmetrics = self.fontMetrics()
            self.setMinimumSize(fontmetrics.boundingRect("-0.0 dB").size()*1.1)
        self.update()

class MixerChannel(QtGui.QWidget):
    def __init__(self, number, parent=None, name="", smallFont=False):
        QtGui.QWidget.__init__(self, parent)
        layout = QtGui.QGridLayout(self)
        self.number = number
        self.name = name
        self.lbl = QtGui.QLabel(self)
        self.lbl.setAlignment(Qt.Qt.AlignCenter)
        if (smallFont):
            font = self.lbl.font()
            font.setPointSize(font.pointSize()/1.5)
            self.lbl.setFont(font)
        layout.addWidget(self.lbl, 0, 0, 1, 2)
        self.hideChannel(False)

        self.setContextMenuPolicy(Qt.Qt.ActionsContextMenu)

        action = QtGui.QAction("Make this channel small", self)
        action.setCheckable(True)
        self.connect(action, QtCore.SIGNAL("triggered(bool)"), self.hideChannel)
        self.addAction(action)

    def hideChannel(self, hide):
        if hide:
            self.lbl.setText("%i" % (self.number+1));
        else:
            self.lbl.setText(self.name)
        self.emit(QtCore.SIGNAL("hide"), self.number, hide)
        self.update()

# Matrix view widget
class MatrixControlView(QtGui.QWidget):
    def __init__(self, servername, basepath, parent=None, sliderMaxValue=-1, mutespath=None, invertspath=None, smallFont=False, shortname=False, shortcolname="Ch", shortrowname="Ch", transpose=False):
        QtGui.QWidget.__init__(self, parent)

        self.bus = dbus.SessionBus()
        self.dev = self.bus.get_object(servername, basepath)
        self.interface = dbus.Interface(self.dev, dbus_interface="org.ffado.Control.Element.MatrixMixer")

        self.transpose = transpose
        if (transpose):
            self.shortcolname = shortrowname
            self.shortrowname = shortcolname
            self.cols = self.interface.getRowCount()
            self.rows = self.interface.getColCount()
        else:
            self.shortcolname = shortcolname
            self.shortrowname = shortrowname
            self.cols = self.interface.getColCount()
            self.rows = self.interface.getRowCount()

        log.debug("Mixer has %i rows and %i columns" % (self.rows, self.cols))

        self.mutes_dev = None
        self.mutes_interface = None
        if (mutespath != None):
            self.mutes_dev = self.bus.get_object(servername, mutespath)
            self.mutes_interface = dbus.Interface(self.mutes_dev, dbus_interface="org.ffado.Control.Element.MatrixMixer")

        self.inverts_dev = None
        self.inverts_interface = None
        if (invertspath != None):
            self.inverts_dev = self.bus.get_object(servername, invertspath)
            self.inverts_interface = dbus.Interface(self.inverts_dev, dbus_interface="org.ffado.Control.Element.MatrixMixer")

        layout = QtGui.QGridLayout(self)
        layout.setSizeConstraint(QtGui.QLayout.SetNoConstraint);
        self.setLayout(layout)

        self.rowHeaders = []
        self.columnHeaders = []
        self.items = []
        self.shortname = shortname

        # Add row/column headers, but only if there's more than one 
        # row/column
        if (self.cols > 1):
            for i in range(self.cols):
                ch = MixerChannel(i, self, self.getColName(i, self.shortname), smallFont)
                self.connect(ch, QtCore.SIGNAL("hide"), self.hideColumn)
                layout.addWidget(ch, 0, i+1)
                self.columnHeaders.append( ch )
            layout.setRowStretch(0, 0)
            layout.setRowStretch(1, 10)
        if (self.rows > 1):
            for i in range(self.rows):
                ch = MixerChannel(i, self, self.getRowName(i, self.shortname), smallFont)
                self.connect(ch, QtCore.SIGNAL("hide"), self.hideRow)
                layout.addWidget(ch, i+1, 0)
                self.rowHeaders.append( ch )

        # Add node-widgets
        for i in range(self.rows):
            self.items.append([])
            for j in range(self.cols):
                if (transpose):
                    mute_value = None
                    if (self.mutes_interface != None):
                        mute_value = self.mutes_interface.getValue(j,i)
                    inv_value = None
                    if (self.inverts_interface != None):
                        inv_value = self.inverts_interface.getValue(j,i)
                    node = MixerNode(i, j, self.interface.getValue(j,i), sliderMaxValue, mute_value, inv_value, self, self)
                else:
                    mute_value = None
                    if (self.mutes_interface != None):
                        mute_value = self.mutes_interface.getValue(i,j)
                    inv_value = None
                    if (self.inverts_interface != None):
                        inv_value = self.inverts_interface.getValue(i,j)
                    node = MixerNode(j, i, self.interface.getValue(i,j), sliderMaxValue, mute_value, inv_value, self, self)
                if (smallFont):
                    font = node.font()
                    font.setPointSize(font.pointSize()/1.5)
                    node.setFont(font)
                self.connect(node, QtCore.SIGNAL("valueChanged"), self.valueChanged)
                layout.addWidget(node, i+1, j+1)
                self.items[i].append(node)

        self.hiddenRows = []
        self.hiddenCols = []

    def checkVisibilities(self):
        for x in range(len(self.items)):
            for y in range(len(self.items[x])):
                self.items[x][y].setSmall(
                        (x in self.hiddenRows)
                        | (y in self.hiddenCols)
                        )

    def hideColumn(self, column, hide):
        if hide:
            self.hiddenCols.append(column)
        else:
            self.hiddenCols.remove(column)
        self.checkVisibilities()

    def hideRow(self, row, hide):
        if hide:
            self.hiddenRows.append(row)
        else:
            self.hiddenRows.remove(row)
        self.checkVisibilities()

    # Columns and rows
    def getColName(self, i, shortname):
        if (self.transpose):
            name = self.interface.getRowName(i)
        else:
            name = self.interface.getColName(i)
        self.shortname = shortname
        if (shortname or (name == '')):
            number = " %d" % (i+1)
            name = self.shortcolname + number
        return name

    def getRowName(self, j, shortname):
        if (self.transpose):
            name = self.interface.getColName(j)
        else:
            name = self.interface.getRowName(j)
        self.shortname = shortname
        if (shortname or (name == '')):
            number = " %d" % (j+1)
            name = self.shortrowname + number
        return name

    def valueChanged(self, n):
        #log.debug("MatrixNode.valueChanged( %s )" % str(n))
        self.interface.setValue(n[1], n[0], n[2])
        self.emit(QtCore.SIGNAL("valueChanged"), n)
        
    # Update when routing is modified
    def updateRouting(self):
        if (self.cols > 1):
            for i in range(self.cols):
                last_name = self.columnHeaders[i].lbl.text()
                col_name = self.getColName(i, self.shortname)
                if last_name != col_name:
                    #log.debug("MatrixControlView.updateRouting( %s )" % str(col_name))
                    self.columnHeaders[i].name = col_name
                    self.columnHeaders[i].lbl.setText(col_name)
      
        if (self.rows > 1):
            for j in range(self.rows):
                last_name = self.rowHeaders[j].lbl.text()
                row_name = self.getRowName(j, self.shortname)
                if last_name != row_name:
                    #log.debug("MatrixControlView.updateRouting( %s )" % str(row_name))
                    self.rowHeaders[j].name = row_name
                    self.rowHeaders[j].lbl.setText(row_name)       

class ChannelSlider(QtGui.QSlider):
    def __init__(self, In, Out, value, parent):
        QtGui.QSlider.__init__(self, QtCore.Qt.Vertical, parent)

        self.setTickPosition(QtGui.QSlider.TicksBothSides)
        v_min = 10.0*toDBvalue(0)
        v_max = 10.0*toDBvalue(65536)
        self.setTickInterval((v_max-v_min)/10)
        self.setMinimum(v_min)
        self.setMaximum(v_max)
        self.setSingleStep(1)
        value = 10.0*toDBvalue(value)
        self.setValue(value)
        self.In = In
        self.Out = Out
        self.connect(self, QtCore.SIGNAL("valueChanged(int)"), self.slider_read_value)

    def slider_set_value(self, value):
        #log.debug("Slider value changed( %i )" % value)
        v = 10.0*toDBvalue(value)
        #log.debug("Slider value changed(dB: %g )" % (0.1*v))
        self.setValue(v)

    # Restore absolute value from DB
    # Emit signal for further use, especially for matrix view
    def slider_read_value(self, value):
        value = fromDBvalue(0.1*value)
        self.emit(QtCore.SIGNAL("valueChanged"), (self.In, self.Out, value))
        self.update()

class ChannelSliderValueInfo(QtGui.QLineEdit):
    def __init__(self, In, Out, value, parent):
        QtGui.QLineEdit.__init__(self, parent)

        self.setReadOnly(True)
        self.setAlignment(Qt.Qt.AlignCenter)
        self.setAutoFillBackground(True)
        self.setFrame(False)

        self.setMinimalDim()

        self.bgcolors = BckgrdColorForNumber()

        self.setValue(value)

    def setMinimalDim(self):
        fontmetrics = self.fontMetrics()
        self.setMinimumSize(fontmetrics.boundingRect("-00.0 dB").size()*1.1)
        
    def setValue(self, value):
        color = self.bgcolors.getColor(value)
        palette = self.palette()
        palette.setColor(QtGui.QPalette.Active, QtGui.QPalette.Base, color)
        palette.setColor(QtGui.QPalette.Active, QtGui.QPalette.Text, self.bgcolors.getFrgdColor(color))
        self.setPalette(palette)

        v = round(toDBvalue(value),1)
        if (v > -40):
            text = "%.1f dB" % v
        else:
            symb_inf = u"\u221E"
            text = "-" + symb_inf + " dB"

        self.setText(text)
        
# Slider view widget
class SliderControlView(QtGui.QWidget):
    def __init__(self, parent, servername, basepath, rule="Columns_are_inputs", shortname=False, shortinname="Ch", shortoutname="Ch"):
        QtGui.QWidget.__init__(self, parent)

        self.bus = dbus.SessionBus()
        self.dev = self.bus.get_object(servername, basepath)
        self.interface = dbus.Interface(self.dev, dbus_interface="org.ffado.Control.Element.MatrixMixer")

        self.rule = rule
        self.shortname = shortname
        self.shortinname = shortinname
        self.shortoutname = shortoutname

        self.out = []
        self.nbIn = self.getNbIn()
        self.nbOut = self.getNbOut()
            
        for i in range(self.nbOut):
            widget = QtGui.QWidget(parent)
            v_layout = QtGui.QVBoxLayout(widget)
            v_layout.setAlignment(Qt.Qt.AlignCenter)
            widget.setLayout(v_layout)
            self.out.append(widget)

            self.out[i].lbl = []

            # Mixer/Out info label
            if (self.nbOut > 1):
                lbl = QtGui.QLabel(widget)
                lbl.setText(self.getOutName(i, self.shortname))
                lbl.setAlignment(Qt.Qt.AlignCenter)
                v_layout.addWidget(lbl)
                self.out[i].lbl.append(lbl)

            h_layout_wid = QtGui.QWidget(widget)
            h_layout = QtGui.QHBoxLayout(h_layout_wid)
            h_layout.setAlignment(Qt.Qt.AlignCenter)
            h_layout_wid.setLayout(h_layout)
            v_layout.addWidget(h_layout_wid)
            self.out[i].slider = []
            self.out[i].svl = []

            for j in range(self.nbIn):
                h_v_layout_wid = QtGui.QWidget(h_layout_wid)
                h_v_layout = QtGui.QVBoxLayout(h_v_layout_wid)
                h_v_layout.setAlignment(Qt.Qt.AlignCenter)
                h_v_layout_wid.setLayout(h_v_layout)
                h_layout.addWidget(h_v_layout_wid)

                # Mixer/In info label
                if (self.nbIn > 1):
                    lbl = QtGui.QLabel(h_v_layout_wid)
                    lbl.setText(self.getInName(j, self.shortname))
                    lbl.setAlignment(Qt.Qt.AlignCenter)
                    h_v_layout.addWidget(lbl)
                    self.out[i].lbl.append(lbl)

                h_v_h_layout_wid = QtGui.QWidget(h_v_layout_wid)
                h_v_h_layout = QtGui.QHBoxLayout(h_v_h_layout_wid)
                h_v_h_layout.setAlignment(Qt.Qt.AlignCenter)
                h_v_h_layout_wid.setLayout(h_v_h_layout)
                h_v_layout.addWidget(h_v_h_layout_wid)

                slider = ChannelSlider(j, i, self.getValue(j,i), h_v_h_layout_wid)
                h_v_h_layout.addWidget(slider)
                self.out[i].slider.append(slider)
                self.out[i].connect(slider, QtCore.SIGNAL("valueChanged"), self.valueChanged)

                # Slider value info
                svl = ChannelSliderValueInfo(j, i, self.getValue(j,i), h_v_layout_wid)
                h_v_layout.addWidget(svl)
                self.out[i].svl.append(svl)

    def getNbIn(self):
        if (self.rule == "Columns_are_inputs"):
            return self.interface.getColCount()
        else:
            return self.interface.getRowCount()
        
    def getNbOut(self):
        if (self.rule == "Columns_are_inputs"):
            return self.interface.getRowCount()
        else:
            return self.interface.getColCount()
        
    def getValue(self, In, Out):
        if (self.rule == "Columns_are_inputs"):
            return self.interface.getValue(Out, In)           
        else:
            return self.interface.getValue(In, Out)            

    def setValue(self, In, Out, val):
        if (self.rule == "Columns_are_inputs"):
            return self.interface.setValue(Out, In, val)           
        else:
            return self.interface.setValue(In, Out, val)            

    def valueChanged(self, n):
        #log.debug("ChannelSlider.valueChanged( %s )" % str(n))
        self.setValue(n[0], n[1], n[2])
        self.out[n[1]].svl[n[0]].setValue(n[2])
        self.emit(QtCore.SIGNAL("valueChanged"), n)

    def getOutName(self, i, shortname):
        self.shortname = shortname
        if (shortname):
            number = " %d" % (i+1)
            name = self.shortoutname + number
            return name
        else:
            if (self.rule == "Columns_are_inputs"):
                return self.interface.getRowName(i)            
            else:
                return self.interface.getColName(i)             

    def getInName(self, j, shortname):
        self.shortname = shortname
        if (shortname):
            number = " %d" % (j+1)
            name = self.shortinname + number
            return name
        else:
            if (self.rule == "Columns_are_inputs"):
                return self.interface.getColName(j)            
            else:
                return self.interface.getRowName(j)            

    # Update when routing is modified
    def updateRouting(self):
        for i in range(self.nbOut):
            if (self.nbOut > 1):
                last_name = self.out[i].lbl[0].text()
                out_name = self.getOutName(i, self.shortname)
                if last_name != out_name:
                    #log.debug("SliderControlView.updateRouting( %s )" % str(out_name))
                    self.out[i].lbl[0].setText(out_name)
            if (self.nbIn > 1):
                for j in range(self.nbIn):
                    last_name = self.out[i].lbl[j+1].text()
                    in_name = self.getInName(j, self.shortname)
                    if last_name != in_name:
                        #log.debug("SliderControlView.updateRouting( %s )" % str(in_name))
                        self.out[i].lbl[j+1].setText(in_name)

class MatrixMixer(QtGui.QWidget):
    def __init__(self, servername, basepath, parent=None, rule="Columns_are_inputs", sliderMaxValue=-1, mutespath=None, invertspath=None, smallFont=False, taborientation=QtGui.QTabWidget.West, tabshape=QtGui.QTabWidget.Triangular):
        QtGui.QWidget.__init__(self, parent)
        self.servername = servername
        self.basepath = basepath
        self.sliderMaxValue = sliderMaxValue
        self.mutespath = mutespath
        self.invertspath = invertspath
        self.smallFont = smallFont

        self.layout = QtGui.QVBoxLayout(self)
        self.setLayout(self.layout)

        # Mixer view Tool bar
        mxv_set = QtGui.QToolBar("View settings", self)

        # Here is a hack; the first action button appears to behaves strangely,
        # possibly a PyQt bug (or an unsufficient fair implementation of it)
        # Feel free to remove the next three lines at a time in the future
        hack = QtGui.QAction(" ", mxv_set)
        hack.setDisabled(True)
        mxv_set.addAction(hack)

        transpose_matrix = QtGui.QAction("Transpose", mxv_set)
        self.transpose = False
        #transpose_matrix.setShortcut('Ctrl+T')
        transpose_matrix.setToolTip("Invert rows and columns in Matrix view")
        mxv_set.addAction(transpose_matrix)
        transpose_matrix.triggered.connect(self.transposeMatrixView)
        mxv_set.addSeparator()

        self.hide_matrix = QtGui.QAction("Hide matrix", mxv_set)
        self.hide_matrix_bool = False
        mxv_set.addAction(self.hide_matrix)
        self.hide_matrix.triggered.connect(self.hideMatrixView)
        mxv_set.addSeparator()

        self.hide_per_output = QtGui.QAction("Hide per Output", mxv_set)
        self.hide_per_output_bool = False
        mxv_set.addAction(self.hide_per_output)
        self.hide_per_output.triggered.connect(self.hidePerOutputView)
        mxv_set.addSeparator()

        self.use_short_names = QtGui.QAction("Short names", mxv_set)
        self.short_names_bool = False
        mxv_set.addAction(self.use_short_names)
        self.use_short_names.setToolTip("Use short or full names for input and output channels")
        self.use_short_names.triggered.connect(self.shortChannelNames)
        mxv_set.addSeparator()

        font_switch_lbl = QtGui.QLabel(mxv_set)
        font_switch_lbl.setText("Font size ")
        mxv_set.addWidget(font_switch_lbl)
        font_switch = QtGui.QComboBox(mxv_set)
        font_switch.setToolTip("Labels font size")
        font = font_switch.font()
        for i in range(10):
            font_switch.addItem(" %d " % (font.pointSize()+4-i))
        font_switch.setCurrentIndex(font_switch.findText(" %d " % font.pointSize()))
        mxv_set.addWidget(font_switch)
        self.connect(font_switch, QtCore.SIGNAL("activated(QString)"), self.changeFontSize)

        self.layout.addWidget(mxv_set)
        self.mxv_set = mxv_set

        # First tab is for matrix view
        # Next are for "per Out" view
        self.tabs = QtGui.QTabWidget(self)
        self.tabs.setTabPosition(taborientation)
        self.tabs.setTabShape(tabshape)
        self.layout.addWidget(self.tabs)

        # Inputs/Outputs versus rows/columns rule
        self.rule = rule

        # Matrix view tab
        if (rule == "Columns_are_inputs"):
            self.matrix = MatrixControlView(servername, basepath, self, sliderMaxValue, mutespath, invertspath, smallFont, self.short_names_bool, "In", "Out", self.transpose)
        else:
            self.matrix = MatrixControlView(servername, basepath, self, sliderMaxValue, mutespath, invertspath, smallFont, self.short_names_bool, "Out", "In", self.transpose)
        self.connect(self.matrix, QtCore.SIGNAL("valueChanged"), self.matrixControlChanged)

        self.scrollarea_matrix = QtGui.QScrollArea(self.tabs)
        self.scrollarea_matrix.setWidgetResizable(True)
        self.scrollarea_matrix.setWidget(self.matrix)
        self.tabs.addTab(self.scrollarea_matrix, " Matrix ")

        # Per out view tabs
        self.perOut = SliderControlView(self, servername, basepath, rule, self.short_names_bool, "In", "Out")
        self.connect(self.perOut, QtCore.SIGNAL("valueChanged"), self.sliderControlChanged)
        for i in range(self.perOut.nbOut):
            self.perOut.out[i].scrollarea = QtGui.QScrollArea(self.tabs)
            self.perOut.out[i].scrollarea.setWidgetResizable(True)
            self.perOut.out[i].scrollarea.setWidget(self.perOut.out[i])
            self.tabs.addTab(self.perOut.out[i].scrollarea, " Out:%02d " % (i+1))

    def transposeMatrixView(self):
        self.transpose = not(self.transpose)
        self.tabs.removeTab(0)
        self.scrollarea_matrix.destroy()
        if (self.rule == "Columns_are_inputs"):
            self.matrix = MatrixControlView(self.servername, self.basepath, self, self.sliderMaxValue, self.mutespath, self.invertspath, self.smallFont, self.short_names_bool, "In", "Out", self.transpose)
        else:
            self.matrix = MatrixControlView(self.servername, self.basepath, self, self.sliderMaxValue, self.mutespath, self.invertspath, self.smallFont, self.short_names_bool, "Out", "In", self.transpose)
        self.connect(self.matrix, QtCore.SIGNAL("valueChanged"), self.matrixControlChanged)

        self.scrollarea_matrix = QtGui.QScrollArea(self.tabs)
        self.scrollarea_matrix.setWidgetResizable(True)
        self.scrollarea_matrix.setWidget(self.matrix)
        self.tabs.insertTab(0, self.scrollarea_matrix, "Matrix")
        self.tabs.setCurrentIndex(0)
        
    def hideMatrixView(self):
        self.hide_matrix_bool = not(self.hide_matrix_bool)
        if (self.hide_matrix_bool):
            self.tabs.removeTab(0)
            self.hide_matrix.setText("Show Matrix")
        else:
            self.tabs.insertTab(0, self.scrollarea_matrix, "Matrix")
            self.tabs.setCurrentIndex(0)
            self.hide_matrix.setText("Hide Matrix")
            
    def hidePerOutputView(self):
        self.hide_per_output_bool = not(self.hide_per_output_bool)
        if (self.hide_per_output_bool):
            for i in range(self.perOut.nbOut):
                self.tabs.removeTab(1)
            self.hide_per_output.setText("Show per Output")
        else:
            for i in range(self.perOut.nbOut):
                self.tabs.insertTab(i+1, self.perOut.out[i].scrollarea, "Out:%02d" % (i+1))
            self.hide_per_output.setText("Hide per Output")

    # Font size for channel names
    def changeFontSize(self, size):
        font = self.mxv_set.font()
        font.setPointSize(int(size))
        self.mxv_set.setFont(font)

        font = self.tabs.font()
        font.setPointSize(int(size))
        self.tabs.setFont(font)

        font = self.matrix.font()
        font.setPointSize(int(size))
        self.matrix.setFont(font)

        font = self.perOut.font()
        font.setPointSize(int(size))
        self.perOut.setFont(font)

        for i in range(self.perOut.nbOut):
            for j in range(self.perOut.nbIn):
                self.perOut.out[i].svl[j].setMinimalDim()

    # Allows long name for Mixer/Out and /In to be hidden 
    def shortChannelNames(self):
        checked = not(self.short_names_bool)
        if (self.matrix.cols > 1):
            for i in range(self.matrix.cols):
                self.matrix.columnHeaders[i].name = self.matrix.getColName(i, checked)
                self.matrix.columnHeaders[i].lbl.setText(self.matrix.columnHeaders[i].name)

        if (self.matrix.rows > 1):
            for j in range(self.matrix.rows):
                self.matrix.rowHeaders[j].name = self.matrix.getRowName(j, checked)
                self.matrix.rowHeaders[j].lbl.setText(self.matrix.rowHeaders[j].name)       

        for i in range(self.perOut.nbOut):
            if (self.perOut.nbOut > 1):
                self.perOut.out[i].lbl[0].setText(self.perOut.getOutName(i, checked))
            if (self.perOut.nbIn > 1):
                for j in range(self.perOut.nbIn):
                    self.perOut.out[i].lbl[j+1].setText(self.perOut.getInName(j, checked))

        # Care for hidden columns
        if (self.matrix.cols > 1):
            for i in self.matrix.hiddenCols:
                self.matrix.columnHeaders[i].lbl.setText("%d" % (i+1))
        # Care for hidden rows
        if (self.matrix.rows > 1):
            for j in self.matrix.hiddenRows:
                self.matrix.rowHeaders[j].lbl.setText("%d" % (j+1))

        self.short_names_bool = checked
        if (self.short_names_bool):
            self.use_short_names.setText("Long names")
        else:
            self.use_short_names.setText("Short names")

    # Sliders value change
    #   Care that some recursive process is involved and only stop when exactly same values are involved
    # Matrix view
    def matrixControlChanged(self, n):
        # Update value needed for "per Out" view
        #log.debug("Update per Output( %s )" % str(n))
        if (self.rule == "Columns_are_inputs"):
            self.perOut.out[n[1]].slider[n[0]].slider_set_value(n[2])
        else:
            self.perOut.out[n[0]].slider[n[1]].slider_set_value(n[2])

    # "per Out" view
    def sliderControlChanged(self, n):
        # Update value needed for matrix view
        #log.debug("Update Matrix( %s )" % str(n))
        if (self.rule == "Columns_are_inputs"):
            if (self.transpose):
                self.matrix.items[n[0]][n[1]].setValue(n[2])
            else:
                self.matrix.items[n[1]][n[0]].setValue(n[2])
        else:
            if (self.transpose):
                self.matrix.items[n[1]][n[0]].setValue(n[2])
            else:
                self.matrix.items[n[0]][n[1]].setValue(n[2])

    def refreshValues(self):
        for x in range(len(self.matrix.items)):
            for y in range(len(self.matrix.items[x])):
                val = self.matrix.interface.getValue(x,y)
                self.matrix.items[x][y].setValue(val)
                self.matrix.items[x][y].internalValueChanged(val)
                if (self.rule == "Columns_are_inputs"):
                    self.perOut.out[x].slider[y].slider_set_value(val)
                else:
                    self.perOut.out[y].slider[x].slider_set_value(val)

    # Update when routing is modified
    def updateRouting(self):
        self.matrix.updateRouting()
        self.perOut.updateRouting()
        
#
# vim: et ts=4 sw=4 fileencoding=utf8
