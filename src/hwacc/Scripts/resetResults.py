# Reset results file
import os
import sys
from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument("-f", "--file", dest="myFile", help="Open specified file")
args = parser.parse_args()
myFile = args.myFile

open(myFile, "w").close()
