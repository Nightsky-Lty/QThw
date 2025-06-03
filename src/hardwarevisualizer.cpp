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
#include <QGraphicsDropShadowEffect>
#include <QPixmap>
#include <QPainter>

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
    
    // 设置现代化背景
    QLinearGradient bgGradient(0, -500, 0, 500);
    bgGradient.setColorAt(0, QColor(32, 33, 36));
    bgGradient.setColorAt(1, QColor(42, 43, 46));
    setBackgroundBrush(QBrush(bgGradient));
    
    // 加载模块图标
    loadModuleIcons();
    
    // 绘制网格线
    drawGrid();
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
    
    // 连接位置变化的信号
    connect(module, &HardwareModule::positionChanged,
            this, [this, module](const QPointF &newPos) {
                if (auto item = m_moduleItems.value(module)) {
                    if (item->pos() != newPos) {
                        item->setPos(newPos);
                        drawConnections();
                    }
                }
            });
    
    // 连接统计数据变化的信号
    connect(module, &HardwareModule::statisticsChanged,
            this, [this, module]() {
                updateStatistics(module);
            });
            
    // 如果已经有总线模块，执行自动布局
    if (m_busModule) {
        autoLayout();
    }
}

QGraphicsItem* HardwareVisualizer::createModuleItem(HardwareModule* module)
{
    QGraphicsItemGroup* group = new QGraphicsItemGroup;
    
    // 使用硬件图标代替矩形
    QGraphicsPixmapItem* pixmapItem = new QGraphicsPixmapItem(m_moduleIcons[module->type()]);
    
    // 添加阴影效果
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setOffset(3, 3);
    shadow->setBlurRadius(10);
    shadow->setColor(QColor(0, 0, 0, 80));
    pixmapItem->setGraphicsEffect(shadow);
    
    group->addToGroup(pixmapItem);
    
    // 添加模块名称 - 位置放在图标下方
    QGraphicsTextItem* nameText = new QGraphicsTextItem(module->name());
    nameText->setDefaultTextColor(Qt::white);
    QFont nameFont = nameText->font();
    nameFont.setBold(true);
    nameFont.setPointSize(10);
    nameText->setFont(nameFont);
    nameText->setPos(10, -10);
    nameText->setData(Qt::UserRole, "name");  // 添加标识
    group->addToGroup(nameText);

    // 添加统计信息
    QString statsText = createStatsText(module);
    QGraphicsTextItem* statsTextItem = new QGraphicsTextItem(statsText);
    statsTextItem->setDefaultTextColor(Qt::white);
    QFont statsFont = statsTextItem->font();
    statsFont.setPointSize(8);
    statsTextItem->setFont(statsFont);
    statsTextItem->setPos(10, 90);
    statsTextItem->setData(Qt::UserRole, "stats");  // 添加标识
    group->addToGroup(statsTextItem);
    
    // 移除可移动标志，只保留可选择标志
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
    
    // 添加调试输出
    // qDebug() << "Creating stats text for module:" << module->name();
    // qDebug() << "Stats count:" << stats.size();
    // for (auto it = stats.begin(); it != stats.end(); ++it) {
    //     qDebug() << "  Key:" << it.key() << "Value:" << it.value();
    // }
    
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
    
    // qDebug() << "Final stats text:" << text;
    return text;
}

