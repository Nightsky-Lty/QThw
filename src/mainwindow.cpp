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

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("//")) continue;

        if (line.startsWith("port_id:")) {  // 新增：读取portId
            currentPortId = line.split(":").last().trimmed().toInt();
            continue;
        }

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
            } else {
                continue;
            }
            
            auto module = new HardwareModule(type, moduleName, this);
            if (currentPortId != -1) {  // 新增：设置portId
                module->setPortId(currentPortId);
                currentPortId = -1;  // 重置currentPortId
            }
            m_modules.append(module);
            m_moduleMap[moduleName] = module;
            m_visualizer->addModule(module);
            
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