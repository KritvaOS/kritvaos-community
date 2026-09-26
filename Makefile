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

PROJECT_NAME := kritvaos-community

SCRIPT_DIR := scripts
CONFIG_DIR := config

PYTHON := python3

HEADER_CHECKER := $(SCRIPT_DIR)/lint/check_source_headers.py

#------------------------------------------------------------------------------
# Default Target
#------------------------------------------------------------------------------

.DEFAULT_GOAL := help

#------------------------------------------------------------------------------
# Help
#
# Targets containing "##" are automatically displayed by "make help".
#------------------------------------------------------------------------------

.PHONY: help

help: ## Show available development commands
	@echo ""
	@echo "KritvaOS Community - Development Commands"
	@echo "=========================================="
	@echo ""
	@grep -E '^[a-zA-Z0-9_-]+:.*##' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS=":.*## "}; {printf "  %-22s %s\n", $$1, $$2}'
	@echo ""

#------------------------------------------------------------------------------
# Repository Information
#------------------------------------------------------------------------------

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

.PHONY: header-check

header-check: ## Validate KritvaOS source headers
	@echo "[header-check] Validating source headers..."
	$(PYTHON) $(HEADER_CHECKER) --mode tracked --strict

#------------------------------------------------------------------------------
# Test Source Header Checker
#
# NOTE:
# The regression tests for the Source Header Checker are maintained in the
# canonical kritvaos-source-header-check repository.
#
# This repository intentionally does not maintain the checker regression suite.
#------------------------------------------------------------------------------

.PHONY: header-check-version

header-check-version: ## Show installed source-header checker version
	@echo "[header-check] Version:"
	@cat $(CONFIG_DIR)/source_header_check.version

#------------------------------------------------------------------------------
# Git Status
#------------------------------------------------------------------------------

.PHONY: status

status: ## Show Git repository status
	@git status --short --branch

#------------------------------------------------------------------------------
# Git Branch
#------------------------------------------------------------------------------

.PHONY: branch

branch: ## Show current Git branch
	@git branch --show-current

#------------------------------------------------------------------------------
# Submodules
#
# Initialize and update all KritvaOS repository submodules.
#------------------------------------------------------------------------------

.PHONY: submodules

submodules: ## Initialize and update Git submodules
	@echo "[submodules] Updating submodules..."
	git submodule update --init --recursive

#------------------------------------------------------------------------------
# Submodule Status
#------------------------------------------------------------------------------

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

.PHONY: check

check: header-check ## Run all community repository checks
	@echo ""
	@echo "[check] All community repository checks passed."

#------------------------------------------------------------------------------
# Git Configuration
#------------------------------------------------------------------------------

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

.PHONY: clean

clean: ## Remove local generated artifacts
	@echo "[clean] Removing local generated artifacts..."
	rm -rf build/
	rm -rf out/
	@echo "[clean] Done."

#------------------------------------------------------------------------------
# Development Environment
#------------------------------------------------------------------------------

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

.PHONY: pre-pr

pre-pr: check status ## Run checks and show repository status
	@echo ""
	@echo "[pre-pr] Repository is ready for review."

#------------------------------------------------------------------------------
# End
#------------------------------------------------------------------------------
