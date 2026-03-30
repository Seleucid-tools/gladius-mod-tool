#!/usr/bin/python

import re
import sys

if(len(sys.argv)>1):
    dataFolder = str(sys.argv[1])
else:
    dataFolder = ""

unitunitsIDX = dataFolder+"units/unitunits.idx"
unitnamesIDX = dataFolder+"units/unitnames.idx"
unittintsIDX = dataFolder+"units/unittints.idx"
unitskillsIDX = dataFolder+"units/unitskills.idx"
unitstatsIDX = dataFolder+"units/unitstats.idx"
unititemsIDX = dataFolder+"units/unititems.idx"
unitschoolsIDX = dataFolder+"units/unitschools.idx"
skillsTOK = dataFolder+"config/skills.tok"
classdefsTOK = dataFolder+"config/classdefs.tok"
itemsTOK = dataFolder+"config/items.tok"
gladiatorsOutput = dataFolder+"units/gladiators.txt"
skillsOutput = dataFolder+"debug/skills.txt"
classesOutput = dataFolder+"debug/classes.txt"
itemsOutput = dataFolder+"debug/items.txt"
tintsOutput = dataFolder+"units/tints.txt"
skillsetsOutput = dataFolder+"units/skillsets.txt"
statsetsOutput = dataFolder+"units/statsets.txt"
itemsetsOutput = dataFolder+"units/itemsets.txt"
schoolsOutput = dataFolder+"units/schools.txt"

affinity = ["None", "Air", "Dark", "Earth", "Fire", "5 - this should not appear; if it does, something broke", "Water"]

#Parse unit entries from UnitUnits.IDX (dictionary file)
unitEntries = []
#File input to raw unit data array
with open(unitunitsIDX, "rb") as f:
    numUnits = int.from_bytes(f.read(2), "big")
    print("Number of units recorded in file header: "+str(numUnits)+"\n")
    line = f.read(14)
    while line:
        unitEntries.append(line)
        #print(unitEntries)
        line = f.read(14)

#Checking for parity between header and number of scanned entries
print("Number of unit entries scanned: "+str(len(unitEntries))+"\n")
if(len(unitEntries) == numUnits):
    print("File input success: Header matches number of entries.\n")
else:
    print("Error: Header does not match number of entries. Exiting...")
    exit

#Populate unit name addresses and parse UnitNames.IDX
unitNames = []
i=0
with open(unitnamesIDX, "rb") as f:
    for line in unitEntries:
        #print(line[0:2])
        f.seek(int.from_bytes(line[0:2], "big"))
        temp = f.read(1)
        unitNames.append(temp)
        while True:
            temp = f.read(1)
            if(temp == b'\x00'):
                break
            else:
                unitNames[i]+=temp
                #print(unitNames[i])
        #print(unitNames[i])
        unitNames[i] = bytes.decode(unitNames[i],"utf8")
        #print(unitNames[i])
        i+=1

#Populate unit school addresses and parse UnitSchools.IDX
unitSchoolAddresses = []
unitSchools = []
for line in unitEntries:
    #print(line[12:14])
    unitSchoolAddresses.append(int.from_bytes(line[12:14], "big"))
#Remove duplicates and sort
unitSchoolAddressesIndex = list(set(unitSchoolAddresses))
unitSchoolAddressesIndex.sort()
#Parse IDX
i=0
with open(unitschoolsIDX, "rb") as f:
    unitSchoolsIDXFull = f.read()
    #print(len(unitSchoolsIDXFull))
    #Stop school addresses at end of file by adding last entry with EOF offset
    unitSchoolAddressesIndex.append(len(unitSchoolsIDXFull))
    #print(len(unitSchoolAddressesIndex))
    for line in unitSchoolAddressesIndex:
        if(i+1 == len(unitSchoolAddressesIndex)):
            break
        f.seek(unitSchoolAddressesIndex[i])
        #print(i)
        unitSchools.append(bytes.decode(f.read(unitSchoolAddressesIndex[i+1] - unitSchoolAddressesIndex[i] - 1),"utf8"))
        #print(str(unitSchoolAddressesIndex[i]) +" "+ unitSchools[i])
        i+=1
