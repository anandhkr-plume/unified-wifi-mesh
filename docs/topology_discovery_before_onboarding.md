# Topology Discovery Before Onboarding

## Question
**How does the Controller send Topology Discovery messages before device onboarding?**

## Answer

Topology Discovery messages are sent by the **IEEE 1905.1 layer** (not directly by unified-wifi-mesh), which operates **independently** of the onboarding process. This is part of the IEEE 1905.1 standard where devices periodically announce their presence.

---

## Architecture Overview

The unified-wifi-mesh implementation uses a **two-layer architecture**:

1. **IEEE 1905.1 Layer** (`ieee1905-rs` - Rust implementation)
   - Handles periodic Topology Discovery transmission
   - Operates at the AL SAP (Abstraction Layer Service Access Point) level
   - Sends messages independently of EasyMesh onboarding state

2. **EasyMesh Layer** (`unified-wifi-mesh` - C++ implementation)
   - Handles EasyMesh-specific messages (autoconfig, WSC, etc.)
   - Uses topology discovery messages created by IEEE 1905.1 layer
   - Processes received topology discovery messages

---

## How Topology Discovery Works

### IEEE 1905.1 Layer (ieee1905-rs)

**File**: `ieee1905-rs/src/cmdu_proxy.rs`

**Function**: `cmdu_topology_discovery_transmission()`

**Key Code** (lines 40-135):
```rust
pub async fn cmdu_topology_discovery_transmission(
    interface: String,
    sender: Arc<EthernetSender>,
    message_id_generator: Arc<MessageIdGenerator>,
    local_al_mac_address: MacAddr,
    interface_mac_address: MacAddr,
) {
    let task_handle = task::spawn(async move {
        // Periodic timer: sends every 30 seconds
        let mut ticker = interval(Duration::from_secs(30));

        loop {
            ticker.tick().await; // Wait for next tick
            
            // Create Topology Discovery CMDU
            let cmdu_topology_discovery = CMDU {
                message_version: 1,
                message_type: CMDUType::TopologyDiscovery.to_u16(),
                message_id: message_id_generator.next_id(),
                // ... TLVs: AL MAC, Interface MAC, EOM
            };
            
            // Send to multicast address
            let destination_mac = MacAddr::new(0x01, 0x80, 0xC2, 0x00, 0x00, 0x13);
            sender.send_frame(destination_mac, interface_mac_address, 0x893A, &serialized_cmdu).await;
        }
    });
}
```

**Initialization** (File: `ieee1905-rs/src/main.rs`, line 324):
```rust
// Start of discovery process
cmdu_topology_discovery_transmission(
    discovery_interface_ieee1905,
    Arc::clone(&sender),
    Arc::clone(&message_id_generator),
    al_mac,
    forwarding_mac,
)
.await;
```

**Key Points**:
- **Periodic Transmission**: Every 30 seconds (as per IEEE 1905.1 standard)
- **Multicast Address**: `01:80:c2:00:00:13` (IEEE 1905 Control Multicast)
- **Independent of Onboarding**: Runs continuously regardless of EasyMesh state
- **Started at Boot**: Initialized when IEEE 1905.1 service starts

---

## Unified-WiFi-Mesh Layer

### Topology Discovery Message Creation

**File**: `src/em/disc/em_discovery.cpp`

**Function**: `em_discovery_t::create_topo_discovery_msg()`

**Key Code** (lines 95-164):
```cpp
unsigned int em_discovery_t::create_topo_discovery_msg(unsigned char *buff)
{
    unsigned short msg_type = em_msg_type_topo_disc;
    mac_address_t multi_addr = {0x01, 0x80, 0xc2, 0x00, 0x00, 0x13};
    
    // Destination: Multicast
    memcpy(tmp, multi_addr, sizeof(mac_address_t));
    
    // Source: AL MAC
    memcpy(tmp, get_current_cmd()->get_al_interface_mac(), sizeof(mac_address_t));
    
    // CMDU Header
    cmdu->type = htons(msg_type);
    cmdu->id = htons(get_mgr()->get_next_msg_id());
    
    // TLVs:
    // - AL MAC Address TLV
    // - MAC Address TLV (interface MAC)
    // - Vendor Specific TLV
    // - End of Message TLV
}
```

**Note**: This function exists but `em_discovery_t::process_state()` is **empty** (line 171-174), meaning unified-wifi-mesh doesn't actively send periodic topology discovery messages itself.

---

## Message Flow

### Controller Startup Sequence

```
1. Controller Process Starts
   └─> main() in src/ctrl/em_ctrl.cpp:994
       └─> g_ctrl.init()
           └─> data_model_init()
           └─> orch_init()
           └─> start()
               
2. IEEE 1905.1 Layer Starts (Parallel Process)
   └─> ieee1905-rs main() starts
       └─> cmdu_topology_discovery_transmission() called
           └─> Spawns async task
           └─> Sends Topology Discovery every 30 seconds
           └─> Destination: 01:80:c2:00:00:13 (multicast)
           
3. Topology Discovery Messages Sent
   └─> Every 30 seconds, regardless of:
       ├─> Onboarding state
       ├─> Number of agents
       ├─> Network configuration
       └─> EasyMesh state
```

