#!/usr/bin/python

import re
import sys

if(len(sys.argv)>1):
    dataFolder = str(sys.argv[1])
else:
    dataFolder = ""

gladiatorsTXT = dataFolder+"units/gladiators.txt"
tintsTXT = dataFolder+"units/tints.txt"
skillsetsTXT = dataFolder+"units/skillsets.txt"
statsetsTXT = dataFolder+"units/statsets.txt"
itemsetsTXT = dataFolder+"units/itemsets.txt"
schoolsTXT = dataFolder+"units/schools.txt"
classdefsTOK = dataFolder+"config/classdefs.tok"
skillsTOK = dataFolder+"config/skills.tok"
itemsTOK = dataFolder+"config/items.tok"
unitNamesIDX = dataFolder+"units/unitnames.idx"
unitTintsIDX = dataFolder+"units/unittints.idx"
unitSkillsIDX = dataFolder+"units/unitskills.idx"
unitStatsIDX = dataFolder+"units/unitstats.idx"
unitItemsIDX = dataFolder+"units/unititems.idx"
unitSchoolsIDX = dataFolder+"units/unitschools.idx"
unitUnitsIDX = dataFolder+"units/unitunits.idx"
affinity = ["None", "Air", "Dark", "Earth", "Fire", "5 - this should not appear; if it does, something broke", "Water"]

unitData = []
unitDataRaw = []
unitUnitsRaw = []

#Parse Gladiators.txt
unitNames = []
nameOffsets = []
unitOutfits = []
unitAffinities = []
unitTints = []
unitSkillsets = []
unitItemsets = []
unitStatsets = []
unitSchools = []
numEntries = []
i = 1
with open(gladiatorsTXT, "r", newline="\n") as f:
    unitData = f.readlines()
    f.seek(0)
    unitDataRaw = f.read()
    reOutput = re.findall("(?s)Name: (.*?)\nClass", unitDataRaw)
    for matchObj in reOutput:
        unitNames.append(matchObj)
        nameOffsets.append(i)
        i+=len(matchObj)+1
        #print(i)
numEntries = len(unitNames)
#print("Entries: "+str(numEntries))
i=0
for element in nameOffsets:
    #unitUnitsRaw.append(bytes(f"{element:0{4}x}","utf8"))
    unitUnitsRaw.append(element.to_bytes(2, "big"))
    #print(unitUnitsRaw[i])
    i+=1
for line in unitData:
    reOutput = re.search("^Outfit: (.*?)$", line)
    if(reOutput):
        unitOutfits.append(reOutput.group(1))
        #print(reOutput.group(1))
for line in unitData:
    reOutput = re.search("^Affinity: (.*?)$", line)
    if(reOutput):
        try:
            unitAffinities.append(affinity.index(reOutput.group(1)))
        except:
            print("An affinity entry is invalid. Here is the entry so you can fix it: "+reOutput.group(1))
        #print(affinity.index(reOutput.group(1)))
for line in unitData:
    reOutput = re.search("^Tint set: (.*?)$", line)
    if(reOutput):
        unitTints.append(reOutput.group(1))
        #print(reOutput.group(1))
for line in unitData:
    reOutput = re.search("^Skill set: (.*?)$", line)
    if(reOutput):
        unitSkillsets.append(reOutput.group(1))
        #print(reOutput.group(1))
for line in unitData:
    reOutput = re.search("^Stat set: (.*?)$", line)
    if(reOutput):
        unitStatsets.append(reOutput.group(1))
        #print(reOutput.group(1))
for line in unitData:
    reOutput = re.search("^Item set: (.*?)$", line)
    if(reOutput):
        unitItemsets.append(reOutput.group(1))
        #print(reOutput.group(1))
for line in unitData:
    reOutput = re.search("^School: (.*?)$", line)
    if(reOutput):
        #print(reOutput.group(1))
        unitSchools.append(reOutput.group(1))
        
#Index class names from classdefs.tok        
classes = []
i=0
with open(classdefsTOK, "r", encoding="utf8") as f:
    classInput = f.readlines()
    
    for line in classInput:
        reOutput = re.search("^NUMCLASSDEFS: (.*?)\n", classInput[i])
        if reOutput:
            numClasses = int(reOutput.group(1))
            #print("NUMCLASSDEFS according to classdefs.tok: "+str(numClasses)+"\n")
        
        reOutput = re.search("^CREATECLASS: (.*?)\n", classInput[i])
        if reOutput:
            classes.append(reOutput.group(1))
        
        i+=1
#Calculate class index for each unit
unitClasses = []
unitClassesText = []
for line in unitData:
    reOutput = re.search("^Class: (.*?)$", line)
    if reOutput:
        unitClassesText.append(reOutput.group(1))
        #print(reOutput.group(1))
i=0
for line in unitClassesText:
    j=0
    #print(line)
    unitClasses.append(classes.index(line))
    unitUnitsRaw[i] += unitClasses[i].to_bytes(1, "big")
    i+=1

