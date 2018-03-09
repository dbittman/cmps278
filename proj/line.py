import numpy as np
import matplotlib.pyplot as plt

nvkvline = []
bdline = []
tmp = 0

count = 0
m = 1000
with open("line.nv") as f:
    content = f.readlines()
    for l in content:
        l = l.strip()
        # outliers caused by hash function + workload.
        if len(l) <= 5:
            tmp += float(l)
            count += 1
            if count == m:
                nvkvline.append(tmp / m)
                count = 0
                tmp = 0


with open("line.bd") as f:
    content = f.readlines()
    for l in content:
        l = l.strip()
        if len(l) <= 5:
            tmp += float(l)
            count += 1
            if count == m:
                bdline.append(tmp / m)
                count = 0
                tmp = 0


xline = np.arange(0., len(nvkvline)*m, m)
xline2 = np.arange(0., len(bdline)*m, m)
nv = plt.plot(xline, nvkvline, label="NVKV")
bd = plt.plot(xline2, bdline, label="Berkeley DB")
plt.xlabel("Number of inserts")
plt.ylabel("Insert latency (ns)")

plt.legend()

plt.savefig("../paper/fig/line.pdf")
#plt.show()

