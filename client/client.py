import socket

import order
from util import * 

class Client:
    def __init__(self, host: str, port: int, order_manager: order.OrderManager) -> None:
        self.host = host
        self.port = port
        self.sock = None
        self.order_manager = order_manager

    def connect(self) -> None:
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        print(f"Connected to {self.host}:{self.port}")

    def disconnect(self) -> None:
        if self.sock:
            self.sock.close()
            print("Disconnected from server")
            self.order_manager.save_orders()

    def send_and_receive(self, message: str) -> str:
        print(f"Sending: {message.strip()}")
        self.sock.sendall(message.encode('utf-8'))
        response = self.sock.recv(4096).decode('utf-8')
        print(f"Response: {response.strip()}")
        return response

    def add_order(self, order_: order.Order) -> None:
        message = build_add_order_message(
            order_.order_id, order_.symbol, order_.price, order_.quantity,
            order_.side, order_.order_type, order_.tif
        )
        response = self.send_and_receive(message)
        if "ADD_ACK" in response:
            self.order_manager.update_order_status(order_.order_id, "ACKED")
        self.order_manager.add_order(order_)

    def cancel_order(self, order_id: int, participant_id: int) -> None:
        message = build_cancel_order_message(order_id, participant_id)
        response = self.send_and_receive(message)
        if "CANCEL_ACK" in response:
            self.order_manager.update_order_status(order_id, "CANCELED")

    def cancel_replace_order(self, order_id: int, new_price: float, new_quantity: int, participant_id: int) -> None:
        message = build_cancel_replace_message(order_id, new_price, new_quantity, participant_id)
        response = self.send_and_receive(message)
        if "CANCEL_REPLACE_ACK" in response:
            self.order_manager.update_order_status(order_id, "REPLACED")

    def request_snapshot(self, seq: int, symbol: str) -> str:
        message = build_snapshot_request_message(seq, symbol)
        return self.send_and_receive(message)
