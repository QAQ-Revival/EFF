#pragma once

// Build metadata only. Runtime configuration does not participate in versioning.
#define EIEM_VERSION_MAJOR 2
#define EIEM_VERSION_MINOR 0
#define EIEM_VERSION_PATCH 13
#define EIEM_STRINGIFY2(x) #x
#define EIEM_STRINGIFY(x) EIEM_STRINGIFY2(x)
#define EIEM_VERSION EIEM_STRINGIFY(EIEM_VERSION_MAJOR) "." EIEM_STRINGIFY(EIEM_VERSION_MINOR) "." EIEM_STRINGIFY(EIEM_VERSION_PATCH)
// The fourth part numbers a downstream build of this release; build.bat sets it from EFF_VERSION_REVISION.
#ifndef EIEM_VERSION_REVISION
#define EIEM_VERSION_REVISION 0
#endif
#define EIEM_FILE_VERSION EIEM_VERSION "." EIEM_STRINGIFY(EIEM_VERSION_REVISION)