void HardwareVisualizer::updateStatistics(HardwareModule* module)
{
    if (auto item = m_moduleItems.value(module)) {
        // 更新统计信息
        for (auto child : item->childItems()) {
            if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(child)) {
                // 使用数据标识来识别统计信息文本项
                if (textItem->data(Qt::UserRole).toString() == "stats") {
                    QString newStatsText = createStatsText(module);
                    textItem->setPlainText(newStatsText);
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
            return QColor(65, 105, 225);  // 皇家蓝
        case HardwareModule::CACHE_L2:
            return QColor(60, 179, 113);  // 中海绿
        case HardwareModule::CACHE_L3:
            return QColor(106, 90, 205);  // 石板蓝
        case HardwareModule::BUS:
            return QColor(255, 165, 0);   // 橙色
        case HardwareModule::MEMORY_CTRL:
            return QColor(186, 85, 211);  // 适中的兰花紫
        case HardwareModule::DMA:
            return QColor(30, 144, 255);  // 道奇蓝
        case HardwareModule::CACHE_EVENT_TRACER:
            return QColor(250, 128, 114); // 鲑鱼色
        default:
            return QColor(169, 169, 169); // 暗灰色
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

void HardwareVisualizer::drawGrid()
{
    const int gridSize = 50;
    const int sceneWidth = 1000;
    const int sceneHeight = 1000;
    
    QPen gridPen(QColor(60, 60, 60, 120), 1, Qt::DotLine);
    
    // 绘制水平线
    for (int y = -sceneHeight/2; y <= sceneHeight/2; y += gridSize) {
        QGraphicsLineItem *line = m_scene->addLine(-sceneWidth/2, y, sceneWidth/2, y, gridPen);
        line->setZValue(-1000); // 确保网格在背景中
    }
    
    // 绘制垂直线
    for (int x = -sceneWidth/2; x <= sceneWidth/2; x += gridSize) {
        QGraphicsLineItem *line = m_scene->addLine(x, -sceneHeight/2, x, sceneHeight/2, gridPen);
        line->setZValue(-1000); // 确保网格在背景中
    }
}

void HardwareVisualizer::loadModuleIcons()
{
    // 从资源文件加载图标
    m_moduleIcons[HardwareModule::CPU_CORE] = QPixmap(":/icons/cpu.png");
    m_moduleIcons[HardwareModule::CACHE_L2] = QPixmap(":/icons/l2cache.png");
    m_moduleIcons[HardwareModule::CACHE_L3] = QPixmap(":/icons/l3cache.png");
    m_moduleIcons[HardwareModule::BUS] = QPixmap(":/icons/bus.png");
    m_moduleIcons[HardwareModule::MEMORY_CTRL] = QPixmap(":/icons/memory.png");
    m_moduleIcons[HardwareModule::DMA] = QPixmap(":/icons/dma.png");
    m_moduleIcons[HardwareModule::CACHE_EVENT_TRACER] = QPixmap(":/icons/tracer.png");
    
    // 检查图标是否成功加载，如果没有则使用默认图标
    for (auto it = m_moduleIcons.begin(); it != m_moduleIcons.end(); ++it) {
        if (it.value().isNull()) {
            // 如果图标加载失败，创建一个默认图标
            QPixmap defaultIcon(150, 100);
            defaultIcon.fill(Qt::transparent);
            QPainter painter(&defaultIcon);
            painter.setRenderHint(QPainter::Antialiasing);
            
            // 根据模块类型设置不同的颜色
            QColor color;
            switch (it.key()) {
                case HardwareModule::CPU_CORE:
                    color = QColor(65, 105, 225); // 蓝色
                    break;
                case HardwareModule::CACHE_L2:
                    color = QColor(60, 179, 113); // 绿色
                    break;
                case HardwareModule::CACHE_L3:
                    color = QColor(106, 90, 205); // 紫色
                    break;
                case HardwareModule::BUS:
                    color = QColor(255, 165, 0);  // 橙色
                    break;
                case HardwareModule::MEMORY_CTRL:
                    color = QColor(186, 85, 211); // 紫色
                    break;
                case HardwareModule::DMA:
                    color = QColor(30, 144, 255); // 蓝色
                    break;
                case HardwareModule::CACHE_EVENT_TRACER:
                    color = QColor(250, 128, 114); // 红色
                    break;
            }
            
            QLinearGradient gradient(0, 0, 0, 100);
            gradient.setColorAt(0, color.lighter(120));
            gradient.setColorAt(1, color);
            
            painter.setPen(QPen(Qt::black, 1));
            painter.setBrush(QBrush(gradient));
            painter.drawRoundedRect(10, 10, 130, 80, 10, 10);
            
            // 在图标中央显示模块类型名称
            painter.setPen(Qt::white);
            QFont font = painter.font();
            font.setBold(true);
            font.setPointSize(12);
            painter.setFont(font);
            
            QString typeName = getModuleTypeName(it.key());
            QFontMetrics fm(font);
            int textWidth = fm.horizontalAdvance(typeName);
            painter.drawText(QPointF(75 - textWidth / 2, 55), typeName);
            
            it.value() = defaultIcon;
        }
    }
}

void HardwareVisualizer::setBackgroundBrush(const QBrush &brush)
{
    QGraphicsView::setBackgroundBrush(brush);
}

void HardwareVisualizer::mousePressEvent(QMouseEvent *event)
{
    QGraphicsView::mousePressEvent(event);
}

void HardwareVisualizer::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);
}

void HardwareVisualizer::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);
} 
