#include "hardwarevisualizer.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QBrush>
#include <QPen>
#include <QColor>
#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QFontMetrics>
#include <QtMath>
#include <QDebug>
#include <QPainterPath>
#include <QRegularExpression>
#include <algorithm>

HardwareVisualizer::HardwareVisualizer(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_draggedItem(nullptr)
    , m_busModule(nullptr)
    , m_infoDialog(nullptr)
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    
    // 设置场景大小
    m_scene->setSceneRect(-500, -500, 1000, 1000);
    
    // 设置背景
    setBackgroundBrush(QBrush(QColor(240, 240, 240)));
}

HardwareVisualizer::~HardwareVisualizer()
{
    delete m_infoDialog;
    clearModules();
}

void HardwareVisualizer::addModule(HardwareModule* module)
{
    if (!module || m_moduleItems.contains(module)) {
        return;
    }

    // 如果是总线模块，保存引用
    if (module->type() == HardwareModule::BUS) {
        m_busModule = module;
    }

    QGraphicsItem* item = createModuleItem(module);
    m_moduleItems[module] = item;
    m_scene->addItem(item);
    
    // 连接信号
    connect(module, &HardwareModule::configurationChanged,
            this, [this, module]() { updateModulePosition(module); });
    connect(module, &HardwareModule::statisticsChanged,
            this, [this, module]() { updateStatistics(module); });
    connect(module, &HardwareModule::positionChanged,
            this, [this, module](const QPointF &newPos) {
                if (auto item = m_moduleItems.value(module)) {
                    if (item->pos() != newPos) {
                        item->setPos(newPos);
                        drawConnections();
                    }
                }
            });
            
    // 如果已经有总线模块，执行自动布局
    if (m_busModule) {
        autoLayout();
    }
}

QGraphicsItem* HardwareVisualizer::createModuleItem(HardwareModule* module)
{
    QGraphicsItemGroup* group = new QGraphicsItemGroup;
    
    // 创建模块主体
    QGraphicsRectItem* rect = new QGraphicsRectItem(0, 0, 150, 100);
    rect->setBrush(QBrush(getModuleColor(module->type())));
    rect->setPen(QPen(Qt::black));
    group->addToGroup(rect);
    
    // 添加模块名称
    QGraphicsTextItem* nameText = new QGraphicsTextItem(module->name());
    nameText->setDefaultTextColor(Qt::black);
    QFont nameFont = nameText->font();
    nameFont.setBold(true);
    nameFont.setPointSize(10);
    nameText->setFont(nameFont);
    nameText->setPos(5, 5);
    group->addToGroup(nameText);
    
    // 添加类型标签
    QGraphicsTextItem* typeText = new QGraphicsTextItem(getModuleTypeName(module->type()));
    typeText->setDefaultTextColor(Qt::darkGray);
    typeText->setPos(5, 25);
    group->addToGroup(typeText);

    // 添加统计信息
    QGraphicsTextItem* statsText = new QGraphicsTextItem(createStatsText(module));
    statsText->setDefaultTextColor(Qt::black);
    QFont statsFont = statsText->font();
    statsFont.setPointSize(8);
    statsText->setFont(statsFont);
    statsText->setPos(5, 45);
    group->addToGroup(statsText);
    
    group->setFlag(QGraphicsItem::ItemIsMovable);
    group->setFlag(QGraphicsItem::ItemIsSelectable);
    
    return group;
}

