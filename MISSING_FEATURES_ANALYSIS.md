# Missing Features & Implementation Gaps Analysis

## 1. Policy Configuration (`src/em/policy_cfg/em_policy_cfg.cpp`)
The Policy Configuration feature, which allows the Controller to configure various policies on the Agent, is largely **unimplemented**. While the message structure and basic handling for Steering and Metric Reporting exist, most other policies are stubs.

### 1.1. Unimplemented TLV Creation
The following functions return a length of 0, meaning they do not generate any TLV data:
*   `create_def_8021q_settings_policy_tlv`: Default 802.1Q Settings.
*   `create_traffic_sep_policy_tlv`: Traffic Separation Policy.
*   `create_chan_scan_report_policy_tlv`: Channel Scan Reporting Policy.
*   `create_unsucc_assoc_policy_tlv`: Unsuccessful Association Policy.
*   `create_backhaul_bss_conf_policy_tlv`: Backhaul BSS Configuration.
*   `create_qos_mgt_policy_tlv`: QoS Management Policy.

### 1.2. Unimplemented TLV Handling
In `handle_policy_cfg_req`, the following TLV types are checked but have **empty handler blocks**:
*   `em_tlv_type_dflt_8021q_settings`
*   `em_tlv_type_traffic_separation_policy`
*   `em_tlv_type_channel_scan_rprt_policy`
*   `em_tlv_type_unsucc_assoc_policy`
*   `em_tlv_type_backhaul_bss_conf`
*   `em_tlv_type_qos_mgmt_policy`

**Impact**: The Controller cannot configure VLANs, traffic separation, channel scan rules, or QoS policies on the Agent. Only Steering and Metric Reporting policies are currently functional.

## 2. Provisioning / DPP (`src/em/prov/em_provisioning.cpp`)
The Device Provisioning Protocol (DPP) implementation relies heavily on an external `easyconnect` module (`m_ec_manager`), but the integration into the EasyMesh state machine is incomplete.

### 2.1. Incomplete State Machine
*   **Agent State**: The `process_agent_state` function contains a large block of commented-out code (lines 594-626) covering states like `em_state_agent_prov`, `em_state_agent_auth_req_pending`, etc. This indicates the internal state transitions for provisioning are not fully wired up.
*   **Controller State**: The `process_ctrl_state` function is completely empty.

### 2.2. Message Handling
*   Message handlers (`handle_proxy_encap_dpp`, etc.) exist and delegate to `m_ec_manager`. This suggests that while the *protocol* messages might be processed by the library, the *EasyMesh-level orchestration* of these states is missing.

## 3. Client Association Control (`src/em/steering/em_steering.cpp`)
As detailed in the separate `CLIENT_ASSOCIATION_CONTROL_ANALYSIS.md`:
*   **Agent Handler Missing**: The Agent has no logic to receive or process `Client Association Control Request` messages.
*   **Enforcement Missing**: No integration with the underlying Wi-Fi driver/HAL to actually block clients.

## 4. Summary of Gaps
| Feature | Status | Missing Components |
| :--- | :--- | :--- |
| **Steering Policy** | Partial | Implemented, but limited to local and BTM steering. |
| **Metric Policy** | Implemented | Appears functional. |
| **VLAN / Traffic Sep** | **Missing** | TLV creation and handling are stubs. |
| **QoS Policy** | **Missing** | TLV creation and handling are stubs. |
| **Channel Scan Policy** | **Missing** | TLV creation and handling are stubs. |
| **DPP / Provisioning** | Partial | Message passing exists, but EasyMesh state machine integration is commented out/empty. |
| **Client Assoc Control** | **Missing** | Agent-side handler and driver integration are non-existent. |
