from client import Client 
from order import Order, OrderManager

manager = OrderManager("orders.json")

client = Client("127.0.0.1", 9999, manager)
client.connect()

# Add a LIMIT order
order1 = Order(order_id = 1, symbol = "AAPL", side = "BUY", price = 110.0, quantity = 10, order_type = "LIMIT", tif = "GTC")
print(f"Placing order: {order1}")
client.add_order(order1)

# Request a snapshot for AAPL
print("Requesting snapshot for AAPL...")
client.request_snapshot(1, "AAPL")

# Replace the order
print(f"Replacing order {order1.order_id}")
client.cancel_replace_order(order1.order_id, 115.0, 15, 1)

# View current order state
print("\nOrder State:")
for order in manager.orders.values():
    print(order)

# Cancel the order
print(f"Canceling order {order1.order_id}")
client.cancel_order(order1.order_id, 1)
