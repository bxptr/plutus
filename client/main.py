import socket
import time

HOST = "127.0.0.1"
PORT = 9999

def send_and_receive(sock: socket.socket, message: str) -> str:
    print(f"Sending: {message.strip()}")
    sock.sendall(message.encode('utf-8'))
    response = sock.recv(4096).decode('utf-8')
    print(f"Response: {response.strip()}")
    return response

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))
print(f"Connected")

# Add a LIMIT order
message = f"ADD|1|{int(time.time())}|1|AAPL|110.0|10|BUY|GTC|LIMIT|1|0.0|10\n"
send_and_receive(sock, message)

# Add a MARKET order
message = f"ADD|2|{int(time.time())}|2|AAPL|0.0|5|SELL|IOC|MARKET|2|0.0|5\n"
send_and_receive(sock, message)

# Add a STOP-LOSS order
message = f"ADD|3|{int(time.time())}|3|AAPL|0.0|8|BUY|GTC|STOP_LOSS|3|105.0|8\n"
send_and_receive(sock, message)

# Add an ICEBERG order
message = f"ADD|4|{int(time.time())}|4|AAPL|115.0|20|SELL|GTC|ICEBERG|4|0.0|5\n"
send_and_receive(sock, message)

# Request a snapshot for AAPL
message = f"SNAPSHOT_REQUEST|5|{int(time.time())}|AAPL\n"
send_and_receive(sock, message)

# Add a LIMIT order for BTCUSD
message = f"ADD|6|{int(time.time())}|6|BTCUSD|20000.0|1|BUY|FOK|LIMIT|6|0.0|1\n"
send_and_receive(sock, message)

# Request a snapshot for BTCUSD
message = f"SNAPSHOT_REQUEST|7|{int(time.time())}|BTCUSD\n"
send_and_receive(sock, message)

# Cancel an order
message = f"CANCEL|1|{int(time.time())}|1|1\n"
send_and_receive(sock, message)

# Cancel and replace an order
message = f"CANCEL_REPLACE|4|{int(time.time())}|4|117.0|25|4\n"
send_and_receive(sock, message)

# Request VWAP for AAPL 
message = f"SNAPSHOT_REQUEST|8|{int(time.time())}|AAPL\n"
send_and_receive(sock, message)

sock.close()
print("Disconnected")

