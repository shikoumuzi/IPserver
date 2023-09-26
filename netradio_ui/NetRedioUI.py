import os
import sys
import time

from PyQt5.QtWidgets import QWidget, QPushButton, QTextBrowser, QHBoxLayout, QVBoxLayout, QApplication, QLabel
from PyQt5.QtCore import QThread, QProcess, Qt, QObject, pyqtSignal, QByteArray, QWaitCondition, QMutex
from PyQt5.QtGui import QMouseEvent, QCloseEvent
from typing import List
import ctypes
import datetime


class ProjectConfig:
    show_button_width = 300
    show_button_hight = 50


class ChnnelShower(QWidget):
    class Button(QPushButton):
        clicked_with_arg = pyqtSignal(int)

        def __init__(self, message: str, i: int):
            super().__init__(message)
            self.pos = i
            self.setMinimumSize(ProjectConfig.show_button_width, ProjectConfig.show_button_hight)
            self.setMaximumSize(ProjectConfig.show_button_width, ProjectConfig.show_button_hight)

        def mouseReleaseEvent(self, e: QMouseEvent) -> None:
            self.clicked_with_arg.emit(self.pos)
            return super().mouseReleaseEvent(e)

    def __init__(self, chnnel_messages: List[str] = [], click_callback=None):
        super().__init__()

        self.shower_buttons = [ChnnelShower.Button(chnnel_messages[i], i + 1) for i in range(chnnel_messages.__len__())
                               if chnnel_messages[i] != ""]
        self.setStyleSheet("background-color: White; border: 1px")
        self.setMinimumSize(200, 200)

        self.updateShower(chnnel_messages, click_callback)
        self.chooced = False

    def updateShower(self, chnnel_messages: List[str] = [], click_callback=None):

        # print("updateShower", chnnel_messages)
        if chnnel_messages:
            self.shower_buttons = [ChnnelShower.Button(chnnel_messages[i], i + 1) for i in
                                   range(chnnel_messages.__len__())]

            for i in range(self.shower_buttons.__len__()):
                if click_callback is not None:
                    self.shower_buttons[i].clicked_with_arg.connect(click_callback)
                self.shower_buttons[i].setStyleSheet("background-color: rgb(135, 147, 154)")

            hlayout = QHBoxLayout()
            vlayout = QVBoxLayout()

            vlayout.addWidget(self.shower_buttons[0])
            vlayout.addWidget(self.shower_buttons[1])
            vlayout.addWidget(self.shower_buttons[2])
            vlayout.addWidget(self.shower_buttons[3])
            vlayout.addStretch()

            hlayout.addStretch()
            hlayout.addLayout(vlayout)
            hlayout.addStretch()

            self.chooced = True
            self.setLayout(hlayout)
            return


class ClientThread(QThread):
    CMD = {
        "pause": "pause\n",
        "stop": "stop\n",
        "play": "pause\n",
    }

    def __init__(self):
        super().__init__()
        self.mplayer_cmd_pipe_path = "/tmp/mplayer_cmd_pipe"
        # self.client_cmd_pipe_path = "/tmp/mclient_cmd_pipe"
        # self.client_ret_pipe_path = "/tmp/mclient_ret_pipe"

        self.wait_cmd_condition = QWaitCondition()
        self.wait_cmd_mutex = QMutex()

        self.work_state = True
        self.now_cmd = ""

    def stop(self):
        self.work_state = False
        self.wait_cmd_condition.wakeAll()
        try:
            self.mplayer_cmd_pipe_fd.close()
        except:
            pass
        try:
            self.client_ret_pipe_fd.close()
        except:
            pass
        try:
            self.client_cmd_pipe_fd.close()
        except:
            pass
        self.wait_cmd_mutex.unlock()

    def success_handler(self, msg: str):
        pass

    def error_handler(self, msg: str):
        pass

    def run(self) -> None:
        if os.path.exists(self.mplayer_cmd_pipe_path):
            self.mplayer_cmd_pipe_fd = open(self.mplayer_cmd_pipe_path, "w")
        else:
            print("管道文件不存在")
            sys.exit(0)
        # self.client_cmd_pipe_fd = open(self.client_cmd_pipe_path, "w")
        # self.client_ret_pipe_fd = open(self.client_ret_pipe_path, "r")

        print("pipe ready")
        self.wait_cmd_mutex.lock()

        while self.work_state:
            self.wait_cmd_condition.wait(self.wait_cmd_mutex)

            if not self.work_state:
                break
            # print("python-cmd:", self.now_cmd)

            ret = self.mplayer_cmd_pipe_fd.write(self.now_cmd.encode('utf-8').decode('utf-8'))
            self.mplayer_cmd_pipe_fd.flush()
            # if ret > 0:
            #     print("cmd writed")

            # ret = self.client_ret_pipe_fd.readline()
            # if len(ret) > 0:
            #     print(f"ret is {ret}")
            # if ret.find("error"):
            #     self.error_handler(ret[0])
        self.mplayer_cmd_pipe_fd.close()
        pass

    def cmd(self, cmd: str):
        self.now_cmd = cmd
        self.wait_cmd_condition.wakeAll()

        pass


