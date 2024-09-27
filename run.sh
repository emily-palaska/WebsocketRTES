#!/bin/bash

# Remove all existing JSON files in the directory
rm -f *.json

# Create the required JSON files with the desired content

# Trade files
echo '{
    "type": "trade",
    "data": []
}' > AAPL.json

echo '{
    "type": "trade",
    "data": []
}' > GOOG.json

echo '{
    "type": "trade",
    "data": []
}' > MSFT.json

# Candlestick files
echo '{
    "type": "candlestick",
    "data": []
}' > AAPL_cand.json

echo '{
    "type": "candlestick",
    "data": []
}' > GOOG_cand.json

echo '{
    "type": "candlestick",
    "data": []
}' > MSFT_cand.json

# Moving average files
echo '{
    "type": "moving_average",
    "data": []
}' > AAPL_mov.json

echo '{
    "type": "moving_average",
    "data": []
}' > GOOG_mov.json

echo '{
    "type": "moving_average",
    "data": []
}' > MSFT_mov.json

# Define the total runtime in seconds (1 hour = 3600 seconds)
total_runtime=$((48 * 60 * 60))

# Track the start time
start_time=$(date +%s)

# Define the C executable to run
executable="./rtes"  # Replace with your actual executable path

# Loop to run the executable repeatedly until the total time has passed
while true; do
  # Get the current time
  current_time=$(date +%s)
  
  # Calculate elapsed time
  elapsed_time=$((current_time - start_time))
  
  # Break the loop if the total runtime has been reached
  if ((elapsed_time >= total_runtime)); then
    echo "Total runtime of $total_runtime seconds reached. Exiting."
    break
  fi
  
  # Run the C executable until it is terminated
  $executable
  
  # If the executable terminates, restart it
  echo "Executable terminated, restarting..."
  sleep 5
done