void HardwareVisualizer::autoLayout()
{
    if (!m_busModule) return;

    const double centerX = 0.0;
    const double centerY = 0.0;
    const double moduleWidth = 150.0;
    const double moduleHeight = 100.0;
    const double horizontalSpacing = 200.0;  // 模块之间的水平间距
    const double verticalSpacing = 150.0;    // 垂直间距

    // 计算每列的水平位置（从左到右）
    const double cpuX = centerX - horizontalSpacing * 2.5;    // CPU列
    const double l2X = centerX - horizontalSpacing * 1.5;     // L2缓存列
    const double l3X = centerX - horizontalSpacing * 0.5;     // L3缓存列
    const double busX = centerX + horizontalSpacing * 0.5;    // 总线列
    const double dmaX = centerX + horizontalSpacing * 1.5;    // DMA和内存控制器列

    // 为每个模块分配位置
    QMap<int, HardwareModule*> cpuModules;      // CPU核心 (index -> module)
    QMap<int, HardwareModule*> l2Modules;       // L2缓存 (index -> module)
    QMap<int, HardwareModule*> l3Modules;       // L3缓存 (index -> module)
    QList<HardwareModule*> busModules;          // 总线、内存控制器、DMA
    
    // 对模块进行分类
    for (auto it = m_moduleItems.begin(); it != m_moduleItems.end(); ++it) {
        HardwareModule* module = it.key();
        QString name = module->name();
        int index = -1;

        // 提取模块编号
        QRegularExpression re("\\d+");
        QRegularExpressionMatch match = re.match(name);
        if (match.hasMatch()) {
            index = match.captured(0).toInt();  // 直接使用模块名中的数字作为索引
        }
        
        switch (module->type()) {
            case HardwareModule::CPU_CORE:
                cpuModules[index] = module;
                break;
            case HardwareModule::CACHE_L2:
                l2Modules[index] = module;
                break;
            case HardwareModule::CACHE_L3:
                l3Modules[index] = module;
                break;
            case HardwareModule::BUS:
            case HardwareModule::MEMORY_CTRL:
            case HardwareModule::DMA:
                busModules.append(module);
                break;
        }
    }

    // 计算总行数（基于CPU数量）
    int rowCount = cpuModules.size();
    double startY = centerY - (rowCount - 1) * verticalSpacing / 2;

    // 布局每一行的组件（CPU、L2、L3）
    QList<int> indices = cpuModules.keys();
    std::sort(indices.begin(), indices.end());  // 确保按索引顺序布局

    for (int i = 0; i < indices.size(); ++i) {
        int index = indices[i];
        double rowY = startY + i * verticalSpacing;
        
        // 放置CPU
        if (cpuModules.contains(index)) {
            cpuModules[index]->setPosition(QPointF(cpuX, rowY));
        }
        
        // 放置L2缓存
        if (l2Modules.contains(index)) {
            l2Modules[index]->setPosition(QPointF(l2X, rowY));
        }
        
        // 放置L3缓存（每两个CPU共享一个L3）
        if (i % 2 == 0 && l3Modules.contains(index/2)) {
            double l3Y = rowY + verticalSpacing * 0.5;  // L3位于两个CPU之间
            if (i == indices.size() - 1) {  // 如果是最后一个CPU
                l3Y = rowY;  // L3直接与CPU对齐
            }
            l3Modules[index/2]->setPosition(QPointF(l3X, l3Y));
        }
    }

    // 布局总线和其他组件
    for (auto module : busModules) {
        QPointF pos;
        if (module->type() == HardwareModule::BUS) {
            pos = QPointF(busX, centerY);  // 总线在中间
        } else if (module->type() == HardwareModule::DMA) {
            pos = QPointF(dmaX, centerY - verticalSpacing * 0.5);  // DMA在上方
        } else if (module->type() == HardwareModule::MEMORY_CTRL) {
            pos = QPointF(dmaX, centerY + verticalSpacing * 0.5);  // 内存控制器在下方
        }
        module->setPosition(pos);
    }

    // 更新所有模块位置并重绘连接
    for (auto it = m_moduleItems.begin(); it != m_moduleItems.end(); ++it) {
        updateModulePosition(it.key());
    }
    drawConnections();
}

void HardwareVisualizer::updateModulePosition(HardwareModule* module)
{
    if (auto item = m_moduleItems.value(module)) {
        item->setPos(module->position());
        drawConnections();
    }
}

