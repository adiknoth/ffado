#!/bin/sh
#

#
# Script to gather info useful to debug FreeBoB
#
# should be ran as root 
#

#functions
function log()
{
    echo "" >> $LOG_FILE
    echo "$1" >> $LOG_FILE
    echo "" >> $LOG_FILE
}

function check_jack() 
{
  jack=$1
  found=0
  cnt=0
  
  echo "" >> $LOG_FILE
  version=$(pkg-config --modversion "$jack" 2>> $LOG_FILE)
  echo "" >> $LOG_FILE
  
  if [ $? == 0 ]
  then
    echo "  pkg-config: Using $jack version $version"
    found=1
  else
    echo "  pkg-config: $jack not found or depency error!"
    found=0
  fi
  
  lib="lib$jack"
  
  lib_in_dir $lib "/usr/lib"
  cnt=$(($cnt + $?))
  lib_in_dir $lib "/usr/local/lib"
  cnt=$(($cnt + $?))

  if [ $cnt == 0 ]
  then
    echo "  WARNING: ($lib) no shared library file found in standard locations"
  fi
  
  if [ $cnt -gt 1 ]
  then
    echo "  WARNING: ($lib) more than one ($cnt) shared library file found"
  fi

}

function check_library() 
{
  lib=$1
  found=0
  cnt=0
  
  echo "" >> $LOG_FILE
  version=$(pkg-config --modversion "$lib" 2>> $LOG_FILE)
  echo "" >> $LOG_FILE
  
  if [ $? == 0 ]
  then
    echo "  pkg-config: Using $lib version $version"
    found=1
  else
    echo "  pkg-config: $lib not found or depency error!"
    found=0
  fi
  
  lib_in_dir $lib "/usr/lib"
  cnt=$(($cnt + $?))
  lib_in_dir $lib "/usr/local/lib"
  cnt=$(($cnt + $?))

  if [ $cnt == 0 ]
  then
    echo "  WARNING: ($lib) no shared library file found in standard locations"
  fi
  
  if [ $cnt -gt 1 ]
  then
    echo "  WARNING: ($lib) more than one ($cnt) shared library file found"
  fi

}

function lib_in_dir()
{
  lib=$1
  dir=$2
  
  if [ -e "$dir/$lib.so" ]
  then
    echo "  $lib.so found in $dir"
    return 1
  else 
    echo "  $lib.so not found in $dir"
    return 0
  fi
}

function check_module()
{
  module=$1
  descr=$2
  
  found=$(/sbin/lsmod | grep $module | awk '{print $1}')
  
  if [ "$found" ] 
  then
    echo "  $descr found"
    return 0
  else
    echo "  ERROR: $descr not found"
    return 1
  fi
}

# main script

echo ""
echo "FreeBoB - Firewire audio for Linux"
echo " System & configuration info gathering script"
echo "============================================="
echo ""
echo "Please consult the FAQ at http://freebob.sf.net"
echo "before asking questions on the mailing list(s)"
echo ""
echo "Script usage:"
echo ""
echo " freebob_debug_log username [logfile]"
echo ""
echo "   username: The username to perform permission testing for"
echo "             This should be the username of the user that is going"
echo "             to run the FreeBoB client software, e.g. jackd."
echo "   logfile : The file to write extended information to. (optional)"
echo ""


if [ $1 ]
then 
  TEST_USER=$1
else
  echo "Please provide a user for permission testing."
  exit 1
fi

if [ $2 ]
then 
  LOG_FILE=$2
else
  LOG_FILE="/dev/null"
fi

echo "" > $LOG_FILE

echo "Starting tests..."
echo "" 
echo "Permission testing for user: $TEST_USER"

if [ $2 ]
then
  echo "Logging to: $LOG_FILE"
fi

echo ""

echo "Executing checks..."

# check firewire stack
echo " Checking linux1394 firewire kernel stack..."

log "`/sbin/lsmod`"

driver_error=0

check_module "ieee1394" "IEEE1394 base module"
driver_error=$(( $driver_error + $? ))

