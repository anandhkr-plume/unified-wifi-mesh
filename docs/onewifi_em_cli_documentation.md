# onewifi_em_cli Documentation

## Overview

`onewifi_em_cli` is a command-line interface (CLI) tool for interacting with the Unified WiFi Mesh system. It provides a way to query and configure EasyMesh network parameters such as SSID, channels, radios, devices, and more. The CLI is implemented as a Go application that uses CGO to interface with the underlying C++ library.

## Architecture

### Components

1. **Go Frontend** (`src/cli/main.go`, `src/cli/meshViews.go`)
   - User interface layer using Bubble Tea (TUI framework)
   - Command parsing and execution
   - Network tree visualization

2. **C++ Backend** (`src/cli/em_cli.cpp`, `src/cli/em_cmd_cli.cpp`)
   - Command execution engine
   - Network tree management
   - Communication with EM Agent/Controller

3. **CGO Bridge**
   - Connects Go and C++ code
   - Handles data marshaling between languages

## How onewifi_em_cli Works

### 1. Initialization

```go
// main.go
meshViewMgr := newMeshViewsMgr(platform, remoteIP, remotePort)
program = tea.NewProgram(meshViewMgr, tea.WithAltScreen())
program.Run()
```

The CLI initializes with:
- **Platform**: Target platform (e.g., "OneWifiMesh")
- **Remote IP/Port**: Optional connection to remote EM Controller (default: 127.0.0.1:49153)

### 2. Command Execution Flow

```
User Input → Go CLI → CGO → em_cli_t::exec() → em_cmd_cli_t → em_cmd_t → EM Agent/Controller
```

#### Step-by-Step Process:

1. **User enters command** (e.g., `get_ssid OneWifiMesh`)
2. **Go layer parses command** (`meshViews.go::execSelectedCommand()`)
3. **CGO call to C++** (`C.exec(C.CString(cmd), C.strlen(cmd), nil)`)
4. **em_cli_t::exec()** processes the command:
   ```cpp
   em_cli_t::exec(char *in, size_t sz, em_network_node_t *node)
   {
       cli_cmd = new em_cmd_cli_t(get_command(cmd, sz, node), ...);
       cli_cmd->init();
       cli_cmd->validate();
       cli_cmd->execute(result);
       new_node = em_net_node_t::get_network_tree(result);
       return new_node;
   }
   ```
5. **Command object created** (`em_cmd_cli_t` wraps `em_cmd_t`)
6. **Command executed** via `em_cmd_exec_t::execute()`
7. **Result returned** as `em_network_node_t*` (network tree structure)
8. **Go layer displays** the network tree in the TUI

### 3. Value Fetching (GET Operations)

#### Example: Getting SSID Information

**Command**: `get_ssid OneWifiMesh`

**Flow**:
1. **CLI Parsing** (`em_cli_t::get_command()`)
   - Parses command string
   - Identifies `em_cmd_type_get_ssid`
   - Creates `em_cmd_get_ssid_t` object

2. **Command Execution** (`em_cmd_get_ssid_t::execute()`)
   ```cpp
   // Creates bus event
   bevt->type = em_bus_event_type_get_ssid;
   info = &bevt->u.subdoc;
   snprintf(info->name, sizeof(info->name), "%s", param->u.args.fixed_args);
   ```

3. **Bus Event Processing**
   - Event sent to EM Agent/Controller via bus
   - Agent/Controller queries data model
   - Returns network SSID list from `dm_easy_mesh_t::m_network_ssid_list`

4. **Result Encoding**
   - Data model encodes SSID list to JSON
   - JSON stored in result buffer
   - Converted to `em_network_node_t` tree structure

5. **Display**
   - Go layer receives network tree
   - Tree parsed and displayed in TUI
   - User sees SSID configuration

#### Example: Getting Channel Information

**Command**: `get_channel OneWifiMesh`

**Flow**:
1. Similar to SSID, but queries `dm_easy_mesh_t::m_op_class_list`
2. Returns channel preferences and operating classes
3. Displays channel configuration per band (2.4G, 5G, 6G)

### 4. Value Updates (SET Operations)

#### SSID Update Flow

**Command**: `set_ssid OneWifiMesh`

**Step 1: User Edits Network Tree**
- User navigates to SSID section in TUI
- Edits SSID name, passphrase, bands, AKMs
- Changes stored in `em_network_node_t` structure

**Step 2: Command Execution** (`em_cmd_cli_t::execute()`)
```cpp
case em_cmd_type_set_ssid:
    if ((node = m_cmd.m_param.net_node) == NULL) {
        return -1;
    }
    bevt->type = em_bus_event_type_set_ssid;
    info = &bevt->u.subdoc;
    strncpy(info->name, param->u.args.fixed_args, ...);
    // Extract edited node data
    bevt->data_len = get_edited_node(node, "SetSSID", info->buff);
```

