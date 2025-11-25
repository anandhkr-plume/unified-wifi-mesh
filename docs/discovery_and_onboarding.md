# Agent and Controller Discovery & Onboarding in Unified-WiFi-Mesh

## Overview

The unified-wifi-mesh implementation supports two discovery mechanisms:
1. **IEEE 1905.1 Topology Discovery** - For discovering neighboring devices
2. **Auto-Configuration Discovery** - For discovering and onboarding agents to controllers

## 1. Discovery Mechanisms

### 1.1 Topology Discovery (IEEE 1905.1)

**Purpose**: Discovers neighboring IEEE 1905.1 devices and builds network topology.

**Key Functions**:
- `em_discovery_t::create_topo_discovery_msg()` - Creates topology discovery messages
- `em_discovery_t::create_topo_query_msg()` - Creates topology query messages
- `em_configuration_t::send_topology_query_msg()` - Sends topology queries
- `em_configuration_t::send_topology_response_msg()` - Sends topology responses
- `em_configuration_t::handle_topology_response()` - Handles received topology responses
- `em_configuration_t::handle_topology_notification()` - Handles topology notifications

**Message Flow**:
1. **Topology Discovery** (multicast, periodic):
   - Devices broadcast `em_msg_type_topo_disc` messages periodically
   - Contains AL MAC address and interface MAC address
   - Sent to multicast address `01:80:c2:00:00:13`

2. **Topology Query**:
   - Controller/Agent sends `em_msg_type_topo_query` to discover detailed topology
   - Query includes vendor-specific TLV

3. **Topology Response**:
   - Receiving device responds with `em_msg_type_topo_resp`
   - Contains operational BSS information, radio capabilities, etc.

4. **Topology Notification**:
   - Sent when topology changes (e.g., client association/disassociation)
   - `em_configuration_t::send_topology_notification_by_client()`

**File Locations**:
- `src/em/disc/em_discovery.cpp` - Discovery message creation
- `src/em/config/em_configuration.cpp` - Topology query/response handling

### 1.2 Auto-Configuration Discovery

**Purpose**: Allows agents to discover and onboard to controllers.

**Key Functions**:

#### Agent Side (Initiator):
- `em_configuration_t::create_autoconfig_search_msg()` - Creates autoconfig search message
- `em_configuration_t::handle_state_config_none()` - Triggers autoconfig search when in unconfigured state
- `em_configuration_t::handle_autoconfig_resp()` - Handles autoconfig response from controller
- `em_configuration_t::create_autoconfig_wsc_m1_msg()` - Creates WSC M1 message
- `em_configuration_t::handle_autoconfig_wsc_m2()` - Handles WSC M2 message

#### Controller Side (Responder):
- `em_configuration_t::handle_autoconfig_search()` - Handles autoconfig search from agent
- `em_configuration_t::create_autoconfig_resp_msg()` - Creates autoconfig response
- `em_configuration_t::handle_autoconfig_wsc_m1()` - Handles WSC M1 from agent
- `em_configuration_t::create_autoconfig_wsc_m2_msg()` - Creates WSC M2 response

**File Location**: `src/em/config/em_configuration.cpp`

## 2. Discovery Flow

### 2.1 Agent Discovery Flow (Standard WSC)

```
Agent (Unconfigured)                    Controller
     |                                        |
     |--- Autoconfig Search (multicast) ---->|
     |   - AL MAC Address                    |
     |   - SearchedRole (Registrar)         |
     |   - AutoconfigFreqBand               |
     |   - SupportedService (Agent)          |
     |   - SearchedService (Controller)      |
     |   - Profile (Multi-AP Profile 3)      |
     |                                        |
     |<-- Autoconfig Response ----------------|
     |   - SupportedRole                    |
     |   - SupportedFreqBand                |
     |   - SupportedService (Controller)     |
     |   - 1905 Layer Security Cap          |
     |   - Profile (Multi-AP Profile 3)      |
     |   - Controller Capability            |
     |                                        |
     |--- WSC M1 (unicast) ---------------->|
     |   - WSC TLV                          |
     |   - AP Radio Basic Cap               |
     |                                        |
     |<-- WSC M2 ----------------------------|
     |   - WSC TLV (credentials)            |
     |   - AP Radio Basic Cap               |
     |                                        |
     |--- OneWifi Configuration ------------>|
     |   (via io_process)                    |
     |                                        |
     |--- Topology Query ------------------->|
     |                                        |
     |<-- Topology Response -----------------|
     |   - Operational BSS Info             |
     |   - Radio Capabilities               |
     |                                        |
     | State: em_state_agent_topo_synchronized|
```

