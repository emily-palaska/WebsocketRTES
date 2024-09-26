# Implementation of a Real-Time Transaction Collection and Processing System Using Websockets on Raspberry Pi

This report is created as part of the Real-Time Embedded Systems course in Electrical and Computer Engineering department of Aristotle University of Thessaloniki. Its goal is the design and implementation of a real-time transaction collection and processing system, utilizing Websockets on a Raspberry Pi Zero 2W. The system is engineered to capture and process financial transaction data in real time, leveraging the lightweight and efficient architecture of the Raspberry Pi and the bidirectional communication capabilities of Websockets. By integrating these technologies, the system ensures low-latency data transmission and continuous updates, making it an ideal solution for applications that require rapid processing and dynamic response to transaction events.

Please refer to the report.pdf file for analysis, results and references.

## Overview of files
### finnhub_example.py
Quick python script to test the websocket connection in a high-level language setting

### websockets_example.c
Fundemental functionalities of the libwebsockets library through a simple connection, where the incoming messages are only printed and not saved.

### json_operations.c
Dummy programm to simulate the receiving of thread in order to create functions that save the to the corresponding JSON files using the Jansson library.

### json_threads.c
Similar to json_operations.c but also incorporating the producer-consumer dynamic using the Pthreads library

### rtes.c and rtes
This is the final code and executable, compiled with aarch64-linux-gnu-gcc

