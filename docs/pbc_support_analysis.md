# PBC (Push Button Configuration) Support Analysis

## Question: Does the agent code have logic to receive PBC requests from the underlying stack?

### Answer: **NO**

The agent code **does NOT** currently have logic to receive PBC (Push Button Configuration) requests from the underlying stack (OneWiFi/rdk-wifi-hal).

---

## Findings

### ✅ What Exists: PBC Configuration Method Defines

**File:** `inc/em_base.h:141-144`

```cpp
/* Config Methods */
#define EM_CONFIG_PUSHBUTTON 0x0080
#define EM_CONFIG_VIRT_PUSHBUTTON 0x0280
#define EM_CONFIG_PHY_PUSHBUTTON 0x0480
```

**Purpose:** These defines are part of the WPS (Wi-Fi Protected Setup) / WSC (Wi-Fi Simple Configuration) standard configuration methods. They are used to advertise that a device supports PBC as a configuration method in WSC M1/M2 messages.

**Usage Context:**
- `EM_CONFIG_PUSHBUTTON` (0x0080) - General push button
- `EM_CONFIG_VIRT_PUSHBUTTON` (0x0280) - Virtual push button (software/UI button)
- `EM_CONFIG_PHY_PUSHBUTTON` (0x0480) - Physical push button (hardware button)

**Files Using These Defines:**
- `src/em/config/em_configuration.cpp:2044-2050` - Encoding config methods in WSC messages

```cpp
// config methods   
attr = reinterpret_cast<data_elem_attr_t *> (tmp);
attr->id = htons(attr_id_cfg_methods);
size = sizeof(unsigned short);
attr->len = htons(size);
//memcpy(attr->val, &get_device_info()->sec_1905.cfg_methods, size);
```

### ❌ What's Missing: PBC Event Handler

#### 1. No PBC Bus Event Type

**File:** `inc/em_base.h:2638-2698`

**Available Bus Event Types:**
```cpp
typedef enum {
    em_bus_event_type_none,
    em_bus_event_type_chirp,
    em_bus_event_type_reset,
    em_bus_event_type_dev_test,
    em_bus_event_type_set_dev_test,
    em_bus_event_type_get_network,
    em_bus_event_type_get_device,
    em_bus_event_type_remove_device,
    em_bus_event_type_get_radio,
    em_bus_event_type_get_ssid,
    em_bus_event_type_set_ssid,
    em_bus_event_type_get_channel,
    em_bus_event_type_set_channel,
    em_bus_event_type_scan_channel,
    em_bus_event_type_scan_result,
    em_bus_event_type_get_bss,
    em_bus_event_type_get_sta,
    em_bus_event_type_steer_sta,
    em_bus_event_type_disassoc_sta,
    em_bus_event_type_get_policy,
    em_bus_event_type_set_policy,
    em_bus_event_type_btm_sta,
    em_bus_event_type_start_dpp,
    em_bus_event_type_dev_init,
    em_bus_event_type_cfg_renew,
    em_bus_event_type_radio_config,
    em_bus_event_type_vap_config,
    em_bus_event_type_sta_list,
    em_bus_event_type_ap_cap_query,
    em_bus_event_type_client_cap_query,
    em_bus_event_type_listener_stop,
    em_bus_event_type_dm_commit,
    em_bus_event_type_m2_tx,
    em_bus_event_type_topo_sync,
    em_bus_event_type_onewifi_private_cb,
    em_bus_event_type_onewifi_mesh_sta_cb,
    em_bus_event_type_onewifi_radio_cb,
    em_bus_event_type_m2ctrl_configuration,
    em_bus_event_type_sta_assoc,
    em_bus_event_type_channel_pref_query,
    em_bus_event_type_channel_sel_req,
    em_bus_event_type_sta_link_metrics,
    em_bus_event_type_set_radio,
    em_bus_event_type_bss_tm_req,
    em_bus_event_type_btm_response,
    em_bus_event_type_channel_scan_params,
    em_bus_event_type_get_mld_config,
    em_bus_event_type_mld_reconfig,
    em_bus_event_type_beacon_report,
    em_bus_event_type_recv_wfa_action_frame,
    em_bus_event_type_recv_gas_frame,
    em_bus_event_type_get_sta_client_type,
    em_bus_event_type_assoc_status,
    em_bus_event_type_ap_metrics_report,
    em_bus_event_type_bss_info,
    em_bus_event_type_get_reset,
    em_bus_event_type_recv_csa_beacon_frame,
    em_bus_event_type_bsta_cap_req,
    em_bus_event_type_max
} em_bus_event_type_t;
```

