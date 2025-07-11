#ifndef cti_wifi_h_
#define cti_wifi_h_

namespace CTI {

class PlatformWifi {
public:
    PlatformWifi();

    const char* GetSSID();
    void SetSSID(const char* ssid);

    bool IsConnected();
    bool Connect(const char* ssid, int ssid_len, const char* password, int pw_len);
    bool Disconnect();

    bool SetIP(const char* ip, const char* mask, const char* gateway, const char* dns1, const char* dns2);
    bool SetDHCP();

    char* GetIP();

    bool PersistWifi();

private:
    const char* _ssid;
    const char* _pw;
};

}

#endif //cti_wifi_h_