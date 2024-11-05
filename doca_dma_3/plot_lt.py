import json
import matplotlib.pyplot as plt
import os

files = os.listdir('.')
file_name = 'bench_12.json'
save_dir = 'figures'

with open(file_name, 'r') as f:
    data = json.load(f)

plt.figure(figsize=(10, 6))

work_tasks = 1
for group_idx, group in enumerate(data):
    payload = [entry['payload'] / 1024.0 for entry in group]
    latency = [entry['latency'] for entry in group]
    
    plt.plot(payload, latency, label=f'Working Tasks={work_tasks}', marker='o', alpha=1, markersize=3.5, linewidth=1.5)
    work_tasks *= 2

plt.xlim(left=0)
plt.ylim(bottom=0)
plt.xlabel('Payload (KB)')
plt.ylabel('Latency (μs)')
plt.title('Latency vs Payload (Threads = 16, ctx = 1, pe = 1)')
plt.legend()
plt.grid(True)

save_path = save_dir + '/' + 'bench_12' + '.png'
plt.savefig(save_path)
print('Figure saved to' + save_path)

