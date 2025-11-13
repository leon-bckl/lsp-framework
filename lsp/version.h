#pragma once

#define LSP_VER_MAJOR 1
#define LSP_VER_MINOR 3
#define LSP_VER_PATCH 0

#define LSP_STR_INNER(v) #v
#define LSP_STR(v) LSP_STR_INNER(v)

#define LSP_STRINGIFY_VERSION(major, minor, patch) LSP_STR(major) LSP_STR(.) LSP_STR(minor) LSP_STR(.) LSP_STR(patch)

#define LSP_VERSION LSP_STRINGIFY_VERSION(LSP_VER_MAJOR, LSP_VER_MINOR, LSP_VER_PATCH)
