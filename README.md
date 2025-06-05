# Hardware Visualization System

基于Qt的硬件系统可视化工具，用于展示和分析硬件模块之间的连接关系和性能数据。

## 功能特性

- 硬件模块可视化
  - 支持多种硬件类型：CPU核心、L2缓存、L3缓存、总线、内存控制器和DMA
  - 自动布局算法，合理展示模块位置
  - 可视化模块间的连接关系

## 代码文件说明

### 核心类文件

- `hardwaremodule.h/cpp`
  - 定义了硬件模块的基础类 `HardwareModule`
  - 实现了模块的基本属性和配置管理
  - 支持缓存配置（L1/L2/L3）、总线配置和性能统计数据的存储

- `hardwarevisualizer.h/cpp`
  - 实现了硬件系统的可视化界面
  - 负责模块的布局、连接线的绘制和交互处理
  - 管理模块图标、颜色和连接关系的显示
  - 实现了自动布局算法和性能数据的可视化

- `moduleinfodialog.h/cpp`
  - 实现了模块详细信息对话框
  - 展示模块的配置信息、性能统计和连接关系
  - 使用HTML格式美化信息展示
  - 支持实时更新模块状态

- `mainwindow.h/cpp`
  - 实现了主窗口界面
  - 管理工具栏和基本操作
  - 负责配置文件的读取和解析
  - 协调各个组件之间的交互

## 配置文件格式

### setup.txt

用于配置硬件模块及其连接关系。格式说明：

```
// 模块定义
port_id: 1           // 模块的端口ID
CPU0@1tick           // 模块名称@时钟周期

// 总线配置
node_number: 4       // 总线端口数量
node_id_of_port_1: 0 // 端口1连接到节点0
node_id_of_port_2: 1 // 端口2连接到节点1
edge: 1 to 2         // 端口1到端口2的连接

// 支持的模块类型：
// - CPU0-CPU3
// - L2Cache0-L2Cache3
// - L3Cache0-L3Cache3
// - Bus
// - MemoryNode0
// - DMA
```

### statistic.txt

用于记录性能统计数据。格式说明：

```
CPU0 Latency:           // 模块名称
hit_latency: 1.5       // 统计项名称: 值
miss_latency: 4.2
throughput: 2.8

L2Cache0 Latency:
cache_size: 256
hit_rate: 0.85
```

## 开发环境要求

- Qt 5.15或更高版本
- C++17或更高版本
- CMake 3.15或更高版本

## 构建和运行

1. 确保已安装Qt和CMake
2. 克隆项目并进入项目目录
3. 创建构建目录：
```bash
mkdir build
cd build
```
4. 配置和构建项目：
```bash
cmake ..
cmake --build .
```
5. 运行程序：
```bash
./hardware_visualizer
```

## 注意事项

- 确保 `resources` 目录下存在 `setup.txt` 和 `statistic.txt` 配置文件
- 配置文件的格式必须严格遵循上述规范
- 所有模块名称必须唯一
- 端口ID必须是唯一的正整数 