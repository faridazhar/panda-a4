#
# Copyright (C) 2011 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


#=====================================================================
# Root Path for Other Projects
#=====================================================================

LLVM_ROOT_PATH      := external/llvm
LIBBCC_ROOT_PATH    := frameworks/compile/libbcc
RSLOADER_ROOT_PATH  := frameworks/compile/linkloader


#=====================================================================
# Configurations
#=====================================================================

libbcc_DEBUG_MC_DISASSEMBLER        := 0

libbcc_USE_LOGGER                   := 1
libbcc_USE_FUNC_LOGGER              := 0
libbcc_DEBUG_BCC_REFLECT            := 0
libbcc_DEBUG_MC_REFLECT             := 0


#=====================================================================
# Automatic Configurations
#=====================================================================

ifeq ($(libbcc_DEBUG_MC_DISASSEMBLER),0)
libbcc_USE_DISASSEMBLER := 0
else
libbcc_USE_DISASSEMBLER := 1
endif


#=====================================================================
# Common Variables
#=====================================================================

libbcc_CFLAGS := -Wall -Wno-unused-parameter -Werror
ifneq ($(TARGET_BUILD_VARIANT),eng)
libbcc_CFLAGS += -D__DISABLE_ASSERTS
else
libbcc_CFLAGS += -DANDROID_ENGINEERING_BUILD
endif

# Include File Search Path
libbcc_C_INCLUDES := \
  $(RSLOADER_ROOT_PATH)/android \
  $(LIBBCC_ROOT_PATH)/lib \
  $(LIBBCC_ROOT_PATH)/helper \
  $(LIBBCC_ROOT_PATH)/include \
  $(LIBBCC_ROOT_PATH)
