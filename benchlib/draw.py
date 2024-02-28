import numpy as np
import matplotlib.pyplot as plt

from . import file


class Drawer:
    def __init__(self):
        self.time_str = file.generate_time_str()
        self.thread_list: list = []
        self.payload_list: list = []
        self.corotine_list: list = []
        self.one_testcase_dict: dict = {}
        self.all_testcases_dict_list: list[dict] = []


    def reset_one_testcase_dict(self, server, clients, thread_list, payload_list, corotine_list):
        self.thread_list = thread_list
        self.payload_list = payload_list
        self.corotine_list = corotine_list
        self.one_testcase_dict = {
            "throughput": np.ndarray(
                (len(thread_list), len(payload_list), len(corotine_list)), np.float64
            ),
            "latency": np.ndarray(
                (len(thread_list), len(payload_list), len(corotine_list)), np.float64
            ),
            "thread": np.asarray(thread_list),  # dim1
            "payload": np.asarray(payload_list),  # dim2
            "corotine": np.asarray(corotine_list),  # dim3
            "server": server,
            "clients": clients,
        }


    def add_one_to_all_list_and_clear_one(self):
        if "bandwidth" not in self.one_testcase_dict:
            self.count_bandwidth()
        self.all_testcases_dict_list.append(self.one_testcase_dict)
        self.thread_list = []
        self.payload_list = []
        self.corotine_list = []
        self.one_testcase_dict = {}


    def update_one_testcase(self, thread, payload, corotine, throughput, latency):

        self.one_testcase_dict["throughput"][
            self.thread_list.index(thread)
        ][
            self.payload_list.index(payload)
        ][
            self.corotine_list.index(corotine)
        ] = throughput

        self.one_testcase_dict["latency"][
            self.thread_list.index(thread)
        ][
            self.payload_list.index(payload)
        ][
            self.corotine_list.index(corotine)
        ] = latency


    def count_bandwidth(self):
        self.one_testcase_dict["bandwidth"] = self.one_testcase_dict['throughput'].copy()
        for p in range(len(self.payload_list)):
            self.one_testcase_dict["bandwidth"][:, p, :] *= self.payload_list[p]


    def draw_testcase_pictures(self):

        if "bandwidth" not in self.one_testcase_dict:
            self.count_bandwidth()

        server_clients_title = f"clients={', '.join(self.one_testcase_dict['clients'])}; server={self.one_testcase_dict['server']}"
        server_clients_name = f"clients_{'_'.join(self.one_testcase_dict['clients'])}_server_{self.one_testcase_dict['server']}"

        plt.figure(figsize=(12, 9), dpi=100)

        # same thread, same corotine, different payload
        for y_axis_data in [('throughput', '(reqs/s)'), ('latency', '(μs)'), ('bandwidth', '(B/s)')]:
            y_name = y_axis_data[0]
            y_label = y_axis_data[0] + ' ' + y_axis_data[1]
            plt.clf()  # refresh
            pic = plt.axes()
            for t in range(len(self.thread_list)):
                for c in range(len(self.corotine_list)):
                    y = self.one_testcase_dict[y_name][t, :, c]
                    pic.plot(
                        self.payload_list,
                        y,
                        marker="o",
                        label=f"thread={self.thread_list[t]}, corotine={self.corotine_list[c]}",
                    )
            pic.set_xlabel('payload (Byte)')
            pic.set_ylabel(y_label)
            pic.set_ylim(bottom=0)
            pic.set_xlim(left=0)
            plt.grid()
            plt.legend()
            plt.title(server_clients_title)
            pic.figure.savefig(
                file.generate_single_testcase_picture_filename(
                    dir=f'./img/{server_clients_name}/different_payload/', type=y_name, time_str=self.time_str
                )
            )

        # different thread, same corotine, same payload
        for y_axis_data in [('throughput', '(reqs/s)'), ('latency', '(μs)'), ('bandwidth', '(B/s)')]:
            y_name = y_axis_data[0]
            y_label = y_axis_data[0] + ' ' + y_axis_data[1]
            plt.clf()  # refresh
            pic = plt.axes()
            for p in range(len(self.payload_list)):
                for c in range(len(self.corotine_list)):
                    y = self.one_testcase_dict[y_name][:, p, c]
                    pic.plot(
                        self.thread_list,
                        y,
                        marker="o",
                        label=f"payload={self.payload_list[p]}B, corotine={self.corotine_list[c]}",
                    )
            pic.set_xlabel('thread')
            pic.set_ylabel(y_label)
            pic.set_ylim(bottom=0)
            pic.set_xlim(left=0)
            plt.grid()
            plt.legend()
            plt.title(server_clients_title)
            pic.figure.savefig(
                file.generate_single_testcase_picture_filename(
                    dir=f'./img/{server_clients_name}/different_thread/', type=y_name, time_str=self.time_str
                )
            )

        # same thread, different corotine, same payload
        for y_axis_data in [('throughput', '(reqs/s)'), ('latency', '(μs)'), ('bandwidth', '(B/s)')]:
            y_name = y_axis_data[0]
            y_label = y_axis_data[0] + ' ' + y_axis_data[1]
            plt.clf()  # refresh
            pic = plt.axes()
            for t in range(len(self.thread_list)):
                for p in range(len(self.payload_list)):
                    y = self.one_testcase_dict[y_name][t, p, :]
                    pic.plot(
                        self.corotine_list,
                        y,
                        marker="o",
                        label=f"thread={self.thread_list[t]}, payload={self.payload_list[p]}B",
                    )
            pic.set_xlabel('corotine')
            pic.set_ylabel(y_label)
            pic.set_ylim(bottom=0)
            pic.set_xlim(left=0)
            plt.grid()
            plt.legend()
            plt.title(server_clients_title)
            pic.figure.savefig(
                file.generate_single_testcase_picture_filename(
                    dir=f'./img/{server_clients_name}/different_corotine/', type=y_name, time_str=self.time_str
                )
            )


    def draw_compare_testcases_pictures(self):
        if len(self.all_testcases_dict_list) <= 1:
            return

        # compare_time = file.generate_time_str()

        # # find the same payload & thread for all testcases

        # common_payload_set = set(self.all_testcases_dict_list[0]["payload"])
        # common_thread_set = set(self.all_testcases_dict_list[0]["thread"])
        # for i in range(1, len(self.all_testcases_dict_list)):
        #     this_payload_set = set(self.all_testcases_dict_list[i]["payload"])
        #     this_thread_set = set(self.all_testcases_dict_list[i]["thread"])
        #     common_payload_set &= this_payload_set
        #     common_thread_set &= this_thread_set

        # common_payload_list = list(common_payload_set)
        # common_payload_list.sort()
        # common_thread_list = list(common_thread_set)
        # common_thread_list.sort()

        # print(
        #     f"--- common_payload_list = {common_payload_list}, common_thread_list = {common_thread_list}"
        # )

        # # get common data

        # # common_thread_list = [2, 4, 8]
        # # common_payload_list = [16, 256, 1024, 4096]
        # # throughput_list = [
        # #   [  # server-clients 1
        # #      [1054322.0, 990595.46666667, 894920.66666667, 702000.8],
        # #      [2076115.33333333, 1959232.66666667, 1742281.33333333, 1355087.33333333],
        # #      [4070519.33333333, 3847078.66666667, 3330096.66666667, 2330798.0],
        # #   ],
        # #   [  # server-clients 2
        # #      [1054322.0, 990595.46666667, 894920.66666667, 702000.8],
        # #      [2076115.33333333, 1959232.66666667, 1742281.33333333, 1355087.33333333],
        # #      [4070519.33333333, 3847078.66666667, 3330096.66666667, 2330798.0],
        # #   ],
        # # ]

        # throughput_list = np.ndarray(
        #     (
        #         len(self.all_testcases_dict_list),
        #         len(common_thread_list),
        #         len(common_payload_list),
        #     ),
        #     np.float64,
        # )
        # latency_list = np.ndarray(
        #     (
        #         len(self.all_testcases_dict_list),
        #         len(common_thread_list),
        #         len(common_payload_list),
        #     ),
        #     np.float64,
        # )
        # legend_name_list = [
        #     f"server={testcase['server']}, clients={testcase['clients']}"
        #     for testcase in self.all_testcases_dict_list
        # ]
        # for p in range(len(common_payload_list)):
        #     for t in range(len(common_thread_list)):
        #         i = -1
        #         for testcase in self.all_testcases_dict_list:
        #             i += 1
        #             this_t = list(testcase["thread"]).index(common_thread_list[t])
        #             this_p = list(testcase["payload"]).index(common_payload_list[p])
        #             throughput_list[i][t][p] = testcase["throughput"][this_t][this_p]
        #             latency_list[i][t][p] = testcase["latency"][this_t][this_p]
        # common_payload_list = np.asarray(common_payload_list)
        # common_thread_list = np.asarray(common_thread_list)

        # # draw pictures

        # # 4 pictures [
        # #   {for all fix payload, different ser-cli lines} thread - throughput,
        # #   {for all fix payload, different ser-cli lines} thread - latency,
        # #   {for all fix thread, different ser-cli lines} payload - throughput,
        # #   {for all fix thread, different ser-cli lines} payload - latency
        # # ]

        # # pictures 1, 2
        # p = -1
        # for payload in common_payload_list:
        #     p += 1

        #     plt.clf()
        #     pic = plt.axes()
        #     for i in range(len(legend_name_list)):
        #         y = throughput_list[i, :, p]
        #         pic.plot(common_thread_list, y, marker="o", label=legend_name_list[i])
        #     pic.set_xlabel("thread")
        #     pic.set_ylabel("throughput (reqs/s)")
        #     pic.set_ylim(bottom = 0)
        #     plt.legend()
        #     plt.title(f"payload={payload}")
        #     pic.figure.savefig(
        #         file.generate_compare_testcases_picture_filename(
        #             about="thread_throughput",
        #             fix_value=f"payload_{payload}",
        #             time_str=compare_time,
        #         )
        #     )

        #     plt.clf()
        #     pic = plt.axes()
        #     for i in range(len(legend_name_list)):
        #         y = latency_list[i, :, p]
        #         pic.plot(common_thread_list, y, marker="o", label=legend_name_list[i])
        #     pic.set_xlabel("thread")
        #     pic.set_ylabel("latency (μs)")
        #     pic.set_ylim(bottom = 0)
        #     plt.legend()
        #     plt.title(f"payload={payload}")
        #     pic.figure.savefig(
        #         file.generate_compare_testcases_picture_filename(
        #             about="thread_latency",
        #             fix_value=f"payload_{payload}",
        #             time_str=compare_time,
        #         )
        #     )

        # # pictures 3, 4
        # t = -1
        # for thread in common_thread_list:
        #     t += 1

        #     plt.clf()
        #     pic = plt.axes()
        #     for i in range(len(legend_name_list)):
        #         y = throughput_list[i, t, :]
        #         pic.plot(common_payload_list, y, marker="o", label=legend_name_list[i])
        #     pic.set_xlabel("payload (Byte)")
        #     pic.set_ylabel("throughput (reqs/s)")
        #     pic.set_ylim(bottom = 0)
        #     plt.legend()
        #     plt.title(f"thread={thread}")
        #     pic.figure.savefig(
        #         file.generate_compare_testcases_picture_filename(
        #             about="payload_throughput",
        #             fix_value=f"thread_{thread}",
        #             time_str=compare_time,
        #         )
        #     )

        #     plt.clf()
        #     pic = plt.axes()
        #     for i in range(len(legend_name_list)):
        #         y = latency_list[i, t, :]
        #         pic.plot(common_payload_list, y, marker="o", label=legend_name_list[i])
        #     pic.set_xlabel("payload (Byte)")
        #     pic.set_ylabel("latency (μs)")
        #     pic.set_ylim(bottom = 0)
        #     plt.legend()
        #     plt.title(f"thread={thread}")
        #     pic.figure.savefig(
        #         file.generate_compare_testcases_picture_filename(
        #             about="payload_latency",
        #             fix_value=f"thread_{thread}",
        #             time_str=compare_time,
        #         )
        #     )
