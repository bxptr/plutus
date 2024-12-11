import json
import pprint

class Order:
    def __init__(
        self,
        order_id: int,
        symbol: str,
        side: str,
        price: float,
        quantity: int,
        order_type: str,
        tif: str
    ) -> None:
        self.order_id = order_id
        self.symbol = symbol
        self.side = side
        self.price = price
        self.quantity = quantity
        self.order_type = order_type
        self.tif = tif
        self.status = "PENDING" # PENDING, ACKED, REPLACED, CANCELED, FILLED

    def to_dict(self) -> dict:
        return {
            "order_id": self.order_id,
            "symbol": self.symbol,
            "side": self.side,
            "price": self.price,
            "quantity": self.quantity,
            "order_type": self.order_type,
            "tif": self.tif,
            "status": self.status,
        }

    @staticmethod
    def from_dict(data: dict) -> "Order":
        order = Order(
            data["order_id"],
            data["symbol"],
            data["side"],
            data["price"],
            data["quantity"],
            data["order_type"],
            data["tif"],
        )
        order.status = data["status"]
        return order

    def __str__(self) -> str:
        return f"Order({pprint.pformat(self.to_dict())})\n"

class OrderManager:
    def __init__(self, persistence_file: str) -> None:
        self.orders: Dict[int, Order] = {}
        self.persistence_file = persistence_file
        self.load_orders()

    def add_order(self, order: Order) -> None:
        self.orders[order.order_id] = order

    def update_order_status(self, order_id: int, status: str) -> None:
        if order_id in self.orders:
            self.orders[order_id].status = status

    def get_order(self, order_id: int) -> Order:
        return self.orders.get(order_id)

    def save_orders(self) -> None:
        with open(self.persistence_file, "w") as file:
            json.dump([order.to_dict() for order in self.orders.values()], file)

    def load_orders(self) -> None:
        try:
            with open(self.persistence_file, "r") as file:
                data = json.load(file)
                self.orders = {item["order_id"]: Order.from_dict(item) for item in data}
        except (FileNotFoundError, json.JSONDecodeError):
            self.orders = {}