#Assign schoolset numbers to glads
unitSchoolsIndex = []
for line in unitSchoolAddresses:
    i=0
    for line2 in unitSchoolAddressesIndex:
        if(line2 == line):
            unitSchoolsIndex.append(i)
        i+=1

#Index ability names from skills.tok
skills = []
i=0
with open(skillsTOK, "r", encoding="utf8") as f:
    skillsInput = f.readlines()
    
    for line in skillsInput:         
        reOutput = re.search("^NUMENTRIES: (.*?)\n", skillsInput[i])
        #print(skillsInput[i])
        if reOutput:
            numSkills = int(reOutput.group(1))
            print("NUMENTRIES according to skills.tok: "+str(numSkills)+"\n")
        
        reOutput = re.search("^SKILLCREATE: \"(.*?)\",", skillsInput[i])
        if reOutput:
            skills.append(reOutput.group(1))
            #print("Skill found: "+reOutput.group(1))
        
        i+=1

#Index class names from classdefs.tok        
classes = []
i=0
with open(classdefsTOK, "r", encoding="utf8") as f:
    classInput = f.readlines()
    
    for line in classInput:
        reOutput = re.search("^NUMCLASSDEFS: (.*?)\n", classInput[i])
        if reOutput:
            numClasses = int(reOutput.group(1))
            print("NUMCLASSDEFS according to classdefs.tok: "+str(numClasses)+"\n")
        
        reOutput = re.search("^CREATECLASS: (.*?)\n", classInput[i])
        if reOutput:
            classes.append(reOutput.group(1))
        
        i+=1
        
#Index item names from items.tok
items = []
itemsType = []
i=0
with open(itemsTOK, "r", encoding="utf8") as f:
    itemsInput = f.readlines()
    
    for line in itemsInput:         
        reOutput = re.search("^NUMENTRIES: (.*?)\n", itemsInput[i])
        #print(itemsInput[i])
        if reOutput:
            numItems = int(reOutput.group(1))
            print("NUMENTRIES according to items.tok: "+str(numItems)+"\n")
        
        reOutput = re.search("^ITEMCREATE: \"(.*?)\", \"(.*?)\",", itemsInput[i])
        if reOutput:
            items.append(reOutput.group(1))
            itemsType.append(reOutput.group(2))
            #print("Item found: "+reOutput.group(1))
        
        i+=1

#Index unit classes
unitClasses = []
for line in unitEntries:
    unitClasses.append(classes[int.from_bytes(line[2:3], "big")])
    
#Index unit outfits and affinity.
#This implementation is crazy. It takes the fourth byte (e.g., 0x04 for Aaden), converts to binary with 8 digits (00100011), then does left(5), resulting in 00100, then converts that to decimal for the outfit number (4). Affinity is defined by the remaining 3 bits (011 for Aaden, which is 3 AKA earth).
unitOutfits = []
unitAffinity = []
for line in unitEntries:
    temp = bin(int.from_bytes(line[3:4], "big"))[2:].zfill(8)
    unitOutfits.append(int(temp[0:5],2))
    #print(affinity[int(temp[5:8],2)])
    unitAffinity.append(affinity[int(temp[5:8],2)])
    
#Index tints and parse UnitTints.IDX
tintInput = []
unitTints = []
for line in unitEntries:
    unitTints.append(max(0,int((int.from_bytes(line[4:6], "big")+17)/18)))
    #print(line[4:6])
    #print(max(0,int((int.from_bytes(line[4:6], "big")-1)/18)))
with open(unittintsIDX, "rb") as f:
    tintInput.append(int.from_bytes(f.read(1), "big"))
    line = f.read(18)
    while line:
        tintInput.append(line)
        #print(tintInput)
        line = f.read(18)


#Parse UnitSkills.IDX and populate unit skillset addresses
#New skillset handling 2024

