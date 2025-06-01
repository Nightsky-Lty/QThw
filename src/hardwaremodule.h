#ifndef HARDWAREMODULE_H
#define HARDWAREMODULE_H

#include <QObject>
#include <QString>
#include <QPointF>
#include <QMap>
#include <QVector>

class HardwareModule : public QObject
{
    Q_OBJECT

public:
    // 硬件模块类型（数据文件中出现的类型）
    enum ModuleType {
        CPU_CORE,    // CPU0-CPU3
        CACHE_L2,    // L2Cache0-L2Cache3
        CACHE_L3,    // L3Cache0-L3Cache3
        BUS,         // Bus
        MEMORY_CTRL, // MemoryNode0
        DMA          // DMA
    };

    // 构造函数
    explicit HardwareModule(ModuleType type, const QString &name, QObject *parent = nullptr);
    ~HardwareModule();

    // 基本属性访问
    ModuleType type() const { return m_type; }
    QString name() const { return m_name; }
    QPointF position() const { return m_position; }
    void setPosition(const QPointF &pos) { 
        if (m_position != pos) {
            m_position = pos;
            emit positionChanged(pos);
        }
    }

    // 总线配置
    void setBusConfig(int portNumber, const QMap<int, int> &portToNodeMap,
                     const QVector<QPair<int, int>> &edges);
    int busPortNumber() const { return m_busPortNumber; }
    QMap<int, int> busPortToNodeMap() const { return m_busPortToNodeMap; }
    QVector<QPair<int, int>> busEdges() const { return m_busEdges; }

    // 缓存配置
    struct CacheConfig {
        int wayCount;
        int setCount;
        int mshrCount;
        int indexWidth;
        int indexLatency;
    };

    void setL2CacheConfig(const CacheConfig &l1i,
                         const CacheConfig &l1d,
                         const CacheConfig &l2);

    void setL3CacheConfig(const CacheConfig &l3,
                         int nucaIndex,
                         int nucaNum);

    // 性能统计
    void setStatistic(const QString &key, double value);
    double statistic(const QString &key) const;
    QMap<QString, double> statistics() const { return m_statistics; }

signals:
    void configurationChanged();
    void statisticsChanged();
    void positionChanged(const QPointF &newPos);

private:
    ModuleType m_type;
    QString m_name;
    QPointF m_position;
    QMap<QString, double> m_statistics;

    // 总线属性
    int m_busPortNumber;
    QMap<int, int> m_busPortToNodeMap;
    QVector<QPair<int, int>> m_busEdges;

    // 缓存配置
    CacheConfig m_l1i;
    CacheConfig m_l1d;
    CacheConfig m_l2;
    CacheConfig m_l3;
    int m_nuca_index;
    int m_nuca_num;
};

#endif // HARDWAREMODULE_H 