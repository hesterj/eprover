import os
import itertools
from subprocess import call

siglines = []
functionsymbols = []
predicatesymbols = []
connectives = []

terms = []
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
			
			if len(newatomforms)>100:
				atomicformulas = atomicformulas + newatomforms
				break
			
			if sym == "=":
				newtform = acombination[0]+"="+acombination[1]
				if newatform not in atomicformulas:
					newatforms.append(newatform)
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
	
for x in atomicformulas:
	print(x)
	
print(str(len(atomicformulas)))
	
	
"""
while len(terms) < 1000:
	for funsymb in functionsymbols:
		if ()
"""