#Calculate outfit and affinity bytes
i=0
for line in unitNames:
    temp = '{:05b}'.format(int(unitOutfits[i]))+'{:03b}'.format(int(unitAffinities[i]))
    #print(temp)
    #print(str(unitAffinities[i])+" "+temp2)
    temp = int(temp, 2).to_bytes(1, "big")
    unitUnitsRaw[i] += temp
    #print(temp)
    i+=1

#Index tints from tints TXT
tintsRaw = []
tints = []
with open(tintsTXT, "r") as f:
    tintsRaw = f.readlines()
tintsHeader = re.search(": (.*?)$", tintsRaw[0]).group(1)
#print(tintsHeader)
for line in tintsRaw:
    reOutput = re.search(r"^(?:Skin|Hair|Cloth1|Cloth2|Armor1|Armor2): (\d*?) (\d*?) (\d*?)$", line)
    if reOutput:
        tints.append(reOutput.group(1))
        tints.append(reOutput.group(2))
        tints.append(reOutput.group(3))

#Calculate tint bytes
i=0
for line in unitTints:
    if(int(line)==0):
        temp = b'\x00\x00'
    else:
        temp = int(((int(line)-1)*18+1)).to_bytes(2, "big")
    #print(temp)
    unitUnitsRaw[i] += temp
    i+=1

#Index ability names from skills.tok
skills = []
with open(skillsTOK, "r", encoding="utf8") as f:
    skillsInput = f.readlines()
    
    for line in skillsInput:         
        reOutput = re.search("^NUMENTRIES: (.*?)\n", line)
        #print(line)
        if reOutput:
            numSkills = int(reOutput.group(1))
            #print("NUMENTRIES according to skills.tok: "+str(numSkills)+"\n")
        
        reOutput = re.search("^SKILLCREATE: \"(.*?)\",", line)
        if reOutput:
            skills.append(reOutput.group(1))
            #print("Skill found: "+reOutput.group(1))
        
#Index item names from items.tok
items = []
with open(itemsTOK, "r", encoding="utf8") as f:
    itemsInput = f.readlines()
    
    for line in itemsInput:         
        reOutput = re.search("^NUMENTRIES: (.*?)\n", line)
        #print(line)
        if reOutput:
            numItems = int(reOutput.group(1))
            #print("NUMENTRIES according to items.tok: "+str(numItems)+"\n")
        
        reOutput = re.search("^ITEMCREATE: \"(.*?)\",", line)
        if reOutput:
            items.append(reOutput.group(1))
            #print("Item found: "+reOutput.group(1))

#Index skillsets from skillsets TXT
skillsetsInput = []
skillsets = []
skillsetOffsets = []
skillsetOffsets.append(1)
tempSkillset = []
i=0
with open(skillsetsTXT) as f:
    skillsetsInput = f.readlines()
    
    for line in skillsetsInput:
        reOutput = re.search("^NUMENTRIES: (.*?)\n", line)
        if reOutput:
            numSkillsets = int(reOutput.group(1))
            #print("NUMENTRIES according to skillsets.txt: "+str(numSkillsets)+"\n")
            
        reOutput = re.search(r"^Skillset (\d*?):.*?$", line)
        if reOutput:
            if(int(reOutput.group(1)) == i+1):
                skillsets.append(tempSkillset)
                skillsetOffsets.append(2+(len(tempSkillset)*4)+skillsetOffsets[i])
                #print(skillsetOffsets[i])
                tempSkillset = []
                i+=1
        
        reOutput = re.search(r"^(\d*?) (\d*?) (.*?)$", line)
        if reOutput:
            #print(reOutput.group(3))
            tempSkillset.append([reOutput.group(1), reOutput.group(2), skills.index(reOutput.group(3))])

    skillsets.append(tempSkillset)

#Index statsets from statsets TXT
statsetsInput = []
statsets = []
statsetOffsets = []
statsetOffsets.append(1)
tempStatset = []
i=0
with open(statsetsTXT) as f:
    statsetsInput = f.readlines()
    
    for line in statsetsInput:
        reOutput = re.search("^NUMENTRIES: (.*?)\n", line)
        if reOutput:
            numStatsets = int(reOutput.group(1))
            #print("NUMENTRIES according to statsets.txt: "+str(numStatsets)+"\n")
            
        reOutput = re.search(r"^Statset (\d*?):$", line)
        if reOutput:
            if(int(reOutput.group(1)) == i+1):
                statsets.append(tempStatset)
                statsetOffsets.append((len(tempStatset)*5)+statsetOffsets[i])
                #print(tempStatset)
                #print(statsetOffsets[i])
                tempStatset = []
                i+=1
        
        reOutput = re.search(r"^\d*?: (\d*?) (\d*?) (\d*?) (\d*?) (\d*?)$", line)
        if reOutput:
            tempStatset.append([reOutput.group(1), reOutput.group(2),  reOutput.group(3), reOutput.group(4), reOutput.group(5)])
    
    statsets.append(tempStatset)

