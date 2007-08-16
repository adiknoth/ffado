#! /usr/bin/python

def pyuic_action( target, source, env ):
	print "'"+str( target[0] )+"'"
	print "'"+str( source[0] )+"'"
	print env.Command( str( target[0] ), str( source[0] ), action = "pyuic $SOURCE > $TARGET" )
	return 0

def pyuic_string( target, source, env ):
	return "building '%s' from '%s'" % ( str(target[0]), str( source[0] ) )

def generate( env, **kw ):
	action = env.Action( pyuic_action, pyuic_string )
	env['BUILDERS']['PyUIC'] = env.Builder( action=action, src_suffix=".ui", single_source=True )

def exists( env ):
	return 1