**Step 3: Extract Edited Data** (`get_edited_node()`)
- Traverses network tree to find "SetSSID" node
- Extracts modified values (SSID, passphrase, etc.)
- Encodes to JSON format
- Stores in `info->buff`

**Step 4: Bus Event to Controller** (`em_ctrl_t::handle_set_ssid_list()`)
```cpp
void em_ctrl_t::handle_set_ssid_list(em_bus_event_t *evt)
{
    // Controller receives event
    // Analyzes changes in data model
    // Creates command objects for each affected device
}
```

**Step 5: Data Model Analysis** (`dm_easy_mesh_ctrl_t::analyze_set_ssid()`)
```cpp
int dm_easy_mesh_ctrl_t::analyze_set_ssid(em_bus_event_t *evt, em_cmd_t *pcmd[])
{
    // Decode new SSID configuration
    dm.decode_config(subdoc, "SetSSID");
    
    // Compare with existing configuration
    for (i = 0; i < EM_MAX_NET_SSIDS; i++) {
        tgt = &dm.m_network_ssid[i];
        for (j = 0; j < EM_MAX_NET_SSIDS; j++) {
            src = &pdm->m_network_ssid[j];
            if (*tgt == *src) {
                // Match found
                break;
            }
        }
    }
    
    // Create command objects for changes
    pcmd[num] = new em_cmd_set_ssid_t(evt->params, dm);
    // Clone for each affected device
    while ((pcmd[num] = tmp->clone_for_next()) != NULL) {
        tmp = pcmd[num];
        num++;
    }
}
```

**Step 6: Command Execution** (`em_cmd_set_ssid_t::execute()`)
```cpp
em_cmd_set_ssid_t::em_cmd_set_ssid_t(em_cmd_params_t param, dm_easy_mesh_t& dm)
{
    m_orch_desc[0].op = dm_orch_type_db_cfg;      // Update database
    m_orch_desc[1].op = dm_orch_type_em_update;   // Update EM
    m_orch_desc[2].op = dm_orch_type_net_ssid_update; // Update network SSID
}
```

**Step 7: Orchestration** (`em_orch_ctrl_t::execute()`)
- Executes orchestration descriptors in order:
  1. **Database Configuration**: Updates persistent storage
  2. **EM Update**: Updates EasyMesh data model
  3. **Network SSID Update**: Sends CMDU messages to agents

**Step 8: Agent Processing** (if running as agent)
- Agent receives CMDU message
- Updates local data model
- Applies SSID changes to OneWifi
- Refreshes OneWifi subdoc

#### Channel Update Flow

**Command**: `set_channel OneWifiMesh`

**Step 1: User Edits Channel Preferences**
- User navigates to channel section
- Selects channels per band and operating class
- Updates `AnticipatedChannelPreference` in network tree

**Step 2: Command Execution** (`em_cmd_cli_t::execute()`)
```cpp
case em_cmd_type_set_channel:
    bevt->type = em_bus_event_type_set_channel;
    info = &bevt->u.subdoc;
    strncpy(info->name, param->u.args.fixed_args, ...);
    // Extract edited channel preferences
    bevt->data_len = get_edited_node(node, "SetAnticipatedChannelPreference", info->buff);
```

**Step 3: Extract Channel Data** (`get_edited_node()`)
- Finds "SetAnticipatedChannelPreference" node
- Extracts channel lists per band/class
- Encodes to JSON

**Step 4: Controller Processing** (`em_ctrl_t::handle_set_channel_list()`)
```cpp
void em_ctrl_t::handle_set_channel_list(em_bus_event_t *evt)
{
    // Analyzes channel changes
    // Creates channel selection commands
}
```

**Step 5: Command Creation** (`em_cmd_set_channel_t`)
```cpp
em_cmd_set_channel_t::em_cmd_set_channel_t(em_cmd_params_t param, dm_easy_mesh_t& dm)
{
    m_orch_desc[0].op = dm_orch_type_channel_sel;  // Channel selection
    m_orch_desc[1].op = dm_orch_type_channel_cnf;  // Channel configuration
}
```

**Step 6: Channel Selection & Configuration**
- **Channel Selection**: Determines optimal channels based on preferences
- **Channel Configuration**: Sends CMDU to agents with new channel assignments
- Agents update radio configuration via OneWifi

## Data Flow Diagrams

### GET Operation Flow

```
┌─────────┐
│  User   │
└────┬────┘
     │ "get_ssid OneWifiMesh"
     ▼
┌─────────────────┐
│  Go CLI Layer   │
│  (meshViews.go) │
└────┬────────────┘
     │ C.exec()
     ▼
┌─────────────────┐
│   em_cli_t      │
│   ::exec()      │
└────┬────────────┘
     │ get_command()
     ▼
┌─────────────────┐
│ em_cmd_get_ssid │
│    ::execute()  │
└────┬────────────┘
     │ Bus Event
     ▼
┌─────────────────┐
│  EM Controller  │
│  / Agent        │
└────┬────────────┘
     │ Query Data Model
     ▼
┌─────────────────┐
│ dm_easy_mesh_t  │
│ m_network_ssid  │
└────┬────────────┘
     │ Encode to JSON
     ▼
┌─────────────────┐
│ Network Tree    │
│ (em_network_    │
│  node_t)        │
└────┬────────────┘
     │ Return to Go
     ▼
┌─────────────────┐
│  TUI Display    │
└─────────────────┘
```

