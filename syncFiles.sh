#!/bin/bash

# Use this script to:
# 1)sync protocol and main client files into the esp32 client directory.
# 2)sync protocol and main client files from esp32 into main client and protocol directories

# Options: 
# -d: direction(which direction to tranfer files in): clientToEsp or espToClient
# -p: protocol (transfer protocol files or not): 1(for yes, to transfer protocol files), 0 (for no, wont transfer protocol files)
# -c: client (transfer cleint files or not): 1(for yes, to transfer client files), 0 (for no, wont transfer protocol files)

while getopts d:p:c: flag
do
    case "${flag}" in
        d) direction=${OPTARG};;
        p) protocol=${OPTARG};;
        c) client=${OPTARG};;
    esac
done

    echo "Direction: $direction";
    echo "Protocol: $protocol";
    echo "Client: $client";

    #Check if variables are empty, if not ask to confirm and start file transfer
    if [ -z "$direction" ] || [ -z "$protocol" ] || [ -z "$client" ]
    then
        #Empty
        echo "Incorrect input (need to pass every parameter!)";
        exit 1
    else
        echo "Confirm?";
        read userConfirmation
    fi
