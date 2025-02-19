import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('rtt.csv')

plt.rcParams.update({
    "text.usetex": False, 
    "font.family": "serif",
    "font.size": 13,
    "axes.titlesize": 16,
    "axes.labelsize": 14,
    "xtick.labelsize": 12,
    "ytick.labelsize": 12,
    "legend.fontsize": 12
})

fig, ax = plt.subplots(figsize=(7, 5)) 

ax.plot(df['Payload'], df['RTT'], marker='o', linestyle='-', color='black', markersize=8, linewidth=1.8)

ax.set_xscale('log')
ax.set_xticks([8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
ax.set_xticklabels(['8B', '16B', '32B', '64B', '128B', '256B', '512B', '1KB', '2KB', '4KB'])

ax.set_xlabel('Payload')
ax.set_ylabel('RTT (µs)')
ax.set_title('RTT vs. Payload')

ax.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.6)

plt.tight_layout()

plt.savefig("rtt.png", dpi=300, bbox_inches='tight')

plt.show()
