#!/bin/sh
# This file is protected by Copyright. Please refer to the COPYRIGHT file
# distributed with this source distribution.
#
# This file is part of OpenCPI <http://www.opencpi.org>
#
# OpenCPI is free software: you can redistribute it and/or modify it under the
# terms of the GNU Lesser General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# OpenCPI is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License along
# with this program. If not, see <http://www.gnu.org/licenses/>.

################################################################################################
# This script does a few things to help bootstrap the environment.
# It is sourced by other scripts with an explicit pathname, not via a search path.
# It does setup work useful for runtime, but not everything for development time.
# It can be used in deployed environments.
# I.e. it does not set up or default any OCPI_TARGET variables.
# It should not have any "bashisms"
# It must be told its own location since $0 does not always work for sourced scripts
# In posix shells, source scripts cannot take arguments.
# Hence the calling protocol is to set the OCPI_BOOTSTRAP variable to this
# script's absolute pathname before calling it, like:
# OCPI_BOOTSTRAP=$foo/ocpibootstrap.sh; . $OCPI_BOOTSTRAP
# It sets up:
# 1. OCPI_CDK_DIR
# 2. OCPI_PREREQUISITES_DIR
# 3. OCPI_TOOL_*

# Determine OCPI_CDK_DIR:
# If set and it is a directory, assume it is correct
# If not set, and if this script lives in a "scripts" or "bin" directory,
# assume that directory is at the top of the CDK.
# Otherwise, try the global default place.
# In all cases do a final sanity check.
if [ "$OCPI_CDK_DIR" = "" -o ! -d "$OCPI_CDK_DIR" ]; then # and any other sanity checks?
  [ -z "$OCPI_BOOTSTRAP" ] && echo Error:  ocpibootstrap.sh called without setting OCPI_BOOTSTRAP && exit 1
  [ ! -r "$OCPI_BOOTSTRAP" ] && echo Error:  ocpibootstrap.sh called with unreadable OCPI_BOOTSTRAP: $OCPI_BOOTSTRAP && exit 1
  case $OCPI_BOOTSTRAP in
   /*) ;;
   *) echo Error:  ocpibootstrap.sh called with OCPI_BOOTSTRAP not absolute: $OCPI_BOOTSTRAP && exit 1 ;;
  esac
  _MYDIR=$(dirname $OCPI_BOOTSTRAP)
  _MYBASE=$(basename $_MYDIR)
  if [ $_MYBASE = scripts -o $_MYBASE = bin ]; then
    # Be careful to maintain symbolic link names for UI clarity
    DIRDIR=$(dirname $_MYDIR)
    case $DIRDIR in
    /*)
      OCPI_CDK_DIR=$DIRDIR
      ;;
    *)
      OCPI_CDK_DIR=$(pwd)/$DIRDIR
      ;;
    esac
  else
    OCPI_CDK_DIR=/opt/opencpi/cdk
  fi
  export OCPI_CDK_DIR=$(cd $OCPI_CDK_DIR; pwd)
  # FIXME: someday better "signature" would be more robust...
  if test ! -f $OCPI_CDK_DIR/scripts/getPlatform.sh; then
    echo Error:  ocpibootstrap.sh cannot find a valid place to set OCPI_CDK_DIR "(tried $OCPI_CDK_DIR)"
    exit 1
  fi
fi

# Initialize PREREQUISITES variables
# THIS IS THE SHELL VERSION OF WHAT IS IN util.mk
[ -z "$OCPI_PREREQUISITES_DIR" ] && {
  export OCPI_PREREQUISITES_DIR=$(dirname $OCPI_CDK_DIR)/prerequisites
  [ -d $OCPI_PREREQUISITES_DIR ] ||
    echo "Warning: $OCPI_PREREQUISITES_DIR does not exist yet."
}

# Initialize the TOOL variables
if test "$OCPI_TOOL_PLATFORM" = ""; then
  GETPLATFORM=$OCPI_CDK_DIR/scripts/getPlatform.sh
  if test ! -f $OCPI_CDK_DIR/scripts/getPlatform.sh; then
    echo Error:  ocpibootstrap.sh cannot find getPlatforms.sh    
    exit 1
  fi
  read v0 v1 v2 v3 v4 v5 <<< `${GETPLATFORM}`
  if test "$v5" == "" -o $? != 0; then
    echo Failed to determine runtime platform.
    exit 1
  fi
  export OCPI_TOOL_OS=$v0
  export OCPI_TOOL_OS_VERSION=$v1
  export OCPI_TOOL_ARCH=$v2
  export OCPI_TOOL_HOST=$v3
  export OCPI_TOOL_PLATFORM=$v4
  export OCPI_TOOL_PLATFORM_DIR=$v5
fi
# Determine OCPI_TOOL_MODE if it is not set already
[ -z "$OCPI_TOOL_DIR" ] && export OCPI_TOOL_DIR=$OCPI_TOOL_PLATFORM
return 0
