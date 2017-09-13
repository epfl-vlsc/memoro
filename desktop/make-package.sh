#!/bin/bash

electron-packager . --overwrite --platform=darwin --arch=x64 --icon=assets/icons/icon64.icns --prune=true --out=release-builds
