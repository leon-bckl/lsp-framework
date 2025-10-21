#pragma once

#define LSP_VER_MAJOR 1
#define LSP_VER_MINOR 2
#define LSP_VER_PATCH 0

#define _STR(v) #v
#define STR(v) _STR(v)

#define STRINGIFY_VERSION(major, minor, patch) STR(major) STR(.) STR(minor) STR(.) STR(patch)

#define LSP_VERSION STRINGIFY_VERSION(LSP_VER_MAJOR, LSP_VER_MINOR, LSP_VER_PATCH)