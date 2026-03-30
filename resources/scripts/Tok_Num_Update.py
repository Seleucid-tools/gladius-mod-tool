#!/usr/bin/python

# I don't know if this is actually necessary or not. It's here for safety.

import sys

if(len(sys.argv)>1):
    skillsTOK = str(sys.argv[1])+"skills.tok"
    itemsTOK = str(sys.argv[1])+"items.tok"
else:
    skillsTOK = "skills.tok"
    itemsTOK = "items.tok"

with open(skillsTOK,"r",encoding="utf8") as f:
    lines = f.readlines()

#print(lines[6])

i = 0
numSkills = 0

while i < len(lines):
    tempString = lines[i]
    tempString = tempString[0:11]
    if tempString=="SKILLCREATE":
        numSkills += 1
    i += 1

#print(numSkills)

lines[6] = "NUMENTRIES: " + str(numSkills) + "\n"

#print(lines[6])

with open(skillsTOK,"w",encoding="utf8") as f:
    i=0
    while i < len(lines):
        f.write(lines[i])
        i += 1

##################################################

with open(itemsTOK,"r",encoding="utf8") as f:
    lines2 = f.readlines()

#print(lines2[3])

i = 0
numItems = 0

while i < len(lines2):
    tempString = lines2[i]
    tempString = tempString[0:10]
    if tempString=="ITEMCREATE":
        numItems += 1
    i += 1

lines2[3] = "NUMENTRIES: " + str(numItems) + "\n"

with open(itemsTOK,"w",encoding="utf8") as f:
    i=0
    while i < len(lines2):
        f.write(lines2[i])
        i += 1