**Missing Event Types:**
- `em_bus_event_type_pbc` - NOT defined
- `em_bus_event_type_wps_pbc` - NOT defined
- `em_bus_event_type_pushbutton` - NOT defined

#### 2. No PBC Handler in Agent

**File:** `src/agent/em_agent.cpp:786-888`

**Function:** `em_agent_t::handle_bus_event()`

**Current Event Handlers:**
```cpp
void em_agent_t::handle_bus_event(em_bus_event_t *evt) {   
    switch (evt->type) {
        case em_bus_event_type_dev_init:
            handle_dev_init(evt);
            break;
        case em_bus_event_type_cfg_renew:
            handle_autoconfig_renew(evt);
            break;
        case em_bus_event_type_radio_config:
            handle_radio_config(evt);
            break;
        case em_bus_event_type_vap_config:
            handle_vap_config(evt);
            break;
        case em_bus_event_type_sta_list:
            handle_sta_list(evt);
            break;
        case em_bus_event_type_ap_cap_query:
            handle_ap_cap_query(evt);
            break;
        case em_bus_event_type_m2ctrl_configuration:
            handle_m2ctrl_configuration(evt);
            break;
        case em_bus_event_type_onewifi_private_cb:
            handle_onewifi_private_cb(evt);
            break;
        case em_bus_event_type_onewifi_mesh_sta_cb:
            handle_onewifi_mesh_sta_cb(evt);
            break;
        case em_bus_event_type_onewifi_radio_cb:
            handle_onewifi_radio_cb(evt);
            break;
        case em_bus_event_type_channel_pref_query:
            handle_channel_pref_query(evt);
            break;
        case em_bus_event_type_channel_sel_req:
            handle_channel_sel_req(evt);
            break;
        case em_bus_event_type_recv_csa_beacon_frame:
            handle_csa_beacon_frame(evt);
            break;
        case em_bus_event_type_sta_link_metrics:
            handle_sta_link_metrics(evt);
            break;
        case em_bus_event_type_bss_tm_req:
            handle_btm_request_action_frame(evt);
            break;
        case em_bus_event_type_btm_response:
            handle_btm_response_action_frame(evt);
            break;
        case em_bus_event_type_channel_scan_params:
            handle_channel_scan_params(evt);
            break;
        case em_bus_event_type_scan_result:
            handle_channel_scan_result(evt);
            break;
        case em_bus_event_type_set_policy:
            handle_set_policy(evt);
            break;
        case em_bus_event_type_beacon_report:
            handle_beacon_report(evt);
            break;
        case em_bus_event_type_recv_wfa_action_frame:
            handle_recv_wfa_action_frame(evt);
            break;
        case em_bus_event_type_recv_gas_frame:
            handle_recv_gas_frame(evt);
            break;
        case em_bus_event_type_assoc_status:
            handle_recv_assoc_status(evt);
            break;
        case em_bus_event_type_ap_metrics_report:
            handle_ap_metrics_report(evt);
            break;
        case em_bus_event_type_bss_info:
            handle_bss_info(evt);
            break;
        default:
            break;
    }    
}
```

**Missing Handlers:**
- `handle_pbc()` - NOT present
- `handle_wps_pbc()` - NOT present
- `handle_pushbutton_config()` - NOT present

#### 3. No PBC Subscription from OneWiFi

**File:** `src/agent/em_agent.cpp:1113-1154`

**Function:** `em_agent_t::input_listener()`

