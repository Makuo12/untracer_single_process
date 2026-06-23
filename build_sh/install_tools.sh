#!/bin/bash
set -euxo pipefail

# wllvm
# First make sure pipx is installed via your system package manager
apt update && apt install -y pipx

# Install wllvm via pipx
pipx install wllvm

# Ensure pipx binaries are in your PATH
pipx ensurepath

# gllvm
mkdir -p ${HOME}/go
export GOPATH=${HOME}/go
export PATH=$PATH:/usr/local/go/bin:${GOPATH}/bin

go install github.com/SRI-CSL/gllvm/cmd/...@latest

apt install -y file