import json
import matplotlib.pyplot as plt
import os

files = os.listdir('.')
file_name = 'bench_bw_pl_threads'
save_dir = 'figures'

for file in files:
    if file.startswith(file_name + '_') and file.endswith('.json'):
        try:
            num = int(file[(len(file_name) + 1):-5])
            file_path = file_name + '_' + str(num) + '.json'

            with open(file_path, 'r') as f:
                data = json.load(f)

            plt.figure(figsize=(10, 6))

            threads = 1
            for group_idx, group in enumerate(data):
                payload = [entry['file_size'] for entry in group]
                bandwidths = [entry['latency'] / entry['ops'] * 1000 for entry in group]
                
                plt.plot(payload, bandwidths, label=f'threads={threads}', marker='o', alpha=1, markersize=3.5, linewidth=1.5)
                threads *= 2

            plt.xlim(left=0)
            plt.ylim(bottom=0)
            plt.xlabel('Payload (Bytes)')
            plt.ylabel('Latency (µs)')
            plt.title('Latency vs Payload for Different Number of Threads')
            plt.legend()
            plt.grid(True)

            save_path = save_dir + '/' + 'bench_lt_pl_threads' + '_' + str(num) + '.png'
            plt.savefig(save_path)
            print('Figure saved to' + save_path)

        except ValueError:
            print('num error')
            pass


