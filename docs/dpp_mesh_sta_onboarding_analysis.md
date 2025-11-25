# DPP Onboarding of Mesh STA (bSTA) - Analysis

## Question: Is DPP Onboarding of Mesh STA Possible from OneWiFi and rdk-wifi-hal?

### Answer: **YES, but with a LIMITATION**

DPP onboarding of Mesh STA (backhaul STA/bSTA) is implemented in unified-wifi-mesh, but there is a **temporary workaround** due to limitations with off-channel action frames from a station.

---

## Implementation Status

### ✅ DPP Infrastructure Exists

1. **DPP Enrollee Support:**
   - `ec_enrollee_t` class fully implemented
   - Supports DPP authentication, configuration request/response
   - Can request bSTA configuration

2. **bSTA DPP Configuration Object:**
   - Function: `em_t::create_bsta_response_obj()` - Creates bSTA DPP Configuration Object
   - Function: `em_t::create_bss_dpp_response_obj()` - Creates BSS configuration with backhaul support
   - Contains: SSID, BSSID, RUID, AKMs, credentials, discovery object

3. **OneWiFi Integration:**
   - Integration exists via wifi_hal interface
   - Include path: `ONEWIFI_HAL_INTF_HOME` 
   - Headers: `wifi_hal.h` included in unified-wifi-mesh
   - Mesh STA callback: `em_agent_t::handle_onewifi_mesh_sta_cb()`

### ⚠️ Current Limitation

**File:** `src/agent/em_agent.cpp:1814-1822`

```cpp
#if 0
    em_bss_info_t* bsta_info = m_data_model.get_bsta_bss_info(); 
    uint8_t* enrollee_mac = bsta_info->ruid.mac;
#else
    // TODO: Temporarily using the backhaul BSS info for enrollee MAC
    // instead of the BSTA until off-channel action frames can be sent from a station.
    em_bss_info_t* bss_info = m_data_model.get_backhaul_bss_info();
    uint8_t* enrollee_mac = bss_info->bssid.mac;
#endif
```

**Issue:**
- **Ideal:** Use bSTA MAC address (`bsta_info->ruid.mac`) as enrollee MAC
- **Actual:** Uses backhaul BSS BSSID (`bss_info->bssid.mac`) instead
- **Reason:** Off-channel action frames cannot be sent from a station (STA mode)
- **Impact:** DPP presence announcement and authentication frames must be sent from AP interface instead of STA interface

---

## DPP Onboarding Flow for Mesh STA

### Phase 1: Initialization

**File:** `src/agent/em_agent.cpp:1804-1854`

**Function:** `em_agent_t::try_start_dpp_onboarding()`

**Steps:**
1. Check if DPP onboarding is enabled (`do_start_dpp_onboarding` flag)
2. Check if not in colocated mode
3. Get backhaul BSS info (workaround for bSTA limitation)
4. Get enrollee MAC (currently uses backhaul BSS BSSID)
5. Get AL node reference
6. Get operating class/channel info for the BSS
7. Generate DPP bootstrapping data with enrollee MAC
8. Start enrollee onboarding via EC manager

**Code Flow:**
```cpp
bool em_agent_t::try_start_dpp_onboarding() {
    // 1. Check flags
    if (!do_start_dpp_onboarding || m_data_model.get_colocated()) {
        return false;
    }
    
    // 2. Get backhaul BSS info (workaround)
    em_bss_info_t* bss_info = m_data_model.get_backhaul_bss_info();
    uint8_t* enrollee_mac = bss_info->bssid.mac;
    
    // 3. Get op class info
    em_op_class_info_t *op_class_info = 
        m_data_model.get_opclass_info_for_bss(bss_info->ruid.mac);
    
    // 4. Generate DPP bootstrapping data
    ec_data_t ec_data;
    ec_util::get_dpp_boot_data(&ec_data, enrollee_mac, false, 
                               do_regen_dpp_uri, op_class_info);
    
    // 5. Start onboarding
    al_node->get_ec_mgr().enrollee_start_onboarding(false, &ec_data, 
                                                     ethernet_onboarding);
    
    return true;
}
```

### Phase 2: DPP Presence Announcement

