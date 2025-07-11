#include "cti/platform.h"

#include <pico/stdlib.h>
#include <hardware/adc.h>
#include <pico/cyw43_arch.h>

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include <string.h>

#ifndef CTI_WIFI_BUFLEN
#define CTI_WIFI_BUFLEN 1024
#endif

#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>

namespace PiPico {
    extern CTI::StreamIOImpl stdioStream;
}

namespace PicoW {

    #define CTI_IP_TYPE_DHCP 0
    #define CTI_IP_TYPE_STATIC 1

    //these fields are sized to support maximum sizes per wifi specs
    // write now this all fits in a single flash page size and more handling
    // will be needed if it extends beyond 256 bytes
    typedef struct {
        char ssid[33];
        char password[64];
        int8_t wifi_led_gpio;
        uint8_t ip_type;
        uint8_t ip[4];
        uint8_t mask[4];
        uint8_t gateway[4];
        uint8_t dns1[4];
        uint8_t dns2[4];
    } CtiWifiInfo;

    //Use last sector in flash for wifi info
    #define CTI_WIFI_INFO_OFFSET() ((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE))
    #define CTI_WIFI_INFO() ((CtiWifiInfo*)(CTI_WIFI_INFO_OFFSET() + XIP_BASE))

    //Temporary storage to be able to write a full page to flash at a time
    uint8_t _flash_pageBuf[FLASH_PAGE_SIZE]; // 256 bytes

    char _wifiBuf[CTI_WIFI_BUFLEN + 1];
    uint16_t _wifiBufSize = 0;
    uint16_t _wifiBufCur = 0;

    char _writeBuf[CTI_WIFI_BUFLEN + 1];

    tcp_pcb* _server = nullptr;
    tcp_pcb* _client = nullptr;

    bool _connected = false;

    //_info is a ram copy of the info to be manipulated before saving
    CtiWifiInfo _info = *CTI_WIFI_INFO();

    void clear_ip(uint8_t* ip) {
        ip[0] = 0;
        ip[1] = 0;
        ip[2] = 0;
        ip[3] = 0;
    }

    void reset_wifi_info() {
        for (int i = 0; i < 33; ++i) {
            _info.ssid[i] = 0;
        }

        for (int i = 0; i < 64; ++i) {
            _info.password[i] = 0;
        }

        _info.wifi_led_gpio = -1;
        _info.ip_type = CTI_IP_TYPE_DHCP;
        
        clear_ip(_info.ip);
        clear_ip(_info.mask);
        clear_ip(_info.gateway);
        clear_ip(_info.dns1);
        clear_ip(_info.dns2);
    }

    bool write_wifi_info() {
        memcpy(_flash_pageBuf, &_info, sizeof(CtiWifiInfo));

        uint32_t ints = save_and_disable_interrupts();

        flash_range_erase(CTI_WIFI_INFO_OFFSET(), FLASH_SECTOR_SIZE);
        flash_range_program(CTI_WIFI_INFO_OFFSET(), _flash_pageBuf, FLASH_PAGE_SIZE);

        restore_interrupts(ints);

        return true;
    }

    int32_t wifi_getchar(uint32_t timeout_us) {
        CTI_DEBUG(" wifi");

        uint32_t start = time_us_32();
        if (_wifiBufSize > _wifiBufCur) {
            return _wifiBuf[_wifiBufCur++];
        } else {

            //no buffered data to return, reset and poll for updates
            _wifiBufSize = 0;
            _wifiBufCur = 0;

            cyw43_arch_poll();

            //check if data was received
            if (_wifiBufSize > _wifiBufCur) {
                return _wifiBuf[_wifiBufCur++];
            }

            //approximate timeout delay if no data in buffer.
            //bit hacky but makes it play nicely with status LED etc
            uint32_t delay = timeout_us - (time_us_32() - start);
            //CTI::gPlatform.IO.Printf(" s%d", delay);
            CTI_DEBUG(" t");
            sleep_us(delay);

            return -1;
        }
    }

    int32_t wifi_vprintf(const char* fmt, va_list args) {
        if (_client == nullptr) {
            return -1;
        }

        int len = vsnprintf(_writeBuf, CTI_WIFI_BUFLEN, fmt, args);
        err_t err = tcp_write(_client, _writeBuf, len, TCP_WRITE_FLAG_COPY);
        
        if (err == ERR_OK) {
            return len;
        } else {
            return -1;
        }
    }

    void wifi_printchar(char c) {
        if (_client == nullptr) {
            return;
        }

        err_t err = tcp_write(_client, &c, 1, TCP_WRITE_FLAG_COPY);
    }

    void wifi_printstr(const char* str) {
        if (_client == nullptr) {
            return;
        }

        err_t err = tcp_write(_client, str, strlen(str), TCP_WRITE_FLAG_COPY);
    }

    void wifi_printnstr(int32_t len, const char* str) {
        if (_client == nullptr) {
            return;
        }

        err_t err = tcp_write(_client, str, len, TCP_WRITE_FLAG_COPY);
    }

    void wifi_flush() {
        err_t err = tcp_output(_client);
    }

    CTI::StreamIOImpl wifiStream = {
        wifi_vprintf,
        wifi_printchar,
        wifi_printstr,
        wifi_printnstr,
        wifi_getchar,
        wifi_flush
    };

    err_t wifi_recv(void* arg, tcp_pcb* tpcb, pbuf* p, err_t err) {
        if (!p) {
            return ERR_ABRT;
        }

        if (tpcb != _client) {
            return ERR_VAL;
        }
        const uint16_t len = p->tot_len;
        const uint16_t buffer_left = CTI_WIFI_BUFLEN - _wifiBufSize;
        const uint16_t to_read = len > buffer_left ? buffer_left : len;

        // store received buffer into local buffer which is read from in wifi_getchar
        if (len > 0) {
            _wifiBufSize += pbuf_copy_partial(p, _wifiBuf + _wifiBufSize, to_read, 0);

            tcp_recved(tpcb, len);
        }

        pbuf_free(p);

        if (len > to_read) {
            return ERR_BUF;
        }

        return ERR_OK;
    }