### 2.2 DPP/EasyConnect Discovery Flow

When DPP (Device Provisioning Protocol) is enabled:

```
Agent (DPP Enrollee)                    Controller (DPP Configurator)
     |                                        |
     |--- Autoconfig Search (with DPP Chirp) ->|
     |   - DPP Chirp Value TLV                |
     |                                        |
     |<-- Autoconfig Response (with DPP Chirp) -|
     |   - DPP Chirp Value TLV                |
     |                                        |
     |--- DPP Peer Discovery Request -------->|
     |                                        |
     |<-- DPP Peer Discovery Response --------|
     |                                        |
     |--- 1905 4-Way Handshake -------------->|
     |   (Secure 1905 layer)                  |
     |                                        |
     | State: em_state_agent_1905_securing   |
```

**Key Functions for DPP**:
- `ec_enrollee_t::send_autoconf_search_chirp()` - Sends autoconfig search with DPP chirp
- `ec_ctrl_configurator_t::handle_autoconf_search_chirp()` - Handles DPP chirp in search
- `ec_enrollee_t::handle_autoconf_response_chirp()` - Handles DPP chirp in response
- `ec_manager_t::start_secure_1905_layer()` - Initiates 1905 layer security

## 3. Agent Onboarding Process

### 3.1 State Machine Flow

**Agent States During Onboarding**:
1. `em_state_agent_unconfigured` - Initial state
2. `em_state_agent_autoconfig_rsp_pending` - Waiting for autoconfig response
3. `em_state_agent_wsc_m2_pending` - Waiting for WSC M2 (if WSC used)
4. `em_state_agent_1905_securing` - Securing 1905 layer (if DPP used)
5. `em_state_agent_owconfig_pending` - Configuring OneWifi
6. `em_state_agent_topo_synchronized` - Topology synchronized
7. `em_state_agent_configured` - Fully configured

### 3.2 Onboarding Steps

#### Step 1: Device Initialization
- **Function**: `em_cmd_type_dev_init` command
- **State**: Sets agent to `em_state_agent_unconfigured`
- **Location**: `src/em/em.cpp` (line 108-110)

#### Step 2: Auto-Configuration Search
- **Trigger**: `em_configuration_t::handle_state_config_none()` (line 5349)
- **Function**: `em_configuration_t::create_autoconfig_search_msg()` (line 3564)
- **Message**: `em_msg_type_autoconf_search`
- **Destination**: Multicast `01:80:c2:00:00:13`
- **State Transition**: `em_state_agent_autoconfig_rsp_pending`
- **TLVs Included**:
  - AL MAC Address
  - SearchedRole (0x00 = Registrar)
  - AutoconfigFreqBand
  - SupportedService (Agent)
  - SearchedService (Controller)
  - Profile (Multi-AP Profile 3)
  - Optional: DPP Chirp Value TLV (if DPP enabled)

#### Step 3: Auto-Configuration Response
- **Handler**: `em_configuration_t::handle_autoconfig_resp()` (line 5042)
- **Message**: `em_msg_type_autoconf_resp`
- **Processing**:
  - Validates message
  - Extracts peer profile type
  - If DPP chirp present: forwards to EC manager
  - If DPP onboarding: starts 1905 security
  - Otherwise: creates WSC M1 message

#### Step 4A: WSC Exchange (Standard Onboarding)
- **M1 Creation**: `em_configuration_t::create_autoconfig_wsc_m1_msg()` (line 3215)
- **M1 Handler**: `em_configuration_t::handle_autoconfig_wsc_m1()` (line 4979)
- **M2 Handler**: `em_configuration_t::handle_autoconfig_wsc_m2()` (line 3612)
- **State**: `em_state_agent_wsc_m2_pending`
- **WSC M1 Contains**:
  - WSC TLV (Wi-Fi credentials)
  - AP Radio Basic Capability
  - AP Radio Advanced Capability
