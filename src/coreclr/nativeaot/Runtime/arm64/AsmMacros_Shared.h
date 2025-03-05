// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

// This file is used to allow sharing of assembly code between NativeAOT and CoreCLR, which have different conventions about how to ensure that constants offsets are accessible

#ifdef TARGET_WINDOWS
#include "AsmMacros.h"
#else
#include <unixasmmacros.inc>
#include "AsmOffsets.inc"
#endif