#Compile list of expected skillset addresses from unitunits data; each unit entry points to a spot in unititems where it thinks its skillset starts
unitSkillAddresses = []
for line in unitEntries:
    #print(line[6:8])
    unitSkillAddresses.append(int.from_bytes(line[6:8], "big"))

#Parse skillsets
unitSkillAddressesIndex = [1]
unitSkillsRaw = []
skillsetRaw = bytearray()
i=0
with open(unitskillsIDX, "rb") as f:
    unitSkillsIDXFull = f.read()
    #print(len(unitSkillsIDXFull))
    f.seek(1)
    j=0
    while True:
        data = f.read(2)
        #Check for end of file and break if found
        if not data:
            break
        #print(data)
        #Check for end of skillset, append if not, move forward if so
        if(data == b'\x00\x00' and (len(skillsetRaw) % 4 == 0 or len(skillsetRaw) == 0)):
            #print(skillsetRaw)
            j+=1
            #Checking if skillset length is is divisible by 4
            #print("Skillset "+ str(j))
            #print(len(skillsetRaw))
            unitSkillsRaw.append(bytes(skillsetRaw))
            unitSkillAddressesIndex.append(unitSkillAddressesIndex[i]+len(skillsetRaw)+2)
            skillsetRaw = bytearray()
            #print("END OF SKILLSET")
            i+=1
        else:
            #print(data)
            skillsetRaw.append(int.from_bytes(data[0:1],"big"))
            skillsetRaw.append(int.from_bytes(data[1:2],"big"))

#Assign itemset numbers to glads
unitSkills = []
for line in unitSkillAddresses:
    #print(line)
    i=0
    for line2 in unitSkillAddressesIndex:
        if(line2 == line):
            unitSkills.append(i)
        i+=1

#print("Skillset handling successful.")

'''
#OLD SKILLSET HANDLING - BREAKS IF THERE IS A SKILLSET THAT IS UNUSED  
#Populate unit skill addresses and parse UnitSkills.IDX
unitSkillAddresses = []
unitSkillsRaw = []
for line in unitEntries:
    #print(line[6:8])
    unitSkillAddresses.append(int.from_bytes(line[6:8], "big"))
#Remove duplicates and sort
unitSkillAddressesIndex = list(set(unitSkillAddresses))
unitSkillAddressesIndex.sort()
#Parse IDX
i=0
with open(unitskillsIDX, "rb") as f:
    unitSkillsIDXFull = f.read()
    #print(len(unitSkillsIDXFull))
    #Stop skill addresses at end of file by adding last entry with EOF offset
    unitSkillAddressesIndex.append(len(unitSkillsIDXFull))
    #print(len(unitSkillAddressesIndex))
    for line in unitSkillAddressesIndex:
        if(i+1 == len(unitSkillAddressesIndex)):
            break
        f.seek(unitSkillAddressesIndex[i])
        #print(i)
        unitSkillsRaw.append(f.read(unitSkillAddressesIndex[i+1] - unitSkillAddressesIndex[i] - 2))
        #print(str(unitSkillAddressesIndex[i]) +" "+ str(unitSkillsRaw[i]))
        i+=1
#Assign skillset numbers to glads
unitSkills = []
for line in unitSkillAddresses:
    i=0
    for line2 in unitSkillAddressesIndex:
        if(line2 == line):
            unitSkills.append(i)
        i+=1
'''

'''
#OLD STATSET HANDLING - Results in data loss if statset is unused
#Populate unit stat addresses and parse UnitStats.IDX
unitStatAddresses = []
unitStatsRaw = []
for line in unitEntries:
    unitStatAddresses.append(int.from_bytes(line[8:10], "big"))
    #print(int.from_bytes(line[8:10], "big"))
#Remove duplicates and sort
unitStatAddressesIndex = list(set(unitStatAddresses))
unitStatAddressesIndex.sort()
#Parse IDX
i=0
with open(unitstatsIDX, "rb") as f:
    unitStatsIDXFull = f.read()
    unitStatAddressesIndex.append(len(unitStatsIDXFull))
    for line in unitStatAddressesIndex:
        if(i+1 == len(unitStatAddressesIndex)):
            break
        f.seek(unitStatAddressesIndex[i])
        unitStatsRaw.append(f.read(150))
        i+=1
#Assign statset numbers to glads
unitStats = []
for line in unitStatAddresses:
    i=0
    for line2 in unitStatAddressesIndex:
        if(line2 == line):
            unitStats.append(i)
        i+=1
'''

