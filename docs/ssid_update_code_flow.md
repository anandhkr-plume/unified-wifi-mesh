# SSID Update Code Flow - Controller to Agent

This document explains the complete code flow for setting SSID from Controller to Agent, including how Controller updates OneWifi and how Agent updates SSID.

## Table of Contents
1. [Controller Side Flow](#controller-side-flow)
2. [Agent Side Flow](#agent-side-flow)
3. [OneWifi Update Mechanisms](#onewifi-update-mechanisms)

---

## Controller Side Flow

### Phase 1: SSID Update Request Entry Point

**Entry Points:**
1. **CLI Command** (`set_ssid OneWifiMesh`)
   - File: `src/cli/em_cmd_cli.cpp`, `src/fynecli/em_cmd_cli.cpp`, `src/rdkb-cli/em_cmd_cli.cpp`
   - Function: `em_cmd_cli_t::init()` (line 294-301)
   - Creates `em_bus_event_t` with type `em_bus_event_type_set_ssid`
   - Calls `get_edited_node(node, "SetSSID", info->buff)` to get updated SSID config

2. **TR-181 Command** (`cmd_setssid`)
   - File: `src/ctrl/TR_181/tr_181_cmd_setssid.cpp`
   - Function: `em_ctrl_t::cmd_setssid()` (line 28-198)
   - Parses SSID, PassPhrase, Band, AKMsAllowed from input
   - Updates JSON with `wfa-dataelements:SetSSID` wrapper
   - Calls `g_ctrl.io_process(em_bus_event_type_set_ssid, subdoc->buff, strlen(subdoc->buff))`

3. **OneWifi Callback** (NetworkSSIDList update)
   - File: `src/ctrl/dm_easy_mesh_ctrl.cpp`
   - Function: `dm_easy_mesh_ctrl_t::analyze_set_ssid()` (line 841-893)
   - Triggered when OneWifi sends NetworkSSIDList update

### Phase 2: Controller Processes SSID Update Event

```
em_ctrl_t::io_process()
  └─> em_ctrl_t::handle_bus_event()
      └─> em_ctrl_t::handle_set_ssid_list()  [Line 245-263]
          └─> dm_easy_mesh_ctrl_t::analyze_set_ssid()  [Line 841-893]
```

**File:** `src/ctrl/em_ctrl.cpp:245-263`

**Function:** `em_ctrl_t::handle_set_ssid_list()`

**Process:**
1. Checks if command is already in progress
2. Calls `m_data_model.analyze_set_ssid(evt, pcmd)` to analyze changes
3. If changes detected, creates `em_cmd_set_ssid_t` commands
4. Submits commands to orchestration via `m_orch->submit_commands()`

**File:** `src/ctrl/dm_easy_mesh_ctrl.cpp:841-893`

**Function:** `dm_easy_mesh_ctrl_t::analyze_set_ssid()`

**Process:**
1. Decodes SSID configuration from subdoc: `dm.decode_config(subdoc, "SetSSID")`
2. Compares new SSID config with current data model
3. If no change detected, returns `EM_PARSE_ERR_NO_CHANGE`
4. If changes detected:
   - Sets DB config parameter: `dm.set_db_cfg_param(db_cfg_type_network_ssid_list_update, "")`
   - Creates `em_cmd_set_ssid_t` command: `new em_cmd_set_ssid_t(evt->params, dm)`
   - Clones command for each target agent: `tmp->clone_for_next()`

### Phase 3: Command Orchestration

**File:** `src/cmd/em_cmd_set_ssid.cpp:38-62`

**Function:** `em_cmd_set_ssid_t::em_cmd_set_ssid_t()` (Constructor)

**Orchestration Descriptors:**
```cpp
m_orch_desc[0].op = dm_orch_type_db_cfg;           // Update database
m_orch_desc[1].op = dm_orch_type_em_update;        // Update EM data model
m_orch_desc[1].submit = true;                       // Submit for execution
m_orch_desc[2].op = dm_orch_type_net_ssid_update;  // Update network SSID
```

**File:** `src/orch/em_orch_ctrl.cpp:196-250`

**Function:** `em_orch_ctrl_t::is_em_ready_for_orch_exec()`

**Process:**
- For `em_cmd_type_set_ssid`, always returns `true` (can execute immediately)

**File:** `src/orch/em_orch_ctrl.cpp:361-363`

**Function:** `em_orch_ctrl_t::pre_process_exec()`

**Process:**
- For `dm_orch_type_net_ssid_update`:
  - Calls `m_mgr->load_net_ssid_table()` to reload SSID table from database

**File:** `src/orch/em_orch_ctrl.cpp:423-429`

**Function:** `em_orch_ctrl_t::build_candidates()`

**Process:**
- For `em_cmd_type_set_ssid`:
  - Iterates through all EM nodes
  - Adds non-AL-interface EM nodes to candidate queue
  - Each candidate represents an agent that needs SSID update

### Phase 4: Controller Updates OneWifi

**File:** `src/ctrl/dm_easy_mesh_ctrl.cpp:1919-1933`

**Function:** `dm_easy_mesh_ctrl_t::commit_dirty_dm()`

**Process:**
1. Checks if `db_cfg_type_network_ssid_list_update` is set
2. For each network SSID:
   - Calls `dm_network_ssid_list_t::set_config(m_db_client, dm->get_network_ssid_by_ref(i), parent)`
   - Updates database with new SSID configuration

**File:** `src/dm/dm_network_ssid_list.cpp:111-164`

**Function:** `dm_network_ssid_list_t::set_config()`

**Process:**
1. Updates database via `update_db()`
2. Updates in-memory list via `update_list()`
3. Database update triggers OneWifi notification (via bus mechanism)

**Note:** The actual OneWifi update happens through the database/bus mechanism. When the database is updated, OneWifi receives the update through its callback mechanism.

### Phase 5: Controller Sends SSID Update to Agent

**File:** `src/orch/em_orch_ctrl.cpp:423-429`

**Function:** `em_orch_ctrl_t::build_candidates()`

**Process:**
- Builds candidate list of agents that need SSID update

**File:** `src/orch/em_orch_ctrl.cpp:510-580`

**Function:** `em_orch_ctrl_t::process_ctrl_state()`

**Process:**
- For `em_cmd_type_set_ssid`:
  - Checks if agent is in appropriate state (`em_state_ctrl_configured` or `em_state_ctrl_topo_synchronized`)
  - Triggers sending BSS Configuration Request message

**File:** `src/em/config/em_configuration.cpp:2599-2610`

**Function:** `em_configuration_t::send_bss_config_req_msg()`

**Process:**
1. Creates BSS Configuration Request message: `create_bss_config_req_msg(buff, dest_al_mac)`
2. Sends frame to agent: `send_frame(buff, msg_len)`

**File:** `src/em/config/em_configuration.cpp:2612-2790`

**Function:** `em_configuration_t::create_bss_config_req_msg()`

**Message Contents:**
- Multi-AP Profile TLV
- SupportedService TLV
- AP Radio Basic Capabilities TLV
- AP Radio Advanced Capabilities TLV
- BSS Configuration Request TLV (contains SSID configuration via DPP attributes)
- Other capability TLVs (HT, VHT, HE, etc.)

**Note:** The SSID information is embedded in the BSS Configuration Request TLV as DPP Configuration Object attributes. The controller reads SSID configuration from its data model and encodes it into DPP attributes.

---

## Agent Side Flow

### Phase 1: Agent Receives BSS Configuration Request

**File:** `src/em/em.cpp:269-290`

**Function:** `em_t::proto_process()`

**Process:**
- Routes `em_msg_type_bss_config_req` to `em_configuration_t::proto_process()`

**File:** `src/em/config/em_configuration.cpp:5324-5327`

**Function:** `em_configuration_t::proto_process()`

**Process:**
- Calls `handle_bss_config_req_msg()` for controller service type

**File:** `src/em/config/em_configuration.cpp:3967-4107`

**Function:** `em_configuration_t::handle_bss_config_req_msg()`

**Process:**
1. Parses BSS Configuration Request TLVs
2. Extracts DPP Configuration Object from BSS Configuration Request TLV
3. Processes enrollee Network Access Key (NAK)
4. Creates BSS Configuration Response message with SSID configuration
5. Sends BSS Configuration Response back to controller

**File:** `src/em/config/em_configuration.cpp:4097-4106`

**Function:** `em_configuration_t::create_bss_config_rsp_msg()`

**Process:**
- Creates BSS Configuration Response message
- Includes encrypted SSID configuration using enrollee NAK

### Phase 2: Agent Receives BSS Configuration Result

**File:** `src/em/config/em_configuration.cpp:4391-4502`

**Function:** `em_configuration_t::handle_bss_config_rsp_msg()`

**Process:**
1. Parses BSS Configuration Response message
2. Extracts SSID configuration from DPP Configuration Object attributes
3. Decrypts configuration using enrollee NAK
4. Organizes configurations by frequency band
5. Creates `m2ctrl_radioconfig` structure with SSID, PassPhrase, Band, etc.
6. Sends BSS Configuration Result message back to controller
7. **Calls `refresh_onewifi_subdoc()` to update OneWifi**

**File:** `src/agent/dm_easy_mesh_agent.cpp:247-273`

**Function:** `dm_easy_mesh_agent_t::analyze_m2ctrl_configuration()`

**Process:**
1. Receives `m2ctrl_radioconfig` structure
2. Copies SSID, PassPhrase, AuthType, HaulType, Radio MAC, etc.
3. **Calls `refresh_onewifi_subdoc()` to update OneWifi VAP configuration**

**File:** `src/agent/dm_easy_mesh_agent.cpp:270-272`

**Function:** `refresh_onewifi_subdoc()`

**Process:**
- Updates OneWifi "Private" subdoc with new SSID configuration
- Uses `get_subdoc_vap_type_for_freq()` to determine VAP type (2.4G, 5G, 6G)
- Sends updated configuration to OneWifi via bus mechanism

### Phase 3: Agent Updates OneWifi

**File:** `src/agent/dm_easy_mesh_agent.cpp:270-272`

**Function:** `refresh_onewifi_subdoc()`

**Process:**
1. Creates JSON configuration with SSID, PassPhrase, AuthType, HaulType
2. Determines VAP subdoc name based on frequency band:
   - `Vap_2.4G` for 2.4 GHz
   - `Vap_5G` for 5 GHz
   - `Vap_6G` for 6 GHz
3. Sends configuration to OneWifi via bus mechanism
4. OneWifi applies the configuration to the radio/VAP

**File:** `src/agent/em_agent.cpp:1307-1346`

**Function:** `em_agent_t::onewifi_cb()`

**Process:**
- Receives callbacks from OneWifi when configuration is applied
- Processes callbacks for "private", "radio", "mesh_sta" subdocs

---

## OneWifi Update Mechanisms

### Controller Side: OneWifi Update

**Mechanism:**
1. **Database Update Path:**
   - Controller updates database via `dm_network_ssid_list_t::set_config()`
   - Database change triggers OneWifi notification
   - OneWifi reads updated configuration from database

2. **Direct Bus Path (TR-181):**
   - TR-181 command directly calls `g_ctrl.io_process()` with SSID update
   - Controller processes update and updates database
   - Database update triggers OneWifi notification

**Files:**
- `src/dm/dm_network_ssid_list.cpp:111-164` - Database update
- `src/ctrl/TR_181/tr_181_cmd_setssid.cpp:28-198` - TR-181 command handler

### Agent Side: OneWifi Update

**Mechanism:**
1. **BSS Configuration Response Path:**
   - Agent receives BSS Configuration Response from controller
   - Extracts SSID configuration from DPP attributes
   - Calls `refresh_onewifi_subdoc()` to update OneWifi VAP configuration
   - OneWifi applies configuration to radio/VAP

2. **Callback Confirmation:**
   - OneWifi sends callback when configuration is applied
   - Agent processes callback in `em_agent_t::onewifi_cb()`
   - Agent confirms configuration is active

**Files:**
- `src/agent/dm_easy_mesh_agent.cpp:247-273` - Configuration analysis
- `src/agent/dm_easy_mesh_agent.cpp:270-272` - OneWifi update call
- `src/agent/em_agent.cpp:1307-1346` - OneWifi callback handler

---

## Complete Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    CONTROLLER SIDE                              │
└─────────────────────────────────────────────────────────────────┘

[CLI/TR-181/OneWifi] 
    │
    ├─> em_cmd_cli_t::init() / cmd_setssid()
    │   └─> Creates em_bus_event_type_set_ssid
    │
    ├─> em_ctrl_t::io_process()
    │   └─> em_ctrl_t::handle_bus_event()
    │       └─> em_ctrl_t::handle_set_ssid_list()
    │           └─> dm_easy_mesh_ctrl_t::analyze_set_ssid()
    │               ├─> Decodes SSID config
    │               ├─> Compares with current DM
    │               └─> Creates em_cmd_set_ssid_t commands
    │
    ├─> em_orch_ctrl_t::submit_commands()
    │   └─> em_orch_ctrl_t::build_candidates()
    │       └─> Adds agent EM nodes to queue
    │
    ├─> em_orch_ctrl_t::process_ctrl_state()
    │   └─> em_configuration_t::send_bss_config_req_msg()
    │       └─> create_bss_config_req_msg()
    │           └─> [Sends to Agent via IEEE 1905.1]
    │
    └─> dm_network_ssid_list_t::set_config()
        └─> Updates Database
            └─> [OneWifi receives update via bus]

┌─────────────────────────────────────────────────────────────────┐
│                    AGENT SIDE                                    │
└─────────────────────────────────────────────────────────────────┘

[Receives BSS Configuration Request]
    │
    ├─> em_t::proto_process()
    │   └─> em_configuration_t::proto_process()
    │       └─> handle_bss_config_req_msg()
    │           └─> create_bss_config_rsp_msg()
    │               └─> [Sends Response to Controller]
    │
    ├─> [Receives BSS Configuration Response]
    │   └─> handle_bss_config_rsp_msg()
    │       ├─> Parses DPP Configuration Object
    │       ├─> Extracts SSID configuration
    │       └─> analyze_m2ctrl_configuration()
    │           └─> refresh_onewifi_subdoc()
    │               └─> [Updates OneWifi VAP configuration]
    │
    └─> [OneWifi applies configuration]
        └─> onewifi_cb() callback
            └─> Agent confirms configuration active
```

---

## Key Functions Summary

### Controller Side

| Function | File | Purpose |
|----------|------|---------|
| `em_ctrl_t::handle_set_ssid_list()` | `src/ctrl/em_ctrl.cpp:245` | Entry point for SSID update |
| `dm_easy_mesh_ctrl_t::analyze_set_ssid()` | `src/ctrl/dm_easy_mesh_ctrl.cpp:841` | Analyzes SSID changes |
| `em_cmd_set_ssid_t::em_cmd_set_ssid_t()` | `src/cmd/em_cmd_set_ssid.cpp:38` | Creates SSID command |
| `em_configuration_t::send_bss_config_req_msg()` | `src/em/config/em_configuration.cpp:2599` | Sends SSID to agent |
| `dm_network_ssid_list_t::set_config()` | `src/dm/dm_network_ssid_list.cpp:111` | Updates database/OneWifi |

### Agent Side

| Function | File | Purpose |
|----------|------|---------|
| `em_configuration_t::handle_bss_config_req_msg()` | `src/em/config/em_configuration.cpp:3967` | Receives SSID request |
| `em_configuration_t::handle_bss_config_rsp_msg()` | `src/em/config/em_configuration.cpp:4391` | Processes SSID response |
| `dm_easy_mesh_agent_t::analyze_m2ctrl_configuration()` | `src/agent/dm_easy_mesh_agent.cpp:247` | Analyzes SSID config |
| `refresh_onewifi_subdoc()` | `src/agent/dm_easy_mesh_agent.cpp:270` | Updates OneWifi |
| `em_agent_t::onewifi_cb()` | `src/agent/em_agent.cpp:1307` | OneWifi callback handler |

---

## Message Flow

1. **Controller → Agent:** BSS Configuration Request (contains SSID via DPP attributes)
2. **Agent → Controller:** BSS Configuration Response (acknowledges request)
3. **Controller → Agent:** BSS Configuration Result (confirms configuration)
4. **Agent → OneWifi:** VAP Configuration Update (applies SSID to radio)
5. **OneWifi → Agent:** Configuration Callback (confirms application)

---

## Notes

1. **SSID Encoding:** SSID information is encoded in DPP Configuration Object attributes within the BSS Configuration Request TLV.

2. **OneWifi Update Timing:**
   - **Controller:** Updates OneWifi immediately via database update
   - **Agent:** Updates OneWifi after receiving BSS Configuration Response

3. **State Requirements:**
   - Controller can send SSID update when agent is in `em_state_ctrl_configured` or `em_state_ctrl_topo_synchronized` state

4. **Multiple Agents:** Controller creates separate commands for each agent and sends BSS Configuration Request to each agent individually.

5. **Database Persistence:** Both controller and agent update their respective databases to persist SSID configuration.

