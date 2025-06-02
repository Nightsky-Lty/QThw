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
#include <QQueue>

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
        
        // 提取硬件编号 - 匹配名称末尾的数字
        QRegularExpression re("(\\d+)$");  // 匹配字符串末尾的数字
        QRegularExpressionMatch match = re.match(module->name());
        int index = -1;
        if (match.hasMatch()) {
            index = match.captured(1).toInt();  // 从名称中提取硬件编号
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
            case HardwareModule::CACHE_EVENT_TRACER:
                busModules.append(module);
                break;
        }
    }

    // 首先设置CPU的位置
    QList<int> cpuIndices = cpuModules.keys();
    std::sort(cpuIndices.begin(), cpuIndices.end());  // 确保按索引顺序布局
    
    int rowCount = cpuIndices.size();
    double startY = centerY - (rowCount - 1) * verticalSpacing / 2;
    
    // 设置CPU位置并存储位置信息
    QMap<int, QPointF> cpuPositions;
    for (int i = 0; i < cpuIndices.size(); ++i) {
        int index = cpuIndices[i];
        double rowY = startY + i * verticalSpacing;
        
        // 设置CPU位置
        cpuModules[index]->setPosition(QPointF(cpuX, rowY));
        cpuPositions[index] = QPointF(cpuX, rowY);
    }

    // 为L2缓存设置位置 - 与对应的CPU保持相同的Y坐标
    for (auto it = l2Modules.begin(); it != l2Modules.end(); ++it) {
        HardwareModule* l2Module = it.value();
        int index = it.key();
        
        if (cpuPositions.contains(index)) {
            // 使用对应CPU的Y坐标，但X坐标固定在L2列
            QPointF cpuPos = cpuPositions[index];
            l2Module->setPosition(QPointF(l2X, cpuPos.y()));
        } else {
            // 如果没有对应的CPU，使用默认位置
            l2Module->setPosition(QPointF(l2X, centerY));
        }
    }

    // 为L3缓存设置位置 - 按照CPU的分布
    QList<int> l3Indices = l3Modules.keys();
    std::sort(l3Indices.begin(), l3Indices.end());
    
    if (!l3Indices.isEmpty()) {
        // 获取CPU的Y坐标范围
        double minY = centerY;
        double maxY = centerY;
        if (!cpuPositions.isEmpty()) {
            minY = maxY = cpuPositions.begin().value().y();
            for (auto pos : cpuPositions) {
                minY = qMin(minY, pos.y());
                maxY = qMax(maxY, pos.y());
            }
        }
        
        // 在CPU的Y坐标范围内均匀分布L3缓存
        double l3Range = qMax(maxY - minY, 10.0);  // 防止范围太小
        
        for (int i = 0; i < l3Indices.size(); ++i) {
            int l3Index = l3Indices[i];
            double yPos;
            
            if (l3Indices.size() == 1) {
                // 如果只有一个L3，放在中间
                yPos = (minY + maxY) / 2;
            } else {
                // 多个L3缓存均匀分布
                yPos = minY + (l3Range * i) / (l3Indices.size() - 1);
            }
            
            l3Modules[l3Index]->setPosition(QPointF(l3X, yPos));
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
        } else if (module->type() == HardwareModule::CACHE_EVENT_TRACER) {
            // 缓存事件追踪器放在DMA上方
            pos = QPointF(dmaX, centerY - verticalSpacing * 1.5);
        }
        module->setPosition(pos);
    }

    // 重绘连接
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
            item->type() == QGraphicsPathItem::Type ||
            (item->type() == QGraphicsTextItem::Type && 
             item->parentItem() == nullptr)) {
            m_scene->removeItem(item);
            delete item;
        }
    }

    if (!m_busModule) return;

    // 用于记录已经绘制的连接
    QSet<QPair<HardwareModule*, HardwareModule*>> drawnConnections;

    // 直接枚举所有模块对，检查它们之间是否应该有逻辑连接
    for (auto it1 = m_moduleItems.begin(); it1 != m_moduleItems.end(); ++it1) {
        HardwareModule* fromModule = it1.key();
        
        for (auto it2 = m_moduleItems.begin(); it2 != m_moduleItems.end(); ++it2) {
            HardwareModule* toModule = it2.key();
            
            // 跳过自身连接
            if (fromModule == toModule) continue;
            
            // 检查是否已经绘制过这对模块的连接
            QPair<HardwareModule*, HardwareModule*> connectionPair(
                qMin(fromModule, toModule), qMax(fromModule, toModule));
            if (drawnConnections.contains(connectionPair)) continue;
            
            // 检查是否为有效的逻辑连接
            if (!isValidLogicalConnection(fromModule, toModule)) continue;
            
            // 标记为已绘制
            drawnConnections.insert(connectionPair);
            
            // 获取连接点
            QPointF fromPoint = getConnectionPoint(fromModule, toModule->position());
            QPointF toPoint = getConnectionPoint(toModule, fromModule->position());
            
            // 获取模块类型，用于确定连线样式
            auto fromType = fromModule->type();
            auto toType = toModule->type();
            
            // 计算弯曲程度 - 根据模块类型确定
            double curvature = 0.2;  // 默认弯曲程度
            
            // 根据模块组合类型调整弯曲程度
            if ((fromType == HardwareModule::CPU_CORE && toType == HardwareModule::CACHE_L2) ||
                (fromType == HardwareModule::CACHE_L2 && toType == HardwareModule::CPU_CORE)) {
                curvature = 0.1;  // CPU和L2缓存之间的连线弯曲较小
            } else if ((fromType == HardwareModule::CACHE_L2 && toType == HardwareModule::CACHE_L3) ||
                      (fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::CACHE_L2)) {
                curvature = 0.15;  // L2和L3缓存之间的连线中等弯曲
            } else if ((fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::BUS) ||
                      (fromType == HardwareModule::BUS && toType == HardwareModule::CACHE_L3)) {
                curvature = 0.25;  // L3缓存和总线之间的连线弯曲较大
            }
            
            // 计算中点和控制点
            QPointF midPoint = (fromPoint + toPoint) / 2;
            double dist = QLineF(fromPoint, toPoint).length() * curvature;
            
            // 计算垂直于连线的方向向量
            QPointF dir = toPoint - fromPoint;
            double len = QLineF(QPointF(0, 0), dir).length();
            QPointF normal(-dir.y() / len, dir.x() / len);
            
            // 创建路径
            QPainterPath path;
            path.moveTo(fromPoint);
            QPointF ctrl = midPoint + normal * dist;
            path.quadTo(ctrl, toPoint);
            
            // 根据模块类型设置连线颜色
            QColor lineColor;
            
            if ((fromType == HardwareModule::CPU_CORE && toType == HardwareModule::CACHE_L2) ||
                (fromType == HardwareModule::CACHE_L2 && toType == HardwareModule::CPU_CORE)) {
                lineColor = QColor(220, 20, 60);  // 猩红色 - CPU到L2缓存
            } else if ((fromType == HardwareModule::CACHE_L2 && toType == HardwareModule::CACHE_L3) ||
                      (fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::CACHE_L2)) {
                lineColor = QColor(0, 128, 0);    // 绿色 - L2到L3缓存
            } else if ((fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::BUS) ||
                      (fromType == HardwareModule::BUS && toType == HardwareModule::CACHE_L3)) {
                lineColor = QColor(70, 130, 180); // 钢蓝色 - L3到总线
            } else if ((fromType == HardwareModule::BUS && toType == HardwareModule::MEMORY_CTRL) ||
                      (fromType == HardwareModule::MEMORY_CTRL && toType == HardwareModule::BUS)) {
                lineColor = QColor(255, 140, 0);  // 深橙色 - 总线到内存控制器
            } else if ((fromType == HardwareModule::BUS && toType == HardwareModule::DMA) ||
                      (fromType == HardwareModule::DMA && toType == HardwareModule::BUS)) {
                lineColor = QColor(138, 43, 226); // 紫罗兰色 - 总线到DMA
            } else if ((fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::CACHE_L3)) {
                lineColor = QColor(30, 144, 255); // 道奇蓝 - L3缓存间通信
            } else {
                lineColor = QColor(105, 105, 105); // 暗灰色 - 其他连接
            }
            
            // 设置线条样式
            QPen pen(lineColor, 1.5);
            
            // 创建并添加路径
            QGraphicsPathItem* pathItem = new QGraphicsPathItem(path);
            pathItem->setPen(pen);
            m_scene->addItem(pathItem);
            
            // 添加箭头指示方向
            QPointF direction = toPoint - fromPoint;
            double length = QLineF(QPointF(0, 0), direction).length();
            if (length > 0) {
                direction = QPointF(direction.x() / length, direction.y() / length);
                
                // 箭头点坐标计算
                QPointF arrowP1 = toPoint - direction * 15 + QPointF(-direction.y(), direction.x()) * 5;
                QPointF arrowP2 = toPoint - direction * 15 - QPointF(-direction.y(), direction.x()) * 5;
                
                // 创建箭头路径
                QPainterPath arrowPath;
                arrowPath.moveTo(toPoint);
                arrowPath.lineTo(arrowP1);
                arrowPath.lineTo(arrowP2);
                arrowPath.closeSubpath();
                
                // 添加箭头
                QGraphicsPathItem* arrow = new QGraphicsPathItem(arrowPath);
                arrow->setBrush(lineColor);
                arrow->setPen(QPen(lineColor));
                m_scene->addItem(arrow);
            }
            
            // 可选：添加连接类型标签
            QString connectionType;
            if ((fromType == HardwareModule::CPU_CORE && toType == HardwareModule::CACHE_L2) ||
                (fromType == HardwareModule::CACHE_L2 && toType == HardwareModule::CPU_CORE)) {
                connectionType = "CPU-L2";
            } else if ((fromType == HardwareModule::CACHE_L2 && toType == HardwareModule::CACHE_L3) ||
                      (fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::CACHE_L2)) {
                connectionType = "L2-L3";
            } else if ((fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::BUS) ||
                      (fromType == HardwareModule::BUS && toType == HardwareModule::CACHE_L3)) {
                connectionType = "L3-Bus";
            } else if ((fromType == HardwareModule::BUS && toType == HardwareModule::MEMORY_CTRL) ||
                      (fromType == HardwareModule::MEMORY_CTRL && toType == HardwareModule::BUS)) {
                connectionType = "Bus-Mem";
            } else if ((fromType == HardwareModule::BUS && toType == HardwareModule::DMA) ||
                      (fromType == HardwareModule::DMA && toType == HardwareModule::BUS)) {
                connectionType = "Bus-DMA";
            } else if ((fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::CACHE_L3)) {
                connectionType = "L3-L3";
            }
            
            // 获取可能的数据流量信息（可选显示）
            double dataRate = 0.0;
            int fromPort = fromModule->portId();
            int toPort = toModule->portId();
            
            if (fromPort >= 0 && toPort >= 0) {
                QString key = QString("transmit_package_number_from_%1_to_%2")
                            .arg(fromPort).arg(toPort);
                dataRate = m_busModule->statistic(key);
            }
            
            // 根据需要决定是否显示数据流量
            bool showDataRate = false;  // 设置为true可以同时显示连接类型和数据流量
            
            if (!connectionType.isEmpty()) {
                QString labelText = connectionType;
                if (showDataRate && dataRate > 0) {
                    labelText += QString(" (%1)").arg(int(dataRate));
                }
                
                QGraphicsTextItem* typeLabel = new QGraphicsTextItem(labelText);
                typeLabel->setDefaultTextColor(lineColor);
                typeLabel->setPos(midPoint + normal * dist * 0.5);
                m_scene->addItem(typeLabel);
            }
        }
    }
}

