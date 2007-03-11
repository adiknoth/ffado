/* $Id$ */

/*
 *   FreeBob Streaming API
 *   FreeBob = Firewire (pro-)audio for linux
 *
 *   http://freebob.sf.net
 *
 *   Copyright (C) 2007 Pieter Palmers <pieterpalmers@users.sourceforge.net>
 *
 *   This program is free software {} you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation {} either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY {} without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program {} if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 */

#include "OscMessage.h"

namespace OSC {

IMPL_DEBUG_MODULE( OscMessage, OscMessage, DEBUG_LEVEL_VERBOSE );

OscMessage::OscMessage()
    : m_Path("")
{

}

OscMessage::OscMessage(string path, const char* types, lo_arg** argv, int argc)
    : m_Path(path)
{
    for (int i=0; i < argc; ++i) {
        switch (lo_type(types[i])) {
            /* basic OSC types */
            /** 32 bit signed integer. */
            case LO_INT32:
                addArgument((int32_t)(argv[i]->i));
                break;
            /** 32 bit IEEE-754 float. */
            case LO_FLOAT:
                addArgument(argv[i]->f);
                break;
            /** Standard C, NULL terminated string. */
            case LO_STRING:
                addArgument(string(&(argv[i]->s)));
                break;
            /* extended OSC types */
            /** 64 bit signed integer. */
            case LO_INT64:
                addArgument((int64_t)(argv[i]->h));
                break;
            
            /* unsupported types */
            /** OSC binary blob type. Accessed using the lo_blob_*() functions. */
            case LO_BLOB:
            /** OSC TimeTag type, represented by the lo_timetag structure. */
            case LO_TIMETAG:
            /** 64 bit IEEE-754 double. */
            case LO_DOUBLE:
            /** Standard C, NULL terminated, string. Used in systems which
              * distinguish strings and symbols. */
            case LO_SYMBOL:
            /** Standard C, 8 bit, char variable. */
            case LO_CHAR:
            /** A 4 byte MIDI packet. */
            case LO_MIDI:
            /** Sybol representing the value True. */
            case LO_TRUE:
            /** Sybol representing the value False. */
            case LO_FALSE:
            /** Sybol representing the value Nil. */
            case LO_NIL:
            /** Sybol representing the value Infinitum. */
            case LO_INFINITUM:
            default:
                debugOutput(DEBUG_LEVEL_NORMAL, 
                            "unsupported osc type: %c\n", lo_type(types[i]));
        }
    }

}

OscMessage::~OscMessage() {

}

lo_message
OscMessage::makeLoMessage() {
    lo_message m=lo_message_new();
    for ( OscArgumentVectorIterator it = m_arguments.begin();
    it != m_arguments.end();
    ++it )
    {
        if((*it).isInt()) {
            lo_message_add_int32(m,(*it).getInt());
        }
        if((*it).isInt64()) {
            lo_message_add_int64(m,(*it).getInt64());
        }
        if((*it).isFloat()) {
            lo_message_add_float(m,(*it).getFloat());
        }
        if((*it).isString()) {
            lo_message_add_string(m,(*it).getString().c_str());
        }
    }
    return m;
}

void
OscMessage::setPath(string p) {
    m_Path=p;
}

OscArgument&
OscMessage::getArgument(unsigned int idx) {
    return m_arguments.at(idx);
}

unsigned int
OscMessage::nbArguments() {
    return m_arguments.size();
}

void
OscMessage::addArgument(int32_t v) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Adding int argument %ld\n", v);
    m_arguments.push_back(OscArgument(v));
}

void
OscMessage::addArgument(int64_t v) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Adding long int argument %lld\n", v);
    m_arguments.push_back(OscArgument(v));
}

void
OscMessage::addArgument(float v) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Adding float argument: %f\n",v);
    m_arguments.push_back(OscArgument(v));
}

void
OscMessage::addArgument(string v) {
    debugOutput(DEBUG_LEVEL_VERY_VERBOSE,"Adding string argument: %s\n",v.c_str());
    m_arguments.push_back(OscArgument(v));
}

void
OscMessage::print() {
    debugOutput(DEBUG_LEVEL_NORMAL, "Message on: %s\n", m_Path.c_str());
    for ( OscArgumentVectorIterator it = m_arguments.begin();
    it != m_arguments.end();
    ++it )
    {
        (*it).print();
    }
}

} // end of namespace OSC
