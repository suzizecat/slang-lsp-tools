# Changelog

## Changed

 - Reorganization of all "sub applications" in sub-cmake files

## Fixed

 - Removed definition of hash for slang::SourceRange introduced by Slang 7.0 (#14)

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