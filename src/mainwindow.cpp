#include "mainwindow.h"
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QStyle>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QToolBar>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_visualizer(new HardwareVisualizer(this))
    , m_toolBar(new QToolBar(this))
{
    setWindowTitle("Hardware Visualizer");
    resize(1024, 768);

    createActions();
    createToolBar();
    setupInitialLayout();
    loadConfiguration();
}

MainWindow::~MainWindow()
{
    qDeleteAll(m_modules);
}

void MainWindow::createActions()
{
    // Reset
    m_resetAction = new QAction("Reset Layout", this);
    m_resetAction->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    connect(m_resetAction, &QAction::triggered, this, &MainWindow::resetToInitial);
    // Draw Connections
    m_drawLineAction = new QAction("Draw Connections", this);
    m_drawLineAction->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    connect(m_drawLineAction, &QAction::triggered, this, &MainWindow::drawConnections);
}

void MainWindow::createToolBar()
{
    addToolBar(m_toolBar);
    m_toolBar->addAction(m_resetAction);
    m_toolBar->addAction(m_drawLineAction);
}

void MainWindow::setupInitialLayout()
{
    setCentralWidget(m_visualizer);
}

void MainWindow::resetToInitial()
{
    // 清除所有模块
    m_visualizer->clearModules();
    qDeleteAll(m_modules);
    m_modules.clear();
    m_moduleMap.clear();

    // 重新加载配置
    loadConfiguration();
}

void MainWindow::drawConnections()
{
    m_visualizer->drawConnections();
}

void MainWindow::loadConfiguration()
{
    loadSetupFile("resources/setup.txt");
    loadStatisticFile("resources/statistic.txt");
    m_visualizer->autoLayout();
}

