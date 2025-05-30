#include "hardwaremodule.h"

HardwareModule::HardwareModule(ModuleType type, const QString &name, QObject *parent)
    : QObject(parent)
    , m_type(type)
    , m_name(name)
    , m_busPortNumber(0)
    // L1/L2缓存属性初始化
    , m_l1i_way_count(0)
    , m_l1i_set_count(0)
    , m_l1d_way_count(0)
    , m_l1d_set_count(0)
    , m_l2_way_count(0)
    , m_l2_set_count(0)
    , m_l2_mshr_count(0)
    , m_l2_index_width(0)
    , m_l2_index_latency(0)
    // L3缓存属性初始化
    , m_way_count(0)
    , m_set_count(0)
    , m_mshr_count(0)
    , m_index_width(0)
    , m_index_latency(0)
    , m_nuca_index(0)
    , m_nuca_num(0)
{
}

HardwareModule::~HardwareModule()
{
    // 清理所有连接
    for (Connection *conn : m_connections) {
        delete conn;
    }
    m_connections.clear();
}

void HardwareModule::addConnection(Connection *connection)
{
    if (!m_connections.contains(connection)) {
        m_connections.append(connection);
    }
}

void HardwareModule::removeConnection(Connection *connection)
{
    m_connections.removeOne(connection);
}

QList<HardwareModule*> HardwareModule::connectedModules() const
{
    QList<HardwareModule*> modules;
    for (Connection *conn : m_connections) {
        if (conn->source() == this) {
            modules.append(conn->target());
        } else if (conn->target() == this) {
            modules.append(conn->source());
        }
    }
    return modules;
}

void HardwareModule::setBusConfig(int portNumber, const QMap<int, int> &portToNodeMap,
                                const QVector<QPair<int, int>> &edges)
{
    if (m_type != BUS) {
        return;
    }
    
    m_busPortNumber = portNumber;
    m_busPortToNodeMap = portToNodeMap;
    m_busEdges = edges;
    
    emit configurationChanged();
}

void HardwareModule::setL2CacheConfig(
    int l1i_way_count, int l1i_set_count,
    int l1d_way_count, int l1d_set_count,
    int l2_way_count, int l2_set_count,
    int l2_mshr_count, int l2_index_width,
    int l2_index_latency)
{
    if (m_type != CACHE_L2) {
        return;
    }

    // 设置L1指令缓存配置
    m_l1i_way_count = l1i_way_count;
    m_l1i_set_count = l1i_set_count;

    // 设置L1数据缓存配置
    m_l1d_way_count = l1d_way_count;
    m_l1d_set_count = l1d_set_count;

    // 设置L2缓存配置
    m_l2_way_count = l2_way_count;
    m_l2_set_count = l2_set_count;
    m_l2_mshr_count = l2_mshr_count;
    m_l2_index_width = l2_index_width;
    m_l2_index_latency = l2_index_latency;

    emit configurationChanged();
}

void HardwareModule::setL3CacheConfig(int wayCount, int setCount, int mshrCount,
                                    int indexWidth, int indexLatency,
                                    int nucaIndex, int nucaNum)
{
    if (m_type != CACHE_L3) {
        return;
    }

    m_way_count = wayCount;
    m_set_count = setCount;
    m_mshr_count = mshrCount;
    m_index_width = indexWidth;
    m_index_latency = indexLatency;
    m_nuca_index = nucaIndex;
    m_nuca_num = nucaNum;

    emit configurationChanged();
}

void HardwareModule::setStatistic(const QString &key, double value)
{
    if (m_statistics[key] != value) {
        m_statistics[key] = value;
        emit statisticsChanged();
    }
}

double HardwareModule::statistic(const QString &key) const
{
    return m_statistics.value(key, 0.0);
}

QStringList HardwareModule::availableStatistics() const
{
    // 根据模块类型返回可用的统计数据键名
    QStringList stats;
    switch (m_type) {
        case CPU_CORE:
            stats << "total_tick_processed" << "finished_inst_count"
                  << "ld_cache_miss_count" << "ld_cache_hit_count"
                  << "ld_inst_cnt" << "ld_mem_tick_sum"
                  << "st_cache_miss_count" << "st_cache_hit_count"
                  << "st_inst_cnt" << "st_mem_tick_sum";
            break;
            
        case CACHE_L2:
            stats << "l1i_hit_count" << "l1i_miss_count"
                  << "l1d_hit_count" << "l1d_miss_count"
                  << "l2_hit_count" << "l2_miss_count";
            break;
            
        case CACHE_L3:
            stats << "llc_hit_count" << "llc_miss_count";
            break;
            
        case BUS:
            stats << "transmit_package_number" << "avg_transmit_latency";
            // 添加节点相关统计
            for (int i = 0; i < m_busPortNumber; ++i) {
                stats << QString("node_%1_transmit_package_number").arg(i)
                      << QString("node_%1_busy_rate").arg(i);
            }
            // 添加边相关统计
            for (const auto &edge : m_busEdges) {
                stats << QString("edge_%1_to_%2_busy_rate").arg(edge.first).arg(edge.second);
            }
            break;
            
        case MEMORY_CTRL:
            stats << "message_precossed" << "busy_rate";
            break;
            
        case CACHE_EVENT_TRACE:
            stats << "l1miss_l2hit_cnt" << "l1miss_l2hit_tick"
                  << "l1miss_l2miss_l3hit_cnt" << "l1miss_l2miss_l3hit_tick"
                  << "l1miss_l2miss_l3forward_cnt" << "l1miss_l2miss_l3forward_tick"
                  << "l1miss_l2miss_l3miss_cnt" << "l1miss_l2miss_l3miss_tick"
                  << "l1miss_l2miss_l1_l2_avg" << "l1miss_l2miss_l2_l3_avg"
                  << "l1miss_l2miss_l3_l2_avg" << "l1miss_l2miss_l2_l1_avg"
                  << "l1miss_l2miss_l3forward_l1_l2_avg" << "l1miss_l2miss_l3forward_l2_l3_avg"
                  << "l1miss_l2miss_l3forward_l3_ol2_avg" << "l1miss_l2miss_l3forward_ol2_l2_avg"
                  << "l1miss_l2miss_l3forward_l2_l1_avg";
            break;
            
        case DMA:
            // DMA目前没有统计数据
            break;
    }
    return stats;
}

// Connection class implementation
Connection::Connection(HardwareModule *source, HardwareModule *target,
                     ConnectionType type, QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_target(target)
    , m_type(type)
    , m_bandwidth(0.0)
    , m_utilization(0.0)
    , m_latency(0.0)
    , m_packageCount(0)
{
    // 将连接添加到源和目标模块
    if (m_source) {
        m_source->addConnection(this);
    }
    if (m_target) {
        m_target->addConnection(this);
    }
} 