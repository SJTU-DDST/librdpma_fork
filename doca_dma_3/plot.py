import json
import matplotlib.pyplot as plt
import os

files = os.listdir('.')
file_name = 'bench_2.json'
save_dir = 'figures'

with open(file_name, 'r') as f:
    data = json.load(f)

plt.figure(figsize=(10, 6))

work_tasks = 1
for group_idx, group in enumerate(data):
    payload = [entry['payload'] / 1024.0 for entry in group]
    bandwidths = [entry['bandwidth'] for entry in group]
    
    plt.plot(payload, bandwidths, label=f'Working Tasks={work_tasks}', marker='o', alpha=1, markersize=3.5, linewidth=1.5)
    work_tasks *= 2

plt.xlim(left=0)
plt.ylim(bottom=0)
plt.xlabel('Payload (KB)')
plt.ylabel('Bandwidth (Gbps)')
plt.title('Bandwidth vs Payload for Different Number of Working tasks (Threads = 16)')
plt.legend()
plt.grid(True)

save_path = save_dir + '/' + 'bench_2' + '.png'
plt.savefig(save_path)
print('Figure saved to' + save_path)

