#!/bin/bash


set -o pipefail
set -u -e -x

main() {
	TAG_TO_BE_PUBLISHED=$(head -1 orca_github_release_stage/tag.txt)
	EXISTING_GIT_TAG=$(env GIT_DIR=orca_src/.git git describe --tags)
	if [[ $TAG_TO_BE_PUBLISHED -eq $EXISTING_GIT_TAG ]]; then
		echo "Tag $TAG_TO_BE_PUBLISHED already present on ORCA repository"
		echo "Please BUMP the ORCA version"
		exit 1
	fi
	}

main "$@"
