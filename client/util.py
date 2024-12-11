import time

def build_add_order_message(order_id: int, symbol: str, price: float, quantity: int, side: str, order_type: str, tif: str) -> str:
    timestamp = int(time.time())
    return (f"ADD|{order_id}|{timestamp}|{order_id}|{symbol}|{price}|{quantity}|{side}|{tif}|{order_type}|"
            f"{order_id}|0.0|{quantity}\n")

def build_snapshot_request_message(seq: int, symbol: str) -> str:
    timestamp = int(time.time())
    return f"SNAPSHOT_REQUEST|{seq}|{timestamp}|{symbol}\n"

def build_cancel_order_message(order_id: int, participant_id: int) -> str:
    timestamp = int(time.time())
    return f"CANCEL|{order_id}|{timestamp}|{order_id}|{participant_id}\n"

def build_cancel_replace_message(order_id: int, new_price: float, new_quantity: int, participant_id: int) -> str:
    timestamp = int(time.time())
    return f"CANCEL_REPLACE|{order_id}|{timestamp}|{order_id}|{new_price}|{new_quantity}|{participant_id}\n"