class WorkStateCtrl(QThread):
    work_state_update = pyqtSignal(dict)

    STATE_FORMAT = {
        "packlen": -1,
        "buff_size": -1,
    }

    def __init__(self):
        super().__init__()
        self.work_state_input_pipe_path = "/tmp/mclient_work_state_output_pipe"
        self.work_state_input_pipe = None
        self.work_state = True

    def run(self) -> None:
        self.work_state_input_pipe = open(self.work_state_input_pipe_path, 'r')

        while self.work_state:
            message = self.work_state_input_pipe.readline()

            # print(message)
            if message.find("work:") == 0:
                attribute_pair = message[6: -2].split('=')
                # print(attribute_pair)
                try:
                    if attribute_pair[0].find("packlen") != -1:
                        self.work_state_update.emit({
                            "packlen": int(attribute_pair[-1]),
                            "buff_size": 0,
                        })
                    elif attribute_pair[0].find("now_buff_size") != -1:
                        self.work_state_update.emit({
                            "packlen": -1,
                            "buff_size": int(attribute_pair[-1]),
                        })
                except:
                    pass
            elif message.find("msg:") == 0:
                # print("".join(['[', datetime.datetime.now().__str__(), ']\n', message[5: -2]]))
                pass

    def stop(self):
        self.work_state = False
        try:
            self.work_state_input_pipe.close()
        except:
            pass


