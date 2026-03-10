# Iceberg Channel To Wire

## Devcontainer Setup

This repository includes a `.devcontainer` setup designed for multi-repo workspaces where repositories live directly under `/workspaces`.

On container creation, `.devcontainer/bootstrap-repos.sh` will:

- install build and cross-compilation tools;
- install and initialize Git LFS (`git lfs install`);
- create shared workspace directories at `/workspaces/build` and `/workspaces/install`;
- clone or update required sibling repositories under `/workspaces`.

Default sibling repositories:

- `/workspaces/detdataformats` (`coredaq-v5.4.3`)
- `/workspaces/fddetdataformats` (`fddaq-v5.4.3`)

## Bootstrap Environment Variables

The bootstrap script supports these environment variables:

- `REPO_ROOT` (default: `/workspaces`)
- `BUILD_ROOT` (default: `${REPO_ROOT}/build`)
- `INSTALL_ROOT` (default: `${REPO_ROOT}/install`)
- `WRITE_TOPLEVEL_CMAKE` (default: `1`)
- `TOPLEVEL_CMAKE_PATH` (default: `${REPO_ROOT}/CMakeLists.txt`)

Example: also write a top-level `/workspaces/CMakeLists.txt` from the template:

```bash
WRITE_TOPLEVEL_CMAKE=1 bash .devcontainer/bootstrap-repos.sh
```

## Optional Workspace Superbuild

The file `.devcontainer/CMakeLists.workspace.txt` is a template for a top-level workspace `CMakeLists.txt`.

It is intended for local integration builds and does not replace this repository's package-level `CMakeLists.txt`.

## Notes

- This repository remains independently buildable as a normal package.
- The devcontainer bootstrap script does not disable SSH host key checking.

## CMake Presets

This repository provides `CMakePresets.json` for `/workspaces`-based builds.

Run workflow commands from the repository root directory:

- `/workspaces/icebergchanneltowire`

Prerequisites:

- use this repository as the current working directory so CMake can find `CMakePresets.json`;
- run inside the devcontainer, or run `.devcontainer/bootstrap-repos.sh` first to install required tools and create `/workspaces/build` and `/workspaces/install`;
- ensure required package dependencies (for example `daq-cmake`, `TRACE`, and any sibling repos you depend on) are available in your environment and/or under `/workspaces/install`.

Available preset groups:

- configure: `dev-debug`, `dev-release`, `dev-aarch64-release`
- build: `build-debug`, `build-release`, `build-aarch64-release`
- test: `test-debug`, `test-release`, `test-aarch64-release`
- workflow: `ci-debug`, `ci-release`, `ci-aarch64`

Examples:

```bash
cmake --workflow --preset ci-debug
cmake --workflow --preset ci-release
cmake --workflow --preset ci-aarch64
```
