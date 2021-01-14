#!/bin/bash

SCRIPTPATH=$(dirname "$(readlink -f "$0")")

(cd "$SCRIPTPATH"/cpp && make) && "$SCRIPTPATH"/node_modules/electron/cli.js "$SCRIPTPATH"
