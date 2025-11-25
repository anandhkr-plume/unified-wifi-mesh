# Onboarding Code Flow - Agent and Controller Side

This document shows the detailed code flow during agent onboarding from both Agent and Controller perspectives.

## Table of Contents
1. [Agent Side Flow](#agent-side-flow)
2. [Controller Side Flow](#controller-side-flow)
3. [Message Processing Flow](#message-processing-flow)
4. [State Transitions](#state-transitions)

---

## Agent Side Flow

### Phase 1: Device Initialization

```
1. External Trigger (OneWifi/CLI)
   └─> Sends JSON with "wfa-dataelements:Network"
   
2. em_cmd_agent_t::execute()
   File: src/agent/em_cmd_agent.cpp:54
   └─> Waits for socket connection
   └─> Receives event data
   └─> Calls m_agent.io_process(get_event())
   
3. em_agent_t::io_process()
   File: src/agent/em_agent.cpp
   └─> Creates em_bus_event_t from JSON
   └─> Calls handle_bus_event()
   
4. em_agent_t::handle_bus_event()
   File: src/agent/em_agent.cpp:786
   └─> case em_bus_event_type_dev_init:
       └─> handle_dev_init(evt)
       
5. em_agent_t::handle_dev_init()
   File: src/agent/em_agent.cpp
   └─> m_data_model.analyze_dev_init(evt, pcmd[])
   
6. dm_easy_mesh_agent_t::analyze_dev_init()
   File: src/agent/dm_easy_mesh_agent.cpp:64
   └─> translate_onewifi_dml_data() - Parses OneWifi config
   └─> Creates em_cmd_dev_init_t command
   └─> Returns command array
   
7. em_agent_t::handle_dev_init() (continued)
   └─> m_orch->submit_commands(pcmd, num)
   
8. em_orch_agent_t::submit_commands()
   File: src/orch/em_orch_agent.cpp
   └─> For each command:
       ├─> pre_process_orch_op() - Creates EM nodes
       ├─> build_candidates() - Finds target EM instances
       └─> execute_orch_op() - Executes orchestration
       
9. em_orch_agent_t::pre_process_orch_op()
   File: src/orch/em_orch_agent.cpp:204
   └─> case dm_orch_type_al_insert:
       └─> Creates AL interface EM node
   └─> case dm_orch_type_em_insert:
       └─> Creates radio EM nodes
       
10. em_t::set_orch_state(em_orch_state_progress)
    File: src/em/em.cpp
    └─> Sets orchestration state to progress
    
11. em_t::handle_agent_state()
    File: src/em/em.cpp:357
    └─> case em_cmd_type_dev_init:
        └─> Sets state: em_state_agent_unconfigured
        └─> em_configuration_t::process_agent_state()
        
12. em_configuration_t::process_agent_state()
    File: src/em/config/em_configuration.cpp:5437
    └─> case em_state_agent_unconfigured:
        └─> handle_state_config_none()
```

### Phase 2: Autoconfig Search

```
13. em_configuration_t::handle_state_config_none()
    File: src/em/config/em_configuration.cpp:5349
    └─> Checks if DPP onboarding (skip if active)
    └─> create_autoconfig_search_msg(buff)
    
14. em_configuration_t::create_autoconfig_search_msg()
    File: src/em/config/em_configuration.cpp:3564
    └─> Adds 1905 header (multicast: 01:80:c2:00:00:13)
    └─> Adds TLVs:
        ├─> AL MAC Address
        ├─> SearchedRole (Registrar = 0x00)
        ├─> AutoconfigFreqBand
        ├─> SupportedService (Agent)
        ├─> SearchedService (Controller)
        ├─> Profile (Multi-AP Profile 3)
        └─> Optional: DPP Chirp Value TLV
    └─> Returns message length
    
15. em_configuration_t::handle_state_config_none() (continued)
    └─> Validates message
    └─> send_frame(buff, sz, true) - Multicast send
    └─> set_state(em_state_agent_autoconfig_rsp_pending)
    
16. em_t::send_frame()
    File: src/em/em.cpp:664
    └─> Sends frame via AL SAP or raw socket
    └─> Destination: Multicast 01:80:c2:00:00:13
```

### Phase 3: Receive Autoconfig Response

```
17. Controller receives and processes (see Controller Flow)
    
18. Agent receives Autoconfig Response
    └─> em_t::proto_run() - Main receive loop
        └─> Receives frame from socket
        └─> Calls proto_process()
        
19. em_t::proto_process()
    File: src/em/em.cpp:269
    └─> case em_msg_type_autoconf_resp:
        └─> em_configuration_t::process_msg(data, len)
        
20. em_configuration_t::process_msg()
    File: src/em/config/em_configuration.cpp:5197
    └─> Routes message based on service type
    └─> case em_msg_type_autoconf_resp:
        └─> Checks state: em_state_agent_autoconfig_rsp_pending
        └─> handle_autoconfig_resp(data, len)
        
21. em_configuration_t::handle_autoconfig_resp()
    File: src/em/config/em_configuration.cpp:5042
    └─> Validates message
    └─> Checks for DPP Chirp TLV
    └─> If DPP onboarding:
        ├─> set_state(em_state_agent_1905_securing)
        └─> get_ec_mgr().start_secure_1905_layer()
    └─> Else (WSC):
        └─> create_autoconfig_wsc_m1_msg(msg, hdr->src)
        └─> send_frame(msg, sz)
        └─> set_state(em_state_agent_wsc_m2_pending)
```

### Phase 4: WSC Exchange (Standard Onboarding)

```
22. em_configuration_t::create_autoconfig_wsc_m1_msg()
    File: src/em/config/em_configuration.cpp:3215
    └─> Creates WSC M1 message
    └─> Includes:
        ├─> WSC TLV (Wi-Fi credentials request)
        ├─> AP Radio Basic Capability
        └─> AP Radio Advanced Capability
        
23. Controller receives WSC M1 (see Controller Flow)
    
24. Agent receives WSC M2
    └─> em_t::proto_process()
        └─> case em_msg_type_autoconf_wsc:
            └─> em_configuration_t::process_msg()
                └─> handle_autoconfig_wsc_m2()
                
25. em_configuration_t::handle_autoconfig_wsc_m2()
    File: src/em/config/em_configuration.cpp:3612
    └─> Parses WSC M2 message
    └─> Extracts encrypted settings
    └─> Decrypts Wi-Fi credentials
    └─> Triggers OneWifi configuration
    
26. em_configuration_t::handle_autoconfig_wsc_m2() (continued)
    └─> get_mgr()->io_process(em_bus_event_type_m2_tx, ...)
    
27. em_agent_t::handle_bus_event()
    File: src/agent/em_agent.cpp:786
    └─> case em_bus_event_type_m2ctrl_configuration:
        └─> handle_m2ctrl_configuration(evt)
        
28. em_agent_t::handle_m2ctrl_configuration()
    File: src/agent/em_agent.cpp
    └─> Analyzes WSC M2 configuration
    └─> Creates configuration commands
    └─> Updates OneWifi via io_process
    └─> set_state(em_state_agent_owconfig_pending)
```

### Phase 5: Topology Synchronization

```
29. After OneWifi configuration completes
    └─> em_configuration_t::process_agent_state()
        └─> case em_state_agent_owconfig_pending:
            └─> Sends topology query or waits for query
            
30. em_configuration_t::send_topology_query_msg()
    File: src/em/config/em_configuration.cpp:297
    └─> Creates topology query message
    └─> send_frame() - Unicast to controller
    
31. Agent receives Topology Response
    └─> em_configuration_t::handle_topology_response()
        └─> Parses topology information
        └─> Updates data model
        └─> set_state(em_state_agent_topo_synchronized)
        
32. Final State
    └─> em_state_agent_configured
    └─> Agent is fully onboarded
```

---

## Controller Side Flow

### Phase 1: Receive Autoconfig Search

```
1. Controller receives Autoconfig Search (multicast)
   └─> em_ctrl_t::proto_run() - Main receive loop
       └─> Receives frame from socket
       └─> Calls proto_process()
       
2. em_t::proto_process()
   File: src/em/em.cpp:269
   └─> case em_msg_type_autoconf_search:
       └─> em_configuration_t::process_msg(data, len)
       
3. em_configuration_t::process_msg()
   File: src/em/config/em_configuration.cpp:5197
   └─> Routes to controller handler
   └─> case em_msg_type_autoconf_search:
       └─> if (get_service_type() == em_service_type_ctrl):
           └─> handle_autoconfig_search(data, len)
           
4. em_configuration_t::handle_autoconfig_search()
   File: src/em/config/em_configuration.cpp:5101
   └─> Validates message
   └─> Extracts frequency band
   └─> Extracts AL MAC address
   └─> Checks for DPP Chirp TLV
   └─> If DPP Chirp found:
       └─> ec_mgr.handle_autoconf_chirp() - DPP flow
   └─> Else:
       └─> create_autoconfig_resp_msg(msg, band, al_mac, msg_id)
       
5. em_configuration_t::create_autoconfig_resp_msg()
   File: src/em/config/em_configuration.cpp:3410
   └─> Creates Autoconfig Response message
   └─> Includes TLVs:
       ├─> SupportedRole
       ├─> SupportedFreqBand
       ├─> SupportedService (Controller)
       ├─> 1905 Layer Security Capability
       ├─> Profile (Multi-AP Profile 3)
       ├─> Controller Capability
       └─> Optional: DPP Chirp Value TLV
   └─> Returns message length
   
6. em_configuration_t::handle_autoconfig_search() (continued)
   └─> Validates response message
   └─> send_frame(msg, sz) - Unicast to agent
   └─> set_state(em_state_ctrl_wsc_m1_pending)
   
7. em_ctrl_t::get_em_for_msg()
   File: src/ctrl/em_ctrl.cpp:622
   └─> case em_msg_type_autoconf_search:
       └─> Extracts AL MAC from message
       └─> Creates or retrieves data model for agent
       └─> Returns AL EM node
```

### Phase 2: Receive WSC M1

```
8. Controller receives WSC M1
   └─> em_t::proto_process()
       └─> case em_msg_type_autoconf_wsc:
           └─> em_configuration_t::process_msg()
               └─> handle_autoconfig_wsc_m1()
               
9. em_configuration_t::handle_autoconfig_wsc_m1()
   File: src/em/config/em_configuration.cpp:4979
   └─> Validates WSC M1 message
   └─> Parses TLVs:
       ├─> AP Radio Basic Capability
       ├─> WSC TLV
       └─> AP Radio Advanced Capability
   └─> handle_wsc_m1() - Processes WSC attributes
   └─> create_autoconfig_wsc_m2_msg(msg, haul_type, 1, msg_id)
   
10. em_configuration_t::create_autoconfig_wsc_m2_msg()
    File: src/em/config/em_configuration.cpp:3332
    └─> Creates WSC M2 message
    └─> Includes:
        ├─> WSC TLV (encrypted credentials)
        ├─> AP Radio Basic Capability
        └─> Configuration data
    
11. em_configuration_t::handle_autoconfig_wsc_m1() (continued)
    └─> Validates WSC M2 message
    └─> send_frame(msg, sz) - Unicast to agent
    └─> set_state(em_state_ctrl_wsc_m2_sent)
    └─> get_mgr()->io_process(em_bus_event_type_m2_tx, ...)
    
12. em_ctrl_t::handle_bus_event()
    File: src/ctrl/em_ctrl.cpp
    └─> Processes M2 transmission event
    └─> Updates controller state
```

### Phase 3: Topology Query/Response

```
13. Controller receives Topology Query
    └─> em_configuration_t::handle_topology_query()
        └─> Checks if all radios configured
        └─> send_topology_response_msg()
        
14. em_configuration_t::send_topology_response_msg()
    File: src/em/config/em_configuration.cpp:903
    └─> Creates topology response
    └─> Includes:
        ├─> Operational BSS information
        ├─> Radio capabilities
        ├─> Neighbor information
        └─> Link metrics
    └─> send_frame() - Unicast to agent
    
15. Controller state after onboarding
    └─> Agent data model created/updated
    └─> Ready for further configuration
```

---

## Message Processing Flow

### Agent Message Routing

```
Frame Received
    │
    ├─> em_agent_t::get_em_for_msg()
    │   File: src/agent/em_agent.cpp:1395
    │   └─> Routes based on message type
    │       ├─> autoconf_resp → Find EM by frequency band
    │       ├─> autoconf_wsc → Find EM by frequency band
    │       └─> topo_query → AL EM node
    │
    └─> Returns target EM instance
        │
        └─> em_t::proto_process()
            └─> Routes to appropriate handler
                ├─> em_configuration_t::process_msg()
                ├─> em_capability_t::process_msg()
                └─> em_channel_t::process_msg()
```

### Controller Message Routing

```
Frame Received
    │
    ├─> em_ctrl_t::get_em_for_msg()
    │   File: src/ctrl/em_ctrl.cpp:622
    │   └─> Routes based on message type
    │       ├─> autoconf_search → AL EM, create data model
    │       ├─> autoconf_wsc → Find EM by peer MAC
    │       └─> topo_resp → Find EM by radio MAC
    │
    └─> Returns target EM instance
        │
        └─> em_t::proto_process()
            └─> Routes to appropriate handler
```

---

## State Transitions

### Agent States

```
em_state_agent_unconfigured
    │
    ├─> [Send Autoconfig Search]
    │
    └─> em_state_agent_autoconfig_rsp_pending
        │
        ├─> [Receive Autoconfig Response]
        │
        ├─> [DPP Path]
        │   └─> em_state_agent_1905_securing
        │       └─> [1905 Security Complete]
        │           └─> em_state_agent_owconfig_pending
        │
        └─> [WSC Path]
            └─> em_state_agent_wsc_m2_pending
                │
                ├─> [Receive WSC M2]
                │
                └─> em_state_agent_owconfig_pending
                    │
                    ├─> [OneWifi Configuration]
                    │
                    └─> em_state_agent_topo_synchronized
                        │
                        ├─> [Topology Sync Complete]
                        │
                        └─> em_state_agent_configured
```

### Controller States

```
em_state_ctrl_configured (or initial state)
    │
    ├─> [Receive Autoconfig Search]
    │
    └─> em_state_ctrl_wsc_m1_pending
        │
        ├─> [Receive WSC M1]
        │
        └─> em_state_ctrl_wsc_m2_sent
            │
            └─> [WSC M2 Sent]
                │
                └─> em_state_ctrl_configured
```

---

## Key Function Call Sequence

### Agent Side (WSC Onboarding)

```
1. em_cmd_agent_t::execute()
2. em_agent_t::io_process()
3. em_agent_t::handle_bus_event()
4. em_agent_t::handle_dev_init()
5. dm_easy_mesh_agent_t::analyze_dev_init()
6. em_orch_agent_t::submit_commands()
7. em_t::handle_agent_state()
8. em_configuration_t::process_agent_state()
9. em_configuration_t::handle_state_config_none()
10. em_configuration_t::create_autoconfig_search_msg()
11. em_t::send_frame()
    
    [Message sent - wait for response]
    
12. em_t::proto_process()
13. em_configuration_t::process_msg()
14. em_configuration_t::handle_autoconfig_resp()
15. em_configuration_t::create_autoconfig_wsc_m1_msg()
16. em_t::send_frame()
    
    [WSC M1 sent - wait for M2]
    
17. em_t::proto_process()
18. em_configuration_t::process_msg()
19. em_configuration_t::handle_autoconfig_wsc_m2()
20. em_agent_t::handle_m2ctrl_configuration()
21. [OneWifi Configuration]
22. em_configuration_t::send_topology_query_msg()
23. em_configuration_t::handle_topology_response()
```

### Controller Side (WSC Onboarding)

```
1. em_t::proto_process()
2. em_configuration_t::process_msg()
3. em_configuration_t::handle_autoconfig_search()
4. em_ctrl_t::get_em_for_msg()
5. [Create/Get Data Model]
6. em_configuration_t::create_autoconfig_resp_msg()
7. em_t::send_frame()
    
    [Autoconfig Response sent - wait for WSC M1]
    
8. em_t::proto_process()
9. em_configuration_t::process_msg()
10. em_configuration_t::handle_autoconfig_wsc_m1()
11. em_configuration_t::handle_wsc_m1()
12. em_configuration_t::create_autoconfig_wsc_m2_msg()
13. em_t::send_frame()
    
    [WSC M2 sent]
    
14. em_configuration_t::handle_topology_query()
15. em_configuration_t::send_topology_response_msg()
```

---

## File Locations Summary

### Agent Side Files
- `src/agent/em_cmd_agent.cpp` - Command execution entry point
- `src/agent/em_agent.cpp` - Agent event handling
- `src/agent/dm_easy_mesh_agent.cpp` - Data model analysis
- `src/orch/em_orch_agent.cpp` - Agent orchestration
- `src/em/config/em_configuration.cpp` - Configuration handling
- `src/em/em.cpp` - Core EM processing

### Controller Side Files
- `src/ctrl/em_ctrl.cpp` - Controller event handling
- `src/ctrl/dm_easy_mesh_ctrl.cpp` - Controller data model
- `src/em/config/em_configuration.cpp` - Configuration handling
- `src/em/em.cpp` - Core EM processing

### Common Files
- `src/cmd/em_cmd_dev_init.cpp` - Device initialization command
- `src/em/config/em_configuration.cpp` - Shared configuration logic
- `src/em/em.cpp` - Shared EM processing

---

## Notes

1. **Multicast Address**: All discovery messages use `01:80:c2:00:00:13` (IEEE 1905.1 multicast)
2. **State Machine**: States ensure proper sequencing and prevent race conditions
3. **Orchestration**: Commands are orchestrated through `em_orch_agent_t` and `em_orch_ctrl_t`
4. **Data Model**: Each agent gets its own data model instance created by controller
5. **DPP vs WSC**: Two paths exist - DPP for secure onboarding, WSC for standard onboarding
6. **Message Validation**: All messages are validated before processing
7. **Error Handling**: Failed operations reset state or retry based on timeout logic

