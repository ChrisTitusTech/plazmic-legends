SHELL := /bin/bash
.ONESHELL:
.SHELLFLAGS := -eu -o pipefail -c

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
	spectool -g -C "$$temp_dir/sources" "$$spec_path"
	rpmbuild -bs "$$spec_path" \
		--define "_sourcedir $$temp_dir/sources" \
		--define "_srcrpmdir $(abspath $(outdir))"
