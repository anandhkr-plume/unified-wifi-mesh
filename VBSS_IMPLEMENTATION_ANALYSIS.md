# VBSS (Virtual BSS) / Traffic Separation Implementation Analysis

## 1. Executive Summary
The VBSS (Virtual BSS) feature, implemented via **Traffic Separation Policies** in EasyMesh to map SSIDs to VLAN IDs (VIDs), is **currently non-functional**. While the database schema and data model structures exist to store these mappings, the protocol logic to transmit them from Controller to Agent and the agent-side logic to enforce them are completely missing or stubbed out.

## 2. Data Model Layer (Partial Implementation)
The data model layer appears to be the most complete part of this feature:
*   **File**: `src/dm/dm_ssid_2_vid_map.cpp`
*   **Functionality**: Implements a table `SSIDtoVIDMapping` to store the relationship between an SSID and a VLAN ID.
*   **Input**: Can parse JSON configurations to populate this table.
*   **Status**: **Implemented**, but isolated. It does not appear to be integrated with the main `dm_easy_mesh_t` class or used by the protocol layer.

## 3. Protocol Layer - Controller (Missing)
The logic to construct the **Traffic Separation Policy TLV** (Type 0x88) is effectively missing.

*   **Primary Location**: `src/em/policy_cfg/em_policy_cfg.cpp`
    *   Function: `create_traffic_sep_policy_tlv`
    *   Status: **Stubbed**. Returns `0`, meaning no TLV is generated.
    
*   **Secondary Location**: `src/em/config/em_configuration.cpp`
    *   Function: `create_traffic_separation_policy`
    *   Status: **Incomplete & Unused**. 
        *   It iterates through SSIDs but **fails to assign the VID** to the TLV structure (the field is left uninitialized).
        *   Crucially, this function is **never called** by any message sending logic.

## 4. Protocol Layer - Agent (Missing)
The Agent-side logic to receive and apply the policy is also missing.

*   **File**: `src/em/policy_cfg/em_policy_cfg.cpp`
    *   Function: `handle_policy_cfg_req`
    *   Status: **Empty Handler**. The code checks for `em_tlv_type_traffic_separation_policy` but executes an empty block:
        ```cpp
        } else if (tlv->type == em_tlv_type_traffic_separation_policy) {
        }
        ```

## 5. Conclusion & Recommendations
To enable VBSS/Traffic Separation, the following work is required:
1.  **Integrate Data Model**: Ensure `dm_easy_mesh_t` can access the `SSIDtoVIDMapping` table.
2.  **Implement Controller Logic**: 
    *   Update `create_traffic_sep_policy_tlv` in `em_policy_cfg.cpp` to query the data model and construct the proper TLV with SSID and VID pairs.
3.  **Implement Agent Logic**:
    *   Implement the handler in `handle_policy_cfg_req` to parse the incoming TLV.
    *   Map the received VIDs to the corresponding network interfaces/bridges in the underlying OS (e.g., creating VLAN interfaces, configuring bridge ports).