void MainWindow::loadSetupFile(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open setup file: " + filename);
        return;
    }

    QTextStream in(&file);
    QString line;
    HardwareModule* currentBus = nullptr;
    QVector<QPair<int, int>> busEdges;
    QMap<int, int> portToNodeMap;
    int portNumber = 0;
    int currentPortId = -1;  // 用于临时存储当前读取到的portId
    
    // 用于存储缓存配置
    HardwareModule::CacheConfig l1i, l1d, l2, l3;
    int nucaIndex = -1;
    int nucaNum = -1;

    // 当前正在处理的模块指针
    HardwareModule* currentModule = nullptr;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("//")) continue;

        if (line.contains("@1tick")) {
            
            QString moduleName = line.split("@").first().trimmed();
            
            // 创建对应类型的模块
            HardwareModule::ModuleType type;
            if (moduleName.startsWith("CPU")) {
                type = HardwareModule::CPU_CORE;
            } else if (moduleName.startsWith("L2Cache")) {
                type = HardwareModule::CACHE_L2;
            } else if (moduleName.startsWith("L3Cache")) {
                type = HardwareModule::CACHE_L3;
            } else if (moduleName.startsWith("Bus")) {
                type = HardwareModule::BUS;
            } else if (moduleName.startsWith("MemoryNode")) {
                type = HardwareModule::MEMORY_CTRL;
            } else if (moduleName.startsWith("DMA")) {
                type = HardwareModule::DMA;
            } else if (moduleName == "cache_event_trace") {
                type = HardwareModule::CACHE_EVENT_TRACER;
            } else {
                continue;
            }
            
            auto module = new HardwareModule(type, moduleName, this);
            currentPortId = -1;
            m_modules.append(module);
            m_moduleMap[moduleName] = module;
            m_visualizer->addModule(module);
            
            // 更新当前处理的模块
            currentModule = module;
            
            if (type == HardwareModule::BUS) {
                currentBus = module;
            }
        } else if (line.startsWith("node_number:")) {
            portNumber = line.split(":").last().trimmed().toInt();
        } else if (line.startsWith("node_id_of_port_")) {
            QRegularExpression re("node_id_of_port_(\\d+):\\s*(\\d+)");
            auto match = re.match(line);
            if (match.hasMatch()) {
                int port = match.captured(1).toInt();
                int node = match.captured(2).toInt();
                portToNodeMap[port] = node;
            }
        } else if (line.startsWith("edge:")) {
            QRegularExpression re("edge:\\s*(\\d+)\\s*to\\s*(\\d+)");
            auto match = re.match(line);
            if (match.hasMatch()) {
                int from = match.captured(1).toInt();
                int to = match.captured(2).toInt();
                busEdges.append({from, to});
            }
        } else if (line.startsWith("port_id:")) {
            currentPortId = line.split(":").last().trimmed().toInt();
            if (currentModule) {
                currentModule->setPortId(currentPortId);
            }
        } else if (currentModule) {
            // 处理L3缓存配置
            if (currentModule->type() == HardwareModule::CACHE_L3) {
                if (line.startsWith("way_count:")) {
                    l3.wayCount = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("set_count:")) {
                    l3.setCount = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("mshr_count:")) {
                    l3.mshrCount = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("index_width:")) {
                    l3.indexWidth = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("index_latency:")) {
                    l3.indexLatency = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("nuca_index:")) {
                    nucaIndex = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("nuca_num:")) {
                    nucaNum = line.split(":").last().trimmed().toInt();
                    
                    // 如果读取了完整的配置，则设置L3缓存
                    if (l3.wayCount > 0 && l3.setCount > 0 && nucaIndex >= 0 && nucaNum > 0) {
                        currentModule->setL3CacheConfig(l3, nucaIndex, nucaNum);
                        // 重置配置，为下一个模块做准备
                        l3 = HardwareModule::CacheConfig();
                        nucaIndex = -1;
                        nucaNum = -1;
                    }
                }
            } 
            // 处理L2缓存配置
            else if (currentModule->type() == HardwareModule::CACHE_L2) {
                if (line.startsWith("l1i_way_count:")) {
                    l1i.wayCount = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("l1i_set_count:")) {
                    l1i.setCount = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("l1d_way_count:")) {
                    l1d.wayCount = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("l1d_set_count:")) {
                    l1d.setCount = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("l2_way_count:")) {
                    l2.wayCount = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("l2_set_count:")) {
                    l2.setCount = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("l2_mshr_count:")) {
                    l2.mshrCount = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("l2_index_width:")) {
                    l2.indexWidth = line.split(":").last().trimmed().toInt();
                } else if (line.startsWith("l2_index_latency:")) {
                    l2.indexLatency = line.split(":").last().trimmed().toInt();
                    
                    // 如果读取了完整的配置，则设置L2缓存
                    if (l1i.wayCount > 0 && l1i.setCount > 0 && 
                        l1d.wayCount > 0 && l1d.setCount > 0 && 
                        l2.wayCount > 0 && l2.setCount > 0) {
                        currentModule->setL2CacheConfig(l1i, l1d, l2);
                        // 重置配置，为下一个模块做准备
                        l1i = l1d = l2 = HardwareModule::CacheConfig();
                    }
                }
            }
            // 处理内存控制器配置
            else if (currentModule->type() == HardwareModule::MEMORY_CTRL) {
                if (line.startsWith("data_width:")) {
                    int dataWidth = line.split(":").last().trimmed().toInt();
                    currentModule->setMemoryConfig(dataWidth);
                }
            }
        }
    }

    if (currentBus) {
        currentBus->setBusConfig(portNumber, portToNodeMap, busEdges);
    }

    file.close();
}

void MainWindow::loadStatisticFile(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open statistics file: " + filename);
        return;
    }

    QTextStream in(&file);
    QString currentModule;
    QMap<QString, double> currentStats;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("//")) continue;

        if (line.contains("Latency:")) {
            if (!currentModule.isEmpty() && !currentStats.isEmpty()) {
                updateModuleStatistics(currentModule, currentStats);
            }
            currentModule = line.split(" ").first();
            currentStats.clear();
            continue;
        }

        QStringList parts = line.split(":");
        if (parts.size() == 2) {
            QString key = parts[0].trimmed();
            double value = parts[1].trimmed().toDouble();
            currentStats[key] = value;
        }
    }

    // 处理最后一个模块的统计信息
    if (!currentModule.isEmpty() && !currentStats.isEmpty()) {
        updateModuleStatistics(currentModule, currentStats);
    }

    file.close();
}

void MainWindow::updateModuleStatistics(const QString& moduleName, const QMap<QString, double>& stats)
{
    if (auto module = m_moduleMap.value(moduleName)) {
        for (auto it = stats.begin(); it != stats.end(); ++it) {
            module->setStatistic(it.key(), it.value());
        }
    }
}