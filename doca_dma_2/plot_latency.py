import json
import matplotlib.pyplot as plt



file_path = 'benchmark_results_latency.json'

with open(file_path, 'r') as f:
    data = json.load(f)

plt.figure(figsize=(10, 6))


file_sizes = [entry['file_size'] for entry in data]
latency = [entry['latency'] / entry['ops'] for entry in data]

plt.plot(file_sizes, latency, color='b', marker='o', alpha=1, markersize=5, linewidth=2.5)

plt.xlabel('Payload Size (Bytes)')
plt.ylabel('Latency (ms)')
plt.title('Latency vs Payload Size')
plt.grid(True)

plt.show()

plt.savefig('latency.png')