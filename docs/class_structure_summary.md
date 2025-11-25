# Unified WiFi Mesh - Class Structure Summary

## Overview

This document provides a high-level summary of the class structure for EM Agent and EM Controller in the Unified WiFi Mesh system.

## Core Architecture

The system follows a layered architecture:

```
┌─────────────────────────────────────────┐
│         Application Layer               │
│  (onewifi_em_cli, Agent, Controller)   │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│         Manager Layer                   │
│      (em_mgr_t, em_agent_t,            │
│       em_ctrl_t)                        │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│         Protocol Layer                  │
│      (em_t - EasyMesh Protocol)         │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│         Data Model Layer                │
│  (dm_easy_mesh_t, dm_easy_mesh_agent_t,│
│   dm_easy_mesh_ctrl_t)                  │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│         Command Layer                   │
│  (em_cmd_t, em_cmd_exec_t,             │
│   em_orch_t)                            │
└─────────────────────────────────────────┘
```

## Key Classes for EM Agent

### 1. `em_agent_t` (extends `em_mgr_t`)
**Purpose**: Main agent manager class

**Key Responsibilities**:
- Manages single data model instance
- Handles OneWifi callbacks (private, radio, mesh_sta)
- Processes bus events from OneWifi
- Executes agent-specific commands
- Manages action frame transmission
- Handles device initialization and configuration

**Key Members**:
- `dm_easy_mesh_agent_t m_data_model`: Single data model
- `em_orch_agent_t *m_orch`: Agent orchestration
- `em_cmd_agent_t *m_agent_cmd`: Command executor
- `bus_handle_t m_bus_hdl`: Bus communication handle

**Key Methods**:
- `handle_onewifi_private_cb()`: Processes OneWifi private subdoc
- `handle_onewifi_radio_cb()`: Processes radio configuration
- `handle_vap_config()`: Handles VAP configuration
- `handle_radio_config()`: Handles radio configuration
- `send_action_frame()`: Sends IEEE 1905 action frames

### 2. `dm_easy_mesh_agent_t` (extends `dm_easy_mesh_t`)
**Purpose**: Agent-specific data model

**Key Responsibilities**:
- Stores agent's network configuration
- Translates OneWifi subdocs to data model
- Manages agent AL interface information

**Key Methods**:
- `translate_and_decode_onewifi_subdoc()`: Converts OneWifi JSON to data model

### 3. `em_orch_agent_t` (extends `em_orch_t`)
**Purpose**: Agent orchestration

**Key Responsibilities**:
- Executes commands on agent
- Handles command completion
- Manages agent state transitions

### 4. `em_cmd_agent_t` (extends `em_cmd_exec_t`)
**Purpose**: Agent command execution

**Key Responsibilities**:
- Validates commands for agent
- Executes commands via bus events
- Handles agent-specific command logic

## Key Classes for EM Controller

### 1. `em_ctrl_t` (extends `em_mgr_t`)
**Purpose**: Main controller manager class

**Key Responsibilities**:
- Manages multiple data models (one per agent)
- Orchestrates network-wide operations
- Handles SSID and channel updates
- Manages device onboarding and removal
- Coordinates agent operations

**Key Members**:
- `dm_easy_mesh_ctrl_t m_data_model`: Controller data model
- `em_orch_ctrl_t *m_orch`: Controller orchestration
- `em_cmd_ctrl_t *m_ctrl_cmd`: Command executor
- `bus_handle_t m_bus_hdl`: Bus communication handle

**Key Methods**:
- `handle_set_ssid_list()`: Processes SSID update requests
- `handle_set_channel_list()`: Processes channel update requests
- `handle_remove_device()`: Removes device from network
- `update_network_topology()`: Updates network topology
- `init_network_topology()`: Initializes network topology

### 2. `dm_easy_mesh_ctrl_t` (extends `dm_easy_mesh_t`)
**Purpose**: Controller-specific data model

**Key Responsibilities**:
- Manages list of agent data models
- Analyzes configuration changes
- Creates commands for network-wide updates

**Key Members**:
- `dm_easy_mesh_list_t m_data_model_list`: List of agent data models

**Key Methods**:
- `analyze_set_ssid()`: Analyzes SSID changes, creates commands
- `analyze_set_channel()`: Analyzes channel changes, creates commands
- `get_first_dm()`: Gets first agent data model
- `get_next_dm()`: Gets next agent data model
- `create_data_model()`: Creates new agent data model
- `delete_data_model()`: Removes agent data model

