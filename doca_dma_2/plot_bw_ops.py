import json
import matplotlib.pyplot as plt
import os

files = os.listdir('.')
file_name = 'bench_bw_ops'
save_dir = 'figures'

for file in files:
    if file.startswith(file_name + '_') and file.endswith('.json'):
        try:
            num = int(file[(len(file_name) + 1):-5])
            file_path = file_name + '_' + str(num) + '.json'

            with open(file_path, 'r') as f:
                data = json.load(f)

            plt.figure(figsize=(10, 6))

            colors = plt.get_cmap("tab20").colors

            for group_idx, group in enumerate(data):
                ops = [entry['ops'] for entry in group]
                bandwidths = [entry['bandwidth'] / entry['file_size'] for entry in group]
                payload = group[0]['file_size'] / 1024
                
                plt.plot(ops, bandwidths, color=colors[group_idx % len(colors)], label=f'payload={payload}KB', marker='o', alpha=1, markersize=5, linewidth=2)

            plt.xlim(left=0)
            plt.ylim(bottom=0)
            plt.xlabel('Ops')
            plt.ylabel('Bandwidth (Gbps)')
            plt.title('Bandwidth vs Ops for Different Payload')
            plt.legend()
            plt.grid(True)

            save_path = save_dir + '/' + file_name + '_' + '1' + '_' + str(num) + '.png'
            plt.savefig(save_path)
            print('Figure saved to' + save_path)

        except ValueError:
            print('num error')
            pass


