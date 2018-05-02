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

# Setup to build this target
include $(OCPI_CDK_DIR)/include/hdl/xilinx.mk
# Here we require Vivado SDK version 2013.4 for platform xilinx13_3
OCPI_XILINX_VIVADO_SDK_VERSION:=2013.4
f:=$(OcpiXilinxEdkDir)/gnu/arm/lin/bin
ifeq ($(wildcard $f),)
  $(error When setting up to build for zynq for $(OCPI_TARGET_PLATFORM), cannot find $f. Perhaps the EDK was not installed\
          when Xilinx tools were installed? The non-default Xilinx environment settings were: \
          $(foreach v,$(filter OCPI_XILINX%,$(.VARIABLES)), $v=$($v)))
endif
OcpiCrossCompile=$f/arm-xilinx-linux-gnueabi-
OcpiCFlags+=-mfpu=neon-fp16 -mfloat-abi=softfp -march=armv7-a -mtune=cortex-a9
OcpiCxxFlags+=-mfpu=neon-fp16 -mfloat-abi=softfp -march=armv7-a -mtune=cortex-a9
OcpiStaticProgramFlags=-rdynamic
# Ask Aaron when/why this override is used
ifdef OCPI_TARGET_KERNEL_DIR
  OcpiKernelDir=$(OCPI_TARGET_KERNEL_DIR)
else
  OcpiKernelDir=release/kernel-headers
endif
OcpiPlatformOs=linux
OcpiPlatformOsVersion=x13_3
OcpiPlatformArch=arm