### 3. `em_orch_ctrl_t` (extends `em_orch_t`)
**Purpose**: Controller orchestration

**Key Responsibilities**:
- Executes orchestration descriptors
- Manages command execution order
- Handles data model updates
- Coordinates multi-device operations

**Key Methods**:
- `execute()`: Executes orchestration descriptors
- `handle_dm_update()`: Handles data model updates

### 4. `em_cmd_ctrl_t` (extends `em_cmd_exec_t`)
**Purpose**: Controller command execution

**Key Responsibilities**:
- Analyzes bus events
- Creates command objects
- Executes commands across network

**Key Methods**:
- `analyze_bus_event()`: Analyzes incoming bus events
- `analyze_set_ssid()`: Analyzes SSID update events
- `analyze_set_channel()`: Analyzes channel update events

## Shared Base Classes

### 1. `em_mgr_t` (Abstract Base Class)
**Purpose**: Base manager class

**Key Responsibilities**:
- Manages EM nodes (em_t instances)
- Handles event queue
- Provides node lifecycle management
- Defines interface for derived classes

**Key Methods**:
- `create_node()`: Creates new EM node
- `delete_node()`: Removes EM node
- `get_node_by_freq_band()`: Gets node by frequency band
- `get_al_node()`: Gets AL node
- `proto_process()`: Processes protocol messages

### 2. `em_t` (Protocol Implementation)
**Purpose**: EasyMesh protocol implementation

**Key Responsibilities**:
- Implements IEEE 1905.1 protocol
- Manages protocol state machine
- Handles CMDU messages
- Manages encryption/decryption

**Key Members**:
- `dm_easy_mesh_t* m_data_model`: Associated data model
- `em_mgr_t *m_mgr`: Parent manager
- `em_sm_t m_sm`: State machine
- `em_crypto_t m_crypto`: Cryptographic operations

**Key Methods**:
- `proto_run()`: Starts protocol
- `send_frame()`: Sends protocol frames
- `push_event()`: Pushes event to queue
- `get_state()`: Gets current state

### 3. `dm_easy_mesh_t` (Abstract Base Class)
**Purpose**: Base data model class

**Key Responsibilities**:
- Stores network configuration
- Manages device, network, radio, BSS, STA lists
- Provides encoding/decoding of configuration

**Key Members**:
- `dm_device_t m_device`: Device information
- `dm_network_t m_network`: Network information
- `dm_network_ssid_list_t m_network_ssid_list`: SSID list
- `dm_radio_list_t m_radio_list`: Radio list
- `dm_bss_list_t m_bss_list`: BSS list
- `dm_sta_list_t m_sta_list`: STA list
- `dm_op_class_list_t m_op_class_list`: Operating class list

**Key Methods**:
- `decode_config()`: Decodes JSON configuration
- `encode_config()`: Encodes to JSON
- `get_network_ssid_list()`: Gets SSID list
- `get_op_class_info()`: Gets operating class info

### 4. `em_cmd_t` (Abstract Base Class)
**Purpose**: Base command class

**Key Responsibilities**:
- Defines command structure
- Manages orchestration descriptors
- Provides command cloning for multi-device operations

**Key Members**:
- `em_cmd_type_t m_type`: Command type
- `em_cmd_params_t m_param`: Command parameters
- `dm_easy_mesh_t m_data_model`: Data model snapshot
- `em_orch_desc_t m_orch_desc[]`: Orchestration descriptors

**Key Methods**:
- `init()`: Initializes command
- `validate()`: Validates command
- `execute()`: Executes command
- `clone_for_next()`: Clones for next device

### 5. `em_orch_t` (Abstract Base Class)
**Purpose**: Base orchestration class

**Key Responsibilities**:
- Executes orchestration descriptors
- Manages command execution flow
- Handles command completion

**Key Methods**:
- `execute()`: Executes command
- `handle_cmd_complete()`: Handles completion

## Command Classes

### SSID Commands
- **`em_cmd_get_ssid_t`**: Queries SSID configuration
- **`em_cmd_set_ssid_t`**: Updates SSID configuration
  - Orchestration: DB config → EM update → Network SSID update

### Channel Commands
- **`em_cmd_get_channel_t`**: Queries channel configuration
- **`em_cmd_set_channel_t`**: Updates channel configuration
  - Orchestration: Channel selection → Channel configuration

