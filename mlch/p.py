#!/usr/bin/env python
import math
import numpy as np
import matplotlib.pyplot as plt

with open("out_o") as f:
    content = f.readlines()
content = [x.strip() for x in content] 

means_old = {}
sd_old = {}
count_old = {}

for c in content:
    count, moves = c.split()
    count = int(count)
    moves = int(moves)
    means_old[count] = means_old.get(count, 0) + moves
    sd_old[count] = sd_old.get(count, 0) + moves ** 2
    count_old[count] = count_old.get(count, 0) + 1


x = []
y_old = []
yerr_old = []
for k, v in count_old.items():
    means_old[k] /= v
    sd_old[k] = math.sqrt((sd_old[k] / v) - means_old[k]**2)
    x.append(k)
    y_old.append(means_old[k])
    yerr_old.append(sd_old[k] * 1.96)







with open("out_w") as f:
    content = f.readlines()
content = [x.strip() for x in content] 

means_new = {}
sd_new = {}
count_new = {}

for c in content:
    count, moves = c.split()
    count = int(count)
    moves = int(moves)
    means_new[count] = means_new.get(count, 0) + moves
    sd_new[count] = sd_new.get(count, 0) + moves ** 2
    count_new[count] = count_new.get(count, 0) + 1


x = []
y_new = []
yerr_new = []
for k, v in count_new.items():
    means_new[k] /= v
    sd_new[k] = math.sqrt((sd_new[k] / v) - means_new[k]**2)
    x.append(k)
    y_new.append(means_new[k])
    yerr_new.append(sd_new[k] * 1.96)



with open("out_onore") as f:
    content = f.readlines()
content = [x.strip() for x in content] 

means_nore = {}
sd_nore = {}
count_nore = {}

for c in content:
    count, moves = c.split()
    count = int(count)
    moves = int(moves)
    means_nore[count] = means_nore.get(count, 0) + moves
    sd_nore[count] = sd_nore.get(count, 0) + moves ** 2
    count_nore[count] = count_nore.get(count, 0) + 1


x = []
y_nore = []
yerr_nore = []
for k, v in count_nore.items():
    means_nore[k] /= v
    sd_nore[k] = math.sqrt((sd_nore[k] / v) - means_nore[k]**2)
    x.append(k)
    y_nore.append(means_nore[k])
    yerr_nore.append(sd_nore[k] * 1.96)


for i in range(0, len(yerr_nore)):
    yerr_new[i] /= 2*1.96
    yerr_old[i] /= 2*1.96
    yerr_nore[i] /= 2*1.96








# First illustrate basic pyplot interface, using defaults where possible.
plt.figure()
old = plt.errorbar(x, y_old, yerr=yerr_old)
new = plt.errorbar(x, y_new, yerr=yerr_new)
nore = plt.errorbar(x, y_nore, yerr=yerr_nore)

plt.legend([old, new, nore], ['Old', 'New', 'Old (not counting rehash)'])

plt.title("Simplest errorbars, 0.2 in x, 0.4 in y")

#ax.set_yscale('log')
# Here we have to be careful to keep all y values positive:
#ylower = np.maximum(1e-2, y - yerr)
#yerr_lower = y - ylower

#ax.errorbar(x, y, yerr=[yerr_lower, 2*yerr], xerr=xerr,
#            fmt='o', ecolor='g', capthick=2)
#ax.set_title('Mixed sym., log y')

#fig.suptitle('Variable errorbars')

plt.show()

