# 0.3.0-dev

## Added

 - Added support for Mac OS compilation [@threonyl #20]
 - Added references to genvar
 - Added scope-aware completion.
 - Added scope lookup by path method in the indexer.
 - Added `--version` management for sv-indexer (same version as LSP)
 - Added statement expension formatter (to split multi-statements).
 - Added Doxygen setup
 - Added new workspace elements (blackboxes) caching backend
 - Added `diplomat-server.get-file-bbox` to retrieve infos about a specific file.
 - Partially adds support for wildcard import lookup in the indexer (#18)

 
## Changed

 - Full rework of the indexer.
 - Updated CMakeLists.txt to generate proper semver version strings
 - Show versions upon start of LSP and indexer (standalone)
 - Reduced log level in the indexer (use of trace)
 - Try to handle stop and reboot in TCP mode using --keep-alive (still some instability)
 - Update `diplomat-server.set-top` to accept module name instead of file path
 - Move sv-tree as a cmake "module"

## Fixed

 - Avoid spurious errors in log due to unknown command `textDocument/didClose`
 - Fixed `diplomat-server.list-symbols` to handle design path as an input (#17)

## Dependencies

 - Slang to v8.1

## Removed
 
 - Removed the SVDocument object and dependencies entirely.

# 0.2.1

## Added

 - For reference, added the diplomat workspace settings schema in the source tree.

## Changed

 - Reorganization of all "sub applications" in sub-cmake files

## Fixed

 - Removed definition of hash for slang::SourceRange introduced by Slang 7.0 (#14)
 - Prevent segfault on badly constructed module top level interface (#15)

## Dependencies

 - Slang from 6.0 to 7.0 
 - uri from `master` to a fixed hash + remove shallow clone (#13)


# 0.2.0

## Added

 - Add support for `include` folders in config files.
 - Add support to add user include folder through `diplomat-server.add-incdir` 

## Dependencies
 - Updated fmtlib from 10.2.1 to 11.0.2
 - Updated sockpp from 0.8.1  to 1.0.0
 - Updated spdlog from 1.13.0 to 1.14.1
 - Updated argparse from 3.0 to 3.1
   - Fix warning at compile time

# 0.1.0 
Considered as first release.
Aligned with `diplomat-vscode@0.2.0`. 


# 0.0.5 - Unreleased
Next version will be 0.0.6

# To write a release note

Ensure that the first title is matching the correct verion with the format
```
# <version> [- stuff]
```
No space are allowed in version which shall follow the semver format.

Then run the following bash command to extraxt the first section of the CHANGELOG.md
```bash
head -n $((`grep -n -m 2  '^# ' CHANGELOG.md | sed 's/:.\+//g' | tail -n 1`-1)) CHANGELOG.md
```

Run the following to extract the latest version:
```bash
grep -m 1 "^# " CHANGELOG.md | sed 's/# \([^ ]*\)\+/\1/'
```
