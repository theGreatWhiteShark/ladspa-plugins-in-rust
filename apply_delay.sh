#!/bin/bash

## Applies a LADSPA plugin on a predefined audio file.
##
## Usage:
##     apply_delay.sh <output_audio_file> <ladspa_shared_object> <ladspa_plugin_name>

## Apply the LADSPA plugin on the Snare sample.
LADSPA_PATH=$(pwd) && applyplugin snare.wav $1 $2 $3 0.1 1 0.5 1
