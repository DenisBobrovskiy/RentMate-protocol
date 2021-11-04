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
        correctInputDirection=0;
        correctInputProtocol=0;
        correctInputClient=0;
        if [ $direction == "clientToEsp" ] || [ $direction == "espToClient" ]
        then
            correctInputDirection=1;
        fi
        if [ $protocol == "0" ] || [ $protocol == "1" ]
        then
            correctInputProtocol=1;
        fi
        if [ $client == "0" ] || [ $client == "1" ]
        then
            correctInputClient=1;
        fi
        echo $correctInputDirection $correctInputProtocol $correctInputClient

        if [ $correctInputDirection -eq 1 ] && [ $correctInputProtocol -eq 1 ] && [ $correctInputClient -eq 1 ]
        then
            #Execute the file transfer
            echo "This will transfer the file in direction: $direction. Transferring the files(where 1 means files will be transferred, 0 means wont be transferred): client files: $client; protocol files: $protocol;"
            echo "Type y to confirm that you want to start file transfer:"
            read userConfirm
            if [ $userConfirm == "y" ]
            then
                echo "Starting file transfer"
                if [ $direction == "espToClient" ]
            else
                echo "Not confirmed, aborting script"
                exit 1
            fi
        else
            echo "Wrong input!"
        fi

    fi
