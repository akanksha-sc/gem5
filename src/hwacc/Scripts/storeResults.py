# Script stores simulation results
import os
import sys
from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument("-f", "--file", dest="myFile", help="Open specified file")
parser.add_argument(
    "-d", "--design", dest="myDesign", help="Saves to specified file"
)
args = parser.parse_args()
myFile = args.myFile
myDesign = args.myDesign

flag = False
param = 0
designFile = open(myDesign, "a")
with open(myFile) as parameters:
    for line in parameters:
        line = line.rstrip()
        if not flag:
            if line == r"StatsStart:":
                print("Adjusting Parameters")
                flag = True
        else:
            designFile.write(line)
designFile.write("\n")
designFile.close()
