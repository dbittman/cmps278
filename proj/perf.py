import numpy as np
import matplotlib.pyplot as plt

test_names = [
        'Put',
        'Get\n(present)',
        'Get\n(not present)',
        'Get\n(hot)'
        ]

nvkv_err = [
        23.04,
        0.8,
        1.36,
        0.5,
        ]

nvkv = [
        1519.24,
        231.02,
        1029.01,
        177.99,
        ]

bdb_err = [
        11.87,
        1.90,
        2.477,
        0.76,
        ]

bdb = [
        2973.05,
        1322.17,
        1663.64,
        316.27,
        ]

x = [1, 3.5, 6, 8.5]
x2 = [2, 4.5, 7, 9.5]

nv = plt.bar(x2, nvkv, yerr=[x*2*1.96 for x in nvkv_err])
bd = plt.bar(x, bdb, yerr=[x*2*1.96 for x in bdb_err])
plt.xticks([1.5, 4, 6.5, 9], test_names)

plt.legend([bd, nv], ["Berkeley DB", "NVKV"])
plt.ylabel("Mean Time (ns)")

plt.savefig("../paper/fig/perf.pdf")
#plt.show()

