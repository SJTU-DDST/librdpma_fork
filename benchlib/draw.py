import numpy as np
import matplotlib.pyplot as plt

from . import file


class Drawer:
    def __init__(self):
        self.one_testcase_dict = {}
        self.all_testcases_dict_list = []

    def reset_one_testcase_dict(self, server, clients, thread_count_list, payload_list):
        self.one_testcase_dict = {
            "throughput": np.ndarray(
                (len(thread_count_list), len(payload_list)), np.float64
            ),
            "latency": np.ndarray(
                (len(thread_count_list), len(payload_list)), np.float64
            ),
            "threads": np.asarray(thread_count_list),  # dim1
            "payload": np.asarray(payload_list),  # dim2
            "coros": 1,  # dim3, now empty
            "server": server,
            "clients": clients,
        }

    def add_one_to_all_list_and_clear_one(self):
        self.all_testcases_dict_list.append(self.one_testcase_dict)
        self.one_testcase_dict = {}

    def update_one_testcase_throughput(self, threads_index, payload_index, throughput):
        self.one_testcase_dict["throughput"][threads_index][payload_index] = throughput

    def update_one_testcase_latency(self, threads_index, payload_index, latency):
        self.one_testcase_dict["latency"][threads_index][payload_index] = latency

    def draw_testcase_pictures(self, time_str: str):
        # dim 0: threads for different payload
        plt.clf()  # refresh
        pic = plt.axes()
        for p in range(len(self.one_testcase_dict["payload"])):
            y = self.one_testcase_dict["throughput"][:, p]
            pic.plot(
                self.one_testcase_dict["threads"],
                y,
                marker="o",
                label=f"payload={self.one_testcase_dict['payload'][p]}",
            )
        pic.set_xlabel("threads")
        pic.set_ylabel("throughput")
        plt.legend()
        plt.title(
            f"server={self.one_testcase_dict['server']}, clients={self.one_testcase_dict['clients']}"
        )
        pic.figure.savefig(
            file.generate_single_testcase_picture_filename(
                "throughput_threads", time_str
            )
        )

        plt.clf()
        pic = plt.axes()
        for p in range(len(self.one_testcase_dict["payload"])):
            y = self.one_testcase_dict["latency"][:, p]
            pic.plot(
                self.one_testcase_dict["threads"],
                y,
                marker="o",
                label=f"payload={self.one_testcase_dict['payload'][p]}",
            )
        pic.set_xlabel("threads")
        pic.set_ylabel("latency")
        plt.legend()
        plt.title(
            f"server={self.one_testcase_dict['server']}, clients={self.one_testcase_dict['clients']}"
        )
        pic.figure.savefig(
            file.generate_single_testcase_picture_filename("latency_threads", time_str)
        )

        # dim 1: payload for different threads
        plt.clf()
        pic = plt.axes()
        for t in range(len(self.one_testcase_dict["threads"])):
            y = self.one_testcase_dict["throughput"][t, :]
            pic.plot(
                self.one_testcase_dict["payload"],
                y,
                marker="o",
                label=f"threads={self.one_testcase_dict['threads'][t]}",
            )
        pic.set_xlabel("payload")
        pic.set_ylabel("throughput")
        plt.legend()
        plt.title(
            f"server={self.one_testcase_dict['server']}, clients={self.one_testcase_dict['clients']}"
        )
        pic.figure.savefig(
            file.generate_single_testcase_picture_filename(
                "throughput_payload", time_str
            )
        )
        pic.set_xscale("log")
        pic.figure.savefig(
            file.generate_single_testcase_picture_filename(
                "throughput_payload_log", time_str
            )
        )

        plt.clf()
        pic = plt.axes()
        for t in range(len(self.one_testcase_dict["threads"])):
            y = self.one_testcase_dict["latency"][t, :]
            pic.plot(
                self.one_testcase_dict["payload"],
                y,
                marker="o",
                label=f"threads={self.one_testcase_dict['threads'][t]}",
            )
        pic.set_xlabel("payload")
        pic.set_ylabel("latency")
        plt.legend()
        plt.title(
            f"server={self.one_testcase_dict['server']}, clients={self.one_testcase_dict['clients']}"
        )
        pic.figure.savefig(
            file.generate_single_testcase_picture_filename("latency_payload", time_str)
        )
        pic.set_xscale("log")
        pic.figure.savefig(
            file.generate_single_testcase_picture_filename(
                "latency_payload_log", time_str
            )
        )

    def draw_compare_testcases_pictures(self):
        if len(self.all_testcases_dict_list) <= 1:
            return

        compare_time = file.generate_time_str()

        # find the same payload & threads for all testcases

        common_payload_set = set(self.all_testcases_dict_list[0]["payload"])
        common_threads_set = set(self.all_testcases_dict_list[0]["threads"])
        for i in range(1, len(self.all_testcases_dict_list)):
            this_payload_set = set(self.all_testcases_dict_list[i]["payload"])
            this_threads_set = set(self.all_testcases_dict_list[i]["threads"])
            common_payload_set &= this_payload_set
            common_threads_set &= this_threads_set

        common_payload_list = list(common_payload_set)
        common_payload_list.sort()
        common_threads_list = list(common_threads_set)
        common_threads_list.sort()

        print(
            f"--- common_payload_list = {common_payload_list}, common_threads_list = {common_threads_list}"
        )

        # get common data

        # common_threads_list = [2, 4, 8]
        # common_payload_list = [16, 256, 1024, 4096]
        # throughput_list = [
        #   [  # server-clients 1
        #      [1054322.0, 990595.46666667, 894920.66666667, 702000.8],
        #      [2076115.33333333, 1959232.66666667, 1742281.33333333, 1355087.33333333],
        #      [4070519.33333333, 3847078.66666667, 3330096.66666667, 2330798.0],
        #   ],
        #   [  # server-clients 2
        #      [1054322.0, 990595.46666667, 894920.66666667, 702000.8],
        #      [2076115.33333333, 1959232.66666667, 1742281.33333333, 1355087.33333333],
        #      [4070519.33333333, 3847078.66666667, 3330096.66666667, 2330798.0],
        #   ],
        # ]

        throughput_list = np.ndarray(
            (
                len(self.all_testcases_dict_list),
                len(common_threads_list),
                len(common_payload_list),
            ),
            np.float64,
        )
        latency_list = np.ndarray(
            (
                len(self.all_testcases_dict_list),
                len(common_threads_list),
                len(common_payload_list),
            ),
            np.float64,
        )
        legend_name_list = [
            f"server={testcase['server']}, clients={testcase['clients']}"
            for testcase in self.all_testcases_dict_list
        ]
        for p in range(len(common_payload_list)):
            for t in range(len(common_threads_list)):
                i = -1
                for testcase in self.all_testcases_dict_list:
                    i += 1
                    this_t = list(testcase["threads"]).index(common_threads_list[t])
                    this_p = list(testcase["payload"]).index(common_payload_list[p])
                    throughput_list[i][t][p] = testcase["throughput"][this_t][this_p]
                    latency_list[i][t][p] = testcase["latency"][this_t][this_p]
        common_payload_list = np.asarray(common_payload_list)
        common_threads_list = np.asarray(common_threads_list)

        # draw pictures

        # 4 pictures [
        #   {for all fix payload, different ser-cli lines} threads - throughput,
        #   {for all fix payload, different ser-cli lines} threads - latency,
        #   {for all fix threads, different ser-cli lines} payload - throughput,
        #   {for all fix threads, different ser-cli lines} payload - latency
        # ]

        # pictures 1, 2
        p = -1
        for payload in common_payload_list:
            p += 1

            plt.clf()
            pic = plt.axes()
            for i in range(len(legend_name_list)):
                y = throughput_list[i, :, p]
                pic.plot(common_threads_list, y, marker="o", label=legend_name_list[i])
            pic.set_xlabel("threads")
            pic.set_ylabel("throughput")
            plt.legend()
            plt.title(f"payload={payload}")
            pic.figure.savefig(
                file.generate_compare_testcases_picture_filename(
                    about="threads_throughput",
                    fix_value=f"payload_{payload}",
                    time_str=compare_time,
                )
            )

            plt.clf()
            pic = plt.axes()
            for i in range(len(legend_name_list)):
                y = latency_list[i, :, p]
                pic.plot(common_threads_list, y, marker="o", label=legend_name_list[i])
            pic.set_xlabel("threads")
            pic.set_ylabel("latency")
            plt.legend()
            plt.title(f"payload={payload}")
            pic.figure.savefig(
                file.generate_compare_testcases_picture_filename(
                    about="threads_latency",
                    fix_value=f"payload_{payload}",
                    time_str=compare_time,
                )
            )

        # pictures 3, 4
        t = -1
        for threads in common_threads_list:
            t += 1

            plt.clf()
            pic = plt.axes()
            for i in range(len(legend_name_list)):
                y = throughput_list[i, t, :]
                pic.plot(common_payload_list, y, marker="o", label=legend_name_list[i])
            pic.set_xlabel("payload")
            pic.set_ylabel("throughput")
            plt.legend()
            plt.title(f"threads={threads}")
            pic.figure.savefig(
                file.generate_compare_testcases_picture_filename(
                    about="payload_throughput",
                    fix_value=f"threads_{threads}",
                    time_str=compare_time,
                )
            )

            plt.clf()
            pic = plt.axes()
            for i in range(len(legend_name_list)):
                y = latency_list[i, t, :]
                pic.plot(common_payload_list, y, marker="o", label=legend_name_list[i])
            pic.set_xlabel("payload")
            pic.set_ylabel("latency")
            plt.legend()
            plt.title(f"threads={threads}")
            pic.figure.savefig(
                file.generate_compare_testcases_picture_filename(
                    about="payload_latency",
                    fix_value=f"threads_{threads}",
                    time_str=compare_time,
                )
            )