**File:** `src/em/prov/easyconnect/ec_enrollee.cpp:206-280`

**Function:** `ec_enrollee_t::send_presence_announcement_frames()`

**Process:**
- Enrollee sends DPP Presence Announcement frames on each channel
- Contains DPP bootstrapping hash for configurator discovery
- Sent from backhaul BSS interface (workaround) instead of bSTA interface

### Phase 3: DPP Authentication

**File:** `src/em/prov/easyconnect/ec_enrollee.cpp:583-763`

**Functions:**
- `ec_enrollee_t::handle_auth_request()` - Handles DPP Authentication Request from configurator
- `ec_enrollee_t::handle_auth_confirm()` - Handles DPP Authentication Confirm

**Process:**
1. Configurator sends DPP Authentication Request
2. Enrollee validates request and sends DPP Authentication Response
3. Configurator sends DPP Authentication Confirm
4. Enrollee validates confirm and proceeds to configuration

### Phase 4: bSTA Configuration Request

**File:** `src/em/prov/easyconnect/ec_enrollee.cpp:1566-1687`

**Function:** `ec_enrollee_t::create_config_request()`

**Process:**
1. Create DPP Configuration Request Object
2. Add bSTAList to configuration request
3. Request includes:
   - Enrollee name
   - MAC address
   - Operating bands
   - bSTA information (RUID, channels, etc.)

**Code:**
```cpp
// Get bSTA info from callback
cJSON *bsta_info = m_get_bsta_info(nullptr);
cJSON_AddItemToObject(dpp_config_request_obj, "bSTAList", bsta_info);
```

### Phase 5: bSTA Configuration Response

**File:** `src/em/em.cpp:1427-1448`

**Function:** `em_t::create_bsta_response_obj()`

**Process:**
1. Get backhaul bSTA BSS information
2. Create bSTA DPP Configuration Object with:
   - Wi-Fi tech: "map" (Multi-AP backhaul)
   - SSID of backhaul network
   - BSSID of target backhaul AP
   - RUID (Radio Unique Identifier)
   - AKMs (Authentication and Key Management suites)
   - PSK or passphrase
   - Discovery object
   - Credential object

**Code:**
```cpp
cJSON *em_t::create_bsta_response_obj(mac_addr_t pa_al_mac, 
                                      em_haul_type_t haul_type) {
    // Get bSTA BSS info
    em_bss_info_t* bss_info = get_bss_info(dm, haul_type);
    
    // Create DPP Configuration Object
    cJSON *bss_config_obj = create_bss_dpp_response_obj(bss_info, 
                                                         true, false, dm);
    
    return bss_config_obj;
}
```

**File:** `src/em/em.cpp:1450-1625`

**Function:** `em_t::create_bss_dpp_response_obj()`

**bSTA Configuration Object Structure:**
```json
{
  "wi-fi_tech": "map",
  "XRDK_BSS_Frequency": <band>,
  "discovery": {
    "SSID": "<backhaul_ssid>",
    "BSSID": "<backhaul_bssid>",
    "RUID": "<radio_unique_id>"
  },
  "cred": {
    "akm": "<akm_suites>",
    "psk_hex": "<psk_hash>",
    "pass": "<passphrase>"
  }
}
```

### Phase 6: BSS Configuration Response Processing

**File:** `src/em/prov/easyconnect/ec_enrollee.cpp:905-1135`

**Function:** `ec_enrollee_t::handle_config_response()`

**Process:**
1. Enrollee receives DPP Configuration Response with bSTA configuration
2. Parses bSTA DPP Configuration Object
3. Extracts:
   - SSID for backhaul network
   - BSSID of target AP
   - Credentials (AKMs, PSK, passphrase)
4. Initiates scan and association with backhaul BSS
5. Applies configuration to mesh STA interface via OneWiFi

### Phase 7: OneWiFi Configuration

**File:** `src/agent/em_agent.cpp:283-310`

**Function:** `em_agent_t::handle_onewifi_mesh_sta_cb()`

**Process:**
- Receives callback from OneWifi when mesh_sta subdoc is updated
- Processes mesh STA configuration changes
- Updates data model with mesh STA status

