import json
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import os

files = os.listdir('.')
file_name = 'bench_2.json'
save_dir = 'figures'

with open(file_name, 'r') as f:
    data = json.load(f)

plt.figure(figsize=(10, 6))

for group_idx, group in enumerate(data):
    payload = [entry['payload'] / 1024.0 for entry in group]
    bandwidths = [entry['bandwidth'] for entry in group]
    tasks = group[0]['working_tasks']
    
    plt.plot(payload, bandwidths, label=f'working tasks={tasks}', marker='o', alpha=1, markersize=3.5, linewidth=1.5)

# plt.xlim(left=0)
plt.ylim(bottom=0)
plt.xscale('log')
plt.xticks([entry['payload'] / 1024.0 for entry in group], labels=[f"{int(size)}" if size == int(size) else f"{size}" for size in [entry['payload'] / 1024.0 for entry in group]])
plt.xlabel('Payload (KB)')
plt.ylabel('Bandwidth (Gbps)')
plt.title('Bandwidth vs Payload (Threads = 16)')
plt.legend()
plt.gca().xaxis.set_major_locator(ticker.FixedLocator([entry['payload'] / 1024.0 for entry in group]))
plt.gca().xaxis.set_minor_locator(ticker.NullLocator())
plt.grid(visible=True, which='both', linestyle='--', linewidth=0.5)
plt.tight_layout()

save_path = save_dir + '/' + 'dpu_read_bandwidth_2' + '.png'
plt.savefig(save_path)
print('Figure saved to' + save_path)

