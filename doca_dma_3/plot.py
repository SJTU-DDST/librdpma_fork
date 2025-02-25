# import json
# import matplotlib.pyplot as plt
# import os

# files = os.listdir('.')
# file_name = 'bench_2.json'
# save_dir = 'figures'

# with open(file_name, 'r') as f:
#     data = json.load(f)

# plt.figure(figsize=(10, 6))

# work_tasks = 1
# for group_idx, group in enumerate(data):
#     payload = [entry['payload'] / 1024.0 for entry in group]
#     bandwidths = [entry['bandwidth'] for entry in group]
    
#     plt.plot(payload, bandwidths, label=f'Working Tasks={work_tasks}', marker='o', alpha=1, markersize=3.5, linewidth=1.5)
#     work_tasks *= 2

# plt.xlim(left=0)
# plt.ylim(bottom=0)
# plt.xlabel('Payload (KB)')
# plt.ylabel('Bandwidth (Gbps)')
# plt.title('Bandwidth vs Payload for Different Number of Working tasks (Threads = 16)')
# plt.legend()
# plt.grid(True)

# save_path = save_dir + '/' + 'bench_2' + '.png'
# plt.savefig(save_path)
# print('Figure saved to' + save_path)

import matplotlib.pyplot as plt
import numpy as np

# Data
payload_sizes = [64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768]
bandwidths = [18.261722, 38.083894, 69.414650, 138.388553, 246.365275, 360.707914, 380.884707, 381.345203, 381.414205, 381.464226]
labels = ["64B", "128B", "256B", "512B", "1KB", "2KB", "4KB", "8KB", "16KB", "32KB"]

# Create the figure and axis
fig, ax = plt.subplots(figsize=(6, 4))

# Plot the data with a clean and professional style
ax.plot(payload_sizes, bandwidths, marker='o', linestyle='-', color='#008080', markersize=6, linewidth=2, label='Bandwidth')
# Set logarithmic scale for better visualization
ax.set_xscale("log", base=2)
# Set x-axis labels with common units
ax.set_xticks(payload_sizes)
ax.set_xticklabels(labels)

# Labels and title
ax.set_xlabel("Payload Size", fontsize=12, fontweight='bold')
ax.set_ylabel("Bandwidth (Gbps)", fontsize=12, fontweight='bold')
ax.set_title("Bandwidth vs. Payload Size", fontsize=14, fontweight='bold')

# Grid and formatting
ax.grid(True, which='both', linestyle='--', linewidth=0.5, color='#AAAAAA')
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)
ax.tick_params(axis='both', which='major', labelsize=10)

# Highlight the max bandwidth point
max_index = np.argmax(bandwidths)
ax.scatter(payload_sizes[max_index], bandwidths[max_index], color='#E67E22', s=80, label='Max Bandwidth')

# Legend
ax.legend(frameon=False, fontsize=10)

# Tight layout for better spacing
plt.tight_layout()

# Save the figure (optional)
plt.savefig("bandwidth_vs_payload.pdf", dpi=300, bbox_inches='tight')

# Show the plot
plt.show()