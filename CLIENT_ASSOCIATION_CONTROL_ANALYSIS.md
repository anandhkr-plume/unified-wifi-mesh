# Client Association Control Analysis

## 1. Feature Overview
Client Association Control is a Multi-AP feature that allows a Controller to instruct an Agent to block or unblock a specific client (STA) from associating with a specific BSS. This is typically used for:
*   **Steering**: Forcing a client to move to another AP by blocking it on the current one.
*   **Access Control**: Preventing unauthorized clients from connecting.
*   **Load Balancing**: Distributing clients across APs.

The mechanism involves the Controller sending a **Client Association Control Request (0x8020)** message to the Agent. This message contains a **Client Association Control Request TLV (0x9d)** which specifies:
*   BSSID to apply the control to.
*   Association Control Mode (0: Block, 1: Unblock).
*   Validity Period (how long to block).
*   List of STA MAC addresses to apply the control to.

## 2. Current Implementation Status

### 2.1. Message Definitions
*   **Message Type**: `em_msg_type_client_assoc_ctrl_req` (0x8020) is defined in `inc/em_base.h`.
*   **TLV Type**: `em_tlv_type_client_assoc_ctrl_req` (0x9d) is defined in `inc/em_base.h`.
*   **TLV Structure**: `em_client_assoc_ctrl_req_t` is defined (inferred from usage).
*   **Message Construction**: `em_msg_t::client_assoc_ctrl_req()` in `src/em/em_msg.cpp` correctly defines the mandatory TLV for this message.

### 2.2. Controller Side (Sending)
*   **Status**: **Implemented**.
*   **Location**: `src/em/steering/em_steering.cpp`.
*   **Function**: `em_steering_t::send_client_assoc_ctrl_req_msg()`.
*   **Logic**:
    *   Iterates through disassociation parameters from the command.
    *   Constructs the `em_client_assoc_ctrl_req_t` structure.
    *   Sets association control mode (0x02 for block with validity period, 0x03 for unblock/indefinite?). *Note: Spec says 0x00=Block, 0x01=Unblock. Code uses 0x02/0x03 which might be a discrepancy or custom mapping.*
    *   Sends the message using `send_frame`.

### 2.3. Agent Side (Receiving)
*   **Status**: **MISSING**.
*   **Location**: `src/em/steering/em_steering.cpp` (expected).
*   **Observation**:
    *   The `em_steering_t::process_msg` function handles `em_msg_type_client_steering_req` and `em_msg_type_client_steering_btm_rprt`.
    *   **It does NOT contain a case for `em_msg_type_client_assoc_ctrl_req`**.
    *   There is no `handle_client_assoc_ctrl_req` function implemented in `em_steering.cpp` or `em_agent.cpp`.
    *   Consequently, if an Agent receives this message, it will likely be ignored or dropped by the default handler.

## 3. Missing Components & Gaps

1.  **Agent Message Handler**:
    *   `em_steering_t::process_msg` needs a case for `em_msg_type_client_assoc_ctrl_req`.
    *   A new function `em_steering_t::handle_client_assoc_ctrl_req(unsigned char *buff, unsigned int len)` needs to be implemented.

2.  **TLV Parsing Logic**:
    *   The handler must parse the `em_tlv_type_client_assoc_ctrl_req` TLV to extract the BSSID, Association Control Mode, Validity Period, and STA MAC list.

3.  **OneWifi Integration (Underlay)**:
    *   The Agent must translate the EasyMesh command into a OneWifi/HAL command to actually enforce the blocking/unblocking at the driver level.
    *   This likely involves calling a `wifi_apply_assoc_control` or similar API, or updating a "MacFilter" or "AssociationControl" table in the OneWifi data model.
    *   *Current status*: No code found that bridges this specific EasyMesh command to OneWifi.

4.  **Spec Discrepancy (Potential)**:
    *   The sending logic uses values `0x02` and `0x03` for `assoc_control`.
    *   IEEE 1905.1 / EasyMesh spec typically defines:
        *   0x00: Block
        *   0x01: Unblock
    *   *Action*: Verify if `0x02`/`0x03` are valid or if this is a bug/custom implementation.

## 4. Recommendations

1.  **Implement Agent Handler**: Add the missing case in `em_steering_t::process_msg` and implement the handler function.
2.  **Map to OneWifi**: Identify the correct OneWifi API or data model parameter to perform MAC filtering/blocking and call it from the handler.
3.  **Verify Control Codes**: Check the EasyMesh specification for "Association Control" field values and ensure the sender and receiver use standard values (0x00/0x01) unless there's a specific reason for 0x02/0x03.
