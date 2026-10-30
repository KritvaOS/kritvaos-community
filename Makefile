#==============================================================================
# Copyright (c) 2026 KritvaOS
# SPDX-License-Identifier: Apache-2.0
#
# File        : Makefile
# Description : KritvaOS Community repository development commands
#
# Component   : Infrastructure
# Module      : Development Infrastructure
# Layer       : Development Infrastructure
#
# Requirements: REPO-001
# API         : GNU Make
#
# Author      : KritvaOS
# Created     : 26-09-2026
#==============================================================================

#------------------------------------------------------------------------------
# Project Configuration
#------------------------------------------------------------------------------

# Kritva Makefile standard (kritva-program v2/planning/standards/MAKEFILE_STANDARD.md): `make` or `make help` lists the targets.
# A target is documented by `name: ## text` on its rule line; `##@ Name` starts a help group.
# An unimplemented gate exits 3 (NOT_IMPLEMENTED). It is never a pass.
.DEFAULT_GOAL := help
.PHONY: help

##@ Help
help: ## Show this list
	@awk 'BEGIN { FS = ":.*##"; printf "\nUsage: make <target> [VAR=value ...]\n" } \
	  /^##@/ { g = substr($$0, 5); next } \
	  /^[a-zA-Z0-9_.-]+:[^=].*##/ { if (g != "") { printf "\n%s\n", g; g = "" }; sub(/^ +/, "", $$2); printf "  %-24s %s\n", $$1, $$2 }' $(MAKEFILE_LIST)

PROJECT_NAME := kritvaos-community

SCRIPT_DIR := scripts
CONFIG_DIR := config

PYTHON := python3

HEADER_CHECKER := $(SCRIPT_DIR)/lint/check_source_headers.py

#------------------------------------------------------------------------------
# Default Target
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Help
#
# Targets containing "##" are automatically displayed by "make help".
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Repository Information
#------------------------------------------------------------------------------
##@ Repository Information

.PHONY: info

info: ## Show repository information
	@echo ""
	@echo "KritvaOS Community"
	@echo "=================="
	@echo "Project        : $(PROJECT_NAME)"
	@echo "Python         : $(PYTHON)"
	@echo "Header Checker : $(HEADER_CHECKER)"
	@echo ""

#------------------------------------------------------------------------------
# Source Header Check
#
# Validate all tracked source/config files using the centralized KritvaOS
# source-header policy.
#------------------------------------------------------------------------------
##@ Source Header Check

.PHONY: header-check

header-check: ## Validate KritvaOS source headers
	@echo "[header-check] Validating source headers..."
	$(PYTHON) $(HEADER_CHECKER) --mode tracked --strict

#------------------------------------------------------------------------------
# README standard check
#
# Structural check of README.md only (metadata, topics, relative links,
# placeholders). A PASS does not show that the content is accurate.
#------------------------------------------------------------------------------
##@ README standard check

.PHONY: check-readme

check-readme: ## Structural check of README.md against the Kritva README standard
	$(PYTHON) $(SCRIPT_DIR)/check_readme.py README.md

#------------------------------------------------------------------------------
# Test Source Header Checker
#
# NOTE:
# The regression tests for the Source Header Checker are maintained in the
# canonical kritvaos-source-header-check repository.
#
# This repository intentionally does not maintain the checker regression suite.
#------------------------------------------------------------------------------
##@ Test Source Header Checker

.PHONY: header-check-version

header-check-version: ## Show installed source-header checker version
	@echo "[header-check] Version:"
	@cat $(CONFIG_DIR)/source_header_check.version

#------------------------------------------------------------------------------
# Git Status
#------------------------------------------------------------------------------
##@ Git Status

.PHONY: status

status: ## Show Git repository status
	@git status --short --branch

#------------------------------------------------------------------------------
# Git Branch
#------------------------------------------------------------------------------
##@ Git Branch

.PHONY: branch

branch: ## Show current Git branch
	@git branch --show-current

#------------------------------------------------------------------------------
# Submodules
#
# Initialize and update all KritvaOS repository submodules.
#------------------------------------------------------------------------------
##@ Submodules

.PHONY: submodules

submodules: ## Initialize and update Git submodules
	@echo "[submodules] Updating submodules..."
	git submodule update --init --recursive

#------------------------------------------------------------------------------
# Submodule Status
#------------------------------------------------------------------------------
##@ Submodule Status

.PHONY: submodule-status

submodule-status: ## Show Git submodule status
	@git submodule status --recursive

