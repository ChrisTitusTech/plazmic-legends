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
	if command -v git >/dev/null 2>&1; then
		git -C "$(REPO_ROOT)" ls-files \
			| diff --unified "$(REPO_ROOT)/.copr/source-files" -
	fi
	tar \
		--create \
		--file=- \
		--directory="$(REPO_ROOT)" \
		--sort=name \
		--mtime='@0' \
		--owner=0 \
		--group=0 \
		--numeric-owner \
		--transform="s|^|$$name-$$version/|" \
		--files-from="$(REPO_ROOT)/.copr/source-files" \
		| gzip -n >"$$temp_dir/sources/$$name-$$version.tar.gz"
	rpmbuild -bs "$$spec_path" \
		--define "_sourcedir $$temp_dir/sources" \
		--define "_srcrpmdir $(abspath $(outdir))"