#New statset handling 2024
#Compile list of expected statset addresses from unitunits data; each unit entry points to a spot in unitstats where it thinks its statset starts
unitStatAddresses = []
for line in unitEntries:
    unitStatAddresses.append(int.from_bytes(line[8:10], "big"))

unitStatAddressesIndex = [1]
unitStatsRaw = []
i=0
with open(unitstatsIDX, "rb") as f:
    unitStatsIDXFull = f.read()
    f.seek(1)
    while True:
        data = f.read(150)
        #Check for EOF and break if found
        if not data:
            break
        unitStatsRaw.append(data)
        unitStatAddressesIndex.append(unitStatAddressesIndex[i]+150)
        i+=1
#Assign statset numbers to glads
unitStats = []
for line in unitStatAddresses:
    i=0
    for line2 in unitStatAddressesIndex:
        if(line2 == line):
            unitStats.append(i)
        i+=1


#Parse UnitItems.IDX and populate unit itemset addresses
#New itemset handling 2024

#Compile list of expected itemset addresses from unitunits data; each unit entry points to a spot in unititems where it thinks its itemset starts
unitItemAddresses = []
for line in unitEntries:
    #print(line[6:8])
    unitItemAddresses.append(int.from_bytes(line[10:12], "big"))

#Parse itemsets
unitItemAddressesIndex = [0,1]
unitItemsRaw = []
itemsetRaw = bytearray()
i=1 # Starts at 1 because there is a 00 00 itemset (i.e., nothing)
unitItemsRaw.append(b'') #Manually append first blank itemset
with open(unititemsIDX, "rb") as f:
    unitItemsIDXFull = f.read()
    #print(len(unitItemsIDXFull))
    f.seek(1)
    j=0
    while True:
        data = f.read(2)
        #Check for end of file and break if found
        if not data:
            break
        #print(data)
        #Check for end of itemset, append if not, move forward if so
        if(data == b'\x00\x00' and (len(itemsetRaw) % 4 == 0 or len(itemsetRaw) == 0)):
            #print(itemsetRaw)
            j+=1
            #Checking if itemset length is is divisible by 4
            #print("Itemset "+ str(j))
            #print(len(itemsetRaw))
            unitItemsRaw.append(bytes(itemsetRaw))
            unitItemAddressesIndex.append(unitItemAddressesIndex[i]+len(itemsetRaw)+2)
            itemsetRaw = bytearray()
            #print("END OF ITEMSET")
            i+=1
        else:
            #print(data)
            itemsetRaw.append(int.from_bytes(data[0:1],"big"))
            itemsetRaw.append(int.from_bytes(data[1:2],"big"))

#Assign itemset numbers to glads
unitItems = []
for line in unitItemAddresses:
    #print(line)
    i=0
    for line2 in unitItemAddressesIndex:
        if(line2 == line):
            unitItems.append(i)
        i+=1

#print("Itemset handling successful.")