**Current Bus Event Subscriptions:**
```cpp
// Subscribe to OneWiFi subdoc updates (private, radio, mesh_sta)
if (desc->bus_event_subs_fn(&m_bus_hdl, WIFI_WEBCONFIG_DOC_DATA_NORTH, 
                            (void *)&em_agent_t::onewifi_cb, NULL, 0) != 0) {
    printf("%s:%d bus get failed\n", __func__, __LINE__);
    return;
}

// Subscribe to STA association list
if (desc->bus_event_subs_fn(&m_bus_hdl, WIFI_WEBCONFIG_GET_ASSOC, 
                            (void *)&em_agent_t::sta_cb, NULL, 0) != 0) {
    printf("%s:%d bus get failed\n", __func__, __LINE__);
    return;
}

// Subscribe to STA link metrics
if (desc->bus_event_subs_fn(&m_bus_hdl, "Device.WiFi.EM.STALinkMetricsReport", 
                            (void *)&em_agent_t::assoc_stats_cb, NULL, 0) != 0) {
    printf("%s:%d bus get failed\n", __func__, __LINE__);
    return;
}

// Subscribe to channel scan reports
if (desc->bus_event_subs_fn(&m_bus_hdl, WIFI_EM_CHANNEL_SCAN_REPORT, 
                            (void *)&em_agent_t::channel_scan_cb, NULL, 0) != 0) {
    printf("%s:%d bus get failed\n", __func__, __LINE__);
    return;
}

// Subscribe to beacon reports
if (desc->bus_event_subs_fn(&m_bus_hdl, "Device.WiFi.EM.BeaconReport", 
                            (void *)&em_agent_t::beacon_report_cb, NULL, 0) != 0) {
    printf("%s:%d bus get failed\n", __func__, __LINE__);
    return;
}

// Subscribe to association status
if (desc->bus_event_subs_fn(&m_bus_hdl, "Device.WiFi.EM.AssociationStatus", 
                            reinterpret_cast<void *>(&em_agent_t::association_status_cb), nullptr, 0) != 0) {
    em_printfout("Failed to subscribe to 'Device.WiFi.EM.AssociationStatus'");
    return;
}

// Subscribe to BSS info (for DPP)
if (desc->bus_event_subs_fn(&m_bus_hdl, "Device.WiFi.EC.BSSInfo", 
                            reinterpret_cast<void *>(&em_agent_t::bss_info_cb), nullptr, 0) != 0) {
    em_printfout("Failed to subscribe to 'Device.WiFi.EC.BSSInfo', dynamic DPP channel list for Reconfiguration Announcement is not available");
    // This is fine, not a fatal error
}

// Subscribe to AP metrics report
if (desc->bus_event_subs_fn(&m_bus_hdl, "Device.WiFi.EM.APMetricsReport", 
                            (void *)&em_agent_t::ap_metrics_report_cb, NULL, 0) != 0) {
    printf("%s:%d bus get failed\n", __func__, __LINE__);
    return;
}

// Subscribe to CSA beacon frames
if(desc->bus_event_subs_fn(&m_bus_hdl, "Device.WiFi.CSABeaconFrameRecieved", 
                           (void *)&em_agent_t::mgmt_csa_beacon_frame_cb, NULL, 0) != 0) {
    printf("%s:%d bus get failed\n",__func__,__LINE__);
    return;
}

// Subscribe to management action frames per VAP
for (unsigned int i = 0; i < MAX_NUM_BSSS; i++) {
    std::string path = "Device.WiFi.AccessPoint." + std::to_string(vap_index + 1) + ".RawFrame.Mgmt.Action.Rx";
    if (desc->bus_event_subs_fn(&m_bus_hdl, path.c_str(), 
                                (void *)&em_agent_t::mgmt_action_frame_cb, NULL, 0) != 0) {
        em_printfout("Subscription failed for path %s", path.c_str());
        continue;
    }
}
```

**Missing Subscriptions:**
- No subscription to `Device.WiFi.WPS.PushButton`
- No subscription to `Device.WiFi.WPS.Event`
- No subscription to any WPS/PBC related event path

#### 4. No PBC Handling in OneWiFi Callback

**File:** `src/agent/em_agent.cpp:1307-1346`

**Function:** `em_agent_t::onewifi_cb()`