- **WSC M2 Contains**:
  - WSC TLV (configuration)
  - AP Radio Basic Capability

#### Step 4B: DPP/EasyConnect (Secure Onboarding)
- **DPP Chirp**: Embedded in autoconfig search/response
- **Peer Discovery**: DPP Peer Discovery Request/Response frames
- **1905 Security**: `ec_manager_t::start_secure_1905_layer()`
- **State**: `em_state_agent_1905_securing`
- **Files**: `src/em/prov/easyconnect/ec_enrollee.cpp`, `ec_ctrl_configurator.cpp`

#### Step 5: OneWifi Configuration
- **Event**: `em_bus_event_type_m2_tx` (for WSC) or DPP config completion
- **Handler**: `em_agent_t::handle_onewifi_private_cb()` (line 816)
- **State**: `em_state_agent_owconfig_pending`
- **Process**: Updates OneWifi with SSID, credentials, radio config

#### Step 6: Topology Synchronization
- **Query**: `em_configuration_t::send_topology_query_msg()` (line 297)
- **Response**: `em_configuration_t::handle_topology_response()` (line 1580)
- **State**: `em_state_agent_topo_synchronized`
- **Contains**: Operational BSS info, radio capabilities, neighbor information

#### Step 7: Configuration Complete
- **State**: `em_state_agent_configured`
- **Agent is now fully onboarded and operational**

## 4. Controller Discovery Handling

### 4.1 Controller Receives Autoconfig Search

**Function**: `em_configuration_t::handle_autoconfig_search()` (line 5101)

**Process**:
1. Validates message
2. Extracts frequency band and AL MAC address
3. Creates or retrieves data model for the agent
4. Checks for DPP chirp TLV
5. Creates autoconfig response message
6. Sends response to agent

**Key Code** (line 5128-5143):
```cpp
// Extract AL MAC from search message
if (em_msg_t(...).get_al_mac_address(al_mac) == false) {
    return -1;
}

// Create or get data model
if ((dm = get_data_model(GLOBAL_NET_ID, al_mac)) == NULL) {
    dm = create_data_model(GLOBAL_NET_ID, &intf, profile);
}

// Create and send response
sz = create_autoconfig_resp_msg(msg, band, al_mac, ntohs(cmdu->id));
send_frame(msg, sz);
```

## 5. Key Data Structures

### 5.1 Autoconfig Search Message
- **Message Type**: `em_msg_type_autoconf_search`
- **TLVs**:
  - `em_tlv_type_al_mac_address` - Agent's AL MAC
  - `em_tlv_type_searched_role` - Searched role (Registrar = 0x00)
  - `em_tlv_type_autoconf_freq_band` - Frequency band
  - `em_tlv_type_supported_service` - Service type (Agent)
  - `em_tlv_type_searched_service` - Searched service (Controller)
  - `em_tlv_type_profile` - Multi-AP profile
  - `em_tlv_type_dpp_chirp_value` - Optional DPP chirp

### 5.2 Autoconfig Response Message
- **Message Type**: `em_msg_type_autoconf_resp`
- **TLVs**:
  - `em_tlv_type_supported_role` - Supported role
  - `em_tlv_type_supported_freq_band` - Frequency band
  - `em_tlv_type_supported_service` - Service type (Controller)
  - `em_tlv_type_1905_layer_security_cap` - Security capabilities
  - `em_tlv_type_profile` - Multi-AP profile
  - `em_tlv_type_ctrl_cap` - Controller capabilities
  - `em_tlv_type_dpp_chirp_value` - Optional DPP chirp

## 6. Functions Summary

### Discovery Functions
| Function | Purpose | Location |
|----------|---------|----------|
| `create_topo_discovery_msg()` | Creates topology discovery message | `src/em/disc/em_discovery.cpp:95` |
| `create_topo_query_msg()` | Creates topology query message | `src/em/disc/em_discovery.cpp:43` |
| `send_topology_query_msg()` | Sends topology query | `src/em/config/em_configuration.cpp:297` |
| `send_topology_response_msg()` | Sends topology response | `src/em/config/em_configuration.cpp:903` |
| `handle_topology_response()` | Handles topology response | `src/em/config/em_configuration.cpp:1580` |
| `handle_topology_notification()` | Handles topology notification | `src/em/config/em_configuration.cpp:1487` |

