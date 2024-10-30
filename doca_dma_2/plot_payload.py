import json
import matplotlib.pyplot as plt
import os

files = os.listdir('.')
file_name = 'benchmark_results_payload'
save_dir = 'figures'

for file in files:
    if file.startswith(file_name + '_') and file.endswith('.json'):
        try:
            num = int(file[26:-5])
            file_path = file_name + '_' + str(num) + '.json'

            with open(file_path, 'r') as f:
                data = json.load(f)

            plt.figure(figsize=(10, 6))

            colors = ['b', 'g', 'r', 'm', 'y']

            for group_idx, group in enumerate(data):
                file_sizes = [entry['file_size'] for entry in group]
                bandwidths = [entry['bandwidth'] / entry['file_size'] for entry in group]
                ops = group[0]['ops'] 
                
                plt.plot(file_sizes, bandwidths, color=colors[group_idx % len(colors)], label=f'operations={ops}', marker='0', alpha=1, markersize=5, linewidth=2.5)

            # plt.xlim(left=0)
            plt.ylim(bottom=0)
            plt.xlabel('Payload (Bytes)')
            plt.ylabel('Bandwidth (Bytes/s)')
            plt.title('Bandwidth vs Payload Size for Different Number of Operations')
            plt.legend()
            plt.grid(True)

            save_path = save_dir + '/' + file_name + '_' + str(num) + '_plot.png'
            plt.savefig(save_path)
            print('Figure saved to' + save_path)

        except ValueError:
            print('num error')
            pass