### SET Operation Flow (SSID)

```
┌─────────┐
│  User   │
└────┬────┘
     │ Edit SSID in TUI
     ▼
┌─────────────────┐
│  Go CLI Layer   │
│  (networkName.  │
│   go)           │
└────┬────────────┘
     │ "set_ssid OneWifiMesh"
     │ + edited node
     ▼
┌─────────────────┐
│   em_cli_t      │
│   ::exec()      │
└────┬────────────┘
     │ get_edited_node()
     ▼
┌─────────────────┐
│ em_cmd_set_ssid │
│    ::execute()  │
└────┬────────────┘
     │ Bus Event
     ▼
┌─────────────────┐
│  EM Controller  │
│ handle_set_ssid │
│    _list()      │
└────┬────────────┘
     │ analyze_set_ssid()
     ▼
┌─────────────────┐
│ dm_easy_mesh_   │
│ ctrl_t          │
│ Compare & Create│
│ Commands        │
└────┬────────────┘
     │ Create em_cmd_set_ssid_t
     ▼
┌─────────────────┐
│ em_orch_ctrl_t  │
│   ::execute()   │
└────┬────────────┘
     │ Orchestration
     ├─► Database Update
     ├─► Data Model Update
     └─► Network SSID Update
         │
         ▼
┌─────────────────┐
│  CMDU Messages  │
│  to Agents      │
└────┬────────────┘
     │
     ▼
┌─────────────────┐
│  EM Agents      │
│  Update SSID    │
│  via OneWifi    │
└─────────────────┘
```

## Key Files and Functions

### CLI Implementation
- **`src/cli/main.go`**: Main entry point, TUI initialization
- **`src/cli/meshViews.go`**: Command execution, tree display
- **`src/cli/em_cli.cpp`**: C++ CLI wrapper, command parsing
- **`src/cli/em_cmd_cli.cpp`**: Command execution, node editing

### Command Implementation
- **`src/cmd/em_cmd_set_ssid.cpp`**: SSID update command
- **`src/cmd/em_cmd_set_channel.cpp`**: Channel update command
- **`src/cmd/em_cmd_get_ssid.cpp`**: SSID query command
- **`src/cmd/em_cmd_get_channel.cpp`**: Channel query command

### Data Model
- **`src/ctrl/dm_easy_mesh_ctrl.cpp`**: Controller data model, `analyze_set_ssid()`, `analyze_set_channel()`
- **`src/agent/dm_easy_mesh_agent.cpp`**: Agent data model

### Orchestration
- **`src/orch/em_orch_ctrl.cpp`**: Controller orchestration
- **`src/orch/em_orch_agent.cpp`**: Agent orchestration

## Network Tree Structure

The CLI uses a tree structure (`em_network_node_t`) to represent network configuration:

```
Network
├── Device
│   ├── DeviceInfo
│   └── RadioList
│       └── Radio[0..N]
│           ├── RadioCapabilities
│           └── BSSList
├── Network
│   ├── NetworkSSIDList
│   │   └── NetworkSSID[0..N]
│   │       ├── SSID
│   │       ├── PassPhrase
│   │       ├── Band
│   │       └── AKMsAllowed
│   └── OpClassList
│       └── OpClass[0..N]
│           ├── Class
│           └── AnticipatedChannelPreference
│               └── ChannelList
└── Result (for SET operations)
    └── SetSSID / SetAnticipatedChannelPreference
```

## Example Usage

### Getting SSID Configuration
```bash
onewifi_em_cli OneWifiMesh
# In CLI:
> get_ssid OneWifiMesh
# Displays current SSID configuration
```

### Setting SSID
```bash
# In CLI:
> get_ssid OneWifiMesh
# Navigate to SSID, edit values
> set_ssid OneWifiMesh
# SSID updated across network
```

### Getting Channel Configuration
```bash
> get_channel OneWifiMesh
# Displays channel preferences per band
```

### Setting Channel
```bash
> get_channel OneWifiMesh
# Navigate to AnticipatedChannelPreference, select channels
> set_channel OneWifiMesh
# Channels updated across network
```

## Error Handling

- **Invalid Command**: Returns error status in network tree
- **Validation Failure**: `em_cmd_cli_t::validate()` returns false
- **Network Tree Parsing**: Errors logged, null node returned
- **Bus Communication**: Timeout handling, retry logic

## Remote Operation

The CLI can connect to a remote EM Controller:
```bash
onewifi_em_cli OneWifiMesh <remote_ip> <remote_port>
```

This allows managing the mesh network from a remote location.

