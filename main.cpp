#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <iostream>
#include <chrono>

int main() {
    // 1. 建立 Boost ASIO 的 Event Loop
    boost::asio::io_context io;

    // 2. 建立與系統 D-Bus 的連線
    auto system_bus = std::make_shared<sdbusplus::asio::connection>(io);
    
    // 宣告這個 Daemon 在 D-Bus 上的公車站牌名稱 (Service Name)
    system_bus->request_name("xyz.openbmc_project.my_heartbeat");

    // 3. 建立 Object Server (用來管理 D-Bus 物件與介面)
    sdbusplus::asio::object_server objectServer(system_bus);

    // 4. 在標準路徑下，新增標準的 Sensor.Value 介面
    std::shared_ptr<sdbusplus::asio::dbus_interface> sensorIface =
        objectServer.add_interface(
            "/xyz/openbmc_project/sensors/energy/my_heartbeat",
            "xyz.openbmc_project.Sensor.Value");

    double currentValue = 0.0;

    // 5. 註冊標準 Sensor 必須具備的 Properties (注意權限設為 readOnly)
    sensorIface->register_property("Value", currentValue,
        sdbusplus::asio::PropertyPermission::readOnly);
    
    // 單位設定為 Count (計數)
    sensorIface->register_property("Unit",
        std::string("xyz.openbmc_project.Sensor.Value.Unit.Count"),
        sdbusplus::asio::PropertyPermission::readOnly);
        
    sensorIface->register_property("MaxValue", 1000000.0,
        sdbusplus::asio::PropertyPermission::readOnly);
        
    sensorIface->register_property("MinValue", 0.0,
        sdbusplus::asio::PropertyPermission::readOnly);

    // 初始化介面，這會正式把物件推上 D-Bus
    sensorIface->initialize();

// --- 新增：1. 憑空捏造一張名為 mock_chassis 的板子 ---
    std::shared_ptr<sdbusplus::asio::dbus_interface> chassisIface =
        objectServer.add_interface(
            "/xyz/openbmc_project/inventory/system/chassis/mock_chassis",
            "xyz.openbmc_project.Inventory.Item.Board"); // 宣告這是一個 Board 介面
    chassisIface->initialize();


    // --- 新增：2. 建立 Sensor 與 Chassis 的關聯性 (Association) ---
    std::shared_ptr<sdbusplus::asio::dbus_interface> assocIface =
        objectServer.add_interface(
            "/xyz/openbmc_project/sensors/energy/my_heartbeat",
            "xyz.openbmc_project.Association.Definitions");

    // 定義關聯性：{正向關聯名稱, 反向關聯名稱, 目標路徑}
    std::vector<std::tuple<std::string, std::string, std::string>> associations = {
        {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/board/My_Mock_Board"}
    };

    assocIface->register_property("Associations", associations);
    assocIface->initialize();

    // 6. 設定每秒觸發一次的 Timer
    boost::asio::steady_timer timer(io);
    
    // 定義 Timer 的 Callback Function (Lambda 寫法)
    std::function<void(const boost::system::error_code&)> timerHandler =
        [&](const boost::system::error_code& ec) {
            if (ec) {
                std::cerr << "Timer error: " << ec.message() << "\n";
                return;
            }
            
            // 數值 +1
            currentValue += 1.0;
            
            // 關鍵：呼叫 set_property 來更新 D-Bus 上的數值，並自動觸發 PropertiesChanged 訊號
            sensorIface->set_property("Value", currentValue);

            // 重新設定 Timer，達成無窮迴圈
            timer.expires_after(std::chrono::seconds(1));
            timer.async_wait(timerHandler);
        };

    // 啟動第一次 Timer
    timer.expires_after(std::chrono::seconds(1));
    timer.async_wait(timerHandler);

    std::cout << "my-heartbeat service is running and publishing standard sensor data...\n";

    // 7. 啟動 Event Loop (程式會停在這裡持續運行)
    io.run();

    return 0;
}