class NetRedioUI(QWidget):
    def __init__(self):
        super().__init__()
        self.setMinimumSize(300, 400)

        self.times = 1

        self.client_process = QProcess()
        self.client_thread = ClientThread()
        self.client_work_state_thread = WorkStateCtrl()
        self.client_work_state_thread.work_state_update.connect(self.updateWork)

        self.ChnnelShower = ChnnelShower()
        self.start_app_button = QPushButton("开始接收信号")
        self.start_listen_button = QPushButton("开始播放")
        self.stop_listen_button = QPushButton("停止播放")
        self.pause_listen_button = QPushButton("暂停播放")
        self.start_listen_button.clicked.connect(self.startListen)
        self.stop_listen_button.clicked.connect(self.stopListen)
        self.pause_listen_button.clicked.connect(self.pauseListen)

        self.start_app_button.clicked.connect(self.startApp)
        self.pack_id_label = QLabel("id=0")
        self.packlen_state_label = QLabel("packlen=0")
        self.buff_state_label = QLabel("buff size=0")
        self.receviced_pack_size_label = QLabel("receviced pack size=0")
        self.receviced_pack_size = 0

        self.pack_id_label.setMaximumSize(150, 20)
        self.packlen_state_label.setMaximumSize(150, 20)
        self.buff_state_label.setMaximumSize(150, 20)
        self.receviced_pack_size_label.setMaximumSize(250, 20)
        self.packlen_state_label.setMinimumSize(30, 20)
        self.pack_id_label.setMinimumSize(30, 20)
        self.buff_state_label.setMinimumSize(30, 20)
        self.receviced_pack_size_label.setMinimumSize(30, 20)
        self.packlen_state_label.setStyleSheet("border-width: 1px;border-style: solid;")
        self.buff_state_label.setStyleSheet("border-width: 1px;border-style: solid;")
        self.receviced_pack_size_label.setStyleSheet("border-width: 1px;border-style: solid;")
        self.pack_id_label.setStyleSheet("border-width: 1px;border-style: solid;")

        self.log_text_browser = QTextBrowser()
        self.log_text_browser.setMinimumSize(100, 400)

        self.pause = False
        self.app = False

        hlayout = QHBoxLayout()
        vlayout = QVBoxLayout()

        hlayout1 = QHBoxLayout()
        hlayout1.addStretch()
        hlayout1.addWidget(self.start_app_button)
        hlayout1.addWidget(self.start_listen_button)
        hlayout1.addWidget(self.stop_listen_button)
        hlayout1.addWidget(self.pause_listen_button)
        hlayout1.addStretch()

        vlayout.addWidget(self.ChnnelShower)

        hlayout2 = QHBoxLayout()
        hlayout2.addStretch()
        hlayout2.addWidget(self.pack_id_label)
        hlayout2.addWidget(self.packlen_state_label)
        hlayout2.addWidget(self.buff_state_label)
        hlayout2.addWidget(self.receviced_pack_size_label)
        hlayout2.addStretch()

        vlayout.addLayout(hlayout2)
        vlayout.addLayout(hlayout1)

        hlayout.addLayout(vlayout)
        hlayout.addStretch()
        hlayout.addWidget(self.log_text_browser)
        hlayout.addStretch()

        self.setLayout(hlayout)

    def updateWork(self, data):
        self.times += 1
        self.pack_id_label.setText(f"id={self.times}")

        if data["packlen"] != -1:
            self.receviced_pack_size += int(data['packlen'])
            if self.times % 25 == 0:
                self.packlen_state_label.setText(f"packlen={data['packlen']}")
                self.receviced_pack_size_label.setText(f"receviced pack size={self.receviced_pack_size}")

        elif data["buff_size"] != -1:
            if self.times % 25 == 0:
                self.buff_state_label.setText(f"buff size={data['buff_size']}")

    def closeEvent(self, a0: QCloseEvent) -> None:
        self.client_thread.cmd(ClientThread.CMD["stop"])
        while self.client_process.pid() > 0:
            self.client_process.kill()
            self.client_process.terminate()
            self.client_process.close()
        self.client_thread.stop()
        self.client_work_state_thread.stop()
        os.system("pulseaudio --kill")

    def OutputCtrl(self):
        def input_pos(pos):
            print(pos)
            self.client_process.write((str(pos) + '\n').encode('utf-8'))

        messages = self.client_process.readAllStandardOutput()

        if type(messages) != str:
            messages = str(messages, encoding="utf-8")
        messages = str(messages)
        # print("message", messages)

        messages_list = messages.split('\n')

        for message in messages_list:
            if message.find("c-cmd") != -1:
                print(message)
            if message.find("msg:") == 0:
                self.log_text_browser.append("".join(['[', datetime.datetime.now().__str__(), ']\n', message[5: -1]]))
            elif message.find("error:") == 0:
                self.log_text_browser.append("".join(['[', datetime.datetime.now().__str__(), ']\n', message[7: -1]]))

        if self.ChnnelShower.chooced:
            return
        messages = [message for message in messages.split("\n") if
                    message.find('=') == -1 or message.find('fail') == -1]
        self.ChnnelShower.updateShower(
            [message.split(':')[-1] for message in messages if message.find('channel') != -1], input_pos)

    def updateError(self):
        messages = self.client_process.readAllStandardError()
        # print(messages)
        if type(messages) != str:
            messages = str(messages, encoding="utf-8")
        messages = str(messages)
        if messages.find("Command buffer of file descriptor 0 is full: dropping content") != -1:
            print("0 fd is full")
            return
        print("updateError", messages)

    def startApp(self):
        if self.app is False:
            os.system("pulseaudio --kill")
            time.sleep(1)
            os.system("pulseaudio --start")
            self.client_process.setReadChannel(self.client_process.StandardOutput)
            self.client_process.readyReadStandardOutput.connect(self.OutputCtrl)
            self.client_process.readyReadStandardError.connect(self.updateError)
            if self.client_process.pid() <= 0:
                self.client_process.start("/home/experiment/IPserver/netradio_c/src/client/client", [])
            time.sleep(2)

            while not (os.path.exists(self.client_thread.mplayer_cmd_pipe_path)
                       and os.path.exists(self.client_work_state_thread.work_state_input_pipe_path)):
                time.sleep(1)

            self.client_thread.start()
            self.client_work_state_thread.start()
            self.app = True

    def startListen(self):
        if self.pause is True:
            self.client_thread.cmd(ClientThread.CMD["play"])
            self.pause = False
        pass

    def stopListen(self):

        self.client_thread.cmd(ClientThread.CMD["stop"])
        pass

    def pauseListen(self):
        if self.pause is False:
            self.client_thread.cmd(ClientThread.CMD["pause"])
            self.pause = True
        pass


if __name__ == '__main__':
    app = QApplication(sys.argv)

    netradioui = NetRedioUI()
    netradioui.show()

    sys.exit(app.exec_())
