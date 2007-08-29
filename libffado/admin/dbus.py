#!/usr/bin/python

#
# xml translator
#

def dbusxx_xml2cpp_emitter(target, source, env):
	env.Depends(target, "#/external/dbus/dbusxx-xml2cpp" )
	return (target, source)

def dbusxx_xml2cpp_adaptor_action( target, source, env ):
	env.Execute( "./external/dbus/dbusxx-xml2cpp %s --adaptor=%s" % ( source[0], target[0] ) )
	return 0

def dbusxx_xml2cpp_proxy_action( target, source, env ):
	env.Execute( "./external/dbus/dbusxx-xml2cpp %s --proxy=%s" % ( source[0], target[0] ) )
	return 0

def generate( env, **kw ):
	env['BUILDERS']['Xml2Cpp_Adaptor'] = env.Builder(action = dbusxx_xml2cpp_adaptor_action,
		emitter = dbusxx_xml2cpp_emitter,
		suffix = '.h', src_suffix = '.xml')
	env['BUILDERS']['Xml2Cpp_Proxy'] = env.Builder(action = dbusxx_xml2cpp_proxy_action,
		emitter = dbusxx_xml2cpp_emitter,
		suffix = '.h', src_suffix = '.xml', single_source=True )

def exists( env ):
	return 1

