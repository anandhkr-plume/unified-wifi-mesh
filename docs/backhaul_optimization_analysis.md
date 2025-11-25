# Backhaul Optimization Analysis

## Question 1: Does Backhaul Optimization Use Same Messages/TLVs as Client Steering?

### Answer: **NO, they use DIFFERENT messages and TLVs, but Backhaul Steering is NOT IMPLEMENTED**

### Client Steering Messages/TLVs (IMPLEMENTED):

**Messages:**
- `em_msg_type_client_steering_req` (0x0008) - Client Steering Request
- `em_msg_type_client_steering_btm_rprt` (0x0009) - Client Steering BTM Report

**TLVs:**
- `em_tlv_type_steering_request` (0x9b) - Steering Request TLV
- `em_tlv_type_steering_btm_rprt` (0x9c) - Steering BTM Report TLV

**Structures:**
- `em_steering_req_t` - Contains STA MAC, target BSSID, BTM parameters
- `em_steering_btm_rprt_t` - Contains STA MAC, BTM status code, target BSSID

**Implementation:**
- ✅ `em_steering_t::send_client_steering_req_msg()` - Fully implemented
- ✅ `em_steering_t::handle_client_steering_req()` - Fully implemented
- ✅ `em_steering_t::send_btm_report_msg()` - Fully implemented
- ✅ `em_steering_t::handle_client_steering_report()` - Fully implemented

**Files:**
- `src/em/steering/em_steering.cpp` - Complete implementation
- `src/agent/dm_easy_mesh_agent.cpp` - BTM frame handling
- `src/ctrl/dm_easy_mesh_ctrl.cpp` - Client steering command analysis

### Backhaul Steering Messages/TLVs (DEFINED BUT NOT IMPLEMENTED):

**Messages:**
- `em_msg_type_bh_steering_req` (0x000a) - Backhaul Steering Request
- `em_msg_type_bh_steering_rsp` (0x000b) - Backhaul Steering Response

**TLVs:**
- `em_tlv_type_bh_steering_req` (0x9e) - Backhaul Steering Request TLV
- `em_tlv_type_bh_steering_rsp` (0x9f) - Backhaul Steering Response TLV

**Structures:**
- `em_bh_steering_req_t` - Contains backhaul STA MAC, target BSSID, op class, channel
- `em_bh_steering_resp_t` - Contains backhaul STA MAC, target BSSID, result code

**Implementation Status:**
- ❌ **NO `send_bh_steering_req_msg()` function** - Not implemented
- ❌ **NO `handle_bh_steering_req()` function** - Not implemented
- ❌ **NO `send_bh_steering_rsp_msg()` function** - Not implemented
- ❌ **NO `handle_bh_steering_rsp()` function** - Not implemented
- ❌ **NO message processing** - Messages are defined but not routed/processed

**Files:**
- `inc/em_base.h:789-800` - Structure definitions only
- `src/em/em_msg.cpp:652-661` - Message validation only (no handlers)
- `src/validation_test.cpp:1188-1261` - Test validation only

### Key Differences:

| Aspect | Client Steering | Backhaul Steering |
|--------|----------------|-------------------|
| **Message Type** | `em_msg_type_client_steering_req` | `em_msg_type_bh_steering_req` |
| **TLV Type** | `em_tlv_type_steering_request` | `em_tlv_type_bh_steering_req` |
| **Target** | Client STA (fronthaul) | Backhaul STA |
| **BTM Support** | Yes (uses BTM frames) | No (direct steering) |
| **Implementation** | ✅ Fully implemented | ❌ Not implemented |
| **Status** | Production ready | Defined only |

---

## Question 2: What is the Logic/Implementation for Backhaul Optimization?

### Answer: **Backhaul Optimization is NOT IMPLEMENTED. The CLI feature uses Client Steering instead.**

### Current State:

1. **CLI "Optimize Backhaul Connections" Feature:**
   - **File:** `src/cli/meshViews.go:210-218`
   - **Command:** Uses `steer_sta OneWifiMesh` (same as client steering)
   - **Get Command:** Uses `get_sta OneWifiMesh` (gets client stations, not backhaul)
   - **Reality:** This is actually using CLIENT steering, not backhaul steering

2. **Backhaul Steering Messages:**
   - Messages and TLVs are **defined** in the codebase
   - Message validation exists (`em_msg_t::bh_steering_req()`, `em_msg_t::bh_steering_rsp()`)
   - **NO actual implementation** of send/handle functions
   - **NO message routing** in `em_t::proto_process()`
   - **NO command processing** in controller/agent

