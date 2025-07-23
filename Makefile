Name       := oscillatord

Executable  := zig-out/bin/$(Name)
InstallPath ?= /usr/local/bin
Installed   := $(InstallPath)/$(Name)

SystemdServiceFolder      := systemd
SystemdServiceInstallPath ?= $(shell pkg-config systemd --variable=systemdsystemunitdir)
SystemdServiceSources     != find $(SystemdServiceFolder) -type f -name '*.service'
SystemdServiceInstalled   := $(SystemdServiceSources:$(SystemdServiceFolder)/%=$(SystemdServiceInstallPath)/%)

ConfigFolder              := configuration
ConfigFile                := $(ConfigFolder)/oscillatord_default.conf
ConfigInstallPath         := /etc
ConfigInstalled           := $(ConfigInstallPath)/oscillatord.conf

ClangFormat ?= clang-format
ZIG ?= zig

# When rendering the help, pretty print certain words
Cyan       := \033[36m
Yellow     := \033[33m
Bold       := \033[1m
EOC        := \033[0m
PP_command := $(Cyan)
PP_section := $(Bold)

##@ General

default: build ## When no target is specified, build the executable

help: ## Display this help.
	@awk 'BEGIN {FS = ":.*##"; printf "\nThis Makefile allows one to build, run and test $(Name).\n\nUsage:\n  make $(PP_command)<target>$(EOC)\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  $(PP_command)%-15s$(EOC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(PP_section)%s$(EOC):\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

raw_help: ## Display the help without color
	@$(MAKE) help --no-print-directory PP_command= PP_section= EOC=

version: ## Display the project's version
	@echo $(shell grep --perl-regexp --only-matching '\.version\s*=\s*"\K[\d.]+' build.zig.zon)

info: ## Print the project's name, version, copyright notice and author
	@echo $(Name) version $(Version)
	@echo
	@echo "Copyright (C) 2023 Orolia Systemes et Solutions"
	@echo
	@echo Written by C. Parent, H. Folcher

.PHONY: default help raw_help version info

##@ Build

build: $(Executable) ## Compile oscillatord with zig

##@ Install

install: $(SystemdServiceInstalled) $(Installed) $(ConfigInstalled) ## Install the binary, the config and systemd services

uninstall: ## Remove the binary, config and service files placed by install
	$(RM) -v $(SystemdServiceInstalled) $(Installed) $(ConfigInstalled)

check: ## List installed files if present
	-@ls -1 $(SystemdServiceInstalled) $(Installed) $(ConfigInstalled)

.PHONY: install uninstall

# Non phony rules

$(ConfigInstalled): $(ConfigFile)
	install --mode=644 $< $@

$(Executable): $(shell find . -type f -name '*.[ch]') build.zig build.zig.zon
	$(ZIG) build --release=small --summary all

$(Installed): $(Executable)
	install $< $@

# When a rule is expanded, both the target and the prerequisites
# are immediately evaluated. Enabling a second expansion allows
# a prerequisite to use automatic variables like $@, $*, etc
.SECONDEXPANSION:

$(SystemdServiceInstalled): $(SystemdServiceFolder)/$$(@F)
	install --mode=644 $< $@