'''
#OLD ITEMSET HANDLING - BREAKS IF THERE IS AN ITEMSET THAT IS UNUSED        
#Populate unit item addresses and parse UnitItems.IDX
unitItemAddresses = []
unitItemsRaw = []
for line in unitEntries:
    #print(line[6:8])
    unitItemAddresses.append(int.from_bytes(line[10:12], "big"))
#Remove duplicates and sort
unitItemAddressesIndex = list(set(unitItemAddresses))
unitItemAddressesIndex.sort()
for entry in unitItemAddressesIndex:
    print(entry)
#Parse IDX
i=1 # Starts at 1 because there is a 00 00 item set (i.e., nothing)
unitItemsRaw.append(b'')
with open(unititemsIDX, "rb") as f:
    unitItemsIDXFull = f.read()
    #print(len(unitItemsIDXFull))
    #Stop item addresses at end of file by adding last entry with EOF offset
    unitItemAddressesIndex.append(len(unitItemsIDXFull))
    #print(len(unitItemAddressesIndex))
    for line in unitItemAddressesIndex:
        if(i+1 == len(unitItemAddressesIndex)):
            break
        f.seek(unitItemAddressesIndex[i])
        #print(i)
        unitItemsRaw.append(f.read(unitItemAddressesIndex[i+1] - unitItemAddressesIndex[i] - 2))
        #print(str(unitItemAddressesIndex[i]))
        print(str(unitItemAddressesIndex[i]) +" "+ str(unitItemsRaw[i]))
        i+=1
#Assign itemset numbers to glads
unitItems = []
for line in unitItemAddresses:
    #print(line)
    i=0
    for line2 in unitItemAddressesIndex:
        if(line2 == line):
            unitItems.append(i)
        i+=1
'''


#Output to file
i=0
with open(gladiatorsOutput, "w", encoding="utf8", newline="\n") as f:
    for line in unitEntries:
        f.write("Name: "+unitNames[i]+"\n")
        #f.write("Raw data: "+str(line)+"\n")
        f.write("Class: "+str(unitClasses[i])+"\n")
        f.write("Outfit: "+str(unitOutfits[i])+"\n")
        f.write("Affinity: "+str(unitAffinity[i])+"\n")
        f.write("Tint set: "+str(unitTints[i])+"\n")
        f.write("Skill set: "+str(unitSkills[i])+"\n")
        #f.write("Raw skill set: "+str(unitSkillsRaw[unitSkills[i]])+"\n")
        f.write("Stat set: "+str(unitStats[i])+"\n")
        #f.write("Raw stat set: "+str(unitStatsRaw[unitStats[i]])+"\n")
        f.write("Item set: "+str(unitItems[i])+"\n")
        #f.write("Raw item set: "+str(unitItemsRaw[unitItems[i]])+"\n")
        f.write("School: "+str(unitSchoolsIndex[i])+"\n")
        f.write("\n")
        i+=1

'''
with open(skillsOutput, "w", encoding="utf8") as f:
    f.write("NUMENTRIES: "+str(numSkills)+"\n")
    for line in skills:
        f.write(line+"\n")
        
with open(classesOutput, "w", encoding="utf8") as f:
    f.write("NUMENTRIES: "+str(numClasses)+"\n")
    for line in classes:
        f.write(line+"\n")
        
with open(itemsOutput, "w", encoding="utf8") as f:
    f.write("NUMENTRIES: "+str(numItems)+"\n")
    i=0
    for line in items:
        f.write(line+" ["+itemsType[i]+"]\n")     
        i+=1
'''

with open(schoolsOutput, "w", encoding="utf8") as f:
    f.write("Header (IDK what this means; change it if you want to experiment): "+str(int.from_bytes(unitSchoolsIDXFull[0:1], "big"))+"\n\n")
    f.write("NUMENTRIES: "+str(len(unitSchools))+"\n\n")    
    i=0
    for line in unitSchools:
        f.write(str(i)+": "+line+"\n")
        i+=1
        
with open(skillsetsOutput, "w", encoding="utf8") as f:
    f.write("NUMENTRIES: "+str(len(unitSkillsRaw))+"\n\n")
    f.write("Legend:\n")
    f.write("MinLevelLearned MaxLevelLearned SkillName\n\n")
    i=0
    for line in unitSkillsRaw:
        f.write("Skillset "+str(i)+":\n")
        #f.write("Raw (do not edit): "+str(line)+"\n")
        entries = [line[j:j+4] for j in range(0,len(line),4)]
        for entry in entries:
            f.write(str(int.from_bytes(entry[0:1],"big"))+" "+str(int.from_bytes(entry[1:2],"big"))+" "+skills[int.from_bytes(entry[2:4],"big")]+"\n")
        #f.write(str(entries))
        f.write("\n")
        i+=1