### Other Commands
- **`em_cmd_get_device_t`**: Queries device information
- **`em_cmd_get_radio_t`**: Queries radio information
- **`em_cmd_get_network_t`**: Queries network information
- **`em_cmd_remove_device_t`**: Removes device from network
- **`em_cmd_reset_t`**: Resets device/network

## CLI Integration Classes

### 1. `em_cli_t`
**Purpose**: CLI interface

**Key Methods**:
- `exec()`: Executes CLI command
- `get_command()`: Parses and gets command object
- `set_remote_addr()`: Sets remote controller address

### 2. `em_cmd_cli_t` (extends `em_cmd_exec_t`)
**Purpose**: CLI command execution

**Key Methods**:
- `execute()`: Executes CLI command
- `get_edited_node()`: Extracts edited node data
- `validate()`: Validates command

## Class Relationships

### Inheritance Hierarchy
```
em_mgr_t (abstract)
├── em_agent_t
└── em_ctrl_t

dm_easy_mesh_t (abstract)
├── dm_easy_mesh_agent_t
└── dm_easy_mesh_ctrl_t

em_cmd_exec_t (abstract)
├── em_cmd_agent_t
├── em_cmd_ctrl_t
└── em_cmd_cli_t

em_orch_t (abstract)
├── em_orch_agent_t
└── em_orch_ctrl_t

em_cmd_t (abstract)
├── em_cmd_get_ssid_t
├── em_cmd_set_ssid_t
├── em_cmd_get_channel_t
├── em_cmd_set_channel_t
└── ... (other commands)
```

### Composition Relationships
- `em_agent_t` contains:
  - `dm_easy_mesh_agent_t`
  - `em_orch_agent_t`
  - `em_cmd_agent_t`
  - Multiple `em_t` instances (one per radio/band)

- `em_ctrl_t` contains:
  - `dm_easy_mesh_ctrl_t`
  - `em_orch_ctrl_t`
  - `em_cmd_ctrl_t`
  - Multiple `em_t` instances

- `dm_easy_mesh_ctrl_t` contains:
  - `dm_easy_mesh_list_t` (list of agent data models)

- `em_t` contains:
  - `dm_easy_mesh_t*`
  - `em_mgr_t*`
  - `em_sm_t`
  - `em_crypto_t`

## Data Flow for SSID Update

1. **CLI** → `em_cli_t::exec()`
2. **CLI** → `em_cmd_cli_t::execute()` → Creates bus event
3. **Controller** → `em_ctrl_t::handle_set_ssid_list()`
4. **Controller** → `dm_easy_mesh_ctrl_t::analyze_set_ssid()`
5. **Controller** → Creates `em_cmd_set_ssid_t` objects
6. **Controller** → `em_orch_ctrl_t::execute()` → Executes orchestration
7. **Controller** → Sends CMDU to agents
8. **Agents** → Receive CMDU → Update data model → Apply to OneWifi

## Data Flow for Channel Update

1. **CLI** → `em_cli_t::exec()`
2. **CLI** → `em_cmd_cli_t::execute()` → Creates bus event
3. **Controller** → `em_ctrl_t::handle_set_channel_list()`
4. **Controller** → Creates `em_cmd_set_channel_t` objects
5. **Controller** → `em_orch_ctrl_t::execute()` → Channel selection
6. **Controller** → Channel configuration → Sends CMDU to agents
7. **Agents** → Receive CMDU → Update radio configuration → Apply to OneWifi

## Key Design Patterns

1. **Template Method Pattern**: `em_mgr_t` defines algorithm, derived classes implement steps
2. **Strategy Pattern**: Different orchestration strategies for agent vs controller
3. **Command Pattern**: Commands encapsulated as objects (`em_cmd_t`)
4. **Observer Pattern**: Bus events notify components of changes
5. **Factory Pattern**: Command objects created by command executors

## Threading Model

- **Main Thread**: Manager event loop
- **Node Threads**: Each `em_t` has its own thread for protocol processing
- **Input Thread**: Separate thread for input listening
- **Bus Threads**: Bus communication handled in separate threads

## Memory Management

- **RAII**: Objects manage their own resources
- **Smart Pointers**: Used where appropriate (C++11)
- **Ownership**: Clear ownership semantics
  - Manager owns nodes
  - Data model owns lists
  - Commands are created, executed, and destroyed

## Error Handling

- **Return Codes**: Functions return status codes
- **Exceptions**: Used sparingly for critical errors
- **Validation**: Commands validated before execution
- **Rollback**: Orchestration supports rollback on failure

