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
    
    if a0 == 0:
        state=0
    else :
        state=1
                        
    print "switching front/back state to %d" % state

    osc.Message("/devicemanager/dev0/GenericMixer/Selector/10", ["set", "value", state]).sendlocal(17820)                        
}

void Phase88Control::switchOutAssign( int )
{
    print "switching out assign to %d" % a0
    osc.Message("/devicemanager/dev0/GenericMixer/Selector/6", ["set", "value", a0]).sendlocal(17820)
}

void Phase88Control::switchWaveInAssign( int )
{
    print "switching input assign to %d" % a0
    osc.Message("/devicemanager/dev0/GenericMixer/Selector/7", ["set", "value", a0]).sendlocal(17820)
}

void Phase88Control::switchSyncSource( int )
{
    print "switching sync source to %d" % a0
    osc.Message("/devicemanager/dev0/GenericMixer/Selector/9", ["set", "value", a0]).sendlocal(17820)
}

void Phase88Control::switchExtSyncSource( int )
{
    print "switching external sync source to %d" % a0
    osc.Message("/devicemanager/dev0/GenericMixer/Selector/8", ["set", "value", a0]).sendlocal(17820)
}

void Phase88Control::setVolume12( int )
{
    vol = -a0
    print "setting volume for 1/2 to %d" % vol
    osc.Message("/devicemanager/dev0/GenericMixer/Feature/2", ["set", "volume", 0, vol]).sendlocal(17820)
}

void Phase88Control::setVolume34( int )
{
    vol = -a0
    print "setting volume for 3/4 to %d" % vol
    osc.Message("/devicemanager/dev0/GenericMixer/Feature/3", ["set", "volume", 0, vol]).sendlocal(17820)
}

void Phase88Control::setVolume56( int )
{
    vol = -a0
    print "setting volume for 5/6 to %d" % vol
    osc.Message("/devicemanager/dev0/GenericMixer/Feature/4", ["set", "volume", 0, vol]).sendlocal(17820)
}

void Phase88Control::setVolume78( int )
{
    vol = -a0
    print "setting volume for 7/8 to %d" % vol
    osc.Message("/devicemanager/dev0/GenericMixer/Feature/5", ["set", "volume", 0, vol]).sendlocal(17820)
}

void Phase88Control::setVolumeSPDIF( int )
{
    vol = -a0
    print "setting volume for S/PDIF to %d" % vol
    osc.Message("/devicemanager/dev0/GenericMixer/Feature/6", ["set", "volume", 0, vol]).sendlocal(17820)   
}

void Phase88Control::setVolumeWavePlay( int )
{
    vol = -a0
    print "setting volume for WavePlay to %d" % vol
    osc.Message("/devicemanager/dev0/GenericMixer/Feature/7", ["set", "volume", 0, vol]).sendlocal(17820)
}

void Phase88Control::setVolumeMaster( int )
{
    vol = -a0
    print "setting master volume to %d" % vol
    osc.Message("/devicemanager/dev0/GenericMixer/Feature/1", ["set", "volume", 0, vol]).sendlocal(17820)    
}
