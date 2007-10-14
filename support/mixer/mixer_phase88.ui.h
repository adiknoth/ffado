/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

void Phase88Control::switchFrontState( int )
{
    self.setSelector('frontback', a0)
}

void Phase88Control::switchOutAssign( int )
{
    self.setSelector('outassign', a0)
}

void Phase88Control::switchWaveInAssign( int )
{
    self.setSelector('inassign', a0)
}

void Phase88Control::switchSyncSource( int )
{
    self.setSelector('syncsource', a0)
}

void Phase88Control::switchExtSyncSource( int )
{
    self.setSelector('externalsync', a0)
}

void Phase88Control::setVolume12( int )
{
    self.setVolume('line12', a0)
}

void Phase88Control::setVolume34( int )
{
    self.setVolume('line34', a0)
}

void Phase88Control::setVolume56( int )
{
    self.setVolume('line56', a0)
}

void Phase88Control::setVolume78( int )
{
    self.setVolume('line78', a0)
}

void Phase88Control::setVolumeSPDIF( int )
{
    self.setVolume('spdif', a0)
}

void Phase88Control::setVolumeWavePlay( int )
{
    self.setVolume('waveplay', a0)
}

void Phase88Control::setVolumeMaster( int )
{
    self.setVolume('master', a0)
}

void Phase88Control::setVolume( QString, int )
{
    name=a0
    vol = -a1
    
    print "setting %s volume to %d" % (name, vol)
    self.hw.setContignuous(self.VolumeControls[name][0], vol)
}

void Phase88Control::setSelector( QString, int )
{
    name=a0
    state = a1
    
    print "setting %s state to %d" % (name, state)
    self.hw.setDiscrete(self.SelectorControls[name][0], state)
}

void Phase88Control::init()
{
    print "Init Phase88 mixer window"

    self.VolumeControls={
        'master':    ['/Mixer/Feature_1', self.sldInputMaster], 
        'line12' :   ['/Mixer/Feature_2', self.sldInput12],
        'line34' :   ['/Mixer/Feature_3', self.sldInput34],
        'line56' :   ['/Mixer/Feature_4', self.sldInput56],
        'line78' :   ['/Mixer/Feature_5', self.sldInput78],
        'spdif' :    ['/Mixer/Feature_6', self.sldInputSPDIF],
        'waveplay' : ['/Mixer/Feature_7', self.sldInputWavePlay],
        }

    self.SelectorControls={
        'outassign':    ['/Mixer/Selector_6', self.comboOutAssign], 
        'inassign':     ['/Mixer/Selector_7', self.comboInAssign], 
        'externalsync': ['/Mixer/Selector_8', self.comboExtSync], 
        'syncsource':   ['/Mixer/Selector_9', self.comboSyncSource], 
        'frontback':    ['/Mixer/Selector_10', self.comboFrontBack], 
    }
}

void Phase88Control::initValues()
{
    for name, ctrl in self.VolumeControls.iteritems():
        vol = self.hw.getContignuous(ctrl[0])
        print "%s volume is %d" % (name , vol)
        ctrl[1].setValue(-vol)

    for name, ctrl in self.SelectorControls.iteritems():
        state = self.hw.getDiscrete(ctrl[0])
        print "%s state is %d" % (name , state)
        ctrl[1].setCurrentItem(state)    
}
