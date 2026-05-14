#include "vendor_lookup.h"

VendorLookup& VendorLookup::instance() {
    static VendorLookup inst;
    return inst;
}

VendorLookup::VendorLookup() {
    // Apple
    table_["AC:DE:48"] = "Apple";
    table_["A4:C3:F0"] = "Apple";
    table_["F8:FF:C2"] = "Apple";
    table_["00:1C:B3"] = "Apple";
    table_["3C:22:FB"] = "Apple";
    table_["DC:2B:2A"] = "Apple";

    // Samsung Electronics
    table_["F8:27:93"] = "Samsung Electronics";
    table_["00:16:32"] = "Samsung Electronics";
    table_["8C:71:F8"] = "Samsung Electronics";
    table_["CC:07:AB"] = "Samsung Electronics";
    table_["50:85:69"] = "Samsung Electronics";
    table_["E8:03:9A"] = "Samsung Electronics";

    // LG Electronics
    table_["00:1E:75"] = "LG Electronics";
    table_["C4:36:6C"] = "LG Electronics";
    table_["A8:16:D0"] = "LG Electronics";

    // Google
    table_["3C:5A:B4"] = "Google";
    table_["54:60:09"] = "Google";
    table_["F4:F5:D8"] = "Google";

    // Microsoft
    table_["00:50:F2"] = "Microsoft";
    table_["28:18:78"] = "Microsoft";
    table_["7C:1E:52"] = "Microsoft";

    // Intel (Wi-Fi 칩 다수 사용)
    table_["00:21:6A"] = "Intel";
    table_["8C:8D:28"] = "Intel";
    table_["AC:FD:CE"] = "Intel";
    table_["10:02:B5"] = "Intel";

    // Qualcomm Atheros
    table_["00:03:7F"] = "Qualcomm Atheros";
    table_["E0:91:F5"] = "Qualcomm Atheros";

    // Realtek
    table_["00:E0:4C"] = "Realtek";
    table_["28:D2:44"] = "Realtek";

    // Huawei
    table_["00:18:82"] = "Huawei";
    table_["04:BD:70"] = "Huawei";
    table_["AC:CF:85"] = "Huawei";

    // Xiaomi
    table_["00:9E:C8"] = "Xiaomi";
    table_["28:6C:07"] = "Xiaomi";
    table_["F8:A4:5F"] = "Xiaomi";

    // Sony
    table_["00:1A:80"] = "Sony";
    table_["30:17:C8"] = "Sony";
    table_["FC:0F:E6"] = "Sony";

    // Dell
    table_["00:14:22"] = "Dell";
    table_["B8:CA:3A"] = "Dell";
    table_["18:03:73"] = "Dell";

    // Lenovo
    table_["00:1A:6B"] = "Lenovo";
    table_["88:70:8C"] = "Lenovo";
    table_["10:65:30"] = "Lenovo";

    // HP
    table_["3C:D9:2B"] = "HP";
    table_["00:17:08"] = "HP";
    table_["98:E7:F4"] = "HP";

    // ASUS
    table_["00:1A:92"] = "ASUS";
    table_["AC:22:0B"] = "ASUS";
    table_["B0:6E:BF"] = "ASUS";

    // TP-Link
    table_["00:1D:0F"] = "TP-Link";
    table_["54:E6:FC"] = "TP-Link";
    table_["EC:08:6B"] = "TP-Link";

    // Cisco
    table_["00:0A:41"] = "Cisco";
    table_["00:1B:D4"] = "Cisco";
    table_["58:97:BD"] = "Cisco";

    // Raspberry Pi Foundation
    table_["B8:27:EB"] = "Raspberry Pi";
    table_["DC:A6:32"] = "Raspberry Pi";
    table_["E4:5F:01"] = "Raspberry Pi";
}

QString VendorLookup::lookup(const QString& mac) const {
    // "AA:BB:CC:DD:EE:FF" → "AA:BB:CC" 추출
    if (mac.length() < 8) return "Unknown";
    QString oui = mac.left(8).toUpper();
    return table_.value(oui, "Unknown");
}