    void wifi_tcp_err(void* arg, err_t err) {
        tcp_close(_client);
        _connected = false;
    }

    err_t wifi_server_accept(void* arg, tcp_pcb* cpcb, err_t err) {
        if (err != ERR_OK || cpcb == nullptr || _client != nullptr) {
            return ERR_ABRT;
        }

        _client = cpcb;

        //_client->flags |= TF_NODELAY;

        tcp_recv(_client, wifi_recv);
        tcp_err(_client, wifi_tcp_err);

        _connected = true;

        return ERR_OK;
    }

    bool wifi_connect() {
        if (!_connected) {
            cyw43_arch_enable_sta_mode();

            int res = cyw43_arch_wifi_connect_timeout_ms(_info.ssid, _info.password, CYW43_AUTH_WPA2_AES_PSK, 20000);

            if (res == 0) {
                if (_server != nullptr) {
                    tcp_close(_server);
                }

                tcp_pcb* pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
                if (!pcb) {
                    cyw43_arch_disable_sta_mode();
                    return false;
                }

                pcb->flags |= TF_NODELAY;

                err_t err = tcp_bind(pcb, nullptr, 5555);
                if (err) {
                    tcp_close(pcb);
                    cyw43_arch_disable_sta_mode();
                    return false;
                }

                _server = tcp_listen_with_backlog(pcb, 1);
                if (!_server) {
                    tcp_close(pcb);
                    return false;
                }

                _server->flags |= TF_NODELAY;

                tcp_accept(_server, wifi_server_accept);

                return true;
            }
        }

        return false;
    }

    void wifi_disconnect() {
        cyw43_arch_disable_sta_mode();
    }

    void wifi_apply_ip() {

    }

} // namespace PicoW

using namespace PicoW;

void CTI::Platform::BoardInit() {
    int wifi_init = cyw43_arch_init();

    // Flag for toggling LED while in startup delay
    bool status = false;

    //TODO: Switch between UART or USB based on defines from build config
    bool usb_init = stdio_usb_init();

    //disable automatically adding CR before LF
    stdio_set_translate_crlf(&stdio_usb, false);

    if (!usb_init) {
        while (1) {
            status = !status;
            gPlatform.IO.StatusLED(status);
            sleep_ms(100);
        }
    }

    if (wifi_init != 0) {
        gPlatform.IO.StatusLED(true);
        while (1);
    }
    
    //need to give time for USB init and enumeration on host side
    for (int i = 0; i < 12; ++i) {
        sleep_ms(400);
        status = !status;
        gPlatform.IO.StatusLED(status);
    }

    setbuf(stdout, NULL);

    static StreamIOImpl* streams[CTI_NUM_STREAMS];
    streams[0] = &PiPico::stdioStream;
    streams[1] = &PicoW::wifiStream;

    gPlatform.IO.RegisterStreams(streams);

    const char* ssid = "SSID";
    const char* pw = "PASSWORD";

    gPlatform.IO.StatusLED(true);

    if (gPlatform.WIFI.Connect(ssid, strlen(ssid), pw, strlen(pw))) {
        gPlatform.IO.Printf("SSID: %s\nIP: %s\n", gPlatform.WIFI.GetSSID(), gPlatform.WIFI.GetIP());
    } else {
        gPlatform.IO.Printf("Could not connect to wifi!\n");
    }
}

CTI::PlatformWifi::PlatformWifi() {

}

const char* CTI::PlatformWifi::GetSSID() {
    return _info.ssid;
}

bool CTI::PlatformWifi::IsConnected() {
    return _connected;
}

bool CTI::PlatformWifi::Connect(const char* ssid, int ssid_len, const char* password, int pw_len) {
    if (IsConnected()) {
        //need to disconnect first
        return false;
    }

    memcpy(_info.ssid, ssid, ssid_len);
    _info.ssid[ssid_len] = 0;
    
    memcpy(_info.password, password, pw_len);
    _info.password[pw_len] = 0;

    return wifi_connect();
}

bool CTI::PlatformWifi::Disconnect() {
    wifi_disconnect();

    return true;
}

bool CTI::PlatformWifi::SetIP(const char* ip, const char* mask, const char* gateway, const char* dns1, const char* dns2) {
    ip4_addr_t addr_ip, addr_mask, addr_gw, addr_dns1, addr_dns2;

    ip4addr_aton(ip, &addr_ip);
    ip4addr_aton(mask, &addr_mask);
    ip4addr_aton(gateway, &addr_gw);
    ip4addr_aton(dns1, &addr_dns1);
    ip4addr_aton(dns2, &addr_dns2);

    memcpy(_info.ip, &addr_ip, 4);
    memcpy(_info.mask, &addr_mask, 4);
    memcpy(_info.gateway, &addr_gw, 4);
    memcpy(_info.dns1, &addr_dns1, 4);
    memcpy(_info.dns2, &addr_dns2, 4);

    _info.ip_type = CTI_IP_TYPE_STATIC;

    if (IsConnected()) {
        wifi_apply_ip();
    }

    return true;
}

char* CTI::PlatformWifi::GetIP() {
    return ip4addr_ntoa(netif_ip4_addr(netif_list));
}

bool CTI::PlatformWifi::SetDHCP() {
    return true;
}

bool CTI::PlatformWifi::PersistWifi() {
    write_wifi_info();

    return true;
}