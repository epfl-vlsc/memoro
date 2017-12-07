#!/bin/bash

electron-packager . --overwrite --platform=darwin --arch=x64 --electron-version=1.7.9 --icon=assets/icons/icon64.icns --prune=true --out=release-builds
