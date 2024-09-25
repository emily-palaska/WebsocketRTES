import websocket
import json

# Open a file in write mode to store the messages
message_file = open('messages.json', 'w')

def on_message(ws, message):
    # Append the message to the JSON file
    try:
        # Assuming message is a JSON string, you can append it directly
        message_data = json.loads(message)
        message_file.write(json.dumps(message_data) + "\n")
    except json.JSONDecodeError:
        # If the message isn't a valid JSON, log it as a raw string
        message_file.write(json.dumps({"error": "Invalid JSON", "message": message}) + "\n")

def on_error(ws, error):
    print(error)

def on_close(ws):
    print("### closed ###")
    # Close the file when the WebSocket connection is closed
    message_file.close()

def on_open(ws):
    ws.send('{"type":"subscribe","symbol":"AAPL"}')
    ws.send('{"type":"subscribe","symbol":"AMZN"}')
    ws.send('{"type":"subscribe","symbol":"BINANCE:BTCUSDT"}')
    ws.send('{"type":"subscribe","symbol":"IC MARKETS:1"}')

if __name__ == "__main__":
    websocket.enableTrace(True)
    ws = websocket.WebSocketApp("wss://ws.finnhub.io?token=couu7o1r01qhf5ns046gcouu7o1r01qhf5ns0470",
                              on_message = on_message,
                              on_error = on_error,
                              on_close = on_close)
    ws.on_open = on_open
    ws.run_forever()
