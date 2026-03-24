project('my-heartbeat', 'cpp',
    version: '1.0',
    default_options: [
        'cpp_std=c++23',
        'warning_level=3',
    ]
)

# 1. 尋找依賴函式庫
sdbusplus_dep = dependency('sdbusplus')
boost_dep = dependency('boost')

# 2. 處理功能開關 (如果有打開，就產生巨集)
conf_data = configuration_data()
if get_option('enable-debug-log')
    conf_data.set('ENABLE_DEBUG_LOG', 1)
endif

# 產生 config.h 讓 C++ 可以 #include "config.h"
configure_file(output: 'config.h', configuration: conf_data)

# 3. 編譯執行檔並安裝
executable(
    'my-heartbeat',
    'main.cpp', # 記得你的 main.cpp 也要在這個資料夾裡喔！
    dependencies: [sdbusplus_dep, boost_dep],
    install: true
)