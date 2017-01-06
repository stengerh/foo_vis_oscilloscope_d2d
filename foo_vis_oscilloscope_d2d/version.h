// This file defines common version information for the DLL's version resource
// and the component version shown in foobar2000 itself.

#pragma once

// Component filename (string literal)
// TODO Update component filename
#if !defined(COMPONENT_FILENAME)
#error Please define COMPONENT_FILENAME in the preprocessor settings!
#endif

// Component name (string literal)
// TODO Update component name
#define COMPONENT_NAME "Oscilloscope"

// Component author
// TODO Update component author
#define COMPONENT_AUTHOR "Holger Stenger"

// The parts of the component version number (integer literals)
// TODO Update component version, see http://semver.org
#define COMPONENT_VERSION_MAJOR 1
#define COMPONENT_VERSION_MINOR 0
#define COMPONENT_VERSION_PATCH 0

// Year for copyright notice (string literal)
// TODO Update year for copyright notice
#define COMPONENT_COPYRIGHT_YEAR "2017"

// Helper macros for converting integers to string literals and concatenating them
#define MAKE_STRING(text) #text
#define MAKE_COMPONENT_VERSION(major,minor,patch) MAKE_STRING(major) "." MAKE_STRING(minor) "." MAKE_STRING(patch)

// Assemble the component version as string and as comma-separated list of integers
#define COMPONENT_VERSION MAKE_COMPONENT_VERSION(COMPONENT_VERSION_MAJOR,COMPONENT_VERSION_MINOR,COMPONENT_VERSION_PATCH)
#define COMPONENT_VERSION_NUMERIC COMPONENT_VERSION_MAJOR, COMPONENT_VERSION_MINOR, COMPONENT_VERSION_PATCH, 0