double HardwareVisualizer::getDataTransferRate(HardwareModule* from, HardwareModule* to) const
{
    if (!from || !to || !m_busModule) return 0.0;
    
    // 获取端口ID
    int fromPort = from->portId();
    int toPort = to->portId();
    
    if (fromPort >= 0 && toPort >= 0) {
        // 先尝试从→到的直接连接
        QString key1 = QString("transmit_package_number_from_%1_to_%2")
                      .arg(fromPort).arg(toPort);
        double rate1 = m_busModule->statistic(key1);
        
        // 再尝试到→从的反向连接
        QString key2 = QString("transmit_package_number_from_%1_to_%2")
                      .arg(toPort).arg(fromPort);
        double rate2 = m_busModule->statistic(key2);  
        
        // 返回两个方向上较大的传输率
        return qMax(rate1, rate2);
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
        case HardwareModule::CACHE_EVENT_TRACER: {
            // 显示缓存事件统计信息
            if (stats.contains("l1miss_l2hit_cnt")) {
                text += formatStatistic("L1 Miss, L2 Hit", stats["l1miss_l2hit_cnt"]) + "\n";
            }
            if (stats.contains("l1miss_l2miss_l3hit_cnt")) {
                text += formatStatistic("L2 Miss, L3 Hit", stats["l1miss_l2miss_l3hit_cnt"]) + "\n";
            }
            if (stats.contains("l1miss_l2miss_l3miss_cnt")) {
                text += formatStatistic("L3 Miss (Mem)", stats["l1miss_l2miss_l3miss_cnt"]) + "\n";
            }
            if (stats.contains("l1miss_l2miss_l3forward_cnt")) {
                text += formatStatistic("L3 Forward", stats["l1miss_l2miss_l3forward_cnt"]) + "\n";
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
        case HardwareModule::CACHE_EVENT_TRACER:
            return QColor(255, 228, 181); // 浅杏色
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
        case HardwareModule::CACHE_EVENT_TRACER:
            return "Cache Event Tracer";
        default:
            return "Unknown Module";
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

bool HardwareVisualizer::isValidLogicalConnection(HardwareModule* from, HardwareModule* to) const
{
    if (!from || !to) return false;
    
    auto fromType = from->type();
    auto toType = to->type();

    // 缓存事件追踪器不与其他模块建立连接
    if (fromType == HardwareModule::CACHE_EVENT_TRACER || toType == HardwareModule::CACHE_EVENT_TRACER) {
        return false;
    }

    // 提取硬件编号
    auto getHardwareIndex = [](HardwareModule* module) -> int {
        // 从名称提取最后的数字作为硬件编号
        QRegularExpression re("(\\d+)$");  // 修改为只匹配末尾的数字
        QRegularExpressionMatch match = re.match(module->name());
        if (match.hasMatch()) {
            return match.captured(1).toInt();  // 使用捕获组1
        }
        return -1;
    };

    int fromIndex = getHardwareIndex(from);
    int toIndex = getHardwareIndex(to);

    // CPU核心只与自己对应的L2缓存通信
    if (fromType == HardwareModule::CPU_CORE && toType == HardwareModule::CACHE_L2) {
        // 检查是否为对应的CPU和L2（通常是相同索引）
        return fromIndex == toIndex;
    }
    
    // L2缓存只与对应的CPU和任何一个L3缓存bank通信
    if (fromType == HardwareModule::CACHE_L2) {
        if (toType == HardwareModule::CPU_CORE) {
            // 检查是否为对应的L2和CPU
            return fromIndex == toIndex;
        }
        else if (toType == HardwareModule::CACHE_L3) {
            // 任何L2缓存都可以访问任何L3缓存bank
            // 在实际系统中，这取决于内存地址映射
            return true;
        }
        else {
            // L2缓存不与其他类型的模块直接通信
            return false;
        }
    }
    
    // L3缓存bank与任何L2缓存和总线通信
    if (fromType == HardwareModule::CACHE_L3) {
        if (toType == HardwareModule::CACHE_L2) {
            // 任何L3缓存bank都可以与任何L2缓存通信
            return true;
        }
        else if (toType == HardwareModule::BUS) {
            // L3缓存可以与总线通信
            return true;
        }
        else if (toType == HardwareModule::CACHE_L3) {
            // L3缓存bank之间可以互相通信以保持一致性
            return true;
        }
        else {
            // L3缓存不与其他类型的模块直接通信
            return false;
        }
    }
    
    // 总线可以与L3缓存、内存控制器和DMA通信
    if (fromType == HardwareModule::BUS) {
        return toType == HardwareModule::CACHE_L3 || 
               toType == HardwareModule::MEMORY_CTRL || 
               toType == HardwareModule::DMA;
    }
    
    // 内存控制器只与总线通信
    if (fromType == HardwareModule::MEMORY_CTRL) {
        return toType == HardwareModule::BUS;
    }
    
    // DMA控制器只与总线通信
    if (fromType == HardwareModule::DMA) {
        return toType == HardwareModule::BUS;
    }
    
    // 其他情况默认为无效连接
    return false;
} 