#Index itemsets from itemsets TXT
itemsetsInput = []
itemsets = []
itemsetOffsets = []
itemsetOffsets.append(0)
itemsetOffsets.append(1)
tempItemset = []
i=1
with open(itemsetsTXT) as f:
    itemsetsInput = f.readlines()
    
    for line in itemsetsInput:
        reOutput = re.search("^NUMENTRIES: (.*?)\n", line)
        if reOutput:
            numItemsets = int(reOutput.group(1))
            #print("NUMENTRIES according to itemsets.txt: "+str(numItemsets)+"\n")
            
        reOutput = re.search(r"^Itemset (\d*?):$", line)
        if reOutput:
            if(int(reOutput.group(1)) == i+1):
                itemsets.append(tempItemset)
                itemsetOffsets.append(2+(len(tempItemset)*4)+itemsetOffsets[i])
                #print(tempItemset)
                #print(itemsetOffsets[i-1])
                tempItemset = []
                i+=1
        
        reOutput = re.search(r"^(\d*?) (\d*?) (.*?)(\W*?\[.*?\]|$)", line)
        if reOutput:
            #print(reOutput.group(3))
            tempItemset.append([reOutput.group(1), reOutput.group(2), items.index(reOutput.group(3))])
    
    itemsets.append(tempItemset)

#Assign bytes for skill, stat, and itemsets
i=0
for line in unitNames:
    temp = int(skillsetOffsets[int(unitSkillsets[i])]).to_bytes(2, "big")
    #print(temp)
    unitUnitsRaw[i] += temp
    temp = int(statsetOffsets[int(unitStatsets[i])]).to_bytes(2, "big")
    #print(temp)
    unitUnitsRaw[i] += temp
    temp = int(itemsetOffsets[int(unitItemsets[i])]).to_bytes(2, "big")
    #print(temp)
    unitUnitsRaw[i] += temp
    i+=1

#Index schools from schools TXT
schoolsRaw = []
schools = []
schoolOffsets = []
schoolOffsets.append(0)
with open(schoolsTXT) as f:
    schoolsRaw = f.readlines()
schoolsHeader = re.search(": (.*?)$", schoolsRaw[0]).group(1)
#print(schoolsHeader)
for line in schoolsRaw:
    reOutput = re.search(r"\d: (.*?)$", line)
    if reOutput:
        schools.append(reOutput.group(1))
        #print(reOutput.group(1))
schools.pop(0)
schoolOffsets.append(1)
i=0
for line in schools:
    schoolOffsets.append(i+len(line)+2)
    i+=len(line)+1
    
#Assign school offsets to units
unitSchoolOffsets = []
for line in unitSchools:
    #print(line)
    unitSchoolOffsets.append(schoolOffsets[int(line)])

#Assign bytes for schools
i=0
for line in unitSchoolOffsets:
    temp = int(line).to_bytes(2, "big")
    #print(temp)
    unitUnitsRaw[i] += temp
    i+=1

#Generate UnitNames.IDX
with open(unitNamesIDX, "wb") as f:
    f.write(b'\xCD')
    for line in unitNames:
        f.write(str.encode(line))
        f.write(b'\x00')

#Generate UnitSchoolsIDX
with open(unitSchoolsIDX, "wb") as f:
    f.write(int(schoolsHeader).to_bytes(1, "big"))
    for line in schools:
        f.write(str.encode(line))
        f.write(b'\x00')
        i+=1

#Generate UnitTintsIDX
with open(unitTintsIDX, "wb") as f:
    f.write(int(tintsHeader).to_bytes(1, "big"))
    for line in tints:
        f.write(int(line).to_bytes(1, "big"))
        
#Generate UnitSkillsIDX
with open(unitSkillsIDX, "wb") as f:
    f.write(b'\xCD')
    for skillset in skillsets:
        for skill in skillset:
            f.write(int(skill[0]).to_bytes(1,"big"))
            f.write(int(skill[1]).to_bytes(1,"big"))
            f.write(int(skill[2]).to_bytes(2,"big"))
        f.write(b'\x00\x00')

#Generate UnitStatsIDX
with open(unitStatsIDX, "wb") as f:
    f.write(b'\xCD')
    for statset in statsets:
        for stat in statset:
            f.write(int(stat[0]).to_bytes(1,"big"))
            f.write(int(stat[1]).to_bytes(1,"big"))
            f.write(int(stat[2]).to_bytes(1,"big"))
            f.write(int(stat[3]).to_bytes(1,"big"))
            f.write(int(stat[4]).to_bytes(1,"big"))

#Generate UnitItemsIDX
with open(unitItemsIDX, "wb") as f:
    f.write(b'\xCD')
    for itemset in itemsets:
        for item in itemset:
            f.write(int(item[0]).to_bytes(1,"big"))
            f.write(int(item[1]).to_bytes(1,"big"))
            f.write(int(item[2]).to_bytes(2,"big"))
        f.write(b'\x00\x00')

#Generate UnitUnits.IDX
with open(unitUnitsIDX, "wb") as f:
    f.write(numEntries.to_bytes(2, "big"))
    for line in unitUnitsRaw:
        f.write(line)
        #print(line)