# 0.0.6
## First release

- Setup of the CI/CD


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