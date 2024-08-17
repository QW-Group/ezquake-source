#!/bin/sh

TOP_DIR=$(git rev-parse --show-toplevel)

cd "${TOP_DIR}"

git submodule update --init --recursive

rm -rf release
mkdir release
cd release

EZQ_DIR=$(git rev-parse --absolute-git-dir)
VCPKG_DIR=$(git -C "${TOP_DIR}/vcpkg" rev-parse --absolute-git-dir)
QWPROT_DIR=$(git -C "${TOP_DIR}/src/qwprot" rev-parse --absolute-git-dir)

GIT_REVISION=$(git --git-dir="${EZQ_DIR}" rev-list HEAD --count)
GIT_COMMIT_HASH=$(git --git-dir="${EZQ_DIR}" rev-parse HEAD)
GIT_DESCRIBE=$(git --git-dir="${EZQ_DIR}" describe --tags)
GIT_COMMIT_DATE=$(git --git-dir="${EZQ_DIR}" log -1 --format=%cI)
GIT_COMMIT_TIMESTAMP=$(git --git-dir="${EZQ_DIR}" log -1 --format=%cd --date=format-local:%Y%m%d%H%M.%S)

VCPKG_TAG=$(git --git-dir="${VCPKG_DIR}" describe --tags)

EZQ_NAME="ezquake-${GIT_DESCRIBE}"
EZQ_TAR="${EZQ_NAME}.tar"
EZQ_VERSION="${EZQ_NAME}/version.json"
EZQ_CHECKSUM="${EZQ_NAME}/checksum"

QWPROT_TAR="qwprot.tar"

echo "* Release: ${GIT_DESCRIBE} (rev: ${GIT_REVISION}, vcpkg: ${VCPKG_TAG})"

echo "* Creating ${EZQ_TAR}"
git --git-dir="${EZQ_DIR}" archive --format=tar --prefix="${EZQ_NAME}/" HEAD > "${EZQ_TAR}"

echo "* Creating ${QWPROT_TAR}"
git --git-dir="${QWPROT_DIR}" archive --format=tar --prefix=src/qwprot/ HEAD > "${QWPROT_TAR}"

echo "* Prepare merging tarballs"
tar -xf "${EZQ_TAR}"
tar -xf "${QWPROT_TAR}" -C "${EZQ_NAME}/"

echo "* Generating ${EZQ_VERSION}"
cat > "${EZQ_VERSION}" <<EOF
{
  "version": "${GIT_DESCRIBE}",
  "revision": ${GIT_REVISION},
  "commit": "${GIT_COMMIT_HASH}",
  "date": "${GIT_COMMIT_DATE}",
  "vcpkg": "${VCPKG_TAG}"
}
EOF

echo "* Generating ${EZQ_CHECKSUM}"
echo "${GIT_DESCRIBE}" >> "${EZQ_CHECKSUM}"
shasum "${EZQ_VERSION}" >> "${EZQ_CHECKSUM}"
git --git-dir="${EZQ_DIR}" ls-tree -r HEAD >> "${EZQ_CHECKSUM}"
git --git-dir="${QWPROT_DIR}" ls-tree -r HEAD >> "${EZQ_CHECKSUM}"

echo "* Resetting timestamp of generated files"
touch -t $GIT_COMMIT_TIMESTAMP "${EZQ_VERSION}"
touch -t $GIT_COMMIT_TIMESTAMP "${EZQ_CHECKSUM}"

echo "* Assembling ${EZQ_TAR}.gz"
tar cfz "${TOP_DIR}/${EZQ_TAR}.gz" "${EZQ_NAME}"

cd "${TOP_DIR}"
rm -rf release
