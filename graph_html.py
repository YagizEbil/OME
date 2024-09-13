import re
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

# Function to read and parse the log file
def read_log_file(file_path):
    orders = []
    matches = []
    with open(file_path, 'r') as file:
        for line in file:
            if "Order added" in line:
                match = re.search(r"Order added: (\w+), Symbol: (\w+), Side: (\w+), Price: ([\d.]+), Quantity: (\d+)", line)
                if match:
                    order = {
                        'orderId': match.group(1),
                        'symbol': match.group(2),
                        'side': match.group(3),
                        'price': float(match.group(4)),
                        'quantity': int(match.group(5))
                    }
                    orders.append(order)
            elif "Matched Order" in line:
                match = re.search(r"Matched Order! Buy Order ID: (\w+) Sell Order ID: (\w+) Quantity: (\d+)", line)
                if match:
                    match_pair = {
                        'buyOrderId': match.group(1),
                        'sellOrderId': match.group(2),
                        'quantity': int(match.group(3))
                    }
                    matches.append(match_pair)
    return orders, matches

# Function to plot the data
def plot_data(orders, matches, use_candlestick=False):
    buy_prices = [order['price'] for order in orders if order['side'] == 'buy']
    sell_prices = [order['price'] for order in orders if order['side'] == 'sell']
    quantities = [order['quantity'] for order in orders]

    if use_candlestick:
        match_prices = [order['price'] for order in orders if order['orderId'] in [match['buyOrderId'] for match in matches] or order['orderId'] in [match['sellOrderId'] for match in matches]]
        match_quantities = [order['quantity'] for order in orders if order['orderId'] in [match['buyOrderId'] for match in matches] or order['orderId'] in [match['sellOrderId'] for match in matches]]
        dates = pd.date_range(start='1/1/2022', periods=len(match_prices), freq='min')  # Example dates

        data = pd.DataFrame({
            'Date': dates,
            'Open': match_prices,
            'High': [price + 1 for price in match_prices],  
            'Low': [price - 1 for price in match_prices],   
            'Close': match_prices,
            'Volume': match_quantities
        })
        data.set_index('Date', inplace=True)

        fig = make_subplots(rows=2, cols=1, shared_xaxes=True, vertical_spacing=0.02, row_heights=[0.7, 0.3])
        fig.add_trace(go.Candlestick(x=data.index, open=data['Open'], high=data['High'], low=data['Low'], close=data['Close'], name='Candlesticks'), row=1, col=1)
        fig.add_trace(go.Bar(x=data.index, y=data['Volume'], name='Volume'), row=2, col=1)
        
        fig.add_annotation(x=data.index[0], y=data['Close'][0], text="Start", showarrow=True, arrowhead=1)
        fig.add_annotation(x=data.index[-1], y=data['Close'][-1], text="End", showarrow=True, arrowhead=1)
        
        # Moving Avg
        data['MA20'] = data['Close'].rolling(window=20).mean()
        fig.add_trace(go.Scatter(x=data.index, y=data['MA20'], mode='lines', name='MA20', line=dict(color='orange')), row=1, col=1)
        
        fig.update_layout(title='Matched Orders Candlestick Chart', xaxis_title='Date', yaxis_title='Price', xaxis2_title='Date', yaxis2_title='Volume')
    else:
        fig = make_subplots(rows=1, cols=2, subplot_titles=('Buy and Sell Orders', 'Matched Order Prices'))

        fig.add_trace(go.Scatter(x=list(range(len(buy_prices))), y=buy_prices, mode='markers', name='Buy Orders', marker=dict(color='blue')), row=1, col=1)
        fig.add_trace(go.Scatter(x=list(range(len(sell_prices))), y=sell_prices, mode='markers', name='Sell Orders', marker=dict(color='red')), row=1, col=1)

        match_prices = [order['price'] for order in orders if order['orderId'] in [match['buyOrderId'] for match in matches] or order['orderId'] in [match['sellOrderId'] for match in matches]]
        fig.add_trace(go.Scatter(x=list(range(len(match_prices))), y=match_prices, mode='lines+markers', name='Matched Orders', line=dict(color='green')), row=1, col=2)

        fig.update_layout(title='Order Data', xaxis_title='Order Index', yaxis_title='Price')

    fig.write_html('order_plot.html')

def main():
    log_file_path = 'order_log.txt'
    orders, matches = read_log_file(log_file_path)
    plot_data(orders, matches, use_candlestick=True)

if __name__ == "__main__":
    main()