void HardwareVisualizer::clearModules()
{
    for (auto item : m_moduleItems.values()) {
        m_scene->removeItem(item);
        delete item;
    }
    m_moduleItems.clear();
}

void HardwareVisualizer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_draggedItem = itemAt(event->pos());
        if (m_draggedItem) {
            m_lastMousePos = mapToScene(event->pos());
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void HardwareVisualizer::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggedItem && event->buttons() & Qt::LeftButton) {
        QPointF newPos = mapToScene(event->pos());
        QPointF delta = newPos - m_lastMousePos;
        
        // 更新对应模块的位置
        for (auto it = m_moduleItems.begin(); it != m_moduleItems.end(); ++it) {
            if (it.value() == m_draggedItem) {
                // 计算新位置
                QPointF currentPos = it.key()->position();
                QPointF targetPos = currentPos + delta;
                
                // 更新模块位置（这会触发positionChanged信号）
                it.key()->setPosition(targetPos);
                break;
            }
        }
        
        m_lastMousePos = newPos;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void HardwareVisualizer::mouseReleaseEvent(QMouseEvent *event)
{
    m_draggedItem = nullptr;
    QGraphicsView::mouseReleaseEvent(event);
}

void HardwareVisualizer::wheelEvent(QWheelEvent *event)
{
    // 实现缩放
    double scaleFactor = 1.15;
    if (event->angleDelta().y() < 0) {
        scaleFactor = 1.0 / scaleFactor;
    }
    scale(scaleFactor, scaleFactor);
}

void HardwareVisualizer::drawConnections()
{
    // 清除旧的连接线
    for (QGraphicsItem* item : m_scene->items()) {
        if (item->type() == QGraphicsLineItem::Type ||
            (item->type() == QGraphicsTextItem::Type && 
             item->parentItem() == nullptr)) {
            m_scene->removeItem(item);
            delete item;
        }
    }

    if (!m_busModule) return;

    // 创建端口到模块的映射
    QMap<int, HardwareModule*> portToModule;
    for (auto it = m_moduleItems.begin(); it != m_moduleItems.end(); ++it) {
        HardwareModule* module = it.key();
        if (module->type() != HardwareModule::BUS) {
            int portId = module->portId();
            if (portId >= 0) {
                portToModule[portId] = module;
            }
        }
    }

    // 绘制连接线和数据流量
    const auto& edges = m_busModule->busEdges();
    const auto& portMap = m_busModule->busPortToNodeMap();
    
    for (const auto& edge : edges) {
        int fromNodeId = edge.first;
        int toNodeId = edge.second;
        
        // 找到连接的模块
        HardwareModule* fromModule = nullptr;
        HardwareModule* toModule = nullptr;
        
        // 通过节点ID找到对应的端口和模块
        for (auto it = portMap.begin(); it != portMap.end(); ++it) {
            if (it.value() == fromNodeId && portToModule.contains(it.key())) {
                fromModule = portToModule[it.key()];
            }
            if (it.value() == toNodeId && portToModule.contains(it.key())) {
                toModule = portToModule[it.key()];
            }
        }
        
        if (fromModule && toModule) {
            // 获取连接点
            QPointF fromPoint = getConnectionPoint(fromModule, toModule->position());
            QPointF toPoint = getConnectionPoint(toModule, fromModule->position());
            
            // 创建贝塞尔曲线路径
            QPainterPath path;
            path.moveTo(fromPoint);
            
            // 计算控制点
            double dx = (toPoint.x() - fromPoint.x()) * 0.5;
            QPointF ctrl1(fromPoint.x() + dx, fromPoint.y());
            QPointF ctrl2(toPoint.x() - dx, toPoint.y());
            
            path.cubicTo(ctrl1, ctrl2, toPoint);
            
            // 设置线条样式
            double rate = getDataTransferRate(fromModule, toModule);
            int penWidth = qMax(1, qMin(5, int(rate * 10)));
            
            QPen pen(Qt::darkGray, penWidth);
            if (rate > 0.1) {
                pen.setColor(QColor(255, 128, 0));
            }
            
            QGraphicsPathItem* pathItem = new QGraphicsPathItem(path);
            pathItem->setPen(pen);
            m_scene->addItem(pathItem);
            
            // 添加数据流量标签
            if (rate > 0) {
                QGraphicsTextItem* rateText = new QGraphicsTextItem(
                    QString::number(rate * 100, 'f', 1) + "%");
                rateText->setDefaultTextColor(Qt::blue);
                QPointF midPoint = (fromPoint + toPoint) / 2;
                rateText->setPos(midPoint);
                m_scene->addItem(rateText);
            }
        }
    }
}

double HardwareVisualizer::getDataTransferRate(HardwareModule* from, HardwareModule* to) const
{
    if (!from || !to || !m_busModule) return 0.0;
    
    // 获取端口ID
    int fromPort = -1;
    int toPort = -1;
    
    // 从模块名称获取端口ID
    auto getPortId = [](HardwareModule* module) -> int {
        switch (module->type()) {
            case HardwareModule::CPU_CORE:
                return module->name().right(1).toInt();
            case HardwareModule::CACHE_L2:
                return module->name().right(1).toInt();
            case HardwareModule::CACHE_L3:
                return module->name().right(1).toInt() + 4;
            case HardwareModule::MEMORY_CTRL:
                return 4;
            default:
                return -1;
        }
    };
    
    fromPort = getPortId(from);
    toPort = getPortId(to);
    
    if (fromPort >= 0 && toPort >= 0) {
        QString key = QString("transmit_package_number_from_%1_to_%2")
                        .arg(fromPort).arg(toPort);
        return m_busModule->statistic(key) / 1000.0;  // 归一化显示
    }
    
    return 0.0;
}

QString HardwareVisualizer::formatStatistic(const QString& key, double value) const
{
    if (key.contains("hit_count") || key.contains("miss_count")) {
        return QString("%1: %2").arg(key).arg(int(value));
    } else if (key.contains("rate")) {
        return QString("%1: %2%").arg(key).arg(value * 100, 0, 'f', 1);
    } else {
        return QString("%1: %2").arg(key).arg(value, 0, 'f', 2);
    }
}

QString HardwareVisualizer::createStatsText(HardwareModule* module) const
{
    QString text;
    const auto& stats = module->statistics();
    
    switch (module->type()) {
        case HardwareModule::CPU_CORE: {
            if (stats.contains("finished_inst_count")) {
                text += formatStatistic("Instructions", stats["finished_inst_count"]) + "\n";
            }
            if (stats.contains("ld_cache_hit_count") && stats.contains("ld_cache_miss_count")) {
                double total = stats["ld_cache_hit_count"] + stats["ld_cache_miss_count"];
                double hitRate = stats["ld_cache_hit_count"] / total;
                text += formatStatistic("Load Hit Rate", hitRate) + "\n";
            }
            break;
        }
        case HardwareModule::CACHE_L2:
        case HardwareModule::CACHE_L3: {
            if (stats.contains("llc_hit_count") && stats.contains("llc_miss_count")) {
                double total = stats["llc_hit_count"] + stats["llc_miss_count"];
                double hitRate = stats["llc_hit_count"] / total;
                text += formatStatistic("Cache Hit Rate", hitRate) + "\n";
            }
            break;
        }
        case HardwareModule::BUS: {
            if (stats.contains("transmit_package_number")) {
                text += formatStatistic("Packages", stats["transmit_package_number"]) + "\n";
            }
            if (stats.contains("avg_transmit_latency")) {
                text += formatStatistic("Avg Latency", stats["avg_transmit_latency"]) + "\n";
            }
            break;
        }
        case HardwareModule::MEMORY_CTRL: {
            if (stats.contains("message_precossed")) {
                text += formatStatistic("Processed", stats["message_precossed"]) + "\n";
            }
            if (stats.contains("busy_rate")) {
                text += formatStatistic("Busy Rate", stats["busy_rate"]) + "\n";
            }
            break;
        }
        default:
            break;
    }
    
    return text;
}

void HardwareVisualizer::updateStatistics(HardwareModule* module)
{
    if (auto item = m_moduleItems.value(module)) {
        // 更新统计信息
        for (auto child : item->childItems()) {
            if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(child)) {
                if (textItem->pos().y() >= 45) {  // 统计信息的Y位置
                    textItem->setPlainText(createStatsText(module));
                    break;
                }
            }
        }
    }
}