#------------------------------------------------------------------------------
# Repository Validation
#
# Main local validation target before creating a pull request.
#
# Keep this target lightweight. Component repositories such as kritva-core
# perform their own build and unit-test validation.
#------------------------------------------------------------------------------
##@ Repository Validation

.PHONY: check

check: header-check ## Run all community repository checks
	@echo ""
	@echo "[check] All community repository checks passed."

#------------------------------------------------------------------------------
# Git Configuration
#------------------------------------------------------------------------------
##@ Git Configuration

.PHONY: git-hooks

git-hooks: ## Show configured Git hooks path
	@echo "[git] core.hooksPath:"
	@git config --get core.hooksPath || echo "(not configured)"

#------------------------------------------------------------------------------
# Clean
#
# Community repository normally has no build artifacts. This target removes
# common local/generated directories only when they exist.
#------------------------------------------------------------------------------
##@ Clean

.PHONY: clean

clean: ## Remove local generated artifacts
	@echo "[clean] Removing local generated artifacts..."
	rm -rf build/
	rm -rf out/
	@echo "[clean] Done."

#------------------------------------------------------------------------------
# Development Environment
#------------------------------------------------------------------------------
##@ Development Environment

.PHONY: environment

environment: ## Show development environment information
	@echo ""
	@echo "Development Environment"
	@echo "======================="
	@echo "OS:"
	@uname -srm
	@echo ""
	@echo "Python:"
	@$(PYTHON) --version
	@echo ""
	@echo "Git:"
	@git --version
	@echo ""
	@echo "Make:"
	@make --version | head -n 1
	@echo ""

#------------------------------------------------------------------------------
# Repository Structure
#------------------------------------------------------------------------------
##@ Repository Structure

.PHONY: tree

tree: ## Show repository structure
	@if command -v tree >/dev/null 2>&1; then \
		tree -L 3 -I '.git|build|out'; \
	else \
		echo "[tree] 'tree' command is not installed."; \
		echo "[tree] Install it with: sudo apt install tree"; \
	fi

#------------------------------------------------------------------------------
# Documentation
#------------------------------------------------------------------------------
##@ Documentation

.PHONY: docs

docs: ## Show documentation locations
	@echo ""
	@echo "KritvaOS Community Documentation"
	@echo "================================"
	@echo "Architecture : docs/architecture/"
	@echo "Development  : docs/development/"
	@echo "Requirements  : docs/requirements/"
	@echo "Design        : docs/design/"
	@echo "User          : docs/user/"
	@echo ""

#------------------------------------------------------------------------------
# Pull Request Preparation
#
# Recommended command before opening a pull request.
#------------------------------------------------------------------------------
##@ Pull Request Preparation

.PHONY: pre-pr

pre-pr: check status ## Run checks and show repository status
	@echo ""
	@echo "[pre-pr] Repository is ready for review."

#------------------------------------------------------------------------------
# End
#------------------------------------------------------------------------------
##@ End

##@ Gates (unimplemented gates exit 3)
define not_implemented
	@echo "NOT_IMPLEMENTED: $(1) has no tests yet; this is not a pass"; exit 3
endef

.PHONY: lint
lint: ## Static analysis and lint (NOT_IMPLEMENTED: exits 3)
	$(call not_implemented,lint)

.PHONY: test-unit
test-unit: ## Unit tests (NOT_IMPLEMENTED: exits 3)
	$(call not_implemented,test-unit)

.PHONY: test-integration
test-integration: ## Integration tests (NOT_IMPLEMENTED: exits 3)
	$(call not_implemented,test-integration)

.PHONY: test-regression
test-regression: ## Regression tests (NOT_IMPLEMENTED: exits 3)
	$(call not_implemented,test-regression)

.PHONY: test
test: ## All tests (NOT_IMPLEMENTED: exits 3)
	$(call not_implemented,test)

.PHONY: acceptance
acceptance: ## Acceptance gate (NOT_IMPLEMENTED: exits 3)
	$(call not_implemented,acceptance)

.PHONY: docs-check
docs-check: ## Documentation link and structure check (NOT_IMPLEMENTED: exits 3)
	$(call not_implemented,docs-check)

##@ Git
.PHONY: git_sync
git_sync: ## Switch to main, fast-forward from origin, prune, delete merged local branches
	@git switch main
	@git pull --ff-only --prune
	@git branch --format='%(refname:short)' | grep -vx main | xargs -r git branch -d
	@git log --oneline -3
