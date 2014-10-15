from PyQt4.QtCore import SIGNAL, SLOT, QObject, Qt
from PyQt4.QtGui import QWidget, QMessageBox, QHBoxLayout, QVBoxLayout, QTabWidget, QGroupBox, QLabel, QDial, QSlider, QToolButton, QSizePolicy
from math import log10
from ffado.config import *

import logging
log = logging.getLogger('MAudioBeBoB')

class MAudio_BeBoB_Input_Widget(QWidget):
    def __init__(self, parent=None):
        QWidget.__init__(self, parent)
        uicLoad("ffado/mixer/maudio_bebob_input", self)

class MAudio_BeBoB_Output_Widget(QWidget):
    def __init__(self, parent=None):
        QWidget.__init__(self, parent)
        uicLoad("ffado/mixer/maudio_bebob_output", self)


class MAudio_BeBoB(QWidget):
    def __init__(self, parent=None):
        QWidget.__init__(self, parent)

    info = {
        0x0000000a: (0, "Ozonic"),
        0x00010062: (1, "Firewire Solo"),
        0x00010060: (2, "Firewire Audiophile"),
        0x00010046: (3, "Firewire 410"),
        0x00010071: (4, "Firewire 1814"),
        0x00010091: (4, "ProjectMix I/O"),
    }

    labels = (
        {
            "inputs":   ("Analog 1/2", "Analog 3/4",
                         "Stream 1/2", "Stream 3/4"),
            "mixers":   ("Mixer 1/2", "Mixer 3/4"),
            "outputs":  ("Analog 1/2", "Analog 3/4")
        },
        {
            "inputs":   ("Analog 1/2", "Digital 1/2",
                         "Stream 1/2", "Stream 3/4"),
            "mixers":   ("Mixer 1/2", "Mixer 3/4"),
            "outputs":  ("Analog 1/2", "Digital 1/2")
        },
        {
            "inputs":   ("Analog 1/2", "Digital 1/2",
                         "Stream 1/2", "Stream 3/4", "Stream 5/6"),
            "mixers":   ("Mixer 1/2", "Mixer 3/4", "Mixer 5/6", "Aux 1/2"),
            "outputs":  ("Analog 1/2", "Analog 3/4", "Digital 1/2",
                         "Headphone 1/2")
        },
        {
            "inputs":   ("Analog 1/2", "Digital 1/2",
                         "Stream 1/2", "Stream 3/4", "Stream 5/6",
                         "Stream 7/8", "Stream 9/10"),
            "mixers":   ("Mixer 1/2", "Mixer 3/4", "Mixer 5/6", "Mixer 7/8",
                         "Mixer 9/10", "Aux 1/2"),
            "outputs":  ("Analog 1/2", "Analog 3/4", "Analog 5/6", "Analog 7/8",
                         "Digital 1/2", "Headphone 1/2")
        },
        {
            "inputs":   ("Analog 1/2", "Analog 3/4", "Analog 5/6", "Analog 7/8",
                         "Stream 1/2", "Stream 3/4", "S/PDIF 1/2",
                         "ADAT 1/2", "ADAT 3/4", "ADAT 5/6", "ADAT 7/8"),
            "mixers":   ("Mixer 1/2", "Mixer 3/4", "Aux 1/2"),
            "outputs":  ("Analog 1/2", "Analog 3/4",
                         "Headphone 1/2", "Headphone 3/4")
        }
    )

    # hardware inputs and stream playbacks
    #  format: function_id/channel_idx/panning-able
    #  NOTE: function_id = channel_idx = panning-able =  labels["inputs"]
    inputs = (
        (
            (0x03, 0x04, 0x01, 0x02),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02), (0x01, 0x02)),
            (True, True, False, False)
        ),
        (
            (0x01, 0x02, 0x04, 0x03),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02), (0x01, 0x02)),
            (True, True, False, False)
        ),
        (
            (0x04, 0x05, 0x01, 0x02, 0x03),
            ((0x01, 0x02), (0x01, 0x02),
             (0x01, 0x02), (0x01, 0x02), (0x01, 0x02)),
            (True, True, False, False, False)
        ),
        (
            (0x03, 0x04, 0x02, 0x01, 0x01, 0x01, 0x01),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02),
             (0x01, 0x02), (0x03, 0x04), (0x05, 0x06), (0x07, 0x08)),
            (True, True, False, False, False, False, False)
        ),
        (
            (0x01, 0x02, 0x03, 0x04, 0x0a, 0x0b, 0x05, 0x06, 0x07, 0x08, 0x09),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02), (0x01, 0x02),
             (0x01, 0x02), (0x01, 0x02), (0x01, 0x02),
             (0x01, 0x02), (0x01, 0x02), (0x01, 0x02), (0x01, 0x02)),
            (True, True, True, True, False, False, True,
             True, True, True, True)
        )
    )

    # jack sources except for headphone
    #  format: function_id/source id
    #  NOTE: "function_id" = labels["output"] - "Headphone 1/2/3/4"
    #  NOTE: "source_id" = labels["mixer"]
    jack_src = (
        None,
        None,
        ((0x01, 0x02, 0x03),
         (0x00, 0x00, 0x00, 0x01)),
        ((0x02, 0x03, 0x04, 0x05, 0x06),
         (0x00, 0x00, 0x00, 0x00, 0x00, 0x01)),
        ((0x03, 0x04),
         (0x00, 0x01, 0x02))
    )

    # headphone sources
    #  format: sink id/source id
    #  NOTE: "source_id" = labels["mixer"]
    hp_src = (
        None,
        None,
        ((0x04,),
         (0x00, 0x01, 0x02, 0x03)),
        ((0x07,),
         (0x02, 0x03, 0x04, 0x05, 0x06, 0x07)),
        ((0x01, 0x02),
         (0x01, 0x02, 0x04))
    )

    # hardware outputs
    #  format: function id
    #  NOTE: "function_id" = labels["output"]
    outputs = (
        (0x05, 0x06),
        (0x02, 0x03),
        (0x0c, 0x0d, 0x0e, 0x0f),
        (0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f),
        (0x0c, 0x0d, 0x0f, 0x10)
    )

    # Mixer inputs/outputs
    #  format: function_id/out_stereo_channel_id/in_id/in_stereo_channel_id
    #  NOTE: function_id = out_stereo_channel_id = labels["mixers"]
    #  NOTE: in_id = in_stereo_channel_id = labels["inputs"]
    mixers = (
        (
            (0x01, 0x02),
            ((0x01, 0x02), (0x01, 0x02)),
             (0x02, 0x03, 0x00, 0x01),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02), (0x01, 0x02))
        ),
        (
            (0x01, 0x01),
            ((0x01, 0x02), (0x03, 0x04)),
            (0x00, 0x01, 0x03, 0x02),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02), (0x01, 0x02))
        ),
        (
            (0x01, 0x02, 0x03, 0x04),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02), (0x01, 0x02)),
            (0x03, 0x04, 0x00, 0x01, 0x02),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02),
             (0x01, 0x02), (0x01, 0x02))
        ),
        (
            (0x01, 0x01, 0x01, 0x01, 0x01, 0x07),
            ((0x01, 0x02), (0x03, 0x04),
             (0x05, 0x06), (0x07, 0x08), (0x09, 0x0a), (0x01, 0x02)),
            (0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00),
            ((0x01, 0x02), (0x01, 0x02),
             (0x01, 0x02), (0x01, 0x02), (0x03, 0x04),
             (0x05, 0x06), (0x07, 0x08))
        ),
        (
            (0x01, 0x02, 0x03),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02)),
            (0x01, 0x01, 0x01, 0x01,
             0x02, 0x02, 0x03,
             0x04, 0x04, 0x04, 0x04),
            ((0x01, 0x02), (0x03, 0x04), (0x05, 0x06), (0x07, 0x08),
             (0x01, 0x02), (0x03, 0x04), (0x01, 0x02),
             (0x01, 0x02), (0x03, 0x04), (0x05, 0x06), (0x07, 0x08))
        )
    )

    # Aux mixer inputs/outputs
    #  format: function_id/input_id/input_stereo_channel_id
    #  NOTE: input_id = labels["inputs"]
    aux = (
        None,
        None,
        (
            0x0b,
            (0x09, 0x0a, 0x06, 0x07, 0x08),
            ((0x01, 0x02), (0x01, 0x02),
             (0x01, 0x02), (0x01, 0x02), (0x01, 0x02))
        ),
        (
            0x09,
            (0x07, 0x08, 0x06, 0x05, 0x05, 0x05, 0x05),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02),
             (0x01, 0x02), (0x03, 0x04), (0x05, 0x06), (0x07, 0x08))
        ),
        (
            0x0e,
            (0x13, 0x14, 0x15, 0x16, 0x11, 0x12, 0x17, 0x18, 0x19, 0x1a, 0x1b),
            ((0x01, 0x02), (0x01, 0x02), (0x01, 0x02), (0x01, 0x02),
             (0x01, 0x02), (0x01, 0x02), (0x01, 0x02),
             (0x01, 0x02), (0x01, 0x02), (0x01, 0x02), (0x01, 0x02))
        )
    )

    def getDisplayTitle(self):
        model = self.configrom.getModelId()
        return self.info[model][1]

    def buildMixer(self):
        self.Selectors = {}
        self.Pannings = {}
        self.Volumes = {}
        self.Mutes = {}
        self.Mixers = {}
        self.FW410HP = 0

        model = self.configrom.getModelId()
        if model not in self.info:
            return

        self.id = self.info[model][0]
        name = self.info[model][1]

        tabs_layout = QHBoxLayout(self)
        tabs = QTabWidget(self)

        self.addInputTab(tabs)
        self.addMixTab(tabs)
        if self.aux[self.id] is not None:
            self.addAuxTab(tabs)
        self.addOutputTab(tabs)

        tabs_layout.addWidget(tabs)


    def addInputTab(self, tabs):
        tab_input = QWidget(self)
        tabs.addTab(tab_input, "In")

        tab_input_layout = QHBoxLayout()
        tab_input.setLayout(tab_input_layout)

        in_labels = self.labels[self.id]["inputs"]
        in_ids = self.inputs[self.id][0]
        in_ch_ids = self.inputs[self.id][1]
        in_pan = self.inputs[self.id][2]

        for i in range(len(in_ids)):
            l_idx = self.inputs[self.id][1][i][0]
            r_idx = self.inputs[self.id][1][i][1]

            widget = MAudio_BeBoB_Input_Widget(tab_input)
            tab_input_layout.addWidget(widget)

            widget.name.setText(in_labels[i])

            self.Volumes[widget.l_sld] = (
                "/Mixer/Feature_Volume_%d" % in_ids[i], l_idx,
                widget.r_sld, r_idx,
                widget.link
            )
            self.Volumes[widget.r_sld] = (
                "/Mixer/Feature_Volume_%d" % in_ids[i], r_idx,
                widget.l_sld, l_idx,
                widget.link
            )
            self.Mutes[widget.mute] = (widget.l_sld, widget.r_sld)

            if not in_pan[i]:
                widget.l_pan.setDisabled(True)
                widget.r_pan.setDisabled(True)
            else:
                self.Pannings[widget.l_pan] = (
                    "/Mixer/Feature_LRBalance_%d" % in_ids[i],
                    l_idx
                )
                self.Pannings[widget.r_pan] = (
                    "/Mixer/Feature_LRBalance_%d" % in_ids[i],
                    r_idx
                )

        tab_input_layout.addStretch()


    def addMixTab(self, tabs):
        tab_mix = QWidget(self)
        tabs.addTab(tab_mix, "Mix")

        tab_layout = QHBoxLayout()
        tab_mix.setLayout(tab_layout)

        in_labels = self.labels[self.id]["inputs"]
        in_idxs = self.inputs[self.id][0]

        mix_labels = self.labels[self.id]["mixers"]
        mix_idxs = self.mixers[self.id][0]

        for i in range(len(mix_idxs)):
            if mix_labels[i] == 'Aux 1/2':
                continue

            grp = QGroupBox(tab_mix)
            grp_layout = QVBoxLayout()
            grp.setLayout(grp_layout)
            tab_layout.addWidget(grp)

            label = QLabel(grp)
            grp_layout.addWidget(label)

            label.setText(mix_labels[i])
            label.setAlignment(Qt.AlignCenter)
            label.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Minimum)

            for j in range(len(in_idxs)):
                mix_in_id = self.mixers[self.id][2][j]

                button = QToolButton(grp)
                grp_layout.addWidget(button)

                button.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Minimum)
                button.setText('%s In' % in_labels[j])
                button.setCheckable(True)

                self.Mixers[button] = (
                    "/Mixer/EnhancedMixer_%d" % mix_idxs[i],
                    mix_in_id, j, i
                )

            grp_layout.addStretch()
        tab_layout.addStretch()

    def addAuxTab(self, tabs):
        #local functions
        def addLinkButton(parent, layout):
            button = QToolButton(grp)
            grp_layout.addWidget(button)
            button.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Minimum)
            button.setText('Link')
            button.setCheckable(True)
            return button
        def addMuteButton(parent, layout):
            button = QToolButton(parent)
            layout.addWidget(button)
            button.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Minimum)
            button.setText('Mute')
            button.setCheckable(True)
            return button

        # local processing
        tab_aux = QWidget(self)
        tabs.addTab(tab_aux, "Aux")

        layout = QHBoxLayout()
        tab_aux.setLayout(layout)

        aux_label = self.labels[self.id]["mixers"][-1]
        aux_id = self.aux[self.id][0]

        aux_in_labels = self.labels[self.id]["inputs"]
        aux_in_ids = self.aux[self.id][1]

        for i in range(len(aux_in_ids)):
            in_ch_l = self.aux[self.id][2][i][0]
            in_ch_r = self.aux[self.id][2][i][1]

            grp = QGroupBox(tab_aux)
            grp_layout = QVBoxLayout()
            grp.setLayout(grp_layout)
            layout.addWidget(grp)

            label = QLabel(grp)
            grp_layout.addWidget(label)
            label.setText("%s\nIn" % aux_in_labels[i])
            label.setAlignment(Qt.AlignCenter)

            grp_sld = QGroupBox(grp)
            grp_sld_layout = QHBoxLayout()
            grp_sld.setLayout(grp_sld_layout)
            grp_layout.addWidget(grp_sld)

            l_sld = QSlider(grp_sld)
            grp_sld_layout.addWidget(l_sld)
            r_sld = QSlider(grp_sld)
            grp_sld_layout.addWidget(r_sld)

            button = addLinkButton(grp, grp_layout)
            self.Volumes[l_sld] = (
                "/Mixer/Feature_Volume_%d" % aux_in_ids[i], in_ch_l,
                r_sld, in_ch_r,
                button
            )
            self.Volumes[r_sld] = (
                "/Mixer/Feature_Volume_%d" % aux_in_ids[i], in_ch_r,
                l_sld, in_ch_l,
                button
            )

            button = addMuteButton(grp, grp_layout)
            self.Mutes[button] = (l_sld, r_sld)

        grp = QGroupBox(tab_aux)
        grp_layout = QVBoxLayout()
        grp.setLayout(grp_layout)
        layout.addWidget(grp)

        label = QLabel(grp)
        grp_layout.addWidget(label)
        label.setText("%s\nOut" % aux_label)
        label.setAlignment(Qt.AlignCenter)

        grp_sld = QGroupBox(grp)
        grp_sld_layout = QHBoxLayout()
        grp_sld.setLayout(grp_sld_layout)
        grp_layout.addWidget(grp_sld)

        l_sld = QSlider(grp_sld)
        grp_sld_layout.addWidget(l_sld)
        r_sld = QSlider(grp_sld)
        grp_sld_layout.addWidget(r_sld)

        button = addLinkButton(grp, grp_layout)
        self.Volumes[l_sld] = (
            "/Mixer/Feature_Volume_%d" % aux_id, 1,
            r_sld, 2,
            button
        )
        self.Volumes[r_sld] = (
            "/Mixer/Feature_Volume_%d" % aux_id, 2,
            l_sld, 1,
            button
        )

        button = addMuteButton(grp, grp_layout)
        self.Mutes[button] = (l_sld, r_sld)

        layout.addStretch()

    def addOutputTab(self, tabs):
        tab_out = QWidget(self)
        tabs.addTab(tab_out, "Out")

        layout = QHBoxLayout()
        tab_out.setLayout(layout)

        out_labels = self.labels[self.id]["outputs"]
        out_ids = self.outputs[self.id]

        mixer_labels = self.labels[self.id]["mixers"]

        if self.jack_src[self.id] is None:
            for i in range(len(out_ids)):
                label = QLabel(tab_out)
                layout.addWidget(label)
                label.setText("%s Out is fixed to %s Out" % (mixer_labels[i], out_labels[i]))
            return

        mixer_ids = self.jack_src[self.id][1]


        for i in range(len(out_ids)):
            out_label = out_labels[i]
            if out_label.find('Headphone') >= 0:
                continue

            out_id = self.jack_src[self.id][0][i]

            widget = MAudio_BeBoB_Output_Widget(tab_out)
            layout.addWidget(widget)

            widget.name.setText(out_label)

            self.Volumes[widget.l_sld] = (
                "/Mixer/Feature_Volume_%d" % out_ids[i], 1,
                widget.r_sld, 2,
                widget.link
            )
            self.Volumes[widget.r_sld] = (
                "/Mixer/Feature_Volume_%d" % out_ids[i], 2,
                widget.l_sld, 1,
                widget.link
            )
            self.Mutes[widget.mute] = (widget.l_sld, widget.r_sld)

            self.Selectors[widget.cmb_src] = ("/Mixer/Selector_%d" % out_id, )

            for j in range(len(mixer_ids)):
                if (i != j and j != len(mixer_ids) - 1):
                    continue
                widget.cmb_src.addItem("%s Out" % mixer_labels[j], mixer_ids[j])

        # add headphone
        hp_idx = 0
        for i in range(len(out_ids)):
            out_label = out_labels[i]
            if out_label.find('Headphone') < 0:
                continue

            hp_label = self.labels[self.id]["outputs"][i]
            hp_id = self.hp_src[self.id][0][hp_idx]
            hp_idx += 1

            mixer_labels = self.labels[self.id]["mixers"]

            widget = MAudio_BeBoB_Output_Widget(tab_out)
            layout.addWidget(widget)

            widget.name.setText(hp_label)

            mixer_labels = self.labels[self.id]["mixers"]
            mixer_ids = self.mixers[self.id][0]

            self.Volumes[widget.l_sld] = (
                "/Mixer/Feature_Volume_%d" % out_ids[i], 1,
                widget.r_sld, 2,
                widget.link
            )
            self.Volumes[widget.r_sld] = (
                "/Mixer/Feature_Volume_%d" % out_ids[i], 2,
                widget.l_sld, 1,
                widget.link
            )
            self.Mutes[widget.mute] = (widget.l_sld, widget.r_sld)

            for i in range(len(mixer_ids)):
                widget.cmb_src.addItem("%s Out" % mixer_labels[i], mixer_ids[i])

            if self.id != 3:
                self.Selectors[widget.cmb_src] = ("/Mixer/Selector_%d" % hp_id)
            else:
                QObject.connect(widget.cmb_src, SIGNAL('activated(int)'), self.update410HP)
                self.FW410HP = widget.cmb_src

        layout.addStretch()

    def initValues(self):
        for ctl, params in self.Selectors.items():
            path = params[0]
            state = self.hw.getDiscrete(path)
            ctl.setCurrentIndex(state)
            QObject.connect(ctl, SIGNAL('activated(int)'), self.updateSelector)

        #       Right - Center - Left
        # 0x8000 - 0x0000 - 0x0001 - 0x7FFE
        #        ..., -1, 0, +1, ...
        for ctl, params in self.Pannings.items():
            path = params[0]
            idx = params[1]
            curr = self.hw.getContignuous(path, idx)
            state = -(curr / 0x7FFE) * 50 + 50
            ctl.setValue(state)
            QObject.connect(ctl, SIGNAL('valueChanged(int)'), self.updatePanning)

        for ctl, params in self.Volumes.items():
            path = params[0]
            idx = params[1]
            pair = params[2]
            p_idx = params[3]
            link = params[4]

            db = self.hw.getContignuous(path, idx)
            vol = self.db2vol(db)
            ctl.setValue(vol)
            QObject.connect(ctl, SIGNAL('valueChanged(int)'), self.updateVolume)

            # to activate link button, a pair is checked twice, sign...
            pair_db = self.hw.getContignuous(path, p_idx)
            if pair_db== db:
                link.setChecked(True)

        for ctl, params in self.Mutes.items():
            QObject.connect(ctl, SIGNAL('clicked(bool)'), self.updateMute)

        for ctl, params in self.Mixers.items():
            path = params[0]
            in_id = params[1]
            mix_in_idx = params[2]
            mix_out_idx = params[3]
            in_ch_l = self.mixers[self.id][3][mix_in_idx][0]
            out_ch_l = self.mixers[self.id][1][mix_out_idx][0]
            # see /libffado/src/bebob/bebob_mixer.cpp
            mux_id = self.getMultiplexedId(in_id, in_ch_l, out_ch_l)
            curr = self.hw.getContignuous(path, mux_id)
            if (curr == 0):
                state = True
            else:
                state = False
            ctl.setChecked(state)
            QObject.connect(ctl, SIGNAL('clicked(bool)'), self.updateMixer)

        if self.id == 3:
            self.read410HP()

    # helper functions
    def vol2db(self, vol):
        return (log10(vol + 1) - 2) * 16384

    def db2vol(self, db):
        return pow(10, db / 16384 + 2) - 1

    def getMultiplexedId(self, in_id, in_ch_l, out_ch_l):
        # see /libffado/src/bebob/bebob_mixer.cpp
        return (in_id << 8) | (in_ch_l << 4) | (out_ch_l << 0)

    def updateSelector(self, state):
        sender = self.sender()
        path = self.Selectors[sender][0]
        log.debug("set %s to %d" % (path, state))
        self.hw.setDiscrete(path, state)

    def updatePanning(self, state):
        sender = self.sender()
        path = self.Pannings[sender][0]
        idx = self.Pannings[sender][1]
        value = (state - 50) * 0x7FFE / -50
        log.debug("set %s for %d to %d(%d)" % (path, idx, value, state))
        self.hw.setContignuous(path, value, idx)

    def updateVolume(self, vol):
        sender = self.sender()
        path = self.Volumes[sender][0]
        idx = self.Volumes[sender][1]
        pair = self.Volumes[sender][2]
        p_idx = self.Volumes[sender][3]
        link = self.Volumes[sender][4]

        db = self.vol2db(vol)
        log.debug("set %s for %d to %d(%d)" % (path, idx, db, vol))
        self.hw.setContignuous(path, db, idx)

        if link.isChecked():
            pair.setValue(vol)

    # device remeber gain even if muted
    def updateMute(self, state):
        sender = self.sender()
        l_sld = self.Mutes[sender][0]
        r_sld = self.Mutes[sender][1]

        if state:
            db = 0x8000
        else:
            db = 0x0000
        for w in [l_sld, r_sld]:
            path = self.Volumes[w][0]
            self.hw.setContignuous(self.Volumes[w][0], db)
            w.setDisabled(state)

    def updateMixer(self, checked):
        if checked:
            state = 0x0000
        else:
            state = 0x8000

        sender = self.sender()
        path = self.Mixers[sender][0]
        in_id = self.Mixers[sender][1]
        mix_in_idx = self.Mixers[sender][2]
        mix_out_idx = self.Mixers[sender][3]
        in_ch_l = self.mixers[self.id][3][mix_in_idx][0]
        out_ch_l = self.mixers[self.id][1][mix_out_idx][0]

        mux_id = self.getMultiplexedId(in_id, in_ch_l, out_ch_l)

        log.debug("set %s for 0x%04X(%d/%d/%d) to %d" % (path, mux_id, in_id, in_ch_l, out_ch_l, state))
        self.hw.setContignuous(path, state, mux_id)

    def read410HP(self):
        path = "/Mixer/Selector_7"
        sel = self.hw.getDiscrete(path)
        if sel > 0:
            enbl = 5
        else:
            enbl = -1

        path = "/Mixer/EnhancedMixer_7"
        for i in range(5):
            in_id = 0
            in_ch_l = i * 2 + 1
            out_ch_l = 1
            mux_id = self.getMultiplexedId(in_id, in_ch_l, out_ch_l)
            state = self.hw.getContignuous(path, mux_id)
            if enbl < 0 and state == 0:
                enbl = i
            else:
                self.hw.setContignuous(path, 0x8000, mux_id)
        # if inconsistency between Selector and Mixer, set AUX as default
        if enbl == -1:
            self.hw.setDiscrete('/Mixer/Selector_7', 1)
            enbl = 5

        self.FW410HP.setCurrentIndex(enbl)

    def update410HP(self, state):
        hp_id = self.hp_src[self.id][0][0]

        # each output from mixer can be multiplexed in headphone
        # but here they are exclusive because of GUI simpleness, sigh...
        path = "/Mixer/EnhancedMixer_7"
        for i in range(5):
            in_id = 0
            in_ch_l = i * 2 + 1
            out_ch_l = 1
            mux_id = self.getMultiplexedId(in_id, in_ch_l, out_ch_l)
            if i == state:
                value = 0x0000
            else:
                value = 0x8000
            self.hw.setContignuous(path, value, mux_id)

        # Mixer/Aux is selectable exclusively
        path = "/Mixer/Selector_7"
        sel = (state == 5)
        self.hw.setDiscrete(path, sel)

