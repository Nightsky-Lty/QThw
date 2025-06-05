#include "moduleinfodialog.h"
#include <QVBoxLayout>
#include <QFont>
#include <QGraphicsItem>
#include <QRegularExpression>

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
    
    info += QString("<h2>%1</h2>").arg(m_module->name());
    info += QString("<p><b>Type:</b> %1</p>").arg(getModuleTypeName(m_module->type()));
    
    if (m_module->type() == HardwareModule::MEMORY_CTRL && m_module->memoryDataWidth() > 0) {
        info += "<h3>Memory Controller Configuration</h3>";
        info += "<table border='0' cellspacing='3'>";
        info += QString("<tr><td><b>Data Width:</b></td><td>%1 bits</td></tr>").arg(m_module->memoryDataWidth());
        info += "</table>";
    } else if (m_module->type() == HardwareModule::BUS) {
        info += "<h3>Bus Configuration</h3>";
        info += "<table border='0' cellspacing='3'>";
        info += QString("<tr><td><b>Port Number:</b></td><td>%1</td></tr>").arg(m_module->busPortNumber());
        info += "</table>";
        
        if (!m_module->busPortToNodeMap().isEmpty()) {
            info += "<h4>Port to Node Mapping</h4>";
            info += "<table border='0' cellspacing='3'>";
            const QMap<int,int>& portMap = m_module->busPortToNodeMap();
            for (auto it = portMap.begin(); it != portMap.end(); ++it) {
                info += QString("<tr><td><b>Port %1:</b></td><td>Node %2</td></tr>").arg(it.key()).arg(it.value());
            }
            info += "</table>";
        }
        
        if (!m_module->busEdges().isEmpty()) {
            info += "<h4>Node Connections</h4>";
            info += "<table border='0' cellspacing='3'>";
            for (const auto& edge : m_module->busEdges()) {
                info += QString("<tr><td><b>Node %1</b></td><td>→</td><td><b>Node %2</b></td></tr>")
                        .arg(edge.first)
                        .arg(edge.second);
            }
            info += "</table>";
        }
    } else if (m_module->type() == HardwareModule::CACHE_L2) {
        info += "<h3>Cache Configuration</h3>";
        
        info += "<h4>L1 Instruction Cache</h4>";
        info += "<table border='0' cellspacing='3'>";
        info += QString("<tr><td><b>Way Count:</b></td><td>%1</td></tr>").arg(m_module->l1iConfig().wayCount);
        info += QString("<tr><td><b>Set Count:</b></td><td>%1</td></tr>").arg(m_module->l1iConfig().setCount);
        info += "</table>";
        
        info += "<h4>L1 Data Cache</h4>";
        info += "<table border='0' cellspacing='3'>";
        info += QString("<tr><td><b>Way Count:</b></td><td>%1</td></tr>").arg(m_module->l1dConfig().wayCount);
        info += QString("<tr><td><b>Set Count:</b></td><td>%1</td></tr>").arg(m_module->l1dConfig().setCount);
        info += "</table>";
        
        info += "<h4>L2 Cache</h4>";
        info += "<table border='0' cellspacing='3'>";
        info += QString("<tr><td><b>Way Count:</b></td><td>%1</td></tr>").arg(m_module->l2Config().wayCount);
        info += QString("<tr><td><b>Set Count:</b></td><td>%1</td></tr>").arg(m_module->l2Config().setCount);
        info += QString("<tr><td><b>MSHR Count:</b></td><td>%1</td></tr>").arg(m_module->l2Config().mshrCount);
        info += QString("<tr><td><b>Index Width:</b></td><td>%1</td></tr>").arg(m_module->l2Config().indexWidth);
        info += QString("<tr><td><b>Index Latency:</b></td><td>%1 cycles</td></tr>").arg(m_module->l2Config().indexLatency);
        info += "</table>";
    } else if (m_module->type() == HardwareModule::CACHE_L3) {
        info += "<h3>Cache Configuration</h3>";
        info += "<table border='0' cellspacing='3'>";
        info += QString("<tr><td><b>Way Count:</b></td><td>%1</td></tr>").arg(m_module->l3Config().wayCount);
        info += QString("<tr><td><b>Set Count:</b></td><td>%1</td></tr>").arg(m_module->l3Config().setCount);
        info += QString("<tr><td><b>MSHR Count:</b></td><td>%1</td></tr>").arg(m_module->l3Config().mshrCount);
        info += QString("<tr><td><b>Index Width:</b></td><td>%1</td></tr>").arg(m_module->l3Config().indexWidth);
        info += QString("<tr><td><b>Index Latency:</b></td><td>%1 cycles</td></tr>").arg(m_module->l3Config().indexLatency);
        info += QString("<tr><td><b>NUCA Index:</b></td><td>%1</td></tr>").arg(m_module->nucaIndex());
        info += QString("<tr><td><b>NUCA Total:</b></td><td>%1</td></tr>").arg(m_module->nucaNum());
        info += "</table>";
    }
    
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
        case HardwareModule::CACHE_EVENT_TRACER:
            return "Cache Event Tracer";
        default:
            return "Unknown Module";
    }
}

QString ModuleInfoDialog::getConnectionInfo() const
{
    QString info;
    bool hasConnections = false;

    HardwareModule* busModule = nullptr;
    for (auto it = m_moduleItems.begin(); it != m_moduleItems.end(); ++it) {
        if (it.key()->type() == HardwareModule::BUS) {
            busModule = it.key();
            break;
        }
    }

    if (!busModule) {
        return "<p>No bus module found in the system</p>";
    }

    int currentModulePort = m_module->portId();

    info += QString("<p><b>Module Port ID:</b> %1</p>").arg(currentModulePort);

    if (currentModulePort == -1) {
        if (m_module->type() == HardwareModule::CPU_CORE || m_module->type() == HardwareModule::CACHE_EVENT_TRACER || m_module->type() == HardwareModule::DMA) {
            return "<p>This module do not have direct bus connections (Port ID: -1)</p>";
        } else {
            return "<p>This module does not have a valid port ID (-1)</p>";
        }
    }

    const auto& stats = busModule->statistics();
    
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        QString key = it.key();
        
        if (key.startsWith("transmit_package_number_from_")) {
            QRegularExpression txRe("transmit_package_number_from_(\\d+)_to_(\\d+)");
            QRegularExpressionMatch txMatch = txRe.match(key);
            
            if (txMatch.hasMatch()) {
                int fromPort = txMatch.captured(1).toInt();
                int toPort = txMatch.captured(2).toInt();
                int packages = it.value();
                
                if (fromPort == currentModulePort || toPort == currentModulePort) {
                    hasConnections = true;
                    
                    bool isOutgoing = (fromPort == currentModulePort);
                    int otherPort = isOutgoing ? toPort : fromPort;
                    QString direction = isOutgoing ? "to" : "from";
                    
                    QString otherModuleName = "Unknown";
                    for (auto moduleIt = m_moduleItems.begin(); moduleIt != m_moduleItems.end(); ++moduleIt) {
                        HardwareModule* module = moduleIt.key();
                        if(module->portId() == otherPort){
                            otherModuleName = module->name();
                            break;
                        }
                    }
                    
                    info += QString("<p>Connection %1 <b>%2</b> (Packages: %3)</p>")
                        .arg(direction)
                        .arg(otherModuleName)
                        .arg(packages);
                }
            }
        }
    }

    if (!hasConnections) {
        info += "<p>No direct connections found</p>";
    }

    return info;
} 