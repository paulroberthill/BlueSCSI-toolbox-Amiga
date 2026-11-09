/*
 * Single place for the version number shown by the Amiga "Version" command.
 * Bump VERSION_STRING and VERSION_DATE (d.m.yyyy) for each release, and add
 * a matching entry to the History section in README.md.
 */
#ifndef VERSION_H
#define VERSION_H

#define VERSION_STRING "1.4"
#define VERSION_DATE   "9.11.2026"

#define VERSTAG(name) "$VER: " name " " VERSION_STRING " (" VERSION_DATE ")"

#endif
