# Directory for placing source code imported from the Sora C++ SDK

## Prerequisites

The LAST_UPDATED_MOMO and LAST_UPDATED_SORA_CPP_SDK entries in LAST_UPDATED must be the commit hashes of the last time they were synchronized.

## Scripts

- `diff-sora-cpp-sdk.sh <Sora C++ SDK dir> <target_commit>`
    - Script that displays the differences between the last synchronized commit and the specified commit on the Sora C++ SDK side.
    - This script only displays differences for files in Momo's src/sora-cpp-sdk directory.
- `diff-momo.sh <target_commit>`
    - Script that displays the differences between the last synchronized commit and the specified commit on the Momo side.
    - This script only displays differences for files in Momo's src/sora-cpp-sdk directory.
- `copy-from-sora-cpp-sdk.sh <Sora C++ SDK dir>`
    - Copies files from the actual sora-cpp-sdk directory to files in Momo's src/sora-cpp-sdk directory.
- `copy-to-sora-cpp-sdk.sh <Sora C++ SDK dir>`
    - Copy files from Momo's src/sora-cpp-sdk directory to the sora-cpp-sdk directory.

## Usage

1. Run `./diff-sora-cpp-sdk.sh ../../../sora-cpp-sdk develop` or `./diff-momo.sh develop` to see which one has the smaller difference.
2. If the result of `diff-sora-cpp-sdk.sh` is smaller:
    1. Go to Momo's `src/sora-cpp-sdk` and apply the patch with `./diff-sora-cpp-sdk.sh | patch -p1`.
    2. Fix any conflicts that occur.
    3. `./copy-to-sora-cpp-sdk.sh ../../../sora-cpp-sdk` to reflect the source in the Sora C++ SDK.
3. If the result of `diff-momo.sh` is smaller:
    1. Go to the Sora C++ SDK and apply the patch with `../momo/sora-cpp-sdk/diff-sora-cpp-sdk.sh | patch -p1`.
    2. Fix any conflicts that occur.
    3. Run `../momo/sora-cpp-sdk/copy-to-sora-cpp-sdk.sh .` to reflect the source in Momo.
4. Commit Momo and the Sora C++ SDK.
5. Run `./update-last-updated.sh` to update the commit hash for `LAST_UPDATED` and commit again.