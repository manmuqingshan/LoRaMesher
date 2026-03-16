# LoRaMesher Queued Receive Example

Demonstrates the recommended pattern for safely receiving LoRa messages in your application: the callback only enqueues, while a dedicated task processes messages on its own stack.

## What This Example Does

1. **Creates** a message queue and an application-owned `ReceiveTask` before starting the mesher
2. **Registers** a lightweight `SetDataCallback` that allocates an `AppMessage` and enqueues it — non-blocking, drops on full
3. **Starts** LoRaMesher, which creates its own internal radio and protocol tasks
4. **Processes** received messages in `ReceiveTask`, decoupled from the mesher's internal tasks
5. **Sends** periodic "Hello!" messages to the first peer in the routing table

## Why This Pattern

Calling application logic directly inside `SetDataCallback` is unsafe: the callback runs on LoRaMesher's internal task with its own stack and priority. Heavy processing there can starve the radio, cause stack overflows, or introduce priority inversion.

The queue-based pattern keeps the callback minimal (allocate + enqueue) and moves all application work to a task you control:

```
SetDataCallback  →  enqueue AppMessage* (non-blocking, 0 timeout)
ReceiveTask      →  dequeue and process  (blocks on portMAX_DELAY)
```

If the queue is full the packet is dropped and logged — the mesher's task is never blocked.

## Hardware Requirements

### Supported Boards

| Board | LoRa Module | CS | RST | IRQ (DIO0) | IO1 (DIO1) |
|-------|-------------|-----|-----|------------|------------|
| TTGO T-Beam v1.x | SX1276 | 18 | 23 | 26 | 33 |
| TTGO LoRa32 v1 | SX1276 | 18 | 14 | 26 | 33 |
| Heltec WiFi LoRa 32 | SX1276 | 18 | 14 | 26 | 35 |

If using a different board, update the pin definitions in `main.cpp`:

```cpp
#define LORA_CS 18    // SPI Chip Select (NSS)
#define LORA_RST 23   // Radio Reset pin
#define LORA_IRQ 26   // DIO0 - Primary interrupt
#define LORA_IO1 33   // DIO1 - Secondary interrupt
```

## Radio Configuration

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| Frequency | 869.9 MHz | EU868 band — adjust for your region |
| Spreading Factor | 7 | SF7-SF12: higher = more range, slower data rate |
| Bandwidth | 125 kHz | Standard bandwidth (125/250/500 kHz) |
| Coding Rate | 4/7 | Error correction rate (4/5 to 4/8) |
| TX Power | 6 dBm | Start low for testing |
| Sync Word | 0x14 (20) | Network identifier — all nodes must match |

### Regional Frequency Bands

| Region | Frequency Range | Suggested |
|--------|-----------------|-----------|
| EU868 | 863-870 MHz | 869.9 MHz |
| US915 | 902-928 MHz | 915.0 MHz |
| AU915 | 915-928 MHz | 915.0 MHz |
| AS923 | 920-923 MHz | 920.0 MHz |

## Building and Uploading

### Using PlatformIO CLI

```bash
# Build the project
pio run -e ttgo-lora32-v1

# Upload to the board
pio run -e ttgo-lora32-v1 --target upload

# Monitor serial output
pio device monitor -b 115200

# Build for native (desktop) to verify compilation
pio run -e test_native
```

### Using PlatformIO IDE

1. Open this folder in VS Code with the PlatformIO extension
2. Select your board environment from the status bar
3. Click the Upload button (arrow icon)
4. Open Serial Monitor (plug icon)

## Expected Serial Output

### Startup

```
LoRaMesher started. Node address: 0x82df
```

### Receiving a Message

```
App received 16 bytes from 0x3adf
```

### Queue Full (burst scenario)

```
[W] Receive queue full, dropping packet
```

## Customizing the Example

### Add Your Processing Logic

Replace the stub in `ReceiveTask`:

```cpp
if (result == os::QueueResult::kOk && msg != nullptr) {
    // Convert payload to string
    std::string text(msg->data.begin(), msg->data.end());
    std::cout << "From 0x" << std::hex << msg->source
              << ": " << text << std::endl;

    // Respond to the sender
    std::string reply = "ACK";
    mesher->Send(msg->source,
        std::vector<uint8_t>(reply.begin(), reply.end()));

    delete msg;
}
```

### Adjust Queue Depth and Task Priority

In `ConfigureAndStart()`:

```cpp
// Deeper queue for high-traffic networks
receive_queue = os::RTOS::instance().CreateQueue(20, sizeof(AppMessage*));

// Higher priority if processing must not be delayed
os::RTOS::instance().CreateTask(ReceiveTask, "App_LoRa_Recv", 4096, nullptr, 5, &receive_task_handle);
```

## Code Structure

```
main.cpp
├── Pin / Radio Configuration   (lines ~28-48)
├── AppMessage struct           (lines ~55-58)
├── Global state                (lines ~64-66)
├── ReceiveTask                 (lines ~74-94)
└── ConfigureAndStart()
    ├── Step 1: CreateQueue     create queue before callback
    ├── Step 2: CreateTask      spawn ReceiveTask
    ├── Step 3: Build mesher    configure radio + mesh
    ├── Step 4: SetDataCallback enqueue-only callback
    └── Step 5: Start()         launch mesher background tasks
```

## Further Reading

- [LoRaMesher Protocol Specification](../../docs/PROTOCOL_SPEC.md)
- [Simple Example](../simple_example/README.md) — basic mesh setup without the queue pattern
- [RadioLib Documentation](https://jgromes.github.io/RadioLib/)
