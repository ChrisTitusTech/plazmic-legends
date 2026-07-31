SHELL := /bin/bash
.ONESHELL:
.SHELLFLAGS := -eu -o pipefail -c

REPO_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

.PHONY: srpm

srpm:
	@test -n "$(outdir)" || {
		printf '%s\n' 'outdir is required' >&2
		exit 2
	}
	test -n "$(spec)" || {
		printf '%s\n' 'spec is required' >&2
		exit 2
	}
	spec_path="$(spec)"
	if [[ "$$spec_path" != /* ]]; then
		spec_path="$(CURDIR)/$$spec_path"
	fi
	temp_dir="$$(mktemp -d -t plazmic-legends-srpm.XXXXXXXX)"
	trap 'rm -rf -- "$$temp_dir"' EXIT
	mkdir -p "$(outdir)" "$$temp_dir/sources"
	name="$$(rpmspec -q --srpm --qf '%{NAME}' "$$spec_path")"
	version="$$(rpmspec -q --srpm --qf '%{VERSION}' "$$spec_path")"
	tar \
		--create \
		--file=- \
		--directory="$(REPO_ROOT)" \
		--exclude-vcs \
		--exclude-vcs-ignores \
		--exclude='./.codex/local' \
		--exclude='./.env' \
		--exclude='./.env.*' \
		--exclude='./.idea' \
		--exclude='./.vscode' \
		--exclude='./.pytest_cache' \
		--exclude='./.ruff_cache' \
		--exclude='./artifacts' \
		--exclude='./build' \
		--exclude='./captures' \
		--exclude='./cmake_install.cmake' \
		--exclude='./compile_commands.json' \
		--exclude='./CMakeCache.txt' \
		--exclude='./CMakeFiles' \
		--exclude='./CMakeUserPresets.json' \
		--exclude='./logs' \
		--exclude='./out' \
		--exclude='*/__pycache__' \
		--exclude='*.dmp' \
		--exclude='*.log' \
		--exclude='*.pyc' \
		--exclude='*.pyo' \
		--exclude='*.trace' \
		--sort=name \
		--mtime='@0' \
		--owner=0 \
		--group=0 \
		--numeric-owner \
		--transform="s|^\\./|$$name-$$version/|" \
		. \
		| gzip -n >"$$temp_dir/sources/$$name-$$version.tar.gz"
	rpmbuild -bs "$$spec_path" \
		--define "_sourcedir $$temp_dir/sources" \
		--define "_srcrpmdir $(abspath $(outdir))"
