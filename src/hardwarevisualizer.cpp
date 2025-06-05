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
    
    m_scene->setSceneRect(-500, -500, 1000, 1000);
    
    QLinearGradient bgGradient(0, -500, 0, 500);
    bgGradient.setColorAt(0, QColor(32, 33, 36));
    bgGradient.setColorAt(1, QColor(42, 43, 46));
    setBackgroundBrush(QBrush(bgGradient));
    
    loadModuleIcons();
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

    if (module->type() == HardwareModule::BUS) {
        m_busModule = module;
    }

    QGraphicsItem* item = createModuleItem(module);
    m_moduleItems[module] = item;
    m_scene->addItem(item);
    
    connect(module, &HardwareModule::positionChanged,
            this, [this, module](const QPointF &newPos) {
                if (auto item = m_moduleItems.value(module)) {
                    if (item->pos() != newPos) {
                        item->setPos(newPos);
                        drawConnections();
                    }
                }
            });
    
    connect(module, &HardwareModule::statisticsChanged,
            this, [this, module]() {
                updateStatistics(module);
            });
            
    if (m_busModule) {
        autoLayout();
    }
}

QGraphicsItem* HardwareVisualizer::createModuleItem(HardwareModule* module)
{
    QGraphicsItemGroup* group = new QGraphicsItemGroup;
    
    QGraphicsPixmapItem* pixmapItem = new QGraphicsPixmapItem(m_moduleIcons[module->type()]);
    
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect();
    shadow->setOffset(3, 3);
    shadow->setBlurRadius(10);
    shadow->setColor(QColor(0, 0, 0, 80));
    pixmapItem->setGraphicsEffect(shadow);
    
    group->addToGroup(pixmapItem);
    
    QGraphicsTextItem* nameText = new QGraphicsTextItem(module->name());
    nameText->setDefaultTextColor(Qt::white);
    QFont nameFont = nameText->font();
    nameFont.setBold(true);
    nameFont.setPointSize(10);
    nameText->setFont(nameFont);
    nameText->setPos(10, -10);
    nameText->setData(Qt::UserRole, "name");
    group->addToGroup(nameText);

    QString statsText = createStatsText(module);
    QGraphicsTextItem* statsTextItem = new QGraphicsTextItem(statsText);
    statsTextItem->setDefaultTextColor(Qt::white);
    QFont statsFont = statsTextItem->font();
    statsFont.setPointSize(8);
    statsTextItem->setFont(statsFont);
    statsTextItem->setPos(10, 90);
    statsTextItem->setData(Qt::UserRole, "stats");
    group->addToGroup(statsTextItem);
    
    return group;
}

