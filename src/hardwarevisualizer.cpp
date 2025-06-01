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

HardwareVisualizer::HardwareVisualizer(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_draggedItem(nullptr)
    , m_busModule(nullptr)
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
    const double verticalSpacing = 150.0;    // 层之间的垂直间距

    // 计算每层的垂直位置
    const double cpuY = centerY - verticalSpacing * 2;    // CPU层
    const double l2Y = centerY - verticalSpacing;         // L2缓存层
    const double l3Y = centerY;                           // L3缓存层
    const double busY = centerY + verticalSpacing;        // 总线层

    // 为每个模块分配位置
    QList<HardwareModule*> cpuModules;      // CPU核心
    QList<HardwareModule*> l2Modules;       // L2缓存
    QList<HardwareModule*> l3Modules;       // L3缓存
    QList<HardwareModule*> busModules;      // 总线、内存控制器、DMA
    
    // 对模块进行分类
    for (auto it = m_moduleItems.begin(); it != m_moduleItems.end(); ++it) {
        HardwareModule* module = it.key();
        switch (module->type()) {
            case HardwareModule::CPU_CORE:
                cpuModules.append(module);
                break;
            case HardwareModule::CACHE_L2:
                l2Modules.append(module);
                break;
            case HardwareModule::CACHE_L3:
                l3Modules.append(module);
                break;
            case HardwareModule::BUS:
            case HardwareModule::MEMORY_CTRL:
            case HardwareModule::DMA:
                busModules.append(module);
                break;
        }
    }

    // 排序模块以确保顺序一致
    auto moduleSort = [](HardwareModule* a, HardwareModule* b) {
        return a->name().right(1).toInt() < b->name().right(1).toInt();
    };
    std::sort(cpuModules.begin(), cpuModules.end(), moduleSort);
    std::sort(l2Modules.begin(), l2Modules.end(), moduleSort);
    std::sort(l3Modules.begin(), l3Modules.end(), moduleSort);

    // 布局CPU核心
    int cpuCount = cpuModules.size();
    double cpuStartX = centerX - (cpuCount - 1) * horizontalSpacing / 2;
    for (int i = 0; i < cpuCount; ++i) {
        cpuModules[i]->setPosition(QPointF(cpuStartX + i * horizontalSpacing, cpuY));
    }

    // 布局L2缓存（在对应CPU下方）
    for (auto l2Cache : l2Modules) {
        int index = l2Cache->name().right(1).toInt() - 1;
        if (index >= 0 && index < cpuCount) {
            QPointF cpuPos = cpuModules[index]->position();
            l2Cache->setPosition(QPointF(cpuPos.x(), l2Y));
        }
    }

    // 布局L3缓存（每两个CPU共享一个L3）
    int l3Count = l3Modules.size();
    double l3StartX = centerX - (l3Count - 1) * horizontalSpacing * 1.2 / 2;
    for (int i = 0; i < l3Count; ++i) {
        l3Modules[i]->setPosition(QPointF(l3StartX + i * horizontalSpacing * 1.2, l3Y));
    }

    // 布局总线层（总线在中间，DMA在左边，内存控制器在右边）
    for (auto module : busModules) {
        QPointF pos;
        if (module->type() == HardwareModule::BUS) {
            pos = QPointF(centerX, busY);
        } else if (module->type() == HardwareModule::DMA) {
            pos = QPointF(centerX - horizontalSpacing * 1.5, busY);
        } else if (module->type() == HardwareModule::MEMORY_CTRL) {
            pos = QPointF(centerX + horizontalSpacing * 1.5, busY);
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
            int portId = -1;
            switch (module->type()) {
                case HardwareModule::CPU_CORE:
                    portId = module->name().right(1).toInt();
                    break;
                case HardwareModule::CACHE_L2:
                    portId = module->name().right(1).toInt();
                    break;
                case HardwareModule::CACHE_L3:
                    portId = module->name().right(1).toInt() + 4;
                    break;
                case HardwareModule::MEMORY_CTRL:
                    portId = 4;
                    break;
                default:
                    break;
            }
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
            // 计算连接线的起点和终点
            QPointF fromPos = fromModule->position();
            QPointF toPos = toModule->position();
            
            // 根据模块类型调整连接点
            QPointF fromCenter, toCenter;
            
            // 确定连接点位置
            auto getConnectionPoint = [](HardwareModule* module, const QPointF& otherPos) {
                QPointF pos = module->position();
                QPointF center(pos.x() + 75, pos.y() + 50);
                
                // 根据模块类型和相对位置调整连接点
                if (module->type() == HardwareModule::CPU_CORE || 
                    module->type() == HardwareModule::CACHE_L2) {
                    // CPU和L2缓存优先使用底部连接
                    return QPointF(center.x(), pos.y() + 100);
                } else if (module->type() == HardwareModule::CACHE_L3) {
                    // L3缓存根据相对位置选择连接点
                    if (otherPos.y() > pos.y()) {
                        return QPointF(center.x(), pos.y() + 100); // 底部
                    } else {
                        return QPointF(center.x(), pos.y()); // 顶部
                    }
                } else {
                    // 其他模块使用中心点
                    return center;
                }
            };
            
            fromCenter = getConnectionPoint(fromModule, toPos);
            toCenter = getConnectionPoint(toModule, fromPos);
            
            // 创建贝塞尔曲线路径
            QPainterPath path;
            path.moveTo(fromCenter);
            
            // 计算控制点
            QPointF midPoint = (fromCenter + toCenter) / 2;
            QPointF ctrl1, ctrl2;
            
            // 根据模块类型和位置关系调整控制点
            double dx = (toCenter.x() - fromCenter.x()) * 0.25;
            double dy = (toCenter.y() - fromCenter.y()) * 0.25;
            
            // 对于垂直连接使用更大的曲线
            if (qAbs(toCenter.y() - fromCenter.y()) > qAbs(toCenter.x() - fromCenter.x())) {
                dx *= 2;
            }
            
            ctrl1 = QPointF(fromCenter.x() + dx, fromCenter.y() + dy);
            ctrl2 = QPointF(toCenter.x() - dx, toCenter.y() - dy);
            
            path.cubicTo(ctrl1, ctrl2, toCenter);
            
            // 创建路径项
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