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
			predicatesymbols.append(symbolinfo)
	else:
		connectives.append(symbolinfo)
		
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
	
for funsym in functionsymbols:
	sym = funsym[0]
	arity = int(funsym[1])
	termcombinations = itertools.combinations(terms, arity)
	print(termcombinations)
	
	
"""
while len(terms) < 1000:
	for funsymb in functionsymbols:
		if ()
"""

