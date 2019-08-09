#
# Copyright (C) F5 Networks, Inc. 2016-2019
#
# No part of the software may be reproduced or transmitted in any
# form or by any means, electronic or mechanical, for any purpose,
# without express written permission of F5 Networks, Inc.
#
# Descriptor Socket Client Library Makefile.
#

#
# Construct the VERSION to use in the rest of the build
#
ifdef GITLAB_CI
# If we're building under GitLab-CI, check if the git ref is a tag which matches the symver spec.
SYMVER          := $(shell echo $(CI_COMMIT_REF_NAME) | grep -P '^\d+\.\d+\.\d+$$')
ifneq ($(SYMVER),)
VERSION         := $(SYMVER)
else
# Otherwise use <an abbreviated ref name (usually the branch)>.<shortend commit sha>.<the build number>.
REF_SLUG        := $(shell echo $(CI_COMMIT_REF_SLUG) | sed 's/-/_/g')
COMMIT_SHA      := $(shell echo $(CI_COMMIT_SHA) | cut -c -8)
VERSION         := $(REF_SLUG).$(COMMIT_SHA).$(CI_PIPELINE_IID)
endif
# Otherwise this is not a GitLab-CI build. Use the local user login and date.
else
VERSION         ?= $(shell whoami).$(shell date +%Y%m%d.%H%M%S)
endif

BUILD_DATE      := $(shell date +%c)

# When building under GitLab-CI, enable error on warning.
ifdef GITLAB_CI
WERROR          = -Werror
endif

CC              = gcc

FLAGS_VERSION = -DVERSION=\""$(VERSION)"\" -DBUILD_DATE=\""$(BUILD_DATE)\""
FLAGS_WARN    = -Wall $(WERROR)
FLAGS_DIALECT = -std=c99
FLAGS_SHARED  = -shared -fPIC -Wl,-soname,libdescsock-client.so
FLAGS_STATIC  = -c -fPIC
CFLAGS        = $(FLAGS_WARN) $(FLAGS_VERSION) -g3 -O3

SRC_DIR        = src
OBJ_DIR        = obj

libraryC := $(addprefix $(SRC_DIR)/, descsock.c descsock_client.c packet.c sys.c xfrag_mem.c)
libraryH := $(addprefix $(SRC_DIR)/, descsock.h descsock_client.h common.h err.h fixed_queue.h hudconf.h packet.h sys.h types.h xfrag_mem.h)
libraryO := $(libraryC:.c=.o)
publicH := $(addprefix $(SRC_DIR)/, descsock_client.h)
publicO := $(addprefix $(OBJ_DIR)/, libdescsock-client.so libdescsock-client.a)
# XXX todo example/test

.PHONY: all library
all: library example          ## Build all (default)
library: $(publicO) $(publicH) ## Build static and dynamic libraries.
example: library $(exampleO)  ## Build example test program.

$(OBJ_DIR)/libdescsock-client.so: $(libraryO) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(FLAGS_SHARED) -o $@  $^

$(OBJ_DIR)/libdescsock-client.a: $(libraryO) $(publicH) | $(OBJ_DIR)
	$(AR) rcs -o $@ $^

$(libraryO): %.o:%.c
	$(CC) $(CFLAGS) $(FLAGS_STATIC) -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

DESTDIR     ?= .
PREFIX      ?= /usr/local
.PHONY: install
install: ## Install built libraries and headers. Will install under ./ unless overridden.
	install -m 0775 -d $(DESTDIR)$(PREFIX)/lib/
	install -m 0775 $(publicO) $(DESTDIR)$(PREFIX)/lib/
	install -m 0775 -d $(DESTDIR)$(PREFIX)/include/
	install -m 0775 $(publicH) $(DESTDIR)$(PREFIX)/include/

#
# RPM-related targets and dependencies
#
RPM_BUILD_DIR   := rpmbuild
RPM_SPEC        := libdescsock-client.spec
RPM_FNAME       := libdescsock-client-$(VERSION)-dev.x86_64.rpm
RPM_PATH        := $(RPM_BUILD_DIR)/RPMS/x86_64
RPM             := $(RPM_PATH)/$(RPM_FNAME)
.PHONY: rpm
rpm: all $(RPM) ## Create RPMs. Will build 'all' if necessary.

