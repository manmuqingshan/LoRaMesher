# LoRaMesher 1.0.0

## ⛔⛔ Work in progress, do not try to run this ⛔⛔

## See the [GitHub Pages](https://jaimi5.github.io/LoRaMesher) for more information

## Introduction

The LoRaMesher library implements a distance-vector routing protocol for communicating messages among LoRa nodes. For the interaction with the LoRa radio chip, we leverage RadioLib, a versatile communication library which supports different LoRa series modules.
LoRaMesher library uses C++20.

### Compatibility
At this moment, LoRaMesher has been tested within the following modules:
- SX1262
- SX1268
- SX1276
- SX1278
- SX1280

You can request another module to be added to the library by opening an issue.

## Dependencies

You can check `library.json` for more details. Basically, we use [Radiolib](https://github.com/jgromes/RadioLib) that implements the low level communication to the different LoRa modules and [FreeRTOS](https://freertos.org/index.html) for scheduling maintenance tasks.

## Configure LoRaMesher with PlatformIO and Visual Studio Code

1. Download Visual Studio Code.
2. Download PlatformIO inside Visual Studio Code.
3. Clone the LoraMesher repository.
4. Go to PlatformIO Home, click on the Projects button, then on "Add Existing", and find the examples/beta-sample source in the files.
5. Select the examples/beta-example project.
6. Build the project with PlatformIO.
7. Upload the project to the specified LoRa microcontroller. In our case, we use the TTGO T-Beam module.

## LoRaMesher Example

There is, in the source files of this first implementation, an example to test the new functionalities. This example is an implementation of a counter, sending a broadcast message every 10 seconds. To make it easier to understand, we will remove additional functions that are not necessary to make the microcontroller work with the LoRaMesher library.

### Defining the data type and the data counter

As a proof of concept, we will be sending a numeric counter over LoRa. Its value will be incremented every 10 seconds, then te packet will be transmitted. To start, we need to implement the type of data we will use.

In this case, we will only send a `uint32_t`, which is the counter itself.

```
uint32_t dataCounter = 0;
struct dataPacket {
  uint32_t counter = 0;
};

dataPacket* helloPacket = new dataPacket;
```

### LoRaMesh Initialization

#### Default Configuration

To initialize the new implementation, you can configure the LoRa parameters that the library will use. If your node needs to receive messages to the application, see Received packets function section.

You can configure different parameters for LoRa configuration. 
Using the ```LoRaMesherConfig``` you can configure the following parameters (Mandatory*):
- LoRaCS*
- LoRaIRQ*
- LoRaRST*
- LoRaI01*
- LoRa Module (See Compatibility)*
- Frequency
- Band
- Spreading Factor
- Synchronization Word
- Power
- Preamble Length
- SPI Class.

Here is an example on how to configure LoRaMesher using this configuration:

```
//Get the LoraMesher instance
LoraMesher& radio = LoraMesher::getInstance();

//Get the default configuration
LoraMesher::LoraMesherConfig config = LoraMesher::LoraMesherConfig();

//Change some parameters to the configuration
//(TTGO T-Beam v1.1 pins)
config.loraCS = 18
config.loraRst = 23
config.loraIrq = 26
config.loraIo1 = 33

config.module = LoraMesher::LoraModules::SX1276_MOD;

//Initialize the LoraMesher. You can specify the LoRa parameters here or later with their respective functions
radio.begin(config);

//After initializing you need to start the radio with
radio.start();

//You can pause and resume at any moment with
radio.standby();
//And then
radio.start();
```

*Be aware of the local laws that apply to radio frequencies*

### Received packets function

If your node needs to receive packets from other nodes you should follow the next steps:

1. Create a function that will receive the packets:

The function that gets a notification each time the library receives a packet for the app looks like this one:

```
/**
 * @brief Function that process the received packets
 *
 */
void processReceivedPackets(void*) {
    for (;;) {
        /* Wait for the notification of processReceivedPackets and enter blocking */
        ulTaskNotifyTake(pdPASS, portMAX_DELAY);

        //Iterate through all the packets inside the Received User Packets FiFo
        while (radio.getReceivedQueueSize() > 0) {
            Serial.println("ReceivedUserData_TaskHandle notify received");
            Serial.printf("Queue receiveUserData size: %d\n", radio.getReceivedQueueSize());

            //Get the first element inside the Received User Packets FiFo
            AppPacket<dataPacket>* packet = radio.getNextAppPacket<dataPacket>();

            //Print the data packet
            printDataPacket(packet);

            //Delete the packet when used. It is very important to call this function to release the memory of the packet.
            radio.deletePacket(packet);

        }
    }
}
```

There are some important things we need to be aware of:

- This function should have a `void*` in the parameters.
- The function should contain an endless loop.
- Inside the loop, it is mandatory to have the `ulTaskNotifyTake(pdPASS,portMAX_DELAY)` or equivalent. This function allows the library to notify the function to process pending packets.
- All the packets are stored inside a private queue.
- There is a function to get the size of the queue `radio.getReceivedQueueSize()`.
- You can get the first element with `radio.getNextAppPacket<T>()` where T is the type of your data. 
- IMPORTANT!!! Every time you call Pop, you need to be sure to call `radio.deletePacket(packet)`. It will free the memory that has been allocated for the packet. If not executed it can cause memory leaks and out of memory errors.

2. Create a task containing this function:
```
TaskHandle_t receiveLoRaMessage_Handle = NULL;

/**
 * @brief Create a Receive Messages Task and add it to the LoRaMesher
 *
 */
void createReceiveMessages() {
    int res = xTaskCreate(
        processReceivedPackets,
        "Receive App Task",
        4096,
        (void*) 1,
        2,
        &receiveLoRaMessage_Handle);
    if (res != pdPASS) {
        Serial.printf("Error: Receive App Task creation gave error: %d\n", res);
    }
}

```

2. Add the receiveLoRaMessage_Handle to the LoRaMesher

```
radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
```

### User data packet

In this section we will show you what there are inside a `AppPacket`.
```
class AppPacket {
    uint16_t dst; //Destination address, normally it will be local address or BROADCAST_ADDR
    uint16_t src; //Source address
    uint32_t payloadSize = 0; //Payload size in bytes
    T payload[]; //Payload

    size_t getPayloadLength() { return this->payloadSize / sizeof(T); }
};
```

Functionalities to use after getting the packet with `AppPacket<T>* packet = radio.getNextAppPacket<T>()`:
1. `packet->getPayloadLength()` it will get you the payload size in number of T
2. `radio.deletePacket(packet)` it will release the memory allocated for this packet.

### Send data packet function

In this section we will present how you can create and send packets. in this example we will use the `AppPacket` data structure.

```
  void loop() {
        helloPacket->counter = dataCounter++;

        //Create packet and send it.
         radio.createPacketAndSend(BROADCAST_ADDR, helloPacket, 1);
         
         //Or if you want to send large and reliable payloads you can call this function too.
         radio.sendReliable(dstAddr, helloPacket, 1);

        //Wait 10 seconds to send the next packet
        vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
```

In the previous figure we can see that we are using the helloPacket, we add the counter inside it, and we create and send the packet using the LoRaMesher.

The most important part of this piece of code is the function that we call in the `radio.createPacketAndSend()`:

1. The first parameter is the destination, in this case the broadcast address.
2. And finally, the helloPacket (the packet we created) and the number of elements we are sending, in this case only 1 dataPacket.

## Timing-Aware Data Sending

LoRaMesher is TDMA-based — each node has specific TX data slot(s) within each
superframe where it can transmit user data. Calling `Send()` at the right moment
ensures the message goes out in the very next available TX slot rather than waiting
an extra superframe.

### Query the next TX opportunity

```cpp
// Sleep until just before the next TX data slot, then send
uint32_t wait_ms = mesh.GetTimeUntilNextDataSlot(); // default 200ms guard time
vTaskDelay(pdMS_TO_TICKS(wait_ms));
mesh.Send(destination_address, payload_bytes);
```

The guard time (default: 200 ms) is subtracted from the raw slot start time so
your code has enough time to wake up, prepare data, and call `Send()` before the
slot begins. You can pass a custom guard time if needed:

```cpp
uint32_t wait_ms = mesh.GetTimeUntilNextDataSlot(/*guard_time_ms=*/100);
```

### Multiple TX slots per superframe

If the network manager allocates more than one TX slot to your node, you can fill
all of them each cycle:

```cpp
uint8_t tx_slots = mesh.GetDataSlotsPerSuperframe(); // e.g. 2
for (uint8_t i = 0; i < tx_slots; i++) {
    uint32_t wait_ms = mesh.GetTimeUntilNextDataSlot();
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
    mesh.Send(destination, readings[i]);
}
```

Both functions return `0` if the node has not yet joined a network.

### Print packet example

When receiving the packet, we need to understand what the Queue will return us. For this reason, in the next subsection, we will explain how to implement a simple packet processing.

```
/**
 * @brief Print the counter of the packet
 *
 * @param data
 */
void printPacket(dataPacket data) {
  Serial.printf("Hello Counter received n %d\n", data.counter);
}

/**
 * @brief Iterate through the payload of the packet and print the counter of the packet
 *
 * @param packet
 */
void printDataPacket(AppPacket<dataPacket>* packet) {
    //Get the payload to iterate through it
    dataPacket* dPacket = packet->payload;
    size_t payloadLength = packet->getPayloadLength();

    for (size_t i = 0; i < payloadLength; i++) {
        //Print the packet
        printPacket(dPacket[i]);
    }
}
```

1. After receiving the packet in the `processReceivedPackets()` function, we call the `printDataPacket()` function.
2. We need to get the payload of the packet using `packet->payload`.
3. We iterate through the `packet->getPayloadLength()`. This will let us know how big the payload is, in dataPackets types, for a given packet. In our case, we always send only one dataPacket.
4. Get the payload and call the `printPacket(dPacket[i])` function, that will print the counter received.

## More information on the design and evaluation of LoRaMesher
Please see our open access paper ["Implementation of a LoRa Mesh Library"](https://ieeexplore.ieee.org/document/9930341) for a detailed description. If you use the LoRaMesher library, in academic work, please cite the following:
```
@ARTICLE{9930341,
  author={Solé, Joan Miquel and Centelles, Roger Pueyo and Freitag, Felix and Meseguer, Roc},
  journal={IEEE Access}, 
  title={Implementation of a LoRa Mesh Library}, 
  year={2022},
  volume={10},
  number={},
  pages={113158-113171},
  doi={10.1109/ACCESS.2022.3217215}}
```

## Additional configurations

### Debug

If you want to additional logs you can use the flag DEBUG. It will set the library for testing and it will add additional logs that monitors tasks.

```// #define DEBUG  // Uncomment to enable debug functionality```

### Log configuration
#### Log Level

There is 5 different log levels
- 0 = DEBUG
- 1 = INFO
- 2 = WARNING
- 3 = ERROR
- 4 = NO_LOG

```#define LORAMESHER_LOG_LEVEL 4  // 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR, 4=NO_LOG```

#### Disable colors output

You can disable color for your serial monitor if needed. This can be usefull if you want to store logs in a local storage and optimize memory.

```//#define LOGGER_DISABLE_COLORS   // Disable color output```

#### Log buffer size

You can change buffer size of the log.

```#define LOGGER_BUFFER_SIZE 128  // Adjust buffer size for your needs```

## Cmake building

```
mkdir build && cd build
cmake .. -DBUILD_DESKTOP=ON
cmake --build . --target loramesher_lib
```

### Targets
- `loramesher_lib` -> To build the library.
- `build_all_tests` -> To build the tests.
- `run_all_tests` -> To build and run the tests.

Or insted use `cTest`

## Analysis Tools

### AddressSanitizer + UndefinedBehaviorSanitizer (default test env)

The `test_native` environment runs with ASAN and UBSAN enabled. LSAN suppressions for
intentional leaks (Logger semaphore) are loaded automatically.

```bash
# Run all tests
pio test -e test_native -v

# Run a specific test suite
pio test -e test_native -v -f "protocols/lora_mesh/services/test_routing"
```

What to look for:
- `ERROR: AddressSanitizer:` — heap/stack corruption, use-after-free
- `runtime error:` — undefined behavior (signed overflow, null deref, etc.)
- `ERROR: LeakSanitizer:` — unexpected memory leaks (intentional ones are suppressed)

### ThreadSanitizer

TSAN is mutually exclusive with ASAN; use the dedicated environment:

```bash
pio test -e test_native_tsan -v
```

What to look for: `WARNING: ThreadSanitizer: data race`

### Static Analysis (clang-tidy)

Requires a CMake build with `compile_commands.json`:

```bash
mkdir -p build && cd build
cmake .. -DBUILD_DESKTOP=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cd ..

# Analyse all sources
run-clang-tidy -p build/ src/

# Analyse a single file
clang-tidy -p build/ src/protocols/lora_mesh/services/network_service.cpp
```

Checks enabled: `clang-analyzer-*`, `bugprone-*` (UAF, dangling handles, infinite loops),
`cppcoreguidelines-owning-memory`, `concurrency-mt-unsafe`, and others. See `.clang-tidy`
at the repo root for the full list.

### Profiling / Code Coverage (LLVM instrumentation)

The `test_native_profile` environment compiles and links with
`-fprofile-instr-generate -fcoverage-mapping` plus the clang profiling runtime
(`libclang_rt.profile-x86_64.a`), wired in automatically by `extra_script.py`.

> **Important:** PlatformIO runs the test binary in a subprocess that does not
> inherit environment changes made inside the build script. You must prefix the
> `pio test` command with `LLVM_PROFILE_FILE` in the shell so the raw profile
> data lands in `.pio/coverage/` instead of `default.profraw` in the project root.

#### 1. Run the instrumented tests (background task — takes several minutes)

```bash
LLVM_PROFILE_FILE="$(pwd)/.pio/coverage/%e-%p.profraw" \
    pio test -e test_native_profile -v \
    --filter "protocols/lora_mesh/services/test_routing"
```

The `.pio/coverage/` directory is created automatically by `extra_script.py`.

#### 2. Merge raw profile data

```bash
llvm-profdata merge -sparse .pio/coverage/*.profraw \
    -o .pio/coverage/loramesher.profdata
```

If `llvm-profdata` is not in PATH, try the versioned binary
(e.g. `llvm-profdata-18`).

#### 3. Locate the test binary

```bash
find .pio/build/test_native_profile -name "program" 2>/dev/null
```

Typically: `.pio/build/test_native_profile/program`

#### 4. Generate coverage report

**Text summary (per-file line / function / region coverage):**

```bash
llvm-cov report .pio/build/test_native_profile/program \
    -instr-profile=.pio/coverage/loramesher.profdata \
    --ignore-filename-regex='(test/|googletest|\.pio)'
```

**HTML report (line-by-line, browsable):**

```bash
llvm-cov show .pio/build/test_native_profile/program \
    -instr-profile=.pio/coverage/loramesher.profdata \
    -format=html \
    -output-dir=coverage/ \
    --ignore-filename-regex='(test/|googletest|\.pio)'
# Open coverage/index.html in a browser
```

Replace `llvm-cov` / `llvm-profdata` with the versioned binary if needed
(e.g. `llvm-cov-18`, `llvm-profdata-18`).


# Building the new Protocol using BMX6 and node syncrhonization

## States

The nodes have 6 protocol states:

1. Initialization: System startup and initial configuration

2. Discovery: Searching for or establishing a network.
    - Discovery will set the radio to listening and will wait to a full default super frame to see if it listens a loramesher packet.
        - If it discovers a packet it will pass to next joining state.
        - If no packet is being detectet, start a new network.

3. Joining: Process of connecting to an existing network
    - It will wait until the discovery slot, then it will send the actual routing table.

4. Normal Operation: Regular network activity (sending/receiving data)
    - After defining the routing table order, it will define the TX and RX slots. 
        - RX slots will be defined by the order of the routing table. If the node is slot 2, it will capable of sending when slot time is 2.
        - TX slots will be defined by the RX nodes of the neighbour nodes.
        - If neither RX/TX slot in the frame, it will set the radio sleep and do a light sleep if possible.

5. Network Manager: Special state when acting as network manager
    - In this state the the node will send routing table updates and link quality. The order of RX/TX will be accordingly to the previous slots.
    - After finishing this state, the RX/TX will define the next super frame order.

6. Fault Recovery: Handling connection issues and recovering.
    - If a node has lost their conection to all the nodes, it will start a fault recovery. If after a time it does not capable of reconnect it will start again, creating or joining a network.

## Super fram structure

### Data Transmission Phase

- Ordered transmission/reception based on routing table
- Nodes follow a predetermined schedule for TX/RX/sleep
- Schedule follows prioritization that changes each superframe
- Only active nodes during their assigned slots need to be awake

### Discovery Slots

- All nodes must listen during these slots
- Limited number of slots for new nodes to join
- Uses CSMA/CA to minimize collision during join requests
- All nodes retransmit the message to all the network.
- The slots are defined by:
    - #Devices + 1


### Control Slots

- All nodes must listen during these slots
- Used for slot allocation requests
- Synchronization information distribution
- Network manager role assignment
- Routing table updates 

## Packet types

- DISCOVERY (new node announcement)
- JOIN_REQUEST (request to join network)
- JOIN_RESPONSE (acceptance into network)
- ROUTING_UPDATE (routing table information)
- SLOT_REQUEST (request for transmission slots)
- SLOT_ALLOCATION (slot assignments)
- SYNC (time synchronization)
- DATA (application payload)
- KEEP_ALIVE (link maintenance)
- FAULT_RECOVERY (reconnection attempt)

## Tasks

- Protocol Logic Task: Makes decisions about messages, manages routing, and interfaces with applications
- Slot Manager Task: Handles timing and synchronization, determines when actions should occur
- Radio Event Handler Task: Interfaces with the physical radio, processes radio events

# Known Issues and Future Work

## Network Synchronization
- **Network merge handling**: What happens when two independent networks discover each other? Need a process to decide which network is primary and which should merge.
- **Concurrent discovery**: What happens if two devices start their discovery process simultaneously?

## Routing Table
- **Update validation**: Currently, routing table updates from devices outside the network are not accepted. This behavior should be documented and verified.
- **Table order verification**: The order of the routing table entries should be carefully verified to prevent potential synchronization issues.
- **Distributed message routing**: Consider implementing an inverse routing table order to better distribute messages up and down the network hierarchy. This requires careful implementation to avoid complete desynchronization.
- **Route uptade callback**: Should be a class with types, easy to modify, not only parameters.

## Sync Beacon Management
- **Multi-device sync beacons**: How to manage sync beacons across multiple devices? Consider implementing a dedicated sync beacon TX slot for each device.
- **Network manager synchronization**: Clarify the sync beacon transmission responsibilities when nodes are in Network Manager state.

## Future Enhancements
- **Message types**: Define comprehensive set of message types and their required fields, particularly for routing table updates.
- **Load balancing**: Implement better distribution of network traffic across nodes.
- **Discovery time**: Discovery Time should be calculated depending on the superframe duration of the devices. Maybe it should be an option to not stop the discovery until finding a network if known that an existing network exists. Then a Create network straight forward should be added.
- **Fix function**: CreateRoutingTableMessage() in networ_service.cpp there is 3 loops there, you can do better.


- Be aware if slot time is changed, discovery and join timeouts should be carefully changed too.
- When receiving control packets, if they change the topology, we could change the slot table acordingly without changing the length of it. It happens that the RX/TX slots needs >+1 superframe to align to other slots when joining a new network. 

**IMPORTANT**
- When reasingin a a join request do not reassing a CTR Slot. We need to get track of these CTR SLOTS
- Each node currently computes the number of control slots independently using max(routing_table.size(), my_control_slot_index_ + 1). This leads to inconsistencies when nodes have different routing table views (e.g., 0x1003 computed 5 control slots for a 4-node network because its assigned index was inflated by re-join). The NM is the authoritative source for network topology — adding node_count to the sync beacon ensures all nodes agree on the slot frame structure.

- Loop message prevention for all the messages.
- Sync beacon should be optimized.

## Ideas

- Delta scripting
- Rateless Coding