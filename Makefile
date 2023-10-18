##
 # Simple Makefile
 #
 # Performs the minimal amount of steps required to generate a given file
 #
 # Features:
 #  - Auto rebuild when headers are modified
 #  - Store intermediate objects in a separate folder
 #  - Self-documented
##

Name       := oscillatord
Executable := $(Name)
Library    := lib$(Name).a
Version    ?= $(shell git tag --sort '-version:refname' --merged | head -1)

ClangFormat ?= clang-format

ImplementationFolder := src
InterfaceFolder      := include
BuildFolder          := cache
Subfolders           != find $(ImplementationFolder) -type d

CFLAGS   += -O2
CFLAGS   += -Wall -Wextra
CFLAGS   += -Wmissing-prototypes -Wmissing-declarations
CFLAGS   += -Wformat=2
CFLAGS   += -Wold-style-definition -Wstrict-prototypes
CFLAGS   += -Wpointer-arith
CFLAGS   += -Wno-address-of-packed-member

CPPFLAGS += -I $(InterfaceFolder)
CPPFLAGS += -DPACKAGE_VERSION=$(Version)
CPPFLAGS += -DLOG_USE_COLOR
CPPFLAGS += -MMD
CPPFLAGS += -DNDEBUG

LDLIBS   += $(LIBS)

EntrypointObject := $(BuildFolder)/$(Name).o

Sources != find $(ImplementationFolder) -type f -name '*.c'
Objects := $(Sources:$(ImplementationFolder)/%.c=$(BuildFolder)/%.o)
Headers != find $(InterfaceFolder) $(ImplementationFolder) -type f -name '*.h'

ConanSetupEnv := conanbuild.sh

# When rendering the help, pretty print certain words
Cyan       := \033[36m
Yellow     := \033[33m
Bold       := \033[1m
EOC        := \033[0m
PP_command := $(Cyan)
PP_section := $(Bold)

##@ General

default: help ## When no target is specified, display the usage

help: ## Display this help.
	@awk 'BEGIN {FS = ":.*##"; printf "\nThis Makefile allows one to build, run and test $(Name).\n\nUsage:\n  make $(PP_command)<target>$(EOC)\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  $(PP_command)%-15s$(EOC) %s\n", $$1, $$2 } /^##@/ { printf "\n$(PP_section)%s$(EOC):\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

raw_help: ## Display the help without color
	@$(MAKE) help --no-print-directory PP_command= PP_section= EOC=

version: ## Display the project's version
	@echo $(Version)

info: ## Print the project's name, version, copyright notice and author
	@echo $(Name) version $(Version)
	@echo
	@echo "Copyright (C) 2023 Orolia Systemes et Solutions"
	@echo
	@echo Written by C. Parent, H. Folcher

.PHONY: default help raw_help version info

##@ Building

build: $(Executable) ## Compile both the library and the executable

lib: $(Library) ## Compile only the library (statically)

.PHONY: build lib

##@ Building with Conan

conan_build: ## Compile both the library and the executable, using the conan environment
conan_lib: ## Compile only the library (statically), using the conan environment

conan_build conan_lib: conan_%: $(ConanSetupEnv)
	bash -c "( . $< && $(MAKE) $* --no-print-directory )"

.PHONY: conan_build

##@ Developping

format: $(Sources) $(Headers) ## Apply clang-format on source files and headers
	echo $^ | xargs -L1 $(ClangFormat) -i

.PHONY: format

##@ Cleaning

clean: ## Remove intermediate objects
	$(RM) -r $(BuildFolder)
	$(RM) $(wildcard *conan*.sh)

fclean: clean ## Remove all generated files
	$(RM) $(Executable) $(Library)

.PHONY: clean fclean

# Non phony rules

include $(wildcard $(Objects:.o=.d)) # To know on which header each .o depends

$(Subfolders:$(ImplementationFolder)%=$(BuildFolder)%): # Create the build folder and its subfolders
	mkdir -p $@

# declare the dependency between objects sources
$(Objects): $(BuildFolder)/%.o: $(ImplementationFolder)/%.c
$(EntrypointObject): $(BuildFolder)/%.o: %.c

$(Library): $(Objects) # Group all the compiled objects into an indexed archive
	$(AR) rcs $@ $^

$(Executable): $(EntrypointObject) $(Library)
	$(CC) $(CFLAGS) $< $(LDFLAGS) -L . -l$(Name) $(LDLIBS) -o $@

$(ConanSetupEnv): # Run conan to have dependencies
	conan install . --build=missing

# When a rule is expanded, both the target and the prerequisites
# are immediately evaluated. Enabling a second expansion allows
# a prerequisite to use automatic variables like $@, $*, etc
.SECONDEXPANSION:

$(EntrypointObject): | $$(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) -I $(ImplementationFolder) -c $< -o $@

$(Objects): | $$(@D) # Compile a single object
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@