**OneWifi Integration:**
- Subdoc name: "mesh_sta" or "mesh backhaul sta"
- Bus event: `em_bus_event_type_onewifi_mesh_sta_cb`
- Callback: `em_agent_t::onewifi_cb()` processes subdoc updates

---

## Backhaul STA (bSTA) Information Handling

### Data Structure

**File:** `inc/em_base.h:2284-2310`

**Structure:** `em_bss_info_t`

**Key Fields for bSTA:**
- `vap_mode` - Set to `em_vap_mode_sta` for bSTA
- `haul_type` - Set to `em_haul_type_backhaul`
- `ssid` - Backhaul network SSID
- `bssid` - Target backhaul AP BSSID
- `ruid` - Radio Unique Identifier
- `mesh_sta_passphrase` - Backhaul network passphrase
- `backhaul_akm[]` - Array of backhaul AKMs
- `num_backhaul_akms` - Number of backhaul AKMs
- `backhaul_use` - Boolean flag for backhaul usage
- `enabled` - Boolean flag for BSS enable status

### Getting bSTA Information

**File:** `src/dm/dm_easy_mesh.cpp:2210-2238`

**Function:** `dm_easy_mesh_t::get_bsta_bss_info()`

**Logic:**
```cpp
em_bss_info_t* dm_easy_mesh_t::get_bsta_bss_info() {
    for (int i = 0; i < m_num_bss; i++) {
        em_bss_info_t *bsta_info = this->get_bss_info(i);
        
        // Check haul type
        if (bsta_info->id.haul_type != em_haul_type_backhaul) continue;
        
        // Check VAP mode
        if (bsta_info->vap_mode != em_vap_mode_sta) continue;
        
        // Check if radio and BSS are enabled
        auto radio = this->get_radio(bsta_info->ruid.mac);
        if (!radio->m_radio_info.enabled || !bsta_info->enabled) continue;
        
        return bsta_info;
    }
    return nullptr;
}
```

**Criteria for bSTA BSS:**
1. `haul_type` = `em_haul_type_backhaul`
2. `vap_mode` = `em_vap_mode_sta`
3. Radio enabled
4. BSS enabled

### Getting Backhaul BSS Information (Workaround)

**File:** `src/dm/dm_easy_mesh.cpp:2240-2255`

**Function:** `dm_easy_mesh_t::get_backhaul_bss_info()`

**Purpose:** Gets the backhaul BSS (AP mode) that the bSTA will connect to. Currently used as workaround for DPP enrollee MAC.

---

## OneWiFi and rdk-wifi-hal Integration

### Build Configuration

**Files:**
- `build/makefile.inc` - Defines `ONEWIFI_HAL_INTF_HOME`
- `build/agent/makefile` - Includes OneWiFi HAL interface path
- `build/ctrl/makefile` - Includes OneWiFi HAL interface path

**Include Path:**
```makefile
ONEWIFI_HAL_INTF_HOME = $(BASE)/../../../halinterface/include
-I$(ONEWIFI_HAL_INTF_HOME)
```

### wifi_hal Integration

**File:** `inc/util.h:27`

```cpp
#include "wifi_hal.h"
```

**Functions Referenced:**
- `wifi_hal_startScan()` - For STA-compatible scanning
- `wifi_hal_startNeighborScan()` - For non-STA scanning
- `rdk_wifi_radio_t` - RDK WiFi radio structure
- `get_radio_data()` - Gets RDK WiFi radio data

### OneWiFi Callback Mechanism

**File:** `src/agent/em_agent.cpp:1307-1346`

**Function:** `em_agent_t::onewifi_cb()`

