# Contributing

Thanks for helping the Quality of Life mod manager. Bug reports and fixes for the mod feed and the release checks both help.

## Ways to help

- Report mod feed problems and install or update bugs
- Send fixes for the Qt 6 interface and the build scripts
- Improve the release checks

## Getting set up

The manager is Qt 6 Widgets with MinGW and CMake. Build with:

```
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Clean
```

The build writes to dist\.

## Checks before a pull request

- Run the release checks with `scripts\build-setup.ps1` (DLL check, `-installcheck`, Setup build and installed check)
- Bump the version in src/Version.h if your change ships

## Where to submit

Open issues and pull requests in this repository.