/*
 * Copyright (C) 2015 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <private/bionic_macros.h>

#include "BacktraceData.h"
#include "Config.h"
#include "DebugData.h"
#include "debug_log.h"
#include "malloc_debug.h"

static void ToggleBacktraceEnable(int, siginfo_t*, void*) {
  g_debug->backtrace->ToggleBacktraceEnabled();
}

static void EnableDump(int, siginfo_t*, void*) {
  g_debug->backtrace->EnableDumping();
}

BacktraceData::BacktraceData(DebugData* debug_data, const Config& config, size_t* offset)
    : OptionData(debug_data) {
  size_t hdr_len = sizeof(BacktraceHeader) + sizeof(uintptr_t) * config.backtrace_frames();
  alloc_offset_ = *offset;
  *offset += __BIONIC_ALIGN(hdr_len, MINIMUM_ALIGNMENT_BYTES);
}

bool BacktraceData::Initialize(const Config& config) {
  enabled_ = config.backtrace_enabled();
  if (config.backtrace_enable_on_signal()) {
    struct sigaction enable_act;
    memset(&enable_act, 0, sizeof(enable_act));

    enable_act.sa_sigaction = ToggleBacktraceEnable;
    enable_act.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&enable_act.sa_mask);
    if (sigaction(config.backtrace_signal(), &enable_act, nullptr) != 0) {
      error_log("Unable to set up backtrace signal enable function: %s", strerror(errno));
      return false;
    }
    info_log("%s: Run: 'kill -%d %d' to enable backtracing.", getprogname(),
             config.backtrace_signal(), getpid());
  }

  struct sigaction act;
  memset(&act, 0, sizeof(act));

  act.sa_sigaction = EnableDump;
  act.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&act.sa_mask);
  if (sigaction(config.backtrace_dump_signal(), &act, nullptr) != 0) {
    error_log("Unable to set up backtrace dump signal function: %s", strerror(errno));
    return false;
  }
  info_log("%s: Run: 'kill -%d %d' to dump the backtrace.", getprogname(),
           config.backtrace_dump_signal(), getpid());

  dump_ = false;
  return true;
}