void HardwareVisualizer::autoLayout()
{
    if (!m_busModule) return;

    const double centerX = 0.0;
    const double centerY = 0.0;
    const double moduleWidth = 150.0;
    const double moduleHeight = 100.0;
    const double horizontalSpacing = 200.0;
    const double verticalSpacing = 150.0;

    const double cpuX = centerX - horizontalSpacing * 2.5;
    const double l2X = centerX - horizontalSpacing * 1.5;
    const double l3X = centerX - horizontalSpacing * 0.5;
    const double busX = centerX + horizontalSpacing * 0.5;
    const double dmaX = centerX + horizontalSpacing * 1.5;

    QMap<int, HardwareModule*> cpuModules;
    QMap<int, HardwareModule*> l2Modules;
    QMap<int, HardwareModule*> l3Modules;
    QList<HardwareModule*> busModules;
    
    for (auto it = m_moduleItems.begin(); it != m_moduleItems.end(); ++it) {
        HardwareModule* module = it.key();
        
        QRegularExpression re("(\\d+)$");
        QRegularExpressionMatch match = re.match(module->name());
        int index = -1;
        if (match.hasMatch()) {
            index = match.captured(1).toInt();
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

    QList<int> cpuIndices = cpuModules.keys();
    std::sort(cpuIndices.begin(), cpuIndices.end());
    
    int rowCount = cpuIndices.size();
    double startY = centerY - (rowCount - 1) * verticalSpacing / 2;
    
    QMap<int, QPointF> cpuPositions;
    for (int i = 0; i < cpuIndices.size(); ++i) {
        int index = cpuIndices[i];
        double rowY = startY + i * verticalSpacing;
        
        cpuModules[index]->setPosition(QPointF(cpuX, rowY));
        cpuPositions[index] = QPointF(cpuX, rowY);
    }

    for (auto it = l2Modules.begin(); it != l2Modules.end(); ++it) {
        HardwareModule* l2Module = it.value();
        int index = it.key();
        
        if (cpuPositions.contains(index)) {
            QPointF cpuPos = cpuPositions[index];
            l2Module->setPosition(QPointF(l2X, cpuPos.y()));
        } else {
            l2Module->setPosition(QPointF(l2X, centerY));
        }
    }

    QList<int> l3Indices = l3Modules.keys();
    std::sort(l3Indices.begin(), l3Indices.end());
    
    if (!l3Indices.isEmpty()) {
        double minY = centerY;
        double maxY = centerY;
        if (!cpuPositions.isEmpty()) {
            minY = maxY = cpuPositions.begin().value().y();
            for (auto pos : cpuPositions) {
                minY = qMin(minY, pos.y());
                maxY = qMax(maxY, pos.y());
            }
        }
        
        double l3Range = qMax(maxY - minY, 10.0);
        
        for (int i = 0; i < l3Indices.size(); ++i) {
            int l3Index = l3Indices[i];
            double yPos;
            
            if (l3Indices.size() == 1) {
                yPos = (minY + maxY) / 2;
            } else {
                yPos = minY + (l3Range * i) / (l3Indices.size() - 1);
            }
            
            l3Modules[l3Index]->setPosition(QPointF(l3X, yPos));
        }
    }

    for (auto module : busModules) {
        QPointF pos;
        if (module->type() == HardwareModule::BUS) {
            pos = QPointF(busX, centerY);
        } else if (module->type() == HardwareModule::DMA) {
            pos = QPointF(dmaX, centerY - verticalSpacing * 0.5);
        } else if (module->type() == HardwareModule::MEMORY_CTRL) {
            pos = QPointF(dmaX, centerY + verticalSpacing * 0.5);
        } else if (module->type() == HardwareModule::CACHE_EVENT_TRACER) {
            pos = QPointF(dmaX, centerY - verticalSpacing * 1.5);
        }
        module->setPosition(pos);
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

void HardwareVisualizer::wheelEvent(QWheelEvent *event)
{
    double scaleFactor = 1.15;
    if (event->angleDelta().y() < 0) {
        scaleFactor = 1.0 / scaleFactor;
    }
    scale(scaleFactor, scaleFactor);
}

void HardwareVisualizer::drawConnections()
{
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

    QSet<QPair<HardwareModule*, HardwareModule*>> drawnConnections;

    for (auto it1 = m_moduleItems.begin(); it1 != m_moduleItems.end(); ++it1) {
        HardwareModule* fromModule = it1.key();
        
        for (auto it2 = m_moduleItems.begin(); it2 != m_moduleItems.end(); ++it2) {
            HardwareModule* toModule = it2.key();
            
            if (fromModule == toModule) continue;
            
            QPair<HardwareModule*, HardwareModule*> connectionPair(
                qMin(fromModule, toModule), qMax(fromModule, toModule));
            if (drawnConnections.contains(connectionPair)) continue;
            
            if (!isValidLogicalConnection(fromModule, toModule)) continue;
            
            drawnConnections.insert(connectionPair);
            
            QPointF fromPoint = getConnectionPoint(fromModule, toModule->position());
            QPointF toPoint = getConnectionPoint(toModule, fromModule->position());
            
            auto fromType = fromModule->type();
            auto toType = toModule->type();
            
            double curvature = 0.2;
            
            if ((fromType == HardwareModule::CPU_CORE && toType == HardwareModule::CACHE_L2) ||
                (fromType == HardwareModule::CACHE_L2 && toType == HardwareModule::CPU_CORE)) {
                curvature = 0.1;
            } else if ((fromType == HardwareModule::CACHE_L2 && toType == HardwareModule::CACHE_L3) ||
                      (fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::CACHE_L2)) {
                curvature = 0.15;
            } else if ((fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::BUS) ||
                      (fromType == HardwareModule::BUS && toType == HardwareModule::CACHE_L3)) {
                curvature = 0.25;
            }
            
            QPointF midPoint = (fromPoint + toPoint) / 2;
            double dist = QLineF(fromPoint, toPoint).length() * curvature;
            
            QPointF dir = toPoint - fromPoint;
            double len = QLineF(QPointF(0, 0), dir).length();
            QPointF normal(-dir.y() / len, dir.x() / len);
            
            QPainterPath path;
            path.moveTo(fromPoint);
            QPointF ctrl = midPoint + normal * dist;
            path.quadTo(ctrl, toPoint);
            
            QColor lineColor;
            
            if ((fromType == HardwareModule::CPU_CORE && toType == HardwareModule::CACHE_L2) ||
                (fromType == HardwareModule::CACHE_L2 && toType == HardwareModule::CPU_CORE)) {
                lineColor = QColor(220, 20, 60);
            } else if ((fromType == HardwareModule::CACHE_L2 && toType == HardwareModule::CACHE_L3) ||
                      (fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::CACHE_L2)) {
                lineColor = QColor(0, 128, 0);
            } else if ((fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::BUS) ||
                      (fromType == HardwareModule::BUS && toType == HardwareModule::CACHE_L3)) {
                lineColor = QColor(70, 130, 180);
            } else if ((fromType == HardwareModule::BUS && toType == HardwareModule::MEMORY_CTRL) ||
                      (fromType == HardwareModule::MEMORY_CTRL && toType == HardwareModule::BUS)) {
                lineColor = QColor(255, 140, 0);
            } else if ((fromType == HardwareModule::BUS && toType == HardwareModule::DMA) ||
                      (fromType == HardwareModule::DMA && toType == HardwareModule::BUS)) {
                lineColor = QColor(138, 43, 226);
            } else if ((fromType == HardwareModule::CACHE_L3 && toType == HardwareModule::CACHE_L3)) {
                lineColor = QColor(30, 144, 255);
            } else {
                lineColor = QColor(105, 105, 105);
            }
            
            QPen pen(lineColor, 1.5);
            
            QGraphicsPathItem* pathItem = new QGraphicsPathItem(path);
            pathItem->setPen(pen);
            m_scene->addItem(pathItem);
        }
    }
}

double HardwareVisualizer::getDataTransferRate(HardwareModule* from, HardwareModule* to) const
{
    if (!from || !to || !m_busModule) return 0.0;
    
    int fromPort = from->portId();
    int toPort = to->portId();
    
    if (fromPort >= 0 && toPort >= 0) {
        QString key1 = QString("transmit_package_number_from_%1_to_%2")
                      .arg(fromPort).arg(toPort);
        double rate1 = m_busModule->statistic(key1);
        
        QString key2 = QString("transmit_package_number_from_%1_to_%2")
                      .arg(toPort).arg(fromPort);
        double rate2 = m_busModule->statistic(key2);  
        
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
        for (auto child : item->childItems()) {
            if (auto textItem = qgraphicsitem_cast<QGraphicsTextItem*>(child)) {
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
            return QColor(65, 105, 225);
        case HardwareModule::CACHE_L2:
            return QColor(60, 179, 113);
        case HardwareModule::CACHE_L3:
            return QColor(106, 90, 205);
        case HardwareModule::BUS:
            return QColor(255, 165, 0);
        case HardwareModule::MEMORY_CTRL:
            return QColor(186, 85, 211);
        case HardwareModule::DMA:
            return QColor(30, 144, 255);
        case HardwareModule::CACHE_EVENT_TRACER:
            return QColor(250, 128, 114);
        default:
            return QColor(169, 169, 169);
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
    
    if (otherPos.x() > pos.x()) {
        return QPointF(pos.x() + 150, center.y());
    } else if (otherPos.x() < pos.x()) {
        return QPointF(pos.x(), center.y());
    } else if (otherPos.y() > pos.y()) {
        return QPointF(center.x(), pos.y() + 100);
    } else {
        return QPointF(center.x(), pos.y());
    }
}

void HardwareVisualizer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF scenePos = mapToScene(event->pos());
        HardwareModule* module = getModuleAtPosition(scenePos);
        
        if (module) {
            if (m_infoDialog) {
                delete m_infoDialog;
                m_infoDialog = nullptr;
            }
            
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

    if (item->group()) {
        item = item->group();
    }

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

    if (fromType == HardwareModule::CACHE_EVENT_TRACER || toType == HardwareModule::CACHE_EVENT_TRACER) {
        return false;
    }

    auto getHardwareIndex = [](HardwareModule* module) -> int {
        QRegularExpression re("(\\d+)$");
        QRegularExpressionMatch match = re.match(module->name());
        if (match.hasMatch()) {
            return match.captured(1).toInt();
        }
        return -1;
    };

    int fromIndex = getHardwareIndex(from);
    int toIndex = getHardwareIndex(to);

    if (fromType == HardwareModule::CPU_CORE && toType == HardwareModule::CACHE_L2) {
        return fromIndex == toIndex;
    }
    
    if (fromType == HardwareModule::CACHE_L2) {
        if (toType == HardwareModule::CPU_CORE) {
            return fromIndex == toIndex;
        }
        else if (toType == HardwareModule::CACHE_L3) {
            return true;
        }
        else {
            return false;
        }
    }
    
    if (fromType == HardwareModule::CACHE_L3) {
        if (toType == HardwareModule::CACHE_L2) {
            return true;
        }
        else if (toType == HardwareModule::BUS) {
            return true;
        }
        else if (toType == HardwareModule::CACHE_L3) {
            return true;
        }
        else {
            return false;
        }
    }
    
    if (fromType == HardwareModule::BUS) {
        return toType == HardwareModule::CACHE_L3 || 
               toType == HardwareModule::MEMORY_CTRL || 
               toType == HardwareModule::DMA;
    }
    
    if (fromType == HardwareModule::MEMORY_CTRL) {
        return toType == HardwareModule::BUS;
    }
    
    if (fromType == HardwareModule::DMA) {
        return toType == HardwareModule::BUS;
    }
    
    return false;
}

void HardwareVisualizer::drawGrid()
{
    const int gridSize = 50;
    const int sceneWidth = 1000;
    const int sceneHeight = 1000;
    
    QPen gridPen(QColor(60, 60, 60, 120), 1, Qt::DotLine);
    
    for (int y = -sceneHeight/2; y <= sceneHeight/2; y += gridSize) {
        QGraphicsLineItem *line = m_scene->addLine(-sceneWidth/2, y, sceneWidth/2, y, gridPen);
        line->setZValue(-1000);
    }
    
    for (int x = -sceneWidth/2; x <= sceneWidth/2; x += gridSize) {
        QGraphicsLineItem *line = m_scene->addLine(x, -sceneHeight/2, x, sceneHeight/2, gridPen);
        line->setZValue(-1000);
    }
}

void HardwareVisualizer::loadModuleIcons()
{
    m_moduleIcons[HardwareModule::CPU_CORE] = QPixmap(":/icons/cpu.png");
    m_moduleIcons[HardwareModule::CACHE_L2] = QPixmap(":/icons/l2cache.png");
    m_moduleIcons[HardwareModule::CACHE_L3] = QPixmap(":/icons/l3cache.png");
    m_moduleIcons[HardwareModule::BUS] = QPixmap(":/icons/bus.png");
    m_moduleIcons[HardwareModule::MEMORY_CTRL] = QPixmap(":/icons/memory.png");
    m_moduleIcons[HardwareModule::DMA] = QPixmap(":/icons/dma.png");
    m_moduleIcons[HardwareModule::CACHE_EVENT_TRACER] = QPixmap(":/icons/tracer.png");
    
    for (auto it = m_moduleIcons.begin(); it != m_moduleIcons.end(); ++it) {
        if (it.value().isNull()) {
            QPixmap defaultIcon(150, 100);
            defaultIcon.fill(Qt::transparent);
            QPainter painter(&defaultIcon);
            painter.setRenderHint(QPainter::Antialiasing);
            
            QColor color;
            switch (it.key()) {
                case HardwareModule::CPU_CORE:
                    color = QColor(65, 105, 225);
                    break;
                case HardwareModule::CACHE_L2:
                    color = QColor(60, 179, 113);
                    break;
                case HardwareModule::CACHE_L3:
                    color = QColor(106, 90, 205);
                    break;
                case HardwareModule::BUS:
                    color = QColor(255, 165, 0);
                    break;
                case HardwareModule::MEMORY_CTRL:
                    color = QColor(186, 85, 211);
                    break;
                case HardwareModule::DMA:
                    color = QColor(30, 144, 255);
                    break;
                case HardwareModule::CACHE_EVENT_TRACER:
                    color = QColor(250, 128, 114);
                    break;
            }
            
            QLinearGradient gradient(0, 0, 0, 100);
            gradient.setColorAt(0, color.lighter(120));
            gradient.setColorAt(1, color);
            
            painter.setPen(QPen(Qt::black, 1));
            painter.setBrush(QBrush(gradient));
            painter.drawRoundedRect(10, 10, 130, 80, 10, 10);
            
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
