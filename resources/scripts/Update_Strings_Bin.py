#!/usr/bin/python

import sys
import re

if(len(sys.argv)>1):
    file = str(sys.argv[1])
else:
    file = ""

#print(file)

with open(file+"lookuptext_eng.txt","r",encoding="cp1252") as f:
    lines = f.readlines()

linesBin = []
numLines = len(lines)-2
#print(numLines)

#Get last ID in the file (hopefully the highest number)
highestNumber = int(re.search(r"^(\d*)\^", lines[numLines+1]).group(1))
#print(highestNumber)

fileSize = 13+(highestNumber*4)
addressesEndOffset = fileSize-1

linesFinal = []
with open(file+"lookuptext_eng.bin","wb") as f:
    f.write((highestNumber+1).to_bytes(2, byteorder="little"))
    f.write(b'\x00\x00')
    
    i = 0
    j = 2
    while i < highestNumber:
        
        '''
        #Old Implementation
        #Copy line
        mid=lines[i]
        #Trim line break
        mid = mid[0:len(mid)-1]
        #print(mid)
        #Trim everything before carats (^)
        mid = mid[len(str(i+1))+1:len(mid)]
        #print(mid)
        '''
        
        #New regex implementation 2024
        #print("Line " + str(j) + " is:" + lines[j])
        number = int(re.search(r"^(\d*)\^", lines[j]).group(1))
        #print(i+1)
        #print(number)
        
        if i+1 == number:
            mid = re.search(r"(?<=\^)(?!.*\^)(.*)$", lines[j]).group(1)
            linesFinal.append(mid)
            linesFinal[i] = linesFinal[i].replace(r"\r\n","\n")
            #print(str(i+1) + ": " + linesFinal[i])    
            fileSize = fileSize+len(linesFinal[i])+1
            j+=1
        else:
            mid = ""
            linesFinal.append(mid)
            linesFinal[i] = linesFinal[i].replace(r"\r\n","\n")
            #print(str(i+1) + ": " + linesFinal[i])    
            fileSize = fileSize+len(linesFinal[i])+1
        i += 1
    

        

    
    f.write(fileSize.to_bytes(3, byteorder="little"))
    f.write(b'\x00')
    
    f.write(addressesEndOffset.to_bytes(3, byteorder="little"))
    f.write(b'\x00')
    
    i = 0
    offset = addressesEndOffset+1
    while i < highestNumber:
        f.write(offset.to_bytes(3, byteorder="little"))
        f.write(b'\x00')
        offset += len(linesFinal[i])+1
        #print(offset)
        i += 1
    f.write(b'\x00')
    
    i = 0
    while i < highestNumber:
        f.write(linesFinal[i].encode("cp1252"))
        f.write(b'\x00')
        i += 1