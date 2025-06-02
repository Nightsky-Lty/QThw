#include "hardwaremodule.h"

HardwareModule::HardwareModule(ModuleType type, const QString &name, QObject *parent)
    : QObject(parent)
    , m_type(type)
    , m_name(name)
    , m_portId(-1)
    , m_busPortNumber(0)
    , m_nuca_index(0)
    , m_nuca_num(0)
    , m_memoryDataWidth(0)
{
}

HardwareModule::~HardwareModule()
{
}

void HardwareModule::setBusConfig(int portNumber, const QMap<int, int> &portToNodeMap,
                                const QVector<QPair<int, int>> &edges)
{
    m_busPortNumber = portNumber;
    m_busPortToNodeMap = portToNodeMap;
    m_busEdges = edges;
    emit configurationChanged();
}

void HardwareModule::setL2CacheConfig(const CacheConfig &l1i,
                                    const CacheConfig &l1d,
                                    const CacheConfig &l2)
{
    m_l1i = l1i;
    m_l1d = l1d;
    m_l2 = l2;
    emit configurationChanged();
}

void HardwareModule::setL3CacheConfig(const CacheConfig &l3,
                                    int nucaIndex,
                                    int nucaNum)
{
    m_l3 = l3;
    m_nuca_index = nucaIndex;
    m_nuca_num = nucaNum;
    emit configurationChanged();
}

void HardwareModule::setStatistic(const QString &key, double value)
{
    bool changed = false;
    auto it = m_statistics.find(key);
    
    if (it == m_statistics.end() || it.value() != value) {
        m_statistics[key] = value;
        changed = true;
    }

    if (changed) {
        emit statisticsChanged();
    }
}

double HardwareModule::statistic(const QString &key) const
{
    auto it = m_statistics.find(key);
    return it != m_statistics.end() ? it.value() : 0.0;
} 