3. **Network Optimizer:**
   - **Directory:** `src/network_optimiser/`
   - **Status:** Only contains test file (`test_tr181.cpp`)
   - **Purpose:** Test file for TR-181 bus operations (SSID setting)
   - **No optimization logic:** No actual optimization algorithms found

4. **Backhaul-Related Functions (Partial Support):**
   - `em_agent_t::send_backhaul_action_frame()` - Sends action frames on backhaul BSS
   - `dm_easy_mesh_t::get_bsta_bss_info()` - Gets backhaul STA BSS info
   - `em_network_topo_t::find_topology_by_bh_associated()` - Finds topology by backhaul association
   - **Note:** These support backhaul operations but NOT steering/optimization

### What Would Be Needed for Backhaul Optimization:

1. **Message Implementation:**
   ```cpp
   // Missing functions:
   int em_steering_t::send_bh_steering_req_msg(mac_address_t bh_sta_mac, 
                                                bssid_t target_bssid,
                                                unsigned char op_class,
                                                unsigned char channel);
   
   int em_steering_t::handle_bh_steering_req(uint8_t *buff, unsigned int len);
   
   int em_steering_t::send_bh_steering_rsp_msg(mac_address_t bh_sta_mac,
                                                bssid_t target_bssid,
                                                unsigned char result_code);
   
   int em_steering_t::handle_bh_steering_rsp(uint8_t *buff, unsigned int len);
   ```

2. **Message Routing:**
   - Add cases in `em_t::proto_process()` to route backhaul steering messages
   - Add handlers in `em_configuration_t::proto_process()`

3. **Command Processing:**
   - Create `em_cmd_type_bh_steer` command type
   - Implement `em_cmd_bh_steer_t` class
   - Add orchestration logic in `em_orch_ctrl_t`

4. **Optimization Logic:**
   - Backhaul link quality assessment
   - Path selection algorithm
   - Load balancing across backhaul links
   - Channel optimization for backhaul

### Current Workaround:

The "Optimize Backhaul Connections" CLI feature currently:
1. Uses `get_sta OneWifiMesh` to get station list (includes backhaul STAs)
2. Uses `steer_sta OneWifiMesh` to steer stations (works for any STA, including backhaul)
3. **Limitation:** Uses client steering mechanism, not dedicated backhaul steering

### Backhaul-Related Code Found:

1. **Backhaul STA Capability Query:**
   - `em_capability_t::send_bsta_cap_query_msg()` - Queries backhaul STA capabilities
   - `em_capability_t::send_bsta_cap_report_msg()` - Reports backhaul STA capabilities
   - **Status:** ✅ Implemented

2. **Backhaul BSS Information:**
   - `dm_easy_mesh_t::get_bsta_bss_info()` - Gets backhaul STA BSS info
   - `em_bss_info_t` with `vap_mode == em_vap_mode_sta` - Identifies backhaul BSS
   - **Status:** ✅ Implemented

3. **Backhaul Action Frame Support:**
   - `em_agent_t::send_backhaul_action_frame()` - Sends action frames on backhaul BSS
   - **Status:** ✅ Implemented (but not used for steering)

---

## Summary

### Backhaul Optimization Status:

| Component | Status | Notes |
|-----------|--------|-------|
| **Backhaul Steering Messages** | ❌ Not Implemented | Defined but no handlers |
| **Backhaul Steering TLVs** | ❌ Not Implemented | Defined but no processing |
| **Backhaul Optimization Logic** | ❌ Not Implemented | No optimization algorithms |
| **CLI "Optimize Backhaul"** | ⚠️ Uses Client Steering | Works but uses wrong mechanism |
| **Backhaul STA Capability** | ✅ Implemented | Query/report functions exist |
| **Backhaul BSS Info** | ✅ Implemented | Can identify backhaul BSS |
| **Backhaul Action Frames** | ✅ Implemented | Can send frames on backhaul |

### Conclusion:

1. **Backhaul optimization does NOT use the same messages/TLVs as client steering** - they are different
2. **Backhaul steering messages/TLVs are defined but NOT implemented** - no actual functionality
3. **The "Optimize Backhaul Connections" CLI feature currently uses client steering** as a workaround
4. **No actual backhaul optimization logic exists** in the codebase
5. **Backhaul-related infrastructure exists** (capability queries, BSS info, action frames) but is not used for optimization

### Recommendation:

To implement proper backhaul optimization:
1. Implement backhaul steering message handlers
2. Create backhaul steering command processing
3. Develop backhaul optimization algorithms (link quality, path selection, load balancing)
4. Update CLI to use dedicated backhaul steering commands instead of client steering