$(RPM): $(RPM_SPEC) | $(RPM_BUILD_DIR)
	rpmbuild -bb --define "_topdir $(CURDIR)/$(RPM_BUILD_DIR)" \
	    --define "_sourcedir $(CURDIR)" --define "_builddir $(CURDIR)" \
	    --define "version $(VERSION)" $(RPM_SPEC)

$(RPM_BUILD_DIR):
	mkdir -p $(RPM_BUILD_DIR)


#
# Most below here are misc meta-targets not actually building something, but relating to publishing,
# testing, housekeeping, etc.
#

ARTIFACT_URL          := https://artifactory.pdsea.f5net.com/artifactory/velocity-platform-rpm-dev/descsock-client-lib
ARTIFACT_METADATA_URL := https://artifactory.f5net.com/artifactory/api/metadata/velocity-platform-rpm-dev/descsock-client-lib

# Construct the Artifactory metadata properties as required.
ifdef GITLAB_CI
PROP_SOURCE       := "$(CI_PROJECT_URL)"
PROP_BRANCH       := "$(CI_COMMIT_REF_NAME)"
PROP_COMMIT       := "$(CI_COMMIT_SHA)"
PROP_VERSION      := "$(VERSION)"
PROP_PIPELINE     := "$(CI_PIPELINE_URL)"
PROP_USER         := "$(GITLAB_USER_EMAIL)"
else
PROP_SOURCE       ?= "local-dev-build"
PROP_BRANCH       ?= ""
PROP_COMMIT       ?= ""
PROP_VERSION      ?= "$(VERSION)"
PROP_PIPELINE     ?= "$(MAKE) $(MAKEFLAGS) $(MAKECMDGOALS)"
PROP_USER         ?= "$(shell whoami)@$(shell hostname)"
endif
ARTIFACT_PROPERTIES := '{"props": {\
    "f5.build.source":$(PROP_SOURCE),\
    "f5.build.branch":$(PROP_BRANCH),\
    "f5.build.commit":$(PROP_COMMIT),\
    "f5.build.version":$(PROP_VERSION),\
    "f5.build.pipeline":$(PROP_PIPELINE),\
    "f5.build.user":$(PROP_USER) }}'

ARTIFACT_RPM_URL                := $(ARTIFACT_URL)/$(RPM_FNAME)
ARTIFACT_RPM_METADATA_URL       := $(ARTIFACT_METADATA_URL)/$(RPM_FNAME)

.PHONY: publish
publish: $(RPM)	## Publish RPMs to Artifactory. Will create RPMs if necessary.
# Artifactory credentials are expected to come from the environment via a GitLab "secret variable" setting.
ifndef ARTIFACTORY_AUTH
	$(error Missing ARTIFACTORY_AUTH credentials)
else
	curl -u$(ARTIFACTORY_AUTH) -XPUT $(ARTIFACT_RPM_URL) -T $(RPM)
	curl -u$(ARTIFACTORY_AUTH) -XPATCH -H "Content-Type: application/json" \
	    -d $(ARTIFACT_PROPERTIES) $(ARTIFACT_RPM_METADATA_URL)
endif

.PHONY: release
release: ## Mark RPM as a release. Should only be used from GitLab-CI context.
# Artifactory credentials are expected to come from the environment via a GitLab "secret variable" setting.
ifndef ARTIFACTORY_AUTH
	$(error Missing ARTIFACTORY_AUTH credentials)
else
	curl --header "PRIVATE-TOKEN: $(GITLAB_AUTH_TOKEN)" -XPOST \
	    --data name="RPM Package" --data url="$(ARTIFACT_RPM_URL)" \
	    "https://gitlab.f5net.com/api/v4/projects/$(CI_PROJECT_ID)/releases/$(VERSION)/assets/links"
endif

.PHONY: cppcheck
cppcheck: ## Run cppcheck static analysis on source.
	cppcheck --enable=performance --error-exitcode=1 ${SRC_DIR}

.PHONY: clean
clean:  ## Remove all built binaries and RPMs.
	rm -rf $(OBJ_DIR)
	rm -f  $(SRC_DIR)/*.o
	rm -rf $(RPM_BUILD_DIR)

# Self-documenting Makefile hack. Prefix comments after targets with "##" to include in help output.
.PHONY: help
help:
	@printf "\033[1mAvailable targets:\033[0m\n"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort |\
	    awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-16s\033[0m %s\n", $$1, $$2}'