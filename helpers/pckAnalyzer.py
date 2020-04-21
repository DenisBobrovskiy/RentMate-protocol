import socket, sys

#Analyzes a packet
def main():
    print("Input the packet, form = every byte separated by 1 spaces such as 38 29 255 42.... for 4 bytes\n Option -h for hex form and -d for decimal form")
    packetRaw = input()
    
    pckParsed = packetRaw.split(" ")




if __name__ == '__main__':
    main() 
