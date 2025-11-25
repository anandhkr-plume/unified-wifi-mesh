# Channel Management in RDKB EasyMesh - Detailed Code Flow Documentation

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Channel Management Flow - Overlay to Underlay](#channel-management-flow---overlay-to-underlay)
4. [Key Components](#key-components)
5. [Message Types and TLVs](#message-types-and-tlvs)
6. [Orchestration Flow](#orchestration-flow)
7. [OneWifi Integration](#onewifi-integration)
8. [Code Flow Diagrams](#code-flow-diagrams)

---

## Overview

Channel management in the RDKB EasyMesh implementation handles the coordination of WiFi channel selection and configuration across the mesh network. It implements the Multi-AP channel management protocol, allowing the controller to query channel preferences from agents and configure optimal channels based on network conditions.

### Key Features
- **Channel Preference Reporting**: Agents report preferred channels per operating class
- **Channel Selection**: Controller selects optimal channels based on preferences
- **Channel Configuration**: Agents apply channel changes via OneWifi
- **Channel Scanning**: Agents perform channel scans and report results
- **Spatial Reuse**: WiFi 6/7 spatial reuse configuration
- **EHT Operations**: WiFi 7 EHT operations support

---

## CLI Command Examples

### Starting the CLI

```bash
# Start the EasyMesh CLI
onewifi_em_cli OneWifiMesh

# Start with remote controller connection
onewifi_em_cli OneWifiMesh <remote_ip> <remote_port>
```

### 1. Get Channel Configuration

#### Basic Channel Query
```bash
# In CLI prompt:
> get_channel OneWifiMesh
```

**What it does:**
- Queries current channel configuration from the mesh network
- Displays operating classes and channel preferences per radio
- Shows current operating channels and anticipated channel preferences

**Expected Output Structure:**
```
Network: OneWifiMesh
└── Device
    └── RadioList
        ├── Radio[0] (2.4 GHz)
        │   ├── CurrentOperatingClass: 81
        │   ├── CurrentChannel: 6
        │   └── AnticipatedChannelPreference
        │       ├── OpClass: 81
        │       └── ChannelList: [1, 6, 11]
        └── Radio[1] (5 GHz)
            ├── CurrentOperatingClass: 115
            ├── CurrentChannel: 36
            └── AnticipatedChannelPreference
                ├── OpClass: 115
                └── ChannelList: [36, 40, 44, 48]
```

#### Extended Channel Query (Per Radio)
```bash
# Query specific radio (2.4 GHz)
> get_channel OneWifiMesh 1

# Query specific radio (5 GHz)
> get_channel OneWifiMesh 2
```

**What it does:**
- Queries channel configuration for a specific radio index
- Provides detailed operating class information
- Shows channel capabilities and restrictions

### 2. Set Channel Configuration

#### Interactive Channel Configuration
```bash
# Step 1: Get current configuration
> get_channel OneWifiMesh

# Step 2: Navigate to channel preferences in TUI
# Use arrow keys to navigate to:
#   Network → OpClassList → OpClass[N] → AnticipatedChannelPreference → ChannelList

# Step 3: Edit channel list
# - Press 'e' to edit
# - Modify channel list (e.g., change from [1,6,11] to [1,11])
# - Press Enter to confirm

# Step 4: Apply changes
> set_channel OneWifiMesh
```

**What it does:**
1. Extracts edited channel preferences from the network tree
2. Creates `em_cmd_set_channel_t` command with orchestration descriptors
3. Sends Channel Selection Request to agents
4. Agents validate and respond
5. Agents apply configuration via OneWifi
6. Agents send Operating Channel Report back to controller

**Example Workflow:**
```
User Action:
  Edit 2.4 GHz channels from [1,6,11] to [1,11]
  Edit 5 GHz channels from [36,40,44,48] to [149,153,157,161]

CLI Command:
  > set_channel OneWifiMesh

Internal Flow:
  1. Channel Selection Request sent to agents
  2. Agents validate channel preferences
  3. Agents respond with ACCEPT
  4. Agents apply new channels via OneWifi
  5. Radio switches to new channel (with CSA if clients exist)
  6. Agents report new operating channels
  7. Controller updates topology

Result:
  ✓ 2.4 GHz radio now operating on channel 11
  ✓ 5 GHz radio now operating on channel 149
```

### 3. Channel Scanning

#### Trigger Channel Scan
```bash
# Scan all channels on all radios
> scan_channel OneWifiMesh

# Scan specific radio
> scan_channel OneWifiMesh <radio_mac>
```

**What it does:**
- Sends Channel Scan Request to agents
- Agents perform WiFi scan on specified channels
- Collects neighbor information, RSSI, channel utilization
- Reports scan results back to controller

**Scan Result Information:**
```
Scan Results for Radio: aa:bb:cc:dd:ee:ff
Operating Class: 81, Channel: 6
├── Scan Status: Success
├── Timestamp: 2025-11-20T07:10:00Z
├── Utilization: 45%
├── Noise: -95 dBm
└── Neighbors Found: 3
    ├── Neighbor[0]
    │   ├── BSSID: 11:22:33:44:55:66
    │   ├── SSID: "NeighborNetwork1"
    │   ├── Signal Strength: -65 dBm
    │   ├── Bandwidth: 20 MHz
    │   ├── Channel Utilization: 30%
    │   └── STA Count: 5
    ├── Neighbor[1]
    │   ├── BSSID: 77:88:99:aa:bb:cc
    │   ├── SSID: "NeighborNetwork2"
    │   ├── Signal Strength: -72 dBm
    │   ├── Bandwidth: 40 MHz
    │   ├── Channel Utilization: 50%
    │   └── STA Count: 8
    └── Neighbor[2]
        ├── BSSID: dd:ee:ff:00:11:22
        ├── SSID: "NeighborNetwork3"
        ├── Signal Strength: -80 dBm
        ├── Bandwidth: 20 MHz
        ├── Channel Utilization: 20%
        └── STA Count: 2
```

### 4. Advanced Channel Management Scenarios

#### Scenario 1: Optimize Channels Based on Scan Results
```bash
# Step 1: Perform channel scan
> scan_channel OneWifiMesh

# Step 2: Analyze scan results
# - Review channel utilization
# - Identify least congested channels

# Step 3: Update channel preferences
> get_channel OneWifiMesh
# Edit AnticipatedChannelPreference based on scan results

# Step 4: Apply new channels
> set_channel OneWifiMesh
```

#### Scenario 2: Configure DFS Channels (5 GHz)
```bash
# Get current configuration
> get_channel OneWifiMesh

# Navigate to 5 GHz radio operating class
# Edit to include DFS channels:
#   OpClass 122: Channels [100, 104, 108, 112, 116, 120, 124, 128]
#   OpClass 128: Channels [132, 136, 140, 144]

# Apply configuration
> set_channel OneWifiMesh

# Note: CAC (Channel Availability Check) will be performed for DFS channels
# Radio may take 60 seconds to switch to DFS channel
```

#### Scenario 3: Configure WiFi 6E (6 GHz) Channels
```bash
# Get current configuration
> get_channel OneWifiMesh

# Navigate to 6 GHz radio operating class
# Edit to configure 6 GHz channels:
#   OpClass 131: Channels [1, 5, 9, 13, 17, 21, 25, 29, 33, 37, ...]
#   OpClass 132: Channels [3, 11, 19, 27, 35, ...]  # 40 MHz
#   OpClass 133: Channels [7, 23, 39, ...]          # 80 MHz
#   OpClass 134: Channels [15, 47, ...]             # 160 MHz

# Apply configuration
> set_channel OneWifiMesh
```

#### Scenario 4: Multi-Radio Channel Coordination
```bash
# Configure channels to avoid co-channel interference
> get_channel OneWifiMesh

# Edit channel preferences:
#   2.4 GHz Radio 1: Channel 1
#   2.4 GHz Radio 2: Channel 6
#   2.4 GHz Radio 3: Channel 11
#   5 GHz Radio 1: Channel 36
#   5 GHz Radio 2: Channel 149

# Apply configuration
> set_channel OneWifiMesh
```

### 5. Channel Configuration with Spatial Reuse (WiFi 6)

```bash
# Get current configuration
> get_channel OneWifiMesh

# Navigate to radio configuration
# Edit spatial reuse parameters:
#   BSS Color: 5
#   OBSS PD Min Offset: -72 dBm
#   OBSS PD Max Offset: -62 dBm
#   SRG Information Valid: true

# Apply configuration (includes spatial reuse in Channel Selection Request)
> set_channel OneWifiMesh
```

### 6. Monitoring Channel Changes

#### View Current Operating Channels
```bash
# Query all radios
> get_channel OneWifiMesh

# Check CurrentOperatingClass and CurrentChannel for each radio
```

#### View Channel Preferences
```bash
# Query channel preferences
> get_channel OneWifiMesh

# Navigate to:
#   Network → OpClassList → OpClass[N] → AnticipatedChannelPreference
```

### 7. Troubleshooting Channel Issues

#### Check Channel Selection Response
```bash
# After set_channel, check for errors in the network tree
> get_channel OneWifiMesh

# Look for:
#   - ChannelSelectionResponse: ACCEPT (0x00)
#   - ChannelSelectionResponse: DECLINE_PREF_VIOLATED (0x01)
#   - ChannelSelectionResponse: DECLINE_CURR_OP (0x02)
#   - ChannelSelectionResponse: DECLINE_NO_INFO (0x03)
```

#### Verify Channel Switch Completion
```bash
# Before channel change
> get_channel OneWifiMesh
# Note CurrentChannel value

# Apply channel change
> set_channel OneWifiMesh

# Wait for channel switch (may take 5-60 seconds depending on CSA/CAC)

# Verify new channel
> get_channel OneWifiMesh
# Confirm CurrentChannel matches AnticipatedChannelPreference
```

### 8. JSON Output Format (for scripting)

```bash
# Get channel configuration in JSON format
> get_channel OneWifiMesh --json

# Example JSON output:
{
  "wfa-dataelements:Network": {
    "ID": "OneWifiMesh",
    "DeviceList": [{
      "RadioList": [{
        "ID": "aa:bb:cc:dd:ee:ff",
        "CurrentOperatingClass": 81,
        "CurrentChannel": 6,
        "AnticipatedChannelPreference": {
          "OpClass": 81,
          "ChannelList": [1, 6, 11]
        }
      }]
    }]
  }
}
```

### Command Summary Table

| Command | Purpose | Example |
|---------|---------|---------|
| `get_channel <network>` | Query channel configuration | `get_channel OneWifiMesh` |
| `get_channel <network> <radio_idx>` | Query specific radio | `get_channel OneWifiMesh 1` |
| `set_channel <network>` | Apply edited channel config | `set_channel OneWifiMesh` |
| `scan_channel <network>` | Trigger channel scan | `scan_channel OneWifiMesh` |
| `scan_channel <network> <radio_mac>` | Scan specific radio | `scan_channel OneWifiMesh aa:bb:cc:dd:ee:ff` |

### Key Points

1. **Interactive TUI**: The CLI provides a Terminal User Interface for easy navigation and editing
2. **Two-Step Process**: Always `get_channel` first to load configuration, then edit, then `set_channel`
3. **Real-time Updates**: Changes are applied immediately to the mesh network
4. **Validation**: Invalid channel selections are rejected by agents
5. **CSA Support**: Channel switches with clients use Channel Switch Announcement
6. **DFS Support**: DFS channels trigger CAC (60-second wait)
7. **Multi-Radio**: Can configure multiple radios simultaneously
8. **Remote Operation**: Can manage remote mesh networks

---

## Architecture

### Layered Architecture for Channel Management

```
┌─────────────────────────────────────────────────────────────┐
│                    CLI / Application Layer                   │
│              (set_channel command from user)                 │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                    Command Layer                             │
│              em_cmd_set_channel_t                            │
│         (Creates orchestration descriptors)                  │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                  Orchestration Layer                         │
│         em_orch_ctrl_t / em_orch_agent_t                    │
│    (Executes: channel_sel → channel_cnf)                    │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                   Protocol Layer                             │
│                  em_channel_t                                │
│   (Creates/handles channel messages and TLVs)                │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                  Data Model Layer                            │
│              dm_easy_mesh_t                                  │
│         (Stores operating classes, preferences)              │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                   OneWifi Layer                              │
│          (Applies channel configuration to radios)           │
└─────────────────────────────────────────────────────────────┘
```

---

## Channel Management Flow - Overlay to Underlay

### Complete Flow: CLI Command to OneWifi Configuration

```
USER COMMAND: set_channel OneWifiMesh
         │
         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 1: CLI Layer (Go + C++)                               │
│ File: src/cli/meshViews.go, src/cli/em_cli.cpp            │
├────────────────────────────────────────────────────────────┤
│ 1. User edits channel preferences in TUI                   │
│ 2. Go layer calls: C.exec("set_channel OneWifiMesh")      │
│ 3. em_cli_t::exec() creates em_cmd_cli_t                  │
│ 4. Extracts edited channel data from network tree         │
│ 5. Creates bus event: em_bus_event_type_set_channel       │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 2: Bus Event to Controller                            │
│ File: src/ctrl/em_ctrl.cpp                                 │
├────────────────────────────────────────────────────────────┤
│ 1. em_ctrl_t::handle_set_channel_list(evt)                │
│ 2. Decodes channel preferences from event                  │
│ 3. Calls dm_easy_mesh_ctrl_t::analyze_set_channel()       │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 3: Command Creation                                   │
│ File: src/cmd/em_cmd_set_channel.cpp                       │
├────────────────────────────────────────────────────────────┤
│ Constructor: em_cmd_set_channel_t(param, dm)              │
│                                                             │
│ m_orch_desc[0].op = dm_orch_type_channel_sel;             │
│ m_orch_desc[0].submit = true;                             │
│                                                             │
│ m_orch_desc[1].op = dm_orch_type_channel_cnf;             │
│ m_orch_desc[1].submit = true;                             │
│                                                             │
│ Creates 2-step orchestration:                              │
│   1. Channel Selection (send request to agents)            │
│   2. Channel Configuration (agents apply changes)          │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 4: Orchestration Execution - Channel Selection        │
│ File: src/em/em.cpp (em_t::orch_execute)                  │
├────────────────────────────────────────────────────────────┤
│ Orchestration Type: dm_orch_type_channel_sel              │
│                                                             │
│ State Check:                                                │
│   if (m_sm.get_state() == em_state_ctrl_channel_queried)  │
│                                                             │
│ Action: send_channel_sel_request_msg()                    │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 5: Create Channel Selection Request Message           │
│ File: src/em/channel/em_channel.cpp                        │
├────────────────────────────────────────────────────────────┤
│ Function: send_channel_sel_request_msg()                   │
│                                                             │
│ Message Type: em_msg_type_channel_sel_req (0x8007)        │
│                                                             │
│ TLVs Created:                                               │
│ ┌──────────────────────────────────────────────────────┐  │
│ │ 1. Channel Preference TLV (0x8b)                     │  │
│ │    - create_channel_pref_tlv(buff)                   │  │
│ │    - Reads from dm_op_class_t array                  │  │
│ │    - Type: em_op_class_type_anticipated              │  │
│ │    - Contains: RUID, op_class, channels, pref_bits   │  │
│ ├──────────────────────────────────────────────────────┤  │
│ │ 2. Transmit Power Limit TLV (0x8d)                   │  │
│ │    - create_transmit_power_limit_tlv(buff)           │  │
│ │    - Gets from dm_radio_t::transmit_power_limit      │  │
│ ├──────────────────────────────────────────────────────┤  │
│ │ 3. Spatial Reuse Request TLV (0xd8)                  │  │
│ │    - create_spatial_reuse_req_tlv(buff)              │  │
│ │    - WiFi 6 spatial reuse parameters                 │  │
│ ├──────────────────────────────────────────────────────┤  │
│ │ 4. EHT Operations TLV (0xe7)                         │  │
│ │    - create_eht_operations_tlv(buff)                 │  │
│ │    - WiFi 7 EHT parameters                           │  │
│ ├──────────────────────────────────────────────────────┤  │
│ │ 5. End of Message TLV (0x00)                         │  │
│ └──────────────────────────────────────────────────────┘  │
│                                                             │
│ Frame Structure:                                            │
│   [Ethernet Header]                                         │
│   [1905 CMDU Header]                                        │
│   [TLV 1: Channel Preference]                              │
│   [TLV 2: TX Power Limit]                                  │
│   [TLV 3: Spatial Reuse Request]                           │
│   [TLV 4: EHT Operations]                                  │
│   [TLV 5: End of Message]                                  │
│                                                             │
│ Transmission: send_frame(buff, len)                        │
│ State Transition: (no state change on send)                │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 6: Agent Receives Channel Selection Request           │
│ File: src/em/channel/em_channel.cpp                        │
├────────────────────────────────────────────────────────────┤
│ Function: handle_channel_sel_req(buff, len)                │
│                                                             │
│ 1. Parse incoming message                                   │
│ 2. Extract TLVs:                                            │
│    - Channel Preference TLV → handle_channel_pref_tlv()    │
│    - TX Power Limit TLV → (store in data model)           │
│    - Spatial Reuse Request → (store in data model)         │
│    - EHT Operations → handle_eht_operations_tlv()          │
│                                                             │
│ 3. Update data model with controller preferences           │
│    - dm_op_class_t array updated                           │
│    - Type: em_op_class_type_anticipated                    │
│                                                             │
│ 4. Determine channel selection response                    │
│    - Response code: em_chan_sel_resp_code_type_t          │
│      • em_chan_sel_resp_code_accept (0x00)                │
│      • em_chan_sel_resp_code_decline_pref_violated (0x01) │
│      • em_chan_sel_resp_code_decline_curr_op (0x02)       │
│      • em_chan_sel_resp_code_decline_no_info (0x03)       │
│                                                             │
│ 5. Send response: send_channel_sel_response_msg(code, id)  │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 7: Agent Sends Channel Selection Response             │
│ File: src/em/channel/em_channel.cpp                        │
├────────────────────────────────────────────────────────────┤
│ Function: send_channel_sel_response_msg(code, msg_id)      │
│                                                             │
│ Message Type: em_msg_type_channel_sel_rsp (0x8008)        │
│                                                             │
│ TLVs Created:                                               │
│ ┌──────────────────────────────────────────────────────┐  │
│ │ 1. Channel Selection Response TLV (0x8e)             │  │
│ │    - RUID                                             │  │
│ │    - Response code (accept/decline)                   │  │
│ ├──────────────────────────────────────────────────────┤  │
│ │ 2. Spatial Reuse Config Response TLV (0xda)          │  │
│ │    - RUID                                             │  │
│ │    - Response code                                    │  │
│ ├──────────────────────────────────────────────────────┤  │
│ │ 3. Profile-2 Error Code TLV (0xbc) [optional]        │  │
│ │    - Reason code (if error)                           │  │
│ ├──────────────────────────────────────────────────────┤  │
│ │ 4. End of Message TLV (0x00)                         │  │
│ └──────────────────────────────────────────────────────┘  │
│                                                             │
│ Transmission: send_frame(buff, len)                        │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 8: Controller Receives Response                       │
│ File: src/em/channel/em_channel.cpp                        │
├────────────────────────────────────────────────────────────┤
│ Function: handle_channel_sel_rsp(buff, len)                │
│                                                             │
│ 1. Parse response TLVs                                      │
│ 2. Check response code                                      │
│ 3. Update state: em_state_ctrl_channel_selected           │
│ 4. Trigger next orchestration step                         │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 9: Orchestration - Channel Configuration              │
│ File: src/em/em.cpp                                        │
├────────────────────────────────────────────────────────────┤
│ Orchestration Type: dm_orch_type_channel_cnf              │
│                                                             │
│ State Check:                                                │
│   if (m_sm.get_state() == em_state_ctrl_channel_selected) │
│                                                             │
│ Action: Apply channel configuration                        │
│   - Controller sends operating channel report request      │
│   - Agents apply channel changes via OneWifi               │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 10: Agent Applies Channel Configuration               │
│ File: src/orch/em_orch_agent.cpp                          │
├────────────────────────────────────────────────────────────┤
│ Function: execute() - dm_orch_type_channel_pref case       │
│                                                             │
│ 1. Get channel preferences from data model                  │
│    - dm_op_class_t with type em_op_class_type_anticipated │
│                                                             │
│ 2. Convert to OneWifi format                                │
│    - Operating class → OneWifi channel                     │
│    - Channel list → OneWifi channel configuration          │
│                                                             │
│ 3. Call commit_config() to apply to OneWifi                │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 11: Data Model Commit                                 │
│ File: src/dm/dm_easy_mesh.cpp                              │
├────────────────────────────────────────────────────────────┤
│ Function: commit_config(dm_easy_mesh_t& dm, target)        │
│                                                             │
│ Target Type: em_commit_target_radio                        │
│                                                             │
│ Process:                                                    │
│ 1. Find matching radio by MAC                              │
│    for (i = 0; i < m_num_radios; i++) {                   │
│        if (memcmp(mac, m_radio[i].ruid) == 0) {           │
│            // Found matching radio                          │
│        }                                                    │
│    }                                                        │
│                                                             │
│ 2. Update radio configuration                              │
│    m_radio[i] = dm.m_radio[0];  // Copy new config        │
│                                                             │
│ 3. Get radio data from OneWifi                             │
│    rdk_wifi_radio_t *radio = get_radio_data(interface);   │
│                                                             │
│ 4. Update OneWifi radio structure                          │
│    - radio->channel = new_channel                          │
│    - radio->op_class = new_op_class                        │
│    - radio->bandwidth = new_bandwidth                      │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 12: OneWifi Integration (UNDERLAY)                    │
│ File: src/em/em_onewifi.cpp                                │
├────────────────────────────────────────────────────────────┤
│ OneWifi Subdoc Update                                       │
│                                                             │
│ 1. Create OneWifi subdoc                                    │
│    webconfig_subdoc_data_t *subdoc = m_wifi_data;         │
│                                                             │
│ 2. Update radio configuration                              │
│    subdoc->u.decoded.radios[radio_idx].channel = ch;      │
│    subdoc->u.decoded.radios[radio_idx].op_class = op;     │
│                                                             │
│ 3. Trigger OneWifi callback                                │
│    - OneWifi processes radio configuration change          │
│    - HAL layer applies channel change to hardware          │
│                                                             │
│ OneWifi Processing:                                         │
│ ┌──────────────────────────────────────────────────────┐  │
│ │ OneWifi Radio Manager                                 │  │
│ │   ↓                                                   │  │
│ │ WiFi HAL (wifi_hal_setRadioChannel)                  │  │
│ │   ↓                                                   │  │
│ │ Driver (nl80211/cfg80211)                            │  │
│ │   ↓                                                   │  │
│ │ Hardware Radio (Channel Switch)                      │  │
│ └──────────────────────────────────────────────────────┘  │
│                                                             │
│ 4. Wait for channel switch completion                      │
│    - CSA (Channel Switch Announcement) if clients exist    │
│    - Immediate switch if no clients                        │
│                                                             │
│ 5. Update current operating class in data model            │
│    - Type: em_op_class_type_current                       │
│    - Reflects actual hardware state                        │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 13: Operating Channel Report                          │
│ File: src/em/channel/em_channel.cpp                        │
├────────────────────────────────────────────────────────────┤
│ Function: send_operating_channel_report_msg()              │
│                                                             │
│ Message Type: em_msg_type_op_channel_rprt (0x8009)        │
│                                                             │
│ TLV Created:                                                │
│ ┌──────────────────────────────────────────────────────┐  │
│ │ Operating Channel Report TLV (0x8f)                   │  │
│ │   - RUID                                              │  │
│ │   - Number of operating classes                       │  │
│ │   - For each operating class:                         │  │
│ │     • Operating class number                          │  │
│ │     • Current channel                                 │  │
│ │     • TX power EIRP                                   │  │
│ └──────────────────────────────────────────────────────┘  │
│                                                             │
│ Data Source:                                                │
│   dm_op_class_t with type em_op_class_type_current        │
│                                                             │
│ Transmission: send_frame(buff, len)                        │
│ State Transition: em_state_ctrl_configured                │
└────────────────────────┬───────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────┐
│ STEP 14: Controller Receives Operating Channel Report      │
│ File: src/em/channel/em_channel.cpp                        │
├────────────────────────────────────────────────────────────┤
│ Function: handle_operating_channel_rprt(buff, len)         │
│                                                             │
│ 1. Parse Operating Channel Report TLV                      │
│ 2. Update data model with current channel                  │
│    - dm_op_class_t type: em_op_class_type_current         │
│ 3. Verify channel matches requested configuration          │
│ 4. Update network topology                                 │
│ 5. Channel configuration complete!                         │
└─────────────────────────────────────────────────────────────┘
```

---

## Key Components

### 1. Command Class: `em_cmd_set_channel_t`

**File**: `src/cmd/em_cmd_set_channel.cpp`

```cpp
em_cmd_set_channel_t::em_cmd_set_channel_t(em_cmd_params_t param, dm_easy_mesh_t& dm)
{
    m_type = em_cmd_type_set_channel;
    memcpy(&m_param, &param, sizeof(em_cmd_params_t));

    // Define 2-step orchestration
    m_num_orch_desc = 2;
    
    // Step 1: Channel Selection
    m_orch_desc[0].op = dm_orch_type_channel_sel;
    m_orch_desc[0].submit = true;
    
    // Step 2: Channel Configuration
    m_orch_desc[1].op = dm_orch_type_channel_cnf;
    m_orch_desc[1].submit = true;

    strncpy(m_name, "set_channel", strlen("set_channel") + 1);
    m_svc = em_service_type_ctrl;
    init(dm);
}
```

### 2. Protocol Class: `em_channel_t`

**File**: `inc/em_channel.h`, `src/em/channel/em_channel.cpp`

**Key Methods**:

#### Channel Preference TLV Creation
```cpp
short em_channel_t::create_channel_pref_tlv(unsigned char *buff)
{
    short len = 0;
    em_channel_pref_t *pref;
    em_channel_pref_op_class_t *pref_op_class;
    dm_easy_mesh_t *dm = get_data_model();
    
    pref = (em_channel_pref_t *)buff;
    memcpy(pref->ruid, get_radio_interface_mac(), sizeof(mac_address_t));
    pref->op_classes_num = 0;
    
    // Iterate through operating classes
    for (i = 0; i < dm->m_num_opclass; i++) {
        op_class = &dm->m_op_class[i];
        
        // Only include anticipated channel preferences
        if (op_class->m_op_class_info.id.type == em_op_class_type_anticipated) {
            pref_op_class->op_class = op_class->m_op_class_info.op_class;
            pref_op_class->num = op_class->m_op_class_info.num_channels;
            
            // Copy channel list
            for (j = 0; j < num_of_channel; j++) {
                channel_list->channel[j] = op_class->m_op_class_info.channels[j];
            }
            
            // Add preference bits (0xee = preferred)
            pref_bits = 0xee;
            
            pref->op_classes_num++;
        }
    }
    
    return len;
}
```

#### Channel Selection Request Message
```cpp
int em_channel_t::send_channel_sel_request_msg()
{
    unsigned char buff[MAX_EM_BUFF_SZ];
    em_cmdu_t *cmdu;
    em_tlv_t *tlv;
    
    // Create CMDU header
    cmdu->type = htons(em_msg_type_channel_sel_req);
    cmdu->id = htons(get_mgr()->get_next_msg_id());
    
    // Add Channel Preference TLV
    tlv->type = em_tlv_type_channel_pref;
    sz = create_channel_pref_tlv(tlv->value);
    tlv->len = htons(sz);
    
    // Add TX Power Limit TLV
    tlv->type = em_tlv_type_tx_power;
    sz = create_transmit_power_limit_tlv(tlv->value);
    tlv->len = htons(sz);
    
    // Add Spatial Reuse Request TLV
    tlv->type = em_tlv_type_spatial_reuse_req;
    sz = create_spatial_reuse_req_tlv(tlv->value);
    tlv->len = htons(sz);
    
    // Add EHT Operations TLV
    tlv->type = em_tlv_eht_operations;
    sz = create_eht_operations_tlv(tlv->value);
    tlv->len = htons(sz);
    
    // End of message
    tlv->type = em_tlv_type_eom;
    tlv->len = 0;
    
    // Send frame
    return send_frame(buff, len);
}
```

### 3. Data Model: `dm_easy_mesh_t`

**File**: `inc/dm_easy_mesh.h`, `src/dm/dm_easy_mesh.cpp`

**Operating Class Storage**:
```cpp
class dm_easy_mesh_t {
public:
    unsigned int m_num_opclass;
    dm_op_class_t m_op_class[EM_MAX_OPCLASS];
    
    // Operating class types:
    // - em_op_class_type_capability: Radio capabilities
    // - em_op_class_type_current: Currently operating channel
    // - em_op_class_type_anticipated: Controller's channel preference
    // - em_op_class_type_scan_param: Scan parameters
};
```

**Operating Class Structure**:
```cpp
typedef struct {
    em_op_class_id_t id;        // RUID + type + op_class
    unsigned int op_class;       // Operating class number
    unsigned int channel;        // Current/preferred channel
    unsigned int num_channels;   // Number of channels in list
    unsigned int channels[EM_MAX_CHANNELS_IN_LIST];  // Channel list
    unsigned int max_tx_power;   // Max TX power EIRP
} em_op_class_info_t;

typedef enum {
    em_op_class_type_none,
    em_op_class_type_capability,
    em_op_class_type_current,
    em_op_class_type_anticipated,
    em_op_class_type_scan_param
} em_op_class_type_t;
```

### 4. Orchestration Types

**File**: `inc/em_base.h`

```cpp
typedef enum {
    dm_orch_type_none,
    dm_orch_type_db_cfg,
    dm_orch_type_em_update,
    dm_orch_type_net_ssid_update,
    dm_orch_type_channel_pref,      // Query channel preferences
    dm_orch_type_channel_sel,       // Select channel
    dm_orch_type_channel_cnf,       // Configure channel
    dm_orch_type_channel_sel_resp,  // Channel selection response
    dm_orch_type_channel_scan_req,  // Request channel scan
    dm_orch_type_channel_scan_res,  // Channel scan results
    // ... more types
} dm_orch_type_t;
```

---

## Message Types and TLVs

### Channel Management Messages

| Message Type | Value | Direction | Description |
|--------------|-------|-----------|-------------|
| Channel Preference Query | 0x8005 | Controller → Agent | Query channel preferences |
| Channel Preference Report | 0x8006 | Agent → Controller | Report channel preferences |
| Channel Selection Request | 0x8007 | Controller → Agent | Request channel change |
| Channel Selection Response | 0x8008 | Agent → Controller | Accept/decline channel change |
| Operating Channel Report | 0x8009 | Agent → Controller | Report current operating channel |
| Channel Scan Request | 0x801a | Controller → Agent | Request channel scan |
| Channel Scan Report | 0x801b | Agent → Controller | Report scan results |

### Channel Management TLVs

| TLV Type | Value | Description | Structure |
|----------|-------|-------------|-----------|
| Channel Preference | 0x8b | Channel preferences per operating class | RUID + op_classes[] |
| Radio Operation Restriction | 0x8c | Restricted channels | RUID + restrictions[] |
| Transmit Power Limit | 0x8d | TX power limit | RUID + power_limit |
| Channel Selection Response | 0x8e | Response to channel selection | RUID + response_code |
| Operating Channel Report | 0x8f | Current operating channel | RUID + op_class + channel |
| Channel Scan Capabilities | 0xa5 | Scan capabilities | Capabilities flags |
| Channel Scan Request | 0xa6 | Scan request parameters | RUID + op_classes[] |
| Channel Scan Result | 0xa7 | Scan results | RUID + neighbors[] |
| Spatial Reuse Request | 0xd8 | Spatial reuse config | BSS color, OBSS PD |
| Spatial Reuse Report | 0xd9 | Spatial reuse report | BSS color, OBSS PD |
| Spatial Reuse Config Response | 0xda | Spatial reuse response | Response code |
| EHT Operations | 0xe7 | WiFi 7 EHT operations | EHT parameters |

---

## Orchestration Flow

### Channel Set Command Orchestration

```
em_cmd_set_channel_t
    │
    ├─► Orchestration Step 1: dm_orch_type_channel_sel
    │   │
    │   ├─► State Check: em_state_ctrl_channel_queried
    │   │
    │   ├─► Action: send_channel_sel_request_msg()
    │   │   │
    │   │   ├─► Create Channel Preference TLV
    │   │   ├─► Create TX Power Limit TLV
    │   │   ├─► Create Spatial Reuse Request TLV
    │   │   ├─► Create EHT Operations TLV
    │   │   └─► Send to Agent(s)
    │   │
    │   └─► Wait for Response
    │       │
    │       └─► handle_channel_sel_rsp()
    │           └─► State: em_state_ctrl_channel_selected
    │
    └─► Orchestration Step 2: dm_orch_type_channel_cnf
        │
        ├─► State Check: em_state_ctrl_channel_selected
        │
        ├─► Agent applies configuration
        │   │
        │   ├─► commit_config() → OneWifi
        │   │
        │   └─► send_operating_channel_report_msg()
        │
        └─► Controller receives report
            │
            └─► State: em_state_ctrl_configured
```

### State Machine Transitions

```
Controller States:
em_state_ctrl_topo_synchronized
    ↓ (channel preference query)
em_state_ctrl_channel_queried
    ↓ (channel selection request)
em_state_ctrl_channel_selected
    ↓ (channel configuration applied)
em_state_ctrl_configured

Agent States:
em_state_agent_configured
    ↓ (receive channel selection request)
em_state_agent_channel_selected
    ↓ (apply configuration)
em_state_agent_configured
```

---

## OneWifi Integration

### OneWifi Data Structure

```cpp
typedef struct {
    char name[32];                  // Radio interface name (e.g., "wlan0")
    unsigned int channel;           // Current channel
    unsigned int op_class;          // Operating class
    wifi_channelBandwidth_t bandwidth;  // Channel bandwidth
    unsigned int transmit_power_limit;  // TX power limit
    bool enabled;                   // Radio enabled
    // ... more fields
} rdk_wifi_radio_t;

typedef struct {
    unsigned int num_radios;
    rdk_wifi_radio_t radios[MAX_NUM_RADIOS];
    // ... more fields
} webconfig_subdoc_data_t;
```

### Commit Configuration Flow

```cpp
int dm_easy_mesh_t::commit_config(dm_easy_mesh_t& dm, em_commit_target_t target)
{
    if (target.type == em_commit_target_radio) {
        mac_address_t mac;
        string_to_macbytes(target.params, mac);
        
        // Find matching radio
        dm_radio_t *radio = dm.get_radio(mac);
        if (radio != NULL) {
            for (i = 0; i < m_num_radios; i++) {
                if (memcmp(mac, m_radio[i].get_radio_info()->id.mac) == 0) {
                    // Update radio configuration
                    m_radio[i] = dm.m_radio[0];
                    
                    // Get OneWifi radio data
                    rdk_wifi_radio_t *onewifi_radio = get_radio_data(&m_radio[i].m_radio_info.id);
                    
                    // Apply channel configuration
                    onewifi_radio->channel = m_radio[i].m_radio_info.channel;
                    onewifi_radio->op_class = m_radio[i].m_radio_info.op_class;
                    
                    // Trigger OneWifi update
                    // OneWifi callback processes the change
                    // HAL layer applies to hardware
                    
                    break;
                }
            }
        }
    }
    return 0;
}
```

### OneWifi Callback Processing

```
EasyMesh Channel Update
    ↓
OneWifi Subdoc Update
    ↓
OneWifi Radio Manager
    ↓
wifi_hal_setRadioChannel(radio_index, channel)
    ↓
nl80211/cfg80211 Driver
    ↓
Hardware Channel Switch
    ↓
CSA (if clients exist) / Immediate (if no clients)
    ↓
Channel Switch Complete
    ↓
Update em_op_class_type_current in EasyMesh data model
```

---

## Code Flow Diagrams

### Channel Preference Query Flow

```
Controller                          Agent
    │                                 │
    │  Channel Pref Query (0x8005)   │
    ├────────────────────────────────►│
    │                                 │
    │                                 ├─► Read dm_op_class_t
    │                                 │   (type: capability)
    │                                 │
    │                                 ├─► Create Channel Pref TLV
    │                                 │   (RUID, op_class, channels)
    │                                 │
    │  Channel Pref Report (0x8006)  │
    │◄────────────────────────────────┤
    │                                 │
    ├─► Store preferences             │
    │   (type: anticipated)           │
    │                                 │
```

### Channel Selection and Configuration Flow

```
Controller                          Agent                    OneWifi
    │                                 │                         │
    │  Channel Sel Request (0x8007)   │                         │
    ├────────────────────────────────►│                         │
    │  - Channel Pref TLV             │                         │
    │  - TX Power TLV                 │                         │
    │  - Spatial Reuse TLV            │                         │
    │  - EHT Operations TLV           │                         │
    │                                 │                         │
    │                                 ├─► Validate preferences  │
    │                                 │                         │
    │                                 ├─► Determine response    │
    │                                 │   (accept/decline)      │
    │                                 │                         │
    │  Channel Sel Response (0x8008)  │                         │
    │◄────────────────────────────────┤                         │
    │  - Response code: ACCEPT        │                         │
    │                                 │                         │
    ├─► State: channel_selected       │                         │
    │                                 │                         │
    │                                 ├─► commit_config()       │
    │                                 │                         │
    │                                 ├────────────────────────►│
    │                                 │   Update radio config   │
    │                                 │                         │
    │                                 │                         ├─► HAL
    │                                 │                         │   Channel
    │                                 │                         │   Switch
    │                                 │                         │
    │                                 │   Channel switched      │
    │                                 │◄────────────────────────┤
    │                                 │                         │
    │                                 ├─► Update current op_class
    │                                 │   (type: current)       │
    │                                 │                         │
    │  Operating Channel Report       │                         │
    │  (0x8009)                       │                         │
    │◄────────────────────────────────┤                         │
    │  - Current op_class             │                         │
    │  - Current channel              │                         │
    │                                 │                         │
    ├─► Verify configuration          │                         │
    │                                 │                         │
    ├─► State: configured             │                         │
    │                                 │                         │
```

### Channel Scan Flow

```
Controller                          Agent                    OneWifi
    │                                 │                         │
    │  Channel Scan Request (0x801a)  │                         │
    ├────────────────────────────────►│                         │
    │  - RUID                         │                         │
    │  - Op classes to scan           │                         │
    │  - Channels to scan             │                         │
    │                                 │                         │
    │                                 ├────────────────────────►│
    │                                 │   Trigger scan          │
    │                                 │                         │
    │                                 │                         ├─► HAL
    │                                 │                         │   Scan
    │                                 │                         │
    │                                 │   Scan results          │
    │                                 │◄────────────────────────┤
    │                                 │   - Neighbors           │
    │                                 │   - RSSI                │
    │                                 │   - Channel utilization │
    │                                 │                         │
    │                                 ├─► Store in              │
    │                                 │   dm_scan_result_t      │
    │                                 │                         │
    │  Channel Scan Report (0x801b)   │                         │
    │◄────────────────────────────────┤                         │
    │  - Timestamp TLV                │                         │
    │  - Channel Scan Result TLV(s)   │                         │
    │    • RUID, op_class, channel    │                         │
    │    • Utilization, noise         │                         │
    │    • Neighbor list              │                         │
    │                                 │                         │
    ├─► Analyze scan results          │                         │
    │                                 │                         │
    ├─► Make channel selection        │                         │
    │   decision                      │                         │
    │                                 │                         │
```

---

## 9. Identified Gaps and Issues

### Logic Gap: Single Operating Class Restriction & Uninitialized Data Bug
*   **Location**: `src/em/channel/em_channel.cpp` - `em_channel_t::handle_channel_pref_tlv`
*   **Issue**: 
    1.  **Single Class Restriction**: The code iterates through received operating classes but explicitly `break`s after finding the first match for the current band, ignoring any subsequent valid operating classes (e.g., alternative bandwidths).
    2.  **Uninitialized Data Bug**: The code sets `op_class->num = 1` *before* the search loop. If no matching operating class is found (e.g., band mismatch), the function returns with `num=1` but `op_class_info[0]` remains uninitialized (garbage data). **Critical**: The caller `handle_channel_sel_req` declares `op_class` on the stack without initialization and immediately broadcasts this garbage data via `io_process` if the TLV handler fails to populate it.
*   **Impact**: 
    *   **Suboptimal Selection**: Agent cannot choose between multiple valid operating classes provided by the Controller.
    *   **Stability Risk**: Processing uninitialized garbage data as a valid operating class configuration could lead to undefined behavior, crashes, or invalid radio configuration attempts by the event consumer.

### Critical: Validation Failures are Ignored
*   **Location**: `src/em/channel/em_channel.cpp` - `send_channel_scan_request_msg`, `send_channel_sel_request_msg`, `send_channel_sel_response_msg`
*   **Issue**: The code checks for validation errors using `em_msg_t::validate` but explicitly disables the failure handling (the `return -1` is commented out).
*   **Impact**: Invalid messages (missing mandatory TLVs or incorrect lengths) are still sent to the network, potentially causing protocol violations.

### Validation Logic Limitation
*   **Location**: `src/em/em_msg.cpp` - `em_msg_t::validate`
*   **Issue**: The validation logic for TLV length checks if the actual length is less than a fixed minimum expected length. This does not correctly validate variable-length TLVs where the length should be calculated based on the content (e.g., number of array elements).

---

## Summary

### Key Takeaways

1. **Layered Architecture**: Clean separation between CLI, command, orchestration, protocol, data model, and OneWifi layers

2. **Two-Step Orchestration**: Channel management uses a two-step process:
   - **Step 1**: Channel Selection (query preferences, send request)
   - **Step 2**: Channel Configuration (apply changes, report results)

3. **Operating Class Types**: The data model uses different operating class types to track:
   - **Capability**: What the radio supports
   - **Current**: What channel is currently in use
   - **Anticipated**: What the controller wants to configure
   - **Scan Param**: Parameters for scanning

4. **State Machine**: Proper state transitions ensure correct protocol flow

5. **OneWifi Integration**: Seamless integration with OneWifi for actual hardware configuration

6. **Multi-AP Compliance**: Full implementation of Multi-AP channel management protocol including:
   - Channel preference reporting
   - Channel selection with spatial reuse
   - WiFi 6/7 features (spatial reuse, EHT operations)
   - Channel scanning

### File Reference Quick Guide

| Component | Header File | Implementation File |
|-----------|-------------|---------------------|
| Channel Protocol | `inc/em_channel.h` | `src/em/channel/em_channel.cpp` |
| Set Channel Command | `inc/em_cmd_set_channel.h` | `src/cmd/em_cmd_set_channel.cpp` |
| Data Model | `inc/dm_easy_mesh.h` | `src/dm/dm_easy_mesh.cpp` |
| Orchestration | `inc/em_orch.h` | `src/orch/em_orch_ctrl.cpp`, `src/orch/em_orch_agent.cpp` |
| Protocol Execution | `inc/em.h` | `src/em/em.cpp` |
| OneWifi Integration | `inc/dm_easy_mesh.h` | `src/em/em_onewifi.cpp` |

---

**Document Version**: 1.0  
**Date**: 2025-11-20  
**Author**: AI Analysis of unified-wifi-mesh channel management