**Subdoc Processing:**
```cpp
void em_agent_t::onewifi_cb(char *event_name, raw_data_t *data, 
                            void *userData) {
    cJSON *json = cJSON_Parse((char *)data->raw_data.bytes);
    cJSON *subdoc_name = cJSON_GetObjectItemCaseSensitive(json, "SubDocName");
    
    if (strcmp(subdoc_name->valuestring, "private") == 0 ||
        strcmp(subdoc_name->valuestring, "Vap_2.4G") == 0 ||
        strcmp(subdoc_name->valuestring, "Vap_5G") == 0 ||
        strcmp(subdoc_name->valuestring, "Vap_6G") == 0) {
        // Handle private/fronthaul VAP config
        g_agent.io_process(em_bus_event_type_onewifi_private_cb, ...);
    }
    else if (strcmp(subdoc_name->valuestring, "mesh_sta") == 0 ||
             strcmp(subdoc_name->valuestring, "mesh backhaul sta") == 0) {
        // Handle mesh STA config
        g_agent.io_process(em_bus_event_type_onewifi_mesh_sta_cb, ...);
    }
    else if (strcmp(subdoc_name->valuestring, "radio") == 0 || ...) {
        // Handle radio config
        g_agent.io_process(em_bus_event_type_onewifi_radio_cb, ...);
    }
}
```

**Supported Subdocs for Mesh STA:**
- "mesh_sta"
- "mesh backhaul sta"

---

## CLI Command Support

**File:** `src/cli/meshViews.go:228-236`

**Command:** `start_dpp`

```go
DeviceOnboardingCmd: {
    Title:                DeviceOnboardingCmd,
    LoadOrder:            11,
    GetCommand:           "",
    GetCommandEx:         "",
    SetCommand:           "start_dpp",
    Help:                 "",
    AllowUnmodifiedApply: true,
},
```

**Command Processing:**
- **File:** `src/em/em.cpp:117-140`
- **Command Type:** `em_cmd_type_start_dpp`
- **Function:** `m_ec_manager->cfg_onboard_enrollee(dpp_info)`
- **Message:** Sends DPP CCE Indication message

---

## Summary of DPP Mesh STA Onboarding Capability

### ✅ What Works:

1. **DPP Infrastructure:**
   - Full DPP enrollee implementation
   - DPP authentication and configuration exchange
   - BSS Configuration Request/Response

2. **bSTA Configuration Object:**
   - Creation of bSTA DPP Configuration Object
   - SSID, BSSID, RUID, credentials encoding
   - AKM suite handling (PSK, SAE, DPP)

3. **OneWiFi Integration:**
   - wifi_hal interface integration
   - Mesh STA subdoc processing
   - Callback mechanism for mesh_sta updates

4. **Data Model:**
   - bSTA BSS information structure
   - Functions to retrieve bSTA info
   - Backhaul network configuration

### ⚠️ Limitation:

**Off-Channel Action Frame Issue:**
- **Problem:** Cannot send off-channel action frames from STA interface
- **Impact:** DPP presence announcement and authentication cannot use bSTA MAC
- **Workaround:** Uses backhaul BSS (AP mode) interface instead
- **Status:** Temporary solution with TODO comment

### Requirement for Full bSTA DPP Support:

**Needed from OneWiFi/rdk-wifi-hal:**
1. Support for sending off-channel action frames from STA interface
2. Ability to use bSTA MAC address as DPP enrollee MAC
3. STA mode action frame transmission capabilities

**Alternative:**
- Continue using current workaround (backhaul BSS interface)
- May require using a different MAC address for DPP enrollment

---

## Conclusion

### Answer: **YES, DPP onboarding of Mesh STA is possible, but with a workaround**

**Implementation Status:**
- ✅ DPP infrastructure fully implemented
- ✅ bSTA configuration object creation
- ✅ OneWiFi integration exists
- ✅ wifi_hal interface support
- ✅ Mesh STA callback processing
- ⚠️ Workaround for off-channel action frames

**For Production Use:**
- Current implementation works but uses backhaul BSS interface
- Full bSTA DPP support requires off-channel action frame capability in STA mode
- This limitation is noted in the code with a TODO comment
- OneWiFi/rdk-wifi-hal may need enhancement for full bSTA DPP support

**Recommendation:**
1. **Short-term:** Continue using current workaround (backhaul BSS interface for DPP)
2. **Long-term:** Enhance OneWiFi/rdk-wifi-hal to support off-channel action frames from STA interface
3. **Alternative:** Investigate if using backhaul BSS MAC is acceptable for DPP enrollment

The capability exists, the integration is there, but the ideal implementation (using bSTA MAC directly) requires additional wifi_hal support.

