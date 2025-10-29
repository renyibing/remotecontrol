#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ $# -ne 1 ]; then
  echo "Usage: $0 <target_commit>"
  echo "Example: $0 HEAD"
  echo "Example: $0 2025.5.0"
  echo "Example: $0 abc123def"
  exit 1
fi

cd "$SCRIPT_DIR"

TARGET_REF="$1"

source LAST_UPDATED

INCLUDE_FILES=$(find include -type f)
SRC_FILES=$(find src -type f)
THIRD_PARTY_FILES=$(find third_party -type f)

ALL_FILES=$(echo -e "$INCLUDE_FILES\n$SRC_FILES\n$THIRD_PARTY_FILES" | sort)

BASE_COMMIT=$(git rev-parse "$LAST_UPDATED_MOMO")
TARGET_COMMIT=$(git rev-parse "$TARGET_REF")

echo "==================== Diff ===================="
echo "FROM: $LAST_UPDATED_MOMO ($BASE_COMMIT)"
echo "TO:   $TARGET_REF ($TARGET_COMMIT)"
echo ""

git --no-pager diff --relative=src/sora-cpp-sdk "$LAST_UPDATED_MOMO..$TARGET_REF" -- $ALL_FILES

echo ""
echo "==================== Summary ===================="
echo "Commit range: $LAST_UPDATED_MOMO..$TARGET_REF"
echo ""
echo "Commit log:"
git --no-pager log --oneline "$LAST_UPDATED_MOMO..$TARGET_REF" -- $ALL_FILES
