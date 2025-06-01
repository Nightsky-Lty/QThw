#include "moduleinfodialog.h"
#include <QVBoxLayout>
#include <QFont>
#include <QGraphicsItem>

ModuleInfoDialog::ModuleInfoDialog(HardwareModule* module, const QMap<HardwareModule*, QGraphicsItem*>& moduleItems, QWidget* parent)
    : QDialog(parent)
    , m_module(module)
    , m_moduleItems(moduleItems)
{
    setupUI();
    updateModuleInfo();
}

void ModuleInfoDialog::setupUI()
{
    setWindowTitle("Module Information");
    setMinimumSize(400, 300);

    QVBoxLayout* layout = new QVBoxLayout(this);
    
    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setOpenExternalLinks(false);
    m_textBrowser->setFont(QFont("Consolas", 10));
    
    layout->addWidget(m_textBrowser);
}

void ModuleInfoDialog::updateModuleInfo()
{
    QString info;
    
    // 添加模块基本信息
    info += QString("<h2>%1</h2>").arg(m_module->name());
    info += QString("<p><b>Type:</b> %1</p>").arg(getModuleTypeName(m_module->type()));
    
    // 添加性能统计信息
    info += "<h3>Performance Statistics</h3>";
    const auto& stats = m_module->statistics();
    if (stats.isEmpty()) {
        info += "<p>No statistics available</p>";
    } else {
        info += "<ul>";
        for (auto it = stats.begin(); it != stats.end(); ++it) {
            info += QString("<li>%1</li>").arg(formatStatistic(it.key(), it.value()));
        }
        info += "</ul>";
    }
    
    // 添加连接信息
    info += "<h3>Connection Information</h3>";
    info += getConnectionInfo();
    
    m_textBrowser->setHtml(info);
}

QString ModuleInfoDialog::formatStatistic(const QString& key, double value) const
{
    if (key.contains("hit_count") || key.contains("miss_count")) {
        return QString("%1: %2").arg(key).arg(int(value));
    } else if (key.contains("rate")) {
        return QString("%1: %2%").arg(key).arg(value * 100, 0, 'f', 1);
    } else {
        return QString("%1: %2").arg(key).arg(value, 0, 'f', 2);
    }
}

QString ModuleInfoDialog::getModuleTypeName(HardwareModule::ModuleType type) const
{
    switch (type) {
        case HardwareModule::CPU_CORE:
            return "CPU Core";
        case HardwareModule::CACHE_L2:
            return "L2 Cache";
        case HardwareModule::CACHE_L3:
            return "L3 Cache";
        case HardwareModule::BUS:
            return "System Bus";
        case HardwareModule::MEMORY_CTRL:
            return "Memory Controller";
        case HardwareModule::DMA:
            return "DMA Controller";
        default:
            return "Unknown";
    }
}

QString ModuleInfoDialog::getConnectionInfo() const
{
    QString info;
    bool hasConnections = false;

    // 遍历所有模块，查找与当前模块相关的连接
    for (auto it = m_moduleItems.begin(); it != m_moduleItems.end(); ++it) {
        HardwareModule* otherModule = it.key();
        if (otherModule == m_module || otherModule->type() != HardwareModule::BUS) {
            continue;
        }

        // 获取总线配置
        const auto& edges = otherModule->busEdges();
        const auto& portMap = otherModule->busPortToNodeMap();

        // 查找与当前模块相关的连接
        for (const auto& edge : edges) {
            int fromNodeId = edge.first;
            int toNodeId = edge.second;
            
            // 检查当前模块是否参与此连接
            bool isSource = false;
            bool isTarget = false;
            
            for (auto portIt = portMap.begin(); portIt != portMap.end(); ++portIt) {
                if (portIt.value() == fromNodeId) {
                    QString portName = QString("port_%1").arg(portIt.key());
                    if (m_module->name().contains(QString::number(portIt.key()))) {
                        isSource = true;
                    }
                }
                if (portIt.value() == toNodeId) {
                    QString portName = QString("port_%1").arg(portIt.key());
                    if (m_module->name().contains(QString::number(portIt.key()))) {
                        isTarget = true;
                    }
                }
            }

            if (isSource || isTarget) {
                hasConnections = true;
                QString direction = isSource ? "to" : "from";
                int otherNodeId = isSource ? toNodeId : fromNodeId;
                
                // 查找对应的模块名称
                QString otherModuleName = "Unknown";
                for (auto moduleIt = m_moduleItems.begin(); moduleIt != m_moduleItems.end(); ++moduleIt) {
                    HardwareModule* module = moduleIt.key();
                    if (module->name().contains(QString::number(otherNodeId))) {
                        otherModuleName = module->name();
                        break;
                    }
                }

                // 获取数据包数量
                QString key = QString("transmit_package_number_from_%1_to_%2")
                    .arg(isSource ? fromNodeId : toNodeId)
                    .arg(isSource ? toNodeId : fromNodeId);
                int packages = otherModule->statistic(key);

                info += QString("<p>Connection %1 <b>%2</b> (Packages: %3)</p>")
                    .arg(direction)
                    .arg(otherModuleName)
                    .arg(packages);
            }
        }
    }

    if (!hasConnections) {
        info += "<p>No direct connections found</p>";
    }

    return info;
} 