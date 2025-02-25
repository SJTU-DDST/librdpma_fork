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
with open('1.json', 'r') as f:
    data = json.load(f)

# Extract number of threads and bandwidths
# num_threads = [1, 2, 4, 8, 16]
batch_sizes = [1, 4, 16, 64]
payloads = [64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768]
labels = ["64B", "128B", "256B", "512B", "1KB", "2KB", "4KB", "8KB", "16KB", "32KB"]
bandwidths = [[entry['bandwidth'] for entry in batch] for batch in data]

# Colors for Modern Minimalist (Teal & Orange) style with additional harmonic colors
colors = ['#008080', '#E67E22', '#264653', '#A53860', '#FFBA08', '#3D348B',]
markers = ['o', 's', 'D', '^', 'v']

# Create the figure and axis
fig, ax = plt.subplots(figsize=(7, 5))

# Plot each batch size as a different line with distinct markers
for i, batch_size in enumerate(batch_sizes):
    ax.plot(payloads, [a / b for a, b in zip([x / 8 * 1024 * 1024 * 1024 / 1000000 for x in bandwidths[i]], payloads)], marker=markers[i], linestyle='-', 
            color=colors[i], markersize=9, markerfacecolor='none', markeredgewidth=1.5, linewidth=2, label=f'Batch Size {batch_size}')

# Labels and title with improved font styling
ax.set_xlabel("Payload", fontsize=16)
ax.set_ylabel("Throughput (M ops/s)", fontsize=16)
# ax.set_title("Bandwidth vs. Number of Threads", fontsize=18, pad=15)
ax.set_xscale("log", base=2)
ax.set_ybound(0, 50)

# Grid and formatting
ax.grid(True, which='both', linestyle='--', linewidth=0.7, color='#AAAAAA', alpha=0.7)
ax.spines['top'].set_visible(True)
ax.spines['right'].set_visible(True)
ax.tick_params(axis='both', which='major', labelsize=14)
ax.set_xticks(payloads)
ax.set_xticklabels(labels, fontsize=14)

# Legend
ax.legend(frameon=True, loc='best', fancybox=True, framealpha=0.7)

ax.text(0.05, 0.95, "Threads = 1", transform=ax.transAxes, fontsize=14, 
        verticalalignment='top', bbox=dict(facecolor='white', alpha=0.5))

# Tight layout for better spacing
plt.tight_layout()

# Save the figure (optional)
plt.savefig("throughput_vs_payload_batchsize_1.pdf", dpi=300, bbox_inches='tight')

# Show the plot
plt.show()
