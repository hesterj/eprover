import os
import itertools
from subprocess import call

variables = ["X1",
			"X2",
			"X3",
			"X4",
			"X5",
			"X6",
			"X7",
			"X8",
			"X9",
			"X10",
			"X11",
			"X12",
			"X13",]

siglines = []
functionsymbols = []
predicatesymbols = []
connectives = []

terms = variables
wff = []

name = "signature.txt"
fc = open(name,"r")
read = fc.readlines()
for line in read:
	line = ''.join(line.split())
	siglines.append(line)
#print(siglines)

for line in siglines:
	symbolinfo = line.split(":")
	if symbolinfo[2] == "f":
		if (symbolinfo[1] == '0'):
			terms.append(symbolinfo[0])
		else:
			functionsymbols.append(symbolinfo)
	elif symbolinfo[2] == "p":
		if (symbolinfo[1] == '0'):
			wff.append(symbolinfo[0])
		else:
			if "$" not in symbolinfo[0]:
				predicatesymbols.append(symbolinfo)
	else:
		connectives.append(symbolinfo)
		
#qex = ["?["]
		
equals = ["=","2","p"]
predicatesymbols.append(equals)
		
print('connectives')

for line in connectives:
	print(line)
		
print('functions')

for line in functionsymbols:
	print(line)
	
print('predicates')

for line in predicatesymbols:
	print(line)
	
print('terms')

for line in terms:
	print(line)	

while(len(terms)<100):
	newterms = []
	for funsym in functionsymbols:
		sym = funsym[0]
		arity = int(funsym[1])
		termcombinations = itertools.combinations(terms, arity)
		for acombination in termcombinations:
			if len(newterms)>1000:
				break
			newterm = sym + "("
			counter = 0
			for term in acombination:
				newterm += term
				if counter != arity-1:
					newterm += ","
				counter += 1
			newterm += ")"
			if newterm not in terms:
				newterms.append(newterm)
	terms = terms + newterms
	
print("New terms: " + str(len(terms)))
	
atomicformulas = []

while (len(atomicformulas)<1000):
	
	for predsym in predicatesymbols:
		newatomforms = []
		sym = predsym[0]
		arity = int(predsym[1])
		termcombinations = itertools.combinations(terms,arity)
		for acombination in termcombinations:
			
			if len(newatomforms)>2000:
				atomicformulas = atomicformulas + newatomforms
				break
			
			if sym == "=":
				
				newatform = acombination[0]+"="+acombination[1]
				if newatform not in atomicformulas:
					newatomforms.append(newatform)
			else:
				newatform = sym + "("
				counter = 0
				for term in acombination:
					newatform += term
					if (counter !=arity -1):
						newatform += ","
					counter += 1
				newatform += ")"
				if newatform not in atomicformulas:
					newatomforms.append(newatform)
		atomicformulas = atomicformulas + newatomforms
		
wffs = []

logsymbols = [["|",2],
			  ["&",2],
			  ["!",1],
			  ["~",1]]
	
while (len(wffs)<1000):
	
	for logsym in logsymbols:
		newwellforms = []
		sym = logsym[0]
		arity = int(logsym[1])
		termcombinations = itertools.combinations(atomicformulas,arity)
		for acombination in termcombinations:
			
			if len(newwellforms)>1000:
				wffs = wffs + newwellforms
				break
			
			if sym == "|":
				newwff = acombination[0]+"|"+acombination[1]
				if newwff not in wffs:
					newwellforms.append(newwff)
					
			if sym == "&":
				newwff = acombination[0]+"&"+acombination[1]
				if newwff not in wffs:
					newwellforms.append(newwff)
					
			if sym == "~":
				newwff = "~"+acombination[0]
				if newwff not in wffs:
					newwellforms.append(newwff)
					
			if sym == "!":
				for x in variables:
					newwff = "!["+x+"]:"+acombination[0]
					if newwff not in wffs:
						newwellforms.append(newwff)		
							
		wffs = wffs + newwellforms
	
for x in wffs:
	print(x)
	
print(str(len(wffs)))
	
	
"""
while len(terms) < 1000:
	for funsymb in functionsymbols:
		if ()
"""