check_module "ohci1394" "OHCI driver module"
driver_error=$(( $driver_error + $? ))

check_module "raw1394" "raw1394 module module"
driver_error=$(( $driver_error + $? ))

# check the /dev/raw1394 device node
if [ -e /dev/raw1394 ] 
then
  echo "  /dev/raw1394 node found"
else
  echo "  ERROR: /dev/raw1394 node not found"
  driver_error=1
fi

if [ $driver_error == 0 ]
then
  echo "  => Your linux1394 kernel stack looks OK"
else
  echo "  => Your linux1394 kernel stack is not OK"
fi

echo ""

# check version & installation
echo " Checking linux1394 libraries..."

check_library "libraw1394"
lib_is_not_installed=$?

# run the library test
do_lib_test=1
if [ $lib_is_not_installed==0 ]
then
  if [ ! -e freebob_test_raw1394 ]
  then
    echo -n "    [compiling freebob_test_raw1394..."
    
    gcc -lraw1394 -o freebob_test_raw1394 freebob_test_raw1394.c 2>&1 1> $LOG_FILE
    echo "" >> $LOG_FILE
    
    if [ $? == 0 ]
    then
      echo " OK]"
    else 
      echo " Failed, skipping test]"
      do_lib_test=0
    fi
  fi
else
  do_lib_test=0
fi


if [ $do_lib_test == 1 ]
then
  ./freebob_test_raw1394 2>&1 1>> $LOG_FILE
  retval=$?
  
  case "$retval" in
   "0" ) echo "  libraw1394 tests passed."
         lib_error=0
         ;;
   "1" ) echo "  kernel-userspace ABI mismatch."
         lib_error=1
         ;;
   "2" ) echo "  access to /dev/raw1394 denied, please check the permissions."
         lib_error=1
         ;;
   "3" ) echo "  failed to obtain libraw1394 handle."
         lib_error=1
         ;;
   "4" ) echo "  failed to get card info."
         lib_error=1
         ;;
   "5" ) echo "  no firewire cards found."
         lib_error=1
         ;;
   "6" ) echo "  failed to set port."
         lib_error=1
         ;;
  esac
else
  echo "  Compilation of test program failed, skipping extended libraw1394 test."
  lib_error=1
fi

if [ $lib_error == 0 ]
then
  echo "  => libraw1394 looks OK"
else
  echo "  => libraw1394 is not OK"
fi

check_library "libiec61883"
lib_is_not_installed=$?

if [ $lib_is_not_installed==0 ]
then
  lib_error=0
else
  lib_error=1
fi

if [ $lib_error == 0 ]
then
  echo "  => libiec61883 looks OK"
else
  echo "  => libiec61883 is not OK"
fi

check_library "libavc1394"
lib_is_not_installed=$?

if [ $lib_is_not_installed==0 ]
then
  lib_error=0
else
  lib_error=1
fi

if [ $lib_error == 0 ]
then
  echo "  => libavc1394 looks OK"
else
  echo "  => libavc1394 is not OK"
fi

echo ""

# checking for jackd
echo " Checking jack..."

check_jack "jack"
jack_not_found=$?
if [ $jack_not_found == 0 ]
then
  jack_runs=$(jackd -h)
  
  if [ $? == 0 ]
  then
    echo "$jack_runs" >> $LOG_FILE
    $jack_runs=""
  fi
  
  if [ ! "$jack_runs" = "" ]
  then
    echo "  jackd can start..."
    freebob_backend=$(echo "$jack_runs" | grep "freebob")
    
    log "$freebob_backend"
    
    if [ ! "$freebob_backend" = "" ]
    then
      echo "  freebob backend found."
    else
      echo "  freebob backend not found."
    fi
  else
    echo "  jackd can't start..."
  fi

else
  echo "  jack doesn't seem to be installed"
fi

echo ""

# checking kernel
echo " Checking kernel..."
kernel_ver=$(uname -r)

echo "  Kernel version: $kernel_ver"

log "`ps -Leo pid,rtprio,cmd`"