### Auto-Configuration Functions
| Function | Purpose | Location |
|----------|---------|----------|
| `create_autoconfig_search_msg()` | Creates autoconfig search | `src/em/config/em_configuration.cpp:3564` |
| `create_autoconfig_resp_msg()` | Creates autoconfig response | `src/em/config/em_configuration.cpp:3410` |
| `handle_autoconfig_search()` | Handles autoconfig search (Controller) | `src/em/config/em_configuration.cpp:5101` |
| `handle_autoconfig_resp()` | Handles autoconfig response (Agent) | `src/em/config/em_configuration.cpp:5042` |
| `handle_state_config_none()` | Triggers autoconfig search | `src/em/config/em_configuration.cpp:5349` |

### WSC Functions
| Function | Purpose | Location |
|----------|---------|----------|
| `create_autoconfig_wsc_m1_msg()` | Creates WSC M1 message | `src/em/config/em_configuration.cpp:3215` |
| `create_autoconfig_wsc_m2_msg()` | Creates WSC M2 message | `src/em/config/em_configuration.cpp:3332` |
| `handle_autoconfig_wsc_m1()` | Handles WSC M1 (Controller) | `src/em/config/em_configuration.cpp:4979` |
| `handle_autoconfig_wsc_m2()` | Handles WSC M2 (Agent) | `src/em/config/em_configuration.cpp:3612` |

### DPP/EasyConnect Functions
| Function | Purpose | Location |
|----------|---------|----------|
| `send_autoconf_search_ext_chirp()` | Sends autoconfig search with DPP chirp | `src/em/config/em_configuration.cpp:3543` |
| `send_autoconf_search_resp_ext_chirp()` | Sends autoconfig response with DPP chirp | `src/em/config/em_configuration.cpp:3526` |
| `ec_enrollee_t::send_autoconf_search_chirp()` | Enrollee sends DPP chirp | `src/em/prov/easyconnect/ec_enrollee.cpp:1858` |
| `ec_ctrl_configurator_t::handle_autoconf_search_chirp()` | Configurator handles DPP chirp | `src/em/prov/easyconnect/ec_ctrl_configurator.cpp:318` |
| `ec_manager_t::start_secure_1905_layer()` | Starts 1905 layer security | `src/em/prov/easyconnect/ec_manager.cpp` |

## 7. Message Routing

### 7.1 Agent Message Routing
**Function**: `em_agent_t::get_em_for_msg()` (line 1395)
- Routes `em_msg_type_autoconf_resp` to matching radio EM based on frequency band
- Routes `em_msg_type_autoconf_wsc` to matching EM
- Routes `em_msg_type_topo_query` to AL EM

### 7.2 Controller Message Routing
**Function**: `em_ctrl_t::get_em_for_msg()` (line 622)
- Routes `em_msg_type_autoconf_search` to AL EM
- Creates data model for new agents
- Routes messages to appropriate EM instances

## 8. Configuration Files

- **Discovery**: `src/em/disc/em_discovery.cpp`, `inc/em_discovery.h`
- **Configuration**: `src/em/config/em_configuration.cpp`, `inc/em_configuration.h`
- **DPP/EasyConnect**: `src/em/prov/easyconnect/`
- **Agent**: `src/agent/em_agent.cpp`
- **Controller**: `src/ctrl/em_ctrl.cpp`

## 9. Notes

1. **Multicast Address**: All discovery messages use `01:80:c2:00:00:13` (IEEE 1905.1 multicast)
2. **Profile Type**: Uses Multi-AP Profile 3 (EasyMesh)
3. **DPP Support**: Optional secure onboarding via Device Provisioning Protocol
4. **WSC Support**: Standard Wi-Fi Simple Configuration for credential exchange
5. **State Management**: State machine ensures proper sequencing of discovery and onboarding steps