### Message Reception

```
1. Agent Receives Topology Discovery
   └─> IEEE 1905.1 layer receives frame
       └─> Routes to unified-wifi-mesh
           └─> em_t::proto_process()
               └─> case em_msg_type_topo_disc:
                   └─> em_configuration_t::process_msg()
                       └─> Updates topology information
```

---

## Why Topology Discovery Happens Before Onboarding

### IEEE 1905.1 Standard Requirement

According to the IEEE 1905.1 standard:

1. **Periodic Advertisement**: Devices MUST periodically broadcast Topology Discovery messages to announce their presence
2. **Neighbor Discovery**: This allows devices to discover neighbors **before** any higher-layer protocols (like EasyMesh) are established
3. **Topology Building**: Devices build a topology map of all IEEE 1905.1 devices in the network
4. **Independent Operation**: Topology Discovery operates at Layer 2 (IEEE 1905.1) and is independent of Layer 3+ protocols (EasyMesh)

### Benefits

1. **Early Discovery**: Agents can discover controllers before onboarding
2. **Network Visibility**: All devices can see the network topology
3. **Bridge Discovery**: Helps identify bridging devices (intermediate nodes)
4. **Path Selection**: Enables path selection for data forwarding

---

## Code Flow Summary

### Controller Side (Before Onboarding)

```
IEEE 1905.1 Service Startup
    │
    ├─> main() - ieee1905-rs/src/main.rs:324
    │   └─> cmdu_topology_discovery_transmission()
    │       └─> Spawns async task
    │           └─> Loop every 30 seconds:
    │               ├─> Create Topology Discovery CMDU
    │               ├─> Include AL MAC Address TLV
    │               ├─> Include Interface MAC Address TLV
    │               └─> Send to multicast 01:80:c2:00:00:13
    │
    └─> Unified-WiFi-Mesh Controller Starts
        └─> em_ctrl_t::init()
            └─> Creates AL EM node
            └─> Ready to receive messages
            └─> (Topology Discovery already being sent by IEEE 1905.1 layer)
```

### Agent Side (Before Onboarding)

```
IEEE 1905.1 Service Startup (on Agent)
    │
    ├─> Receives Topology Discovery from Controller
    │   └─> Updates topology map
    │   └─> Knows Controller exists
    │
    └─> Unified-WiFi-Mesh Agent Starts
        └─> em_agent_t::init()
            └─> Creates AL EM node
            └─> Receives topology discovery
            └─> Can see Controller in topology
            └─> Then initiates onboarding (autoconfig search)
```

---

## Key Files

### IEEE 1905.1 Layer (Rust)
- `ieee1905-rs/src/cmdu_proxy.rs` - Topology Discovery transmission
- `ieee1905-rs/src/main.rs` - Service initialization
- `ieee1905-rs/src/cmdu_handler.rs` - Topology Discovery reception

### Unified-WiFi-Mesh Layer (C++)
- `src/em/disc/em_discovery.cpp` - Topology Discovery message creation
- `src/em/config/em_configuration.cpp` - Topology Discovery processing
- `src/ctrl/em_ctrl.cpp` - Controller initialization
- `src/agent/em_agent.cpp` - Agent initialization

---

## Important Points

1. **Two-Layer Architecture**:
   - IEEE 1905.1 layer handles periodic Topology Discovery
   - EasyMesh layer handles onboarding and configuration

2. **Independent Operation**:
   - Topology Discovery runs continuously
   - Not dependent on onboarding state
   - Started at IEEE 1905.1 service startup

3. **Standard Compliance**:
   - Follows IEEE 1905.1 standard requirement
   - 30-second interval (as per standard)
   - Multicast to all IEEE 1905.1 devices

4. **Message Format**:
   - CMDU Type: `0x0000` (Topology Discovery)
   - TLVs: AL MAC Address, Interface MAC Address, Vendor Specific, EOM
   - Destination: `01:80:c2:00:00:13` (IEEE 1905 Control Multicast)

5. **Why It's Visible Before Onboarding**:
   - Topology Discovery is a **Layer 2** (IEEE 1905.1) mechanism
   - Onboarding is a **Layer 3+** (EasyMesh) mechanism
   - Layer 2 discovery happens independently and first

---

## Conclusion

The Controller sends Topology Discovery messages **before onboarding** because:

1. **IEEE 1905.1 Layer**: The IEEE 1905.1 service (ieee1905-rs) starts and immediately begins sending periodic Topology Discovery messages every 30 seconds
2. **Independent Process**: This happens independently of the EasyMesh onboarding process
3. **Standard Requirement**: This is required by the IEEE 1905.1 standard for neighbor discovery
4. **Early Visibility**: This allows agents to discover controllers and build topology maps before initiating onboarding

The unified-wifi-mesh code has the capability to create topology discovery messages (`em_discovery_t::create_topo_discovery_msg()`), but the actual periodic transmission is handled by the IEEE 1905.1 layer, which operates at a lower level and starts before EasyMesh onboarding begins.

