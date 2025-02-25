import matplotlib.pyplot as plt
import numpy as np
import json

plt.rcParams.update({
    "font.family": "sans-serif",
    "font.size": 13,
    "axes.titlesize": 16,
    "axes.labelsize": 14,
    "xtick.labelsize": 12,
    "ytick.labelsize": 12,
    "legend.fontsize": 12
})

# Load data from JSON file
with open('bench_7.json', 'r') as f:
    data = json.load(f)

# Extract number of threads and bandwidths
# num_threads = [1, 2, 4, 8, 16]
payloads = [64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768]
labels = ["64B", "128B", "256B", "512B", "1KB", "2KB", "4KB", "8KB", "16KB", "32KB"]
ys = [[entry['latency'] for entry in batch] for batch in data]

# Colors for Modern Minimalist (Teal & Orange) style with additional harmonic colors
colors = ['#008080', '#E67E22', '#264653', '#A53860', '#FFBA08', '#3D348B',]
markers = ['o', 's', 'D', '^', 'v']

# Create the figure and axis
fig, ax = plt.subplots(figsize=(7, 5))

ax.plot(payloads, ys[0], marker=markers[0], linestyle='-', 
            color=colors[0], markersize=9, markerfacecolor='none', markeredgewidth=1.5, linewidth=2, label=f'DPU Read')
ax.plot(payloads, ys[1], marker=markers[1], linestyle='-', 
            color=colors[1], markersize=9, markerfacecolor='none', markeredgewidth=1.5, linewidth=2, label=f'DPU Write')

# Labels and title with improved font styling
ax.set_xlabel("Payload", fontsize=16)
ax.set_ylabel("Latency (μs)", fontsize=16)
# ax.set_title("Bandwidth vs. Number of Threads", fontsize=18, pad=15)
ax.set_xscale("log", base=2)
# ax.set_ybound(0, 330)
# ax.set_yscale("log", base=10)

# Grid and formatting
ax.grid(True, which='both', linestyle='--', linewidth=0.7, color='#AAAAAA', alpha=0.7)
ax.spines['top'].set_visible(True)
ax.spines['right'].set_visible(True)
ax.tick_params(axis='both', which='major', labelsize=14)
ax.set_xticks(payloads)
ax.set_xticklabels(labels, fontsize=14)

# Legend
ax.legend(frameon=True, loc='best', fancybox=True, framealpha=0.7)

ax.text(0.05, 0.95, "Threads = 8, Batch Size = 8", transform=ax.transAxes, fontsize=14, 
        verticalalignment='top', bbox=dict(facecolor='white', alpha=0.5))

# Tight layout for better spacing
plt.tight_layout()

# Save the figure (optional)
plt.savefig("latency_vs_payload_dpureadwrite.pdf", dpi=300, bbox_inches='tight')

# Show the plot
plt.show()