**Current Subdoc Processing:**
```cpp
void em_agent_t::onewifi_cb(char *event_name, raw_data_t *data, void *userData) {
    const char *json_data = (char *)data->raw_data.bytes;
    cJSON *json = cJSON_Parse(json_data);

    cJSON *subdoc_name = cJSON_GetObjectItemCaseSensitive(json, "SubDocName");
    
    if ((strcmp(subdoc_name->valuestring, "private") == 0) || 
        (strcmp(subdoc_name->valuestring, "Vap_6G") == 0) ||
        (strcmp(subdoc_name->valuestring, "Vap_5G") == 0) || 
        (strcmp(subdoc_name->valuestring, "Vap_2.4G") == 0)) {
        // Handle private/fronthaul VAP config
        g_agent.io_process(em_bus_event_type_onewifi_private_cb, 
                          (unsigned char *)data->raw_data.bytes, data->raw_data_len);
    }
    else if ((strcmp(subdoc_name->valuestring, "radio") == 0) || 
             (strcmp(subdoc_name->valuestring, "radio_6G") == 0) ||
             (strcmp(subdoc_name->valuestring, "radio_5G") == 0) || 
             (strcmp(subdoc_name->valuestring, "radio_2.4G") == 0)) {
        // Handle radio config
        g_agent.io_process(em_bus_event_type_onewifi_radio_cb, 
                          (unsigned char *)data->raw_data.bytes, data->raw_data_len);
    }
    else if ((strcmp(subdoc_name->valuestring, "mesh_sta") == 0) || 
             (strcmp(subdoc_name->valuestring, "mesh backhaul sta") == 0)) {
        // Handle mesh STA config
        g_agent.io_process(em_bus_event_type_onewifi_mesh_sta_cb, 
                          (unsigned char *)data->raw_data.bytes, data->raw_data_len);
    }
    else {
        em_printfout("SubDocName (%s) not matching private, mesh_sta, or radio", 
                     subdoc_name->valuestring);
    }
}
```

**Missing Subdoc Handling:**
- No check for "wps" subdoc
- No check for "pbc" subdoc
- No check for "pushbutton" subdoc

---

## Current Onboarding Methods Supported

### 1. Standard WSC (Wi-Fi Simple Configuration)

**Trigger:** `em_cmd_type_dev_init` or `em_cmd_type_cfg_renew`

**Flow:**
1. Agent sends Autoconfig Search (multicast)
2. Controller responds with Autoconfig Response
3. Agent sends WSC M1 (with device capabilities)
4. Controller sends WSC M2 (with network credentials)
5. Agent configures OneWifi with received credentials

**Files:**
- `src/em/config/em_configuration.cpp:3215-3612` - WSC M1/M2 message creation and handling
- `src/agent/em_agent.cpp:135-199` - Device initialization handler

### 2. DPP (Device Provisioning Protocol / EasyConnect)

**Trigger:** `em_cmd_type_start_dpp` or `--start-dpp-onboard` CLI flag

**Flow:**
1. Agent sends DPP chirp in Autoconfig Search
2. Controller responds with DPP chirp
3. DPP authentication (public key exchange)
4. DPP configuration (network credentials)
5. Agent configures OneWifi with DPP configuration

**Files:**
- `src/em/prov/easyconnect/ec_enrollee.cpp` - DPP enrollee implementation
- `src/agent/em_agent.cpp:1804-1854` - DPP onboarding initialization

### 3. Manual Configuration

**Trigger:** TR-181/CLI commands or OneWiFi subdoc updates

**Methods:**
- CLI: `set_ssid`, `set_channel` commands
- TR-181: SETSSID command
- OneWiFi: Direct subdoc updates

**Files:**
- `src/cli/meshViews.go` - CLI command definitions
- `src/ctrl/TR_181/tr_181_cmd_setssid.cpp` - TR-181 SSID configuration

---

## What Would Be Needed for PBC Support

### 1. Define PBC Bus Event Type

**File:** `inc/em_base.h` (add to em_bus_event_type_t enum)

```cpp
typedef enum {
    // ... existing event types ...
    em_bus_event_type_wps_pbc,    // New: WPS PBC button press event
    em_bus_event_type_max
} em_bus_event_type_t;
```

### 2. Add PBC Event Handler

**File:** `src/agent/em_agent.cpp`

```cpp
void em_agent_t::handle_wps_pbc(em_bus_event_t *evt) {
    em_cmd_t *pcmd[EM_MAX_CMD] = {NULL};
    unsigned int num;
    
    // Parse PBC event data
    // Determine if this is physical or virtual button press
    // Initiate WSC M1 exchange with PBC configuration method
    
    if ((num = m_data_model.analyze_pbc_event(evt, pcmd)) == 0) {
        printf("analyze_pbc_event completed\n");
    } else if (m_orch->submit_commands(pcmd, num) > 0) {
        printf("submitted PBC command for orchestration\n");
    }
}

// Add case to handle_bus_event()
void em_agent_t::handle_bus_event(em_bus_event_t *evt) {   
    switch (evt->type) {
        // ... existing cases ...
        
        case em_bus_event_type_wps_pbc:
            handle_wps_pbc(evt);
            break;
            
        // ... rest of cases ...
    }    
}
```