with open(statsetsOutput, "w", encoding="utf8") as f:
    f.write("NUMENTRIES: "+str(len(unitStatsRaw))+"\n")
    f.write("Legend:\n")
    f.write("Level: Con Pow Acc Def Ini/Mov\n\n")
    i=0
    for line in unitStatsRaw:
        f.write("Statset "+str(i)+":\n")
        #f.write("Raw (do not edit): "+str(line)+"\n")
        entries = [line[j:j+5] for j in range(0,len(line),5)]
        j=1
        for entry in entries:
            f.write(str(j)+": "+str(int.from_bytes(entry[0:1],"big"))+" "+str(int.from_bytes(entry[1:2],"big"))+" "+str(int.from_bytes(entry[2:3],"big"))+" "+str(int.from_bytes(entry[3:4],"big"))+" "+str(int.from_bytes(entry[4:5],"big"))+"\n")
            j+=1
        f.write("\n")
        i+=1

with open(itemsetsOutput, "w", encoding="utf8") as f:
    f.write("NUMENTRIES: "+str(len(unitItemsRaw))+"\n\n")
    f.write("Legend:\n")
    f.write("MinLevelUsed MaxLevelUsed ItemName  [Any text here is ignored; use this space for notes. If you don't want to use it, delete the square brackets and remove any whitespace after the item name.]\n\n")
    i=0
    for line in unitItemsRaw:
        f.write("Itemset "+str(i)+":\n")
        #f.write("Raw (do not edit): "+str(line)+"\n")
        #print(line)
        entries = [line[j:j+4] for j in range(0,len(line),4)]
        for entry in entries:
            #print(entry)
            #print(str(int.from_bytes(entry[0:1],"big"))+" "+str(int.from_bytes(entry[1:2],"big"))+" "+items[int.from_bytes(entry[2:4],"big")]+"\n")
            f.write(str(int.from_bytes(entry[0:1],"big"))+" "+str(int.from_bytes(entry[1:2],"big"))+" "+items[int.from_bytes(entry[2:4],"big")]+" ["+itemsType[int.from_bytes(entry[2:4],"big")]+"]\n")
        #f.write(str(entries))
        f.write("\n")
        i+=1

i=0        
with open(tintsOutput, "w", encoding="utf8") as f:
    for line in tintInput:
        if(i!=0):
            f.write("Tint set "+str(i)+":\n")
            #f.write("Raw (do not edit): "+str(line)+"\n")
            f.write("Cloth1: "+str(int.from_bytes(line[0:1],"big"))+" "+str(int.from_bytes(line[1:2],"big"))+" "+str(int.from_bytes(line[2:3],"big"))+"\n")
            f.write("Cloth2: "+str(int.from_bytes(line[3:4],"big"))+" "+str(int.from_bytes(line[4:5],"big"))+" "+str(int.from_bytes(line[5:6],"big"))+"\n")
            f.write("Armor1: "+str(int.from_bytes(line[6:7],"big"))+" "+str(int.from_bytes(line[7:8],"big"))+" "+str(int.from_bytes(line[8:9],"big"))+"\n")
            f.write("Armor2: "+str(int.from_bytes(line[9:10],"big"))+" "+str(int.from_bytes(line[10:11],"big"))+" "+str(int.from_bytes(line[11:12],"big"))+"\n")
            f.write("Skin: "+str(int.from_bytes(line[12:13],"big"))+" "+str(int.from_bytes(line[13:14],"big"))+" "+str(int.from_bytes(line[14:15],"big"))+"\n")
            f.write("Hair: "+str(int.from_bytes(line[15:16],"big"))+" "+str(int.from_bytes(line[16:17],"big"))+" "+str(int.from_bytes(line[17:18],"big"))+"\n\n")
        else:
            f.write("Header (IDK what this means; change it if you want to experiment): "+str(tintInput[0])+"\n\n")
            f.write("NUMENTRIES: "+str(len(tintInput))+"\n\n")
            f.write("Tint set 0:\nThis tint set is blank; it is unknown what it means for a unit to use tint set 0 (possibly default colours?). Needs more research.\n\n")
        i+=1