QColor HardwareVisualizer::getModuleColor(HardwareModule::ModuleType type) const
{
    switch (type) {
        case HardwareModule::CPU_CORE:
            return QColor(255, 200, 200); // 浅红色
        case HardwareModule::CACHE_L2:
            return QColor(200, 255, 200); // 浅绿色
        case HardwareModule::CACHE_L3:
            return QColor(200, 200, 255); // 浅蓝色
        case HardwareModule::BUS:
            return QColor(255, 255, 200); // 浅黄色
        case HardwareModule::MEMORY_CTRL:
            return QColor(255, 200, 255); // 浅紫色
        case HardwareModule::DMA:
            return QColor(200, 255, 255); // 浅青色
        default:
            return QColor(240, 240, 240); // 浅灰色
    }
}

QString HardwareVisualizer::getModuleTypeName(HardwareModule::ModuleType type) const
{
    switch (type) {
        case HardwareModule::CPU_CORE:
            return "CPU Core";
        case HardwareModule::CACHE_L2:
            return "L2 Cache";
        case HardwareModule::CACHE_L3:
            return "L3 Cache";
        case HardwareModule::BUS:
            return "Bus";
        case HardwareModule::MEMORY_CTRL:
            return "Memory Controller";
        case HardwareModule::DMA:
            return "DMA Controller";
        default:
            return "Unknown";
    }
}

QPointF HardwareVisualizer::getConnectionPoint(HardwareModule* module, const QPointF& otherPos)
{
    QPointF pos = module->position();
    QPointF center(pos.x() + 75, pos.y() + 50);
    
    // 根据模块的相对位置决定连接点
    if (otherPos.x() > pos.x()) {
        // 如果目标在右边，使用右侧连接点
        return QPointF(pos.x() + 150, center.y());
    } else if (otherPos.x() < pos.x()) {
        // 如果目标在左边，使用左侧连接点
        return QPointF(pos.x(), center.y());
    } else if (otherPos.y() > pos.y()) {
        // 如果目标在下方，使用底部连接点
        return QPointF(center.x(), pos.y() + 100);
    } else {
        // 如果目标在上方，使用顶部连接点
        return QPointF(center.x(), pos.y());
    }
}

void HardwareVisualizer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        HardwareModule* module = getModuleAtPosition(scenePos);
        
        if (module) {
            // 如果已经有对话框，先删除它
            if (m_infoDialog) {
                delete m_infoDialog;
                m_infoDialog = nullptr;
            }
            
            // 创建新的对话框
            m_infoDialog = new ModuleInfoDialog(module, m_moduleItems, this);
            m_infoDialog->show();
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

HardwareModule* HardwareVisualizer::getModuleAtPosition(const QPointF& pos) const
{
    QGraphicsItem* item = scene()->itemAt(pos, transform());
    if (!item) return nullptr;

    // 如果点击到的是组内项目，获取其父组
    if (item->group()) {
        item = item->group();
    }

    // 查找对应的模块
    for (auto it = m_moduleItems.begin(); it != m_moduleItems.end(); ++it) {
        if (it.value() == item) {
            return it.key();
        }
    }
    
    return nullptr;
} 