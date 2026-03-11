#!/usr/bin/env bash
set -euo pipefail

# Install SSH key from Codespaces secret SSH_PRIVATE_KEY if present.
# To set this up: github.com → Settings → Codespaces → Secrets → New secret
#   Name: SSH_PRIVATE_KEY  Value: contents of your ~/.ssh/id_ed25519
if [[ -n "${SSH_PRIVATE_KEY:-}" ]]; then
  mkdir -p ~/.ssh
  chmod 700 ~/.ssh
  echo "${SSH_PRIVATE_KEY}" > ~/.ssh/id_ed25519
  chmod 600 ~/.ssh/id_ed25519
  ssh-keyscan -t ed25519 github.com >> ~/.ssh/known_hosts 2>/dev/null
  echo "SSH key installed from SSH_PRIVATE_KEY secret."
else
  echo "SSH_PRIVATE_KEY secret not set; skipping SSH key installation."
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_DIR}"

# Install required build tools and cross-compilation toolchain.
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  gdb \
  git-lfs \
  sqlite3 libsqlite3-dev \
  libboost-program-options-dev \
  libboost-regex-dev \
  libboost-test-dev \
  libssl-dev \
  libprotobuf-dev protobuf-compiler \
  libabsl-dev \
  gcc-aarch64-linux-gnu \
  g++-aarch64-linux-gnu \
  libc6-dev-arm64-cross \
  binutils-aarch64-linux-gnu \
  python3 python3-venv python3-dev

# Ensure Git LFS hooks and filters are configured for this user.
git lfs install

# Workspace layout defaults.
: "${REPO_ROOT:=/workspaces/sourcecode}"
: "${BUILD_ROOT:=${REPO_ROOT}/build}"
: "${INSTALL_ROOT:=${REPO_ROOT}/install}"
: "${WRITE_TOPLEVEL_CMAKE:=1}"
: "${TOPLEVEL_CMAKE_PATH:=${REPO_ROOT}/CMakeLists.txt}"

mkdir -p "${BUILD_ROOT}" "${INSTALL_ROOT}"

# Clone or update additional repositories needed by this workspace.
# Entry format:
#   "<repo-url>|<branch>|<destination-path>"
# Destination may be absolute or relative to REPO_ROOT.
REPOS=(
  "https://github.com/DUNE-DAQ/detdataformats.git|coredaq-v5.4.3|detdataformats"
  "https://github.com/DUNE-DAQ/fddetdataformats.git|fddaq-v5.4.3|fddetdataformats"
  "https://github.com/ron003/script.git||script"
)

if [[ ${#REPOS[@]} -eq 0 ]]; then
  echo "No additional repositories configured in .devcontainer/bootstrap-repos.sh"
fi

for entry in "${REPOS[@]}"; do
  IFS='|' read -r repo_url repo_branch repo_dest <<<"${entry}"

  if [[ -z "${repo_url}" ]]; then
    echo "Skipping invalid entry: '${entry}'"
    continue
  fi

  if [[ -z "${repo_dest}" ]]; then
    repo_name="$(basename "${repo_url}" .git)"
    repo_dest="${repo_name}"
  fi

  if [[ "${repo_dest}" = /* ]]; then
    dest_path="${repo_dest}"
  else
    dest_path="${REPO_ROOT}/${repo_dest}"
  fi

  mkdir -p "$(dirname "${dest_path}")"

  if [[ -d "${dest_path}/.git" ]]; then
    echo "Updating ${dest_path}"
    git -C "${dest_path}" fetch --all --prune
    if [[ -n "${repo_branch}" ]]; then
      git -C "${dest_path}" checkout "${repo_branch}"
      git -C "${dest_path}" pull --ff-only origin "${repo_branch}"
    else
      git -C "${dest_path}" pull --ff-only
    fi
  else
    echo "Cloning ${repo_url} -> ${dest_path}"
    if [[ -n "${repo_branch}" ]]; then
      git clone --branch "${repo_branch}" --single-branch "${repo_url}" "${dest_path}"
    else
      git clone "${repo_url}" "${dest_path}"
    fi
  fi
done

if [[ "${WRITE_TOPLEVEL_CMAKE}" == "1" ]]; then
  if [[ -e "${TOPLEVEL_CMAKE_PATH}" ]]; then
    echo "Skipping top-level CMakeLists generation; file already exists: ${TOPLEVEL_CMAKE_PATH}"
  else
    cp "${SCRIPT_DIR}/CMakeLists.workspace.txt" "${TOPLEVEL_CMAKE_PATH}"
    echo "Wrote ${TOPLEVEL_CMAKE_PATH}"
  fi
fi

echo "Workspace bootstrap complete."
echo "REPO_ROOT=${REPO_ROOT}"
echo "BUILD_ROOT=${BUILD_ROOT}"
echo "INSTALL_ROOT=${INSTALL_ROOT}"
