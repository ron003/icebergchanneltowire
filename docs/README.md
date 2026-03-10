# Iceberg Channel To Wire

## Devcontainer Setup

This repository includes a `.devcontainer` setup designed for multi-repo workspaces where repositories live directly under `/workspaces`.

On container creation, `.devcontainer/bootstrap-repos.sh` will:

- install build and cross-compilation tools;
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
