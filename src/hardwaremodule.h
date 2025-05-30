#ifndef HARDWAREMODULE_H
#define HARDWAREMODULE_H

#include <QObject>
#include <QString>
#include <QPointF>
#include <QMap>
#include <QList>
#include <QVector>

// 前向声明
class Connection;

class HardwareModule : public QObject
{
    Q_OBJECT

public:
    // 硬件模块类型
    enum ModuleType {
        CPU_CORE,    // 处理器核心
        CACHE_L2,    // 二级缓存（包含L1i和L1d配置）
        CACHE_L3,    // 三级缓存
        BUS,         // 总线
        MEMORY_CTRL, // 内存控制器
        DMA,         // 直接内存访问控制器
        CACHE_EVENT_TRACE  // 缓存事件追踪器
    };

    explicit HardwareModule(ModuleType type, const QString &name, QObject *parent = nullptr);
    ~HardwareModule();

    // 基本属性访问
    ModuleType type() const { return m_type; }
    QString name() const { return m_name; }
    QPointF position() const { return m_position; }
    void setPosition(const QPointF &pos) { m_position = pos; }

    // 连接关系管理
    void addConnection(Connection *connection);
    void removeConnection(Connection *connection);
    QList<Connection*> connections() const { return m_connections; }
    QList<HardwareModule*> connectedModules() const;

    // 总线特定配置
    void setBusConfig(int portNumber, const QMap<int, int> &portToNodeMap, 
                     const QVector<QPair<int, int>> &edges);
    int busPortNumber() const { return m_busPortNumber; }
    QMap<int, int> busPortToNodeMap() const { return m_busPortToNodeMap; }
    QVector<QPair<int, int>> busEdges() const { return m_busEdges; }

    // L2缓存配置（包含L1缓存配置）
    void setL2CacheConfig(
        int l1i_way_count, int l1i_set_count,    // L1指令缓存配置
        int l1d_way_count, int l1d_set_count,    // L1数据缓存配置
        int l2_way_count, int l2_set_count,      // L2缓存配置
        int l2_mshr_count, int l2_index_width,
        int l2_index_latency
    );

    // L3缓存配置
    void setL3CacheConfig(int wayCount, int setCount, int mshrCount,
                         int indexWidth, int indexLatency,
                         int nucaIndex, int nucaNum);

    // 性能统计数据管理
    void setStatistic(const QString &key, double value);
    double statistic(const QString &key) const;
    QMap<QString, double> statistics() const { return m_statistics; }

    // 获取模块特定的统计数据键名
    QStringList availableStatistics() const;

signals:
    void statisticsChanged();
    void positionChanged();
    void configurationChanged();

private:
    ModuleType m_type;
    QString m_name;
    QPointF m_position;
    QList<Connection*> m_connections;
    QMap<QString, double> m_statistics;

    // 总线特定属性
    int m_busPortNumber;
    QMap<int, int> m_busPortToNodeMap;  // 端口到节点的映射
    QVector<QPair<int, int>> m_busEdges; // 总线拓扑边

    // L1/L2缓存特定属性
    int m_l1i_way_count;   // L1指令缓存路数
    int m_l1i_set_count;   // L1指令缓存组数
    int m_l1d_way_count;   // L1数据缓存路数
    int m_l1d_set_count;   // L1数据缓存组数
    int m_l2_way_count;    // L2缓存路数
    int m_l2_set_count;    // L2缓存组数
    int m_l2_mshr_count;   // L2 MSHR数量
    int m_l2_index_width;  // L2索引宽度
    int m_l2_index_latency;// L2索引延迟

    // L3缓存特定属性
    int m_way_count;      // 路数
    int m_set_count;      // 组数
    int m_mshr_count;     // MSHR数量
    int m_index_width;    // 索引宽度
    int m_index_latency;  // 索引延迟
    int m_nuca_index;     // NUCA索引
    int m_nuca_num;       // NUCA数量
};

// 表示模块间的连接
class Connection : public QObject
{
    Q_OBJECT

public:
    enum ConnectionType {
        DATA_BUS,        // 数据总线
        CONTROL_BUS,     // 控制总线
        MEMORY_BUS,      // 内存总线
        CACHE_BUS        // 缓存总线
    };

    Connection(HardwareModule *source, HardwareModule *target, 
              ConnectionType type = DATA_BUS, QObject *parent = nullptr);

    HardwareModule* source() const { return m_source; }
    HardwareModule* target() const { return m_target; }
    ConnectionType type() const { return m_type; }

    // 连接性能数据
    void setBandwidth(double bandwidth) { m_bandwidth = bandwidth; }
    void setUtilization(double utilization) { m_utilization = utilization; }
    void setLatency(double latency) { m_latency = latency; }
    void setPackageCount(int count) { m_packageCount = count; }
    
    double bandwidth() const { return m_bandwidth; }
    double utilization() const { return m_utilization; }
    double latency() const { return m_latency; }
    int packageCount() const { return m_packageCount; }

signals:
    void dataChanged();

private:
    HardwareModule *m_source;
    HardwareModule *m_target;
    ConnectionType m_type;
    double m_bandwidth;    // 带宽 (GB/s)
    double m_utilization;  // 利用率 (%)
    double m_latency;      // 延迟 (cycles)
    int m_packageCount;    // 传输的数据包数量
};

#endif // HARDWAREMODULE_H 