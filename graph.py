import json
import seaborn as sns
import matplotlib.pyplot as plt
from datetime import datetime
from collections import Counter
import matplotlib.dates as mdates
import numpy as np

# Helper function to convert timestamps to datetime
def to_datetime(timestamp):
    return datetime.fromtimestamp(timestamp / 1000.0)

# Load data from JSON files
def load_json(filename):
    with open(filename, 'r') as file:
        return json.load(file)['data']

symbol = '../AAPL'

# Load the JSON files
trade_data = load_json(f'{symbol}.json')
candlestick_data = load_json(f'{symbol}_cand.json')
mov_avg_data = load_json(f'{symbol}_mov.json')

# Convert timestamps to datetime
trade_times = [to_datetime(trade['t']) for trade in trade_data]
candlestick_times = [to_datetime(candle['t']) for candle in candlestick_data]
mov_avg_times = [to_datetime(mov['t']) for mov in mov_avg_data]

# Plot 1: Number of trades per second
def plot_trades_per_second():
    times = [t.timestamp() for t in trade_times]
    plt.figure()
    plt.hist(times, bins=np.arange(min(times), max(times), 1), edgecolor='blue')
    plt.title('Number of Trades per Second')
    plt.xlabel('Time (Unix Timestamp)')
    plt.ylabel('Number of Trades')
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig('trades_per_second.png')

# Plot 2: Candlestick Chart
def plot_candlestick():
    fig, ax = plt.subplots()
    for candle in candlestick_data[120:160]:
        color = 'green' if candle['close'] >= candle['open'] else 'red'
        ax.plot([candle['t'], candle['t']], [candle['low'], candle['high']], color=color)
        ax.plot([candle['t'], candle['t']], [candle['open'], candle['close']], color=color, linewidth=6)

    ax.set_title('Candlestick Chart')
    ax.set_xlabel('Time (Unix Timestamp)')
    ax.set_ylabel('Price')
    plt.xticks([candle['t'] for candle in candlestick_data[120:160]], [to_datetime(candle['t']).strftime('%H:%M:%S') for candle in candlestick_data[120:160]], rotation=45, fontsize=7)
    plt.tight_layout()
    plt.savefig('candlestick_chart.png')

# Plot 3: Moving Average
def plot_moving_average():
    plt.figure()
    plt.plot(mov_avg_times, [mov['p'] for mov in mov_avg_data], label='Moving Average')
    plt.title('Moving Average Over Time')
    plt.xlabel('Time')
    plt.ylabel('Moving Average Price')
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig('moving_average.png')

# Plot 4: Delay Distribution (Smoothed with KDE)
def plot_delay_distribution():
    delays = [mov['d'] for mov in mov_avg_data]
    
    plt.figure()
    sns.histplot(delays, kde=True, bins=20, color='blue', edgecolor='black')
    plt.title('Delay Distribution with KDE')
    plt.xlabel('Delay (ms)')
    plt.ylabel('Frequency')
    
    plt.tight_layout()
    plt.savefig('delay_distribution.png')


# Plot 5: Lost Connection Periods with Line for Trades
def plot_lost_connection_periods():
    times = [t.timestamp() for t in trade_times]
    diffs = np.diff(times)
    lost_conn_indices = np.where(diffs > 60)[0]  # Assuming connection loss if gap > 60 seconds

    plt.figure()
    plt.plot(trade_times, np.ones(len(trade_times)), 'b-', label='Trade Data')  # Use a line plot instead of points
    
    # Plot lost connection periods
    if len(lost_conn_indices) > 0:
        plt.vlines([trade_times[i+1] for i in lost_conn_indices], ymin=0.5, ymax=1.5, colors='r', label='Lost Connection')

    plt.title('Lost Connection Periods')
    plt.xlabel('Time')
    plt.ylabel('Connection Status')
    plt.legend()
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig('lost_connection_periods.png')

# Generate all plots
plot_trades_per_second()
plot_candlestick()
plot_moving_average()
plot_delay_distribution()
plot_lost_connection_periods()

print("Plots saved in the current directory.")
