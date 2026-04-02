#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/server/manager.hpp>
#include <iostream>
#include <chrono>

int main() {
    // 1. 建立 Boost ASIO 的 Event Loop
    boost::asio::io_context io;

    // 2. 建立與系統 D-Bus 的連線
    auto system_bus = std::make_shared<sdbusplus::asio::connection>(io);
    
    // 3. 建立 Object Server，並以正確的 API 註冊 ObjectManager
    sdbusplus::asio::object_server objectServer(system_bus);
    objectServer.add_manager("/xyz/openbmc_project/sensors");

    // 改用 fan_tach (轉速) 路徑，讓 ipmitool 將其視為類比數值並顯示 RPM (適合心跳計數)
    const std::string sensorPath = "/xyz/openbmc_project/sensors/fan_tach/my_heartbeat";
    const std::string boardPath = "/xyz/openbmc_project/inventory/system/board/My_Mock_Board";

    // 4. 在標準路徑下，新增標準的 Sensor.Value 介面
    std::shared_ptr<sdbusplus::asio::dbus_interface> sensorIface =
        objectServer.add_interface(sensorPath, "xyz.openbmc_project.Sensor.Value");

    double currentValue = 0.0;

    std::shared_ptr<sdbusplus::asio::dbus_interface> thresholdIface =
        objectServer.add_interface(sensorPath, "xyz.openbmc_project.Sensor.Threshold.Critical");

    thresholdIface->register_property("CriticalHigh", 100.0);
    thresholdIface->register_property("CriticalLow", 0.0);

    // 5. 註冊標準 Sensor 必須具備的 Properties (注意權限設為 readOnly)
    sensorIface->register_property("Value", currentValue,
        sdbusplus::asio::PropertyPermission::readOnly);
    
    // 單位設定與路徑對應，改為 RPMS
    sensorIface->register_property("Unit",
        std::string("xyz.openbmc_project.Sensor.Value.Unit.RPMS"),
        sdbusplus::asio::PropertyPermission::readOnly);
        
    sensorIface->register_property("MaxValue", 255.0, // 設為 255.0，使 IPMI 轉換係數剛好為 1，消除小數點誤差
        sdbusplus::asio::PropertyPermission::readOnly);
        
    sensorIface->register_property("MinValue", 0.0,
        sdbusplus::asio::PropertyPermission::readOnly);

// --- 新增：1. 憑空捏造一張名為 mock_chassis 的板子 ---
    std::shared_ptr<sdbusplus::asio::dbus_interface> chassisIface =
        objectServer.add_interface(boardPath, "xyz.openbmc_project.Inventory.Item.Board");


    // --- 新增：2. 建立 Sensor 與 Chassis 的關聯性 (Association) ---
    std::shared_ptr<sdbusplus::asio::dbus_interface> assocIface =
        objectServer.add_interface(sensorPath, "xyz.openbmc_project.Association.Definitions");

    // 定義關聯性：{正向關聯名稱, 反向關聯名稱, 目標路徑}
    std::vector<std::tuple<std::string, std::string, std::string>> associations = {
         {"chassis", "all_sensors", boardPath}
    };

    assocIface->register_property("Associations", associations);

    std::shared_ptr<sdbusplus::asio::dbus_interface> operationalIface =
        objectServer.add_interface(sensorPath, "xyz.openbmc_project.State.Decorator.OperationalStatus");
    operationalIface->register_property("Functional", true);

    std::shared_ptr<sdbusplus::asio::dbus_interface> availabilityIface =
        objectServer.add_interface(sensorPath, "xyz.openbmc_project.State.Decorator.Availability");
    availabilityIface->register_property("Available", true);

    // --- 集中 Initialize：避免 Race Condition ---
    // 一次性將所有的 Interface 啟用，讓 ipmid 只收到一次完整的 InterfacesAdded 訊號
    sensorIface->initialize();
    thresholdIface->initialize();
    chassisIface->initialize();
    assocIface->initialize();
    operationalIface->initialize();
    availabilityIface->initialize();

    // 6. 設定每秒觸發一次的 Timer
    auto timer = std::make_shared<boost::asio::steady_timer>(io);
    
    // 安全寫法：將 std::function 宣告為 shared_ptr，避免 Lambda 捕捉參考時可能引發的 Dangling Reference
    auto timerHandler = std::make_shared<std::function<void(const boost::system::error_code&)>>();
    
    *timerHandler = [timer, sensorIface, &currentValue, timerHandler](const boost::system::error_code& ec) {
            if (ec) {
                std::cerr << "Timer error: " << ec.message() << "\n";
                return;
            }
            
            // 數值 +1
            currentValue += 1.0;
            if (currentValue >= 255.0) {
                currentValue = 0.0;
                std::cout << "[DEBUG] Value exceeded 99.0, successfully reset to 0.0!\n";
            }
            
            // 加入 Debug 訊息以確認 Timer 確實有在執行
            std::cout << "[DEBUG] Timer fired! Updating D-Bus Value to: " << currentValue << std::endl;

            // 關鍵：呼叫 set_property 來更新 D-Bus 上的數值，並自動觸發 PropertiesChanged 訊號
            sensorIface->set_property("Value", currentValue);

            // 重新設定 Timer，達成無窮迴圈
            timer->expires_after(std::chrono::seconds(1));
            timer->async_wait(*timerHandler);
        };

    // 啟動第一次 Timer
    timer->expires_after(std::chrono::seconds(1));
    timer->async_wait(*timerHandler);

    // --- 關鍵修正 2：將註冊公車站牌名稱移到所有物件初始化完畢之後 ---
    system_bus->request_name("xyz.openbmc_project.my_heartbeat");

    std::cout << "my-heartbeat service is running and publishing standard sensor data...\n";

    // 7. 啟動 Event Loop (程式會停在這裡持續運行)
    io.run();

    return 0;
}