### 3. Subscribe to PBC Events from OneWiFi

**File:** `src/agent/em_agent.cpp` (in input_listener())

```cpp
// Subscribe to WPS PBC events
if (desc->bus_event_subs_fn(&m_bus_hdl, "Device.WiFi.WPS.PushButton", 
                            (void *)&em_agent_t::wps_pbc_cb, NULL, 0) != 0) {
    printf("%s:%d WPS PBC subscription failed\n", __func__, __LINE__);
    // This is optional, not fatal
}
```

### 4. Add PBC Callback

**File:** `src/agent/em_agent.cpp`

```cpp
int em_agent_t::wps_pbc_cb(char *event_name, raw_data_t *data, void *userData) {
    (void)userData;
    
    printf("%s:%d WPS PBC event received\n", __func__, __LINE__);
    
    // Parse PBC event data
    // Determine button type (physical/virtual)
    // Trigger autoconfig renew with PBC method
    
    g_agent.io_process(em_bus_event_type_wps_pbc, 
                      (unsigned char *)data->raw_data.bytes, 
                      data->raw_data_len);
    
    return 0;
}
```

### 5. Update WSC M1 to Include PBC Method

**File:** `src/em/config/em_configuration.cpp`

```cpp
int em_configuration_t::create_autoconfig_wsc_m1_msg(unsigned char *buff, 
                                                      em_freq_band_t band, 
                                                      unsigned char *dst, 
                                                      unsigned short msg_id,
                                                      bool use_pbc) {
    // ... existing code ...
    
    // config methods   
    attr = reinterpret_cast<data_elem_attr_t *> (tmp);
    attr->id = htons(attr_id_cfg_methods);
    size = sizeof(unsigned short);
    attr->len = htons(size);
    
    // Set config method based on PBC flag
    unsigned short config_method = use_pbc ? 
        htons(EM_CONFIG_PUSHBUTTON) : 
        htons(get_device_info()->sec_1905.cfg_methods);
    
    memcpy(attr->val, &config_method, size);
    
    // ... rest of function ...
}
```

### 6. OneWiFi/rdk-wifi-hal Requirements

**Expected Event Path:** `Device.WiFi.WPS.PushButton`

**Expected Event Data Format:**
```json
{
    "event": "WPSPushButton",
    "type": "physical",  // or "virtual"
    "timestamp": 1234567890,
    "interface": "wlan0"
}
```

**Required wifi_hal APIs:**
- `wifi_getWpsPushButton()` - Get PBC button state
- `wifi_setWpsPushButton()` - Trigger PBC
- Event notification mechanism for button press

---

## Summary

### Current Status

**PBC Configuration Method Support:**
- ✅ Defines exist for PBC configuration methods
- ✅ Can encode/decode PBC in WSC messages
- ✅ Can advertise PBC capability to other devices

**PBC Event Handling:**
- ❌ No bus event type for PBC
- ❌ No event handler for PBC button press
- ❌ No subscription to OneWiFi PBC events
- ❌ No callback for PBC events
- ❌ No logic to initiate onboarding on PBC press

### Conclusion

The unified-wifi-mesh agent code **does NOT have logic to receive PBC requests** from the underlying stack (OneWiFi/rdk-wifi-hal).

The PBC configuration method defines exist only for advertising capability in WSC messages, but there is no active event handling or callback mechanism to trigger onboarding when a physical or virtual push button is pressed.

**Onboarding Methods Currently Supported:**
1. ✅ Standard WSC (manual trigger via dev_init/cfg_renew)
2. ✅ DPP/EasyConnect (manual trigger via start_dpp)
3. ✅ Manual Configuration (CLI/TR-181 commands)
4. ❌ WPS PBC (NOT supported)

**To Add PBC Support:**
- Define `em_bus_event_type_wps_pbc` event type
- Add `handle_wps_pbc()` handler in agent
- Subscribe to OneWiFi WPS/PBC event path
- Implement PBC callback to trigger autoconfig
- Update WSC M1 to use PBC configuration method
- Require OneWiFi/rdk-wifi-hal to